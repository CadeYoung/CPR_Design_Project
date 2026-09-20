#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Preferences.h>

// -----------------------------------------------------
// PIN DEFINITIONS
// -----------------------------------------------------

const int backButton = 4;
const int homeButton = 5;
const int snoozeButton = 6;
const int resetButton = 7;

const int photoPin = 8;

const int encoderA = 9;
const int encoderB = 10;
const int encoderSW = 11;

const int buzzerPin = 12;

// -----------------------------------------------------
// RTC
// -----------------------------------------------------

RTC_DS3231 rtc;
DateTime now;

// -----------------------------------------------------
// OLED
// -----------------------------------------------------

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

// -----------------------------------------------------
// SCREEN STATES
// -----------------------------------------------------

enum Screen {
  HOME_SCREEN,
  SETTINGS_MENU,
  SET_ALARMS_MENU,
  ALARM_OPTIONS_MENU,
  EDIT_ALARM_MENU,
  SET_ALARM_TIME_MENU,
  ALARM_SETTINGS_MENU,
  ALARM_SOUND_MENU,
  SNOOZE_SETTINGS_MENU,
  SNOOZE_LENGTH_MENU,
  SNOOZE_COUNT_MENU,
  ALARM_LENGTH_MENU,
  SET_TIME_MENU,
  SET_CLOCK_TIME_EDIT_MENU,
  SET_DATE_MENU,
  SET_STD_MILITARY_MENU,
  SET_BRIGHTNESS_MENU
};

Screen currentScreen = HOME_SCREEN;

// -----------------------------------------------------
// MENU VARIABLES
// -----------------------------------------------------

int menuIndex = 0;
int brightnessMenuIndex = 0;    // Brightness submenu cursor
int brightnessSetting = 0;      // Active brightness setting
                                // 0 = Automatic
                                // 1 = Low
                                // 2 = Medium
                                // 3 = High

int automaticBrightness = 0;    // Photoresistor-determined brightness
                                // 0 = Low
                                // 1 = Medium
                                // 2 = High

int timeFormat = 0;             // TIME FORMAT SETTINGS
                                //   0 = Standard 12-hour time with AM or PM
                                //   1 = Military 24-hour time

int timeFormatMenuIndex = 0;    // temp selection box on the Time Format page. value does NOT change the active format until the encoder button is clicked.

// -----------------------------------------------------
// ALARM SETTINGS
// -----------------------------------------------------

struct Alarm {                  // Every alarm keeps its own settings. The hour is stored in 24-hour time (0-23), even when the Home screen is set to Standard time.
  bool enabled;
  int hour;
  int minute;
  int sound;                    // 0 = Sound 1, 1 = Sound 2, 2 = Sound 3
  int snoozeMinutes;            // 5 - 15 min 
  int snoozeCount;              // 1 - 10 
  int lengthOption;             // 0 = 15m, 1 = 30m, 2 = 60m, 3 = Infinite
};

// Three independent alarms, as required by the design report.
Alarm alarms[3] = {
  {false, 7, 0, 0, 5, 3, 1},
  {false, 8, 0, 0, 5, 3, 1},
  {false, 9, 0, 0, 5, 3, 1}
};

// These variables remember the user's location in the alarm menu tree.
int alarmListIndex = 0;
int selectedAlarmIndex = 0;
int alarmOptionsIndex = 0;
int editAlarmIndex = 0;
int alarmSettingsIndex = 0;
int alarmSoundIndex = 0;
int snoozeSettingsIndex = 0;
int snoozeLengthIndex = 0;
int snoozeCountIndex = 0;
int alarmLengthIndex = 0;

// Time is edited in two steps: hour first, then minute.
int alarmTimeField = 0;
int editingAlarmHour = 0;
int editingAlarmMinute = 0;

// -----------------------------------------------------
// CLOCK TIME AND DATE EDITING
// -----------------------------------------------------

int setTimeDateIndex = 0;       // The Set Time/Date page uses one cursor for its two choices.


// Time editing values are copied from the RTC when the edit page opens.
// They are written back to the RTC only after the final confirmation press.
int editingClockHour = 0;
int editingClockMinute = 0;
int clockTimeField = 0;

// Date editing values follow the same copy-then-confirm pattern.
int editingClockMonth = 1;
int editingClockDay = 1;
int editingClockYear = 2026;
int clockDateField = 0;

int daysInMonth(int year, int month) {                            // Return the valid number of days for the selected month and year. This keeps the date valid when month or anything is switched

  if (month == 2) {
    bool leapYear = (year % 4 == 0 && year % 100 != 0) ||
      (year % 400 == 0);
    return leapYear ? 29 : 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) {    // tracking days in month based on given month
    return 30;
  }
  return 31;
}

// -----------------------------------------------------
// PERSISTENT SETTINGS (ESP32 NVS FLASH)
// -----------------------------------------------------

Preferences preferences;                                          // Preferences is included with the ESP32 Arduino board package. It writes cerain data and keeps its stored if esp gets unplugged

const char* SETTINGS_NAMESPACE = "clockcfg";
const char* SETTINGS_KEY = "config";
const uint8_t SETTINGS_VERSION = 2;

struct __attribute__((packed)) StoredAlarm {
  uint8_t enabled;
  uint8_t hour;
  uint8_t minute;
  uint8_t sound;
  uint8_t snoozeMinutes;
  uint8_t snoozeCount;
  uint8_t lengthOption;
};

struct __attribute__((packed)) StoredSettingsV1 {
  uint8_t version;
  uint8_t timeFormat;
  uint8_t brightnessSetting;
  StoredAlarm alarms[3];
};

struct __attribute__((packed)) StoredSettings {        //The DS3231 stores time data when power is lost 

  uint8_t version;
  uint8_t timeFormat;
  uint8_t brightnessSetting;
  uint16_t clockYear;
  uint8_t clockMonth;
  uint8_t clockDay;
  uint8_t clockHour;
  uint8_t clockMinute;
  uint8_t clockSecond;
  StoredAlarm alarms[3];
};

void applyStoredUserSettings(
  uint8_t savedTimeFormat,
  uint8_t savedBrightnessSetting,
  const void* savedAlarmData
) {

  const StoredAlarm* savedAlarms =
    static_cast<const StoredAlarm*>(savedAlarmData);

  timeFormat = (savedTimeFormat <= 1) ? savedTimeFormat : 0;
  brightnessSetting = (savedBrightnessSetting <= 3) ?
    savedBrightnessSetting : 0;

  for (int i = 0; i < 3; i++) {

    alarms[i].enabled = savedAlarms[i].enabled != 0;
    alarms[i].hour = (savedAlarms[i].hour <= 23) ? savedAlarms[i].hour : alarms[i].hour;
    alarms[i].minute = (savedAlarms[i].minute <= 59) ? savedAlarms[i].minute : alarms[i].minute;
    alarms[i].sound = (savedAlarms[i].sound <= 2) ? savedAlarms[i].sound : alarms[i].sound;
    alarms[i].snoozeMinutes = (savedAlarms[i].snoozeMinutes >= 5 && savedAlarms[i].snoozeMinutes <= 15) ? savedAlarms[i].snoozeMinutes : alarms[i].snoozeMinutes;
    alarms[i].snoozeCount = (savedAlarms[i].snoozeCount >= 1 && savedAlarms[i].snoozeCount <= 10) ? savedAlarms[i].snoozeCount : alarms[i].snoozeCount;
    alarms[i].lengthOption = (savedAlarms[i].lengthOption <= 3) ?savedAlarms[i].lengthOption : alarms[i].lengthOption;
  }
}

bool validStoredClock(             // stored clock info 
  uint16_t year,
  uint8_t month,
  uint8_t day,
  uint8_t hour,
  uint8_t minute,
  uint8_t second
) {

  return year >= 2000 &&
    year <= 2099 &&
    month >= 1 &&
    month <= 12 &&
    day >= 1 &&
    day <= daysInMonth(year, month) &&
    hour <= 23 &&
    minute <= 59 &&
    second <= 59;
}

void loadSettings() {                                           // Read the last confirmed settings during startup. If no saved data exists, keep the default values declared above.

  if (!preferences.begin(SETTINGS_NAMESPACE, true)) { Serial.println("Could not open saved settings");
    return;
  }

  size_t storedSize = preferences.getBytesLength(SETTINGS_KEY);
  bool rtcLostPower = rtc.lostPower();

  if (storedSize == sizeof(StoredSettings)) {

    StoredSettings saved;

    if (preferences.getBytes(SETTINGS_KEY, &saved, sizeof(saved)) ==
          sizeof(saved) &&
        saved.version == SETTINGS_VERSION) {

      applyStoredUserSettings( saved.timeFormat, saved.brightnessSetting, saved.alarms
      );

      if (rtcLostPower && validStoredClock(saved.clockYear,saved.clockMonth,saved.clockDay,saved.clockHour,saved.clockMinute, saved.clockSecond)) 
      {

        rtc.adjust(DateTime(saved.clockYear,saved.clockMonth,saved.clockDay,saved.clockHour,saved.clockMinute,saved.clockSecond
        ));

        Serial.println("RTC restored from saved clock snapshot");
      }

      Serial.println("Saved settings loaded");
    }
  }

  else if (storedSize == sizeof(StoredSettingsV1)) {

    StoredSettingsV1 saved;

    if (preferences.getBytes(SETTINGS_KEY, &saved, sizeof(saved)) == sizeof(saved) && saved.version == 1) 
    {

      applyStoredUserSettings( saved.timeFormat, saved.brightnessSetting, saved.alarms);
      Serial.println("Version 1 saved settings loaded");
    }
  }

  preferences.end();
}

void saveSettings() {                             // Save every user-configurable setting after the user presses the encoder to confirm it.  dont call in loop()

  StoredSettings saved;

  saved.version = SETTINGS_VERSION;
  saved.timeFormat = timeFormat;
  saved.brightnessSetting = brightnessSetting;

  DateTime rtcNow = rtc.now();
  saved.clockYear = rtcNow.year();
  saved.clockMonth = rtcNow.month();
  saved.clockDay = rtcNow.day();
  saved.clockHour = rtcNow.hour();
  saved.clockMinute = rtcNow.minute();
  saved.clockSecond = rtcNow.second();

  for (int i = 0; i < 3; i++) {

    saved.alarms[i].enabled = alarms[i].enabled;
    saved.alarms[i].hour = alarms[i].hour;
    saved.alarms[i].minute = alarms[i].minute;
    saved.alarms[i].sound = alarms[i].sound;
    saved.alarms[i].snoozeMinutes = alarms[i].snoozeMinutes;
    saved.alarms[i].snoozeCount = alarms[i].snoozeCount;
    saved.alarms[i].lengthOption = alarms[i].lengthOption;
  }

  if (!preferences.begin(SETTINGS_NAMESPACE, false)) 
  { 
    Serial.println("Could not save settings");
    return;
  }

  size_t written = preferences.putBytes(
    SETTINGS_KEY,
    &saved,
    sizeof(saved)
  );

  preferences.end();

  if (written != sizeof(saved)) { Serial.println("Settings save failed"); }
}

// -----------------------------------------------------
// ROTARY ENCODER VARIABLES
// -----------------------------------------------------

int lastA = HIGH;

unsigned long lastEncoderTime = 0;

int lastEncoderButtonReading = HIGH;        // Encoder push button debounce
int encoderButtonState = HIGH;  

unsigned long encoderButtonDebounceTime = 0;

// -----------------------------------------------------
// BACK BUTTON VARIABLES
// -----------------------------------------------------

int lastBackReading = HIGH;
int backButtonState = HIGH;

unsigned long backDebounceTime = 0;

// -----------------------------------------------------
// SNOOZE + ALARM RESET BUTTON VARIABLES
// -----------------------------------------------------

int lastSnoozeReading = HIGH;
int snoozeButtonState = HIGH;
unsigned long snoozeDebounceTime = 0;

int lastResetReading = HIGH;
int resetButtonState = HIGH;
unsigned long resetDebounceTime = 0;

// -----------------------------------------------------
// HOME BUTTON VARIABLES
// -----------------------------------------------------

int lastHomeReading = HIGH;
int homeButtonState = HIGH;

unsigned long homeDebounceTime = 0;

// -----------------------------------------------------
// INPUT EVENTS
// -----------------------------------------------------

bool encoderTurned = false;
bool encoderPressed = false;

bool backPressed = false;
bool homePressed = false;
bool snoozePressed = false;
bool resetPressed = false;

// -----------------------------------------------------
// ALARM + BUZZER VARIABLES
// -----------------------------------------------------

enum AlarmRunState {
  ALARM_IDLE,
  ALARM_RINGING,
  ALARM_SNOOZED
};

AlarmRunState alarmRunState = ALARM_IDLE;
int activeAlarmIndex = -1;
int activeSnoozeCount = 0;

// The minute keys prevent a dismissed alarm from re-triggering repeatedly
// during its scheduled minute. Each alarm tracks its own daily occurrence.
uint32_t lastTriggeredMinute[3] = {0, 0, 0};

unsigned long alarmStartedAt = 0;
unsigned long snoozeEndsAt = 0;

bool buzzerToneOn = false;
uint16_t buzzerFrequency = 0;

// -----------------------------------------------------
// SETUP
// -----------------------------------------------------

void setup() {

  Serial.begin(115200);

  // -----------------------------------------------------
  // I2C
  // -----------------------------------------------------

  Wire.begin(13, 14);

  rtc.begin();

  // Restore the last confirmed display and alarm settings from ESP32 flash.
  // The DS3231's coin cell independently retains the real date and time.
  loadSettings();

  u8g2.begin();

  // -----------------------------------------------------
  // PUSH BUTTONS
  // -----------------------------------------------------
  pinMode(backButton, INPUT_PULLUP);     // GPIO 4
  pinMode(homeButton, INPUT_PULLUP);     // GPIO 5
  pinMode(snoozeButton, INPUT_PULLUP);   // GPIO 6
  pinMode(resetButton, INPUT_PULLUP);    // GPIO 7

  // -----------------------------------------------------
  // PHOTORESISTOR
  // -----------------------------------------------------
  pinMode(photoPin, INPUT);              // GPIO 8

  analogReadResolution(12);

  // -----------------------------------------------------
  // ROTARY ENCODER
  // -----------------------------------------------------
  pinMode(encoderA, INPUT_PULLUP);        // GPIO 9
  pinMode(encoderB, INPUT_PULLUP);        // GPIO 10
  pinMode(encoderSW, INPUT_PULLUP);       // GPIO 11

  lastA = digitalRead(encoderA);

  // -----------------------------------------------------
  // BUZZER
  // -----------------------------------------------------
  pinMode(buzzerPin, OUTPUT);             // GPIO 12
}

// -----------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------

void loop() {

  readbuttons();

  readencoder();

  readLightLevel();

  readtime();

  updateAlarmController();

  handleMenu();

  updateDisplay();
}

// -----------------------------------------------------
// READ BACK + HOME BUTTONS
// -----------------------------------------------------
void readbuttons() {

  // -----------------------------------------------------
  // BACK BUTTON
  // -----------------------------------------------------

  int backReading = digitalRead(backButton);


  if (backReading != lastBackReading) {backDebounceTime = millis();}


  if (millis() - backDebounceTime >= 50) {

    if (backReading != backButtonState) {

      backButtonState = backReading;


      if (backButtonState == LOW) {

        backPressed = true;

        Serial.println("BACK");
      }
    }
  }

  lastBackReading = backReading;

  // -----------------------------------------------------
  // HOME BUTTON
  // -----------------------------------------------------

  int homeReading = digitalRead(homeButton);


  if (homeReading != lastHomeReading) {homeDebounceTime = millis();}


  if (millis() - homeDebounceTime >= 50) {

    if (homeReading != homeButtonState) {

      homeButtonState = homeReading;


      if (homeButtonState == LOW) {

        homePressed = true;

        Serial.println("HOME");
      }
    }
  }

  lastHomeReading = homeReading;


  // -----------------------------------------------------
  // SNOOZE BUTTON
  // -----------------------------------------------------

  int snoozeReading = digitalRead(snoozeButton);


  if (snoozeReading != lastSnoozeReading) {

    snoozeDebounceTime = millis();
  }


  if (millis() - snoozeDebounceTime >= 50) {

    if (snoozeReading != snoozeButtonState) {

      snoozeButtonState = snoozeReading;


      if (snoozeButtonState == LOW) {

        snoozePressed = true;
      }
    }
  }


  lastSnoozeReading = snoozeReading;


  // -----------------------------------------------------
  // ALARM RESET / DISMISS BUTTON
  // -----------------------------------------------------

  int resetReading = digitalRead(resetButton);


  if (resetReading != lastResetReading) {

    resetDebounceTime = millis();
  }


  if (millis() - resetDebounceTime >= 50) {

    if (resetReading != resetButtonState) {

      resetButtonState = resetReading;


      if (resetButtonState == LOW) {

        resetPressed = true;
      }
    }
  }


  lastResetReading = resetReading;
}

// -----------------------------------------------------
// READ ROTARY ENCODER
// -----------------------------------------------------

void readencoder() {

  // -----------------------------------------------------
  // ROTATION
  // -----------------------------------------------------

  int currentA = digitalRead(encoderA);


  if (lastA == HIGH && currentA == LOW) {

    if (millis() - lastEncoderTime > 5) {

      encoderTurned = true;

      Serial.println("DIAL TURNED");

      lastEncoderTime = millis();
    }
  }
  lastA = currentA;

  // -----------------------------------------------------
  // ENCODER PUSH BUTTON
  // -----------------------------------------------------

  int buttonReading = digitalRead(encoderSW);


  if (buttonReading != lastEncoderButtonReading) {

    encoderButtonDebounceTime = millis();
  }


  if (millis() - encoderButtonDebounceTime >= 50) {

    if (buttonReading != encoderButtonState) {

      encoderButtonState = buttonReading;


      if (encoderButtonState == LOW) {

        encoderPressed = true;

        Serial.println("ENCODER PRESSED");
      }
    }
  }


  lastEncoderButtonReading = buttonReading;
}

// -----------------------------------------------------
// READ RTC
// -----------------------------------------------------

void readtime() {now = rtc.now();}

// -----------------------------------------------------
// PHOTORESISTOR
// -----------------------------------------------------

void readLightLevel() {

  int adcValue = analogRead(photoPin);

  // Convert ADC reading to approximate voltage

  float voltage =
    (adcValue / 4095.0) * 3.3;

    Serial.println(voltage);

  // -----------------------------------------------------
  // DETERMINE AUTOMATIC BRIGHTNESS CATEGORY
  // -----------------------------------------------------

  if (voltage < 0.95) {automaticBrightness = 2;}
  else if (voltage < 1.7) {automaticBrightness = 1;}
  else {automaticBrightness = 0;}

  // Apply brightness setting
  updateBrightness();
}

// -----------------------------------------------------
// UPDATE OLED BRIGHTNESS
// -----------------------------------------------------

void updateBrightness() {

  // -----------------------------------------------------
  // AUTOMATIC
  // -----------------------------------------------------

  if (brightnessSetting == 0) {

    if (automaticBrightness == 0) {u8g2.setContrast(1);}
    else if (automaticBrightness == 1) {u8g2.setContrast(130);}
    else {u8g2.setContrast(255);}

  }

  // -----------------------------------------------------
  // LOW
  // -----------------------------------------------------

  else if (brightnessSetting == 1) {u8g2.setContrast(1);}

  // -----------------------------------------------------
  // MEDIUM
  // -----------------------------------------------------

  else if (brightnessSetting == 2) {u8g2.setContrast(130);}

  // -----------------------------------------------------
  // HIGH
  // -----------------------------------------------------

  else if (brightnessSetting == 3) {u8g2.setContrast(255);}
}

// -----------------------------------------------------
// ALARM SOUND ENGINE
// -----------------------------------------------------

// Only change the hardware output when the requested note changes. This keeps
// the patterns non-blocking, so buttons, the display, and the RTC keep working
// while an alarm is sounding.
void setBuzzerTone(bool shouldPlay, uint16_t frequency = 0) {

  if (!shouldPlay) {

    if (buzzerToneOn) {

      noTone(buzzerPin);
      buzzerToneOn = false;
      buzzerFrequency = 0;
    }

    return;
  }


  if (!buzzerToneOn || buzzerFrequency != frequency) {

    tone(buzzerPin, frequency);
    buzzerToneOn = true;
    buzzerFrequency = frequency;
  }
}


void silenceAlarm() {

  setBuzzerTone(false);
}


// Three distinct selectable sequences. All use the verified-audible 2 kHz
// piezo frequency; the cadence, rather than a quieter frequency sweep,
// distinguishes the sound choices on either active or passive buzzers.
// Sound 1: one long beep; Sound 2: two short beeps; Sound 3: three quick beeps.
void updateAlarmSound() {

  unsigned long elapsed = millis() - alarmStartedAt;
  unsigned long phase;


  switch (alarms[activeAlarmIndex].sound) {

    case 0:
      phase = elapsed % 1000;
      setBuzzerTone(phase < 650, 2000);
      break;


    case 1:
      phase = elapsed % 1000;
      setBuzzerTone(
        phase < 220 || (phase >= 340 && phase < 560),
        2000
      );
      break;


    case 2:
      phase = elapsed % 1000;
      setBuzzerTone(
        phase < 120 ||
          (phase >= 220 && phase < 340) ||
          (phase >= 440 && phase < 560),
        2000
      );
      break;
  }
}


unsigned long alarmLengthMilliseconds(int lengthOption) {

  switch (lengthOption) {

    case 0:
      return 15UL * 60UL * 1000UL;

    case 1:
      return 30UL * 60UL * 1000UL;

    case 2:
      return 60UL * 60UL * 1000UL;

    default:
      return 0;  // Indefinite: it ends only by Reset or Snooze.
  }
}


void startRingingAlarm(int alarmIndex) {

  activeAlarmIndex = alarmIndex;
  alarmRunState = ALARM_RINGING;
  alarmStartedAt = millis();
  Serial.print("Alarm ringing: ");
  Serial.println(alarmIndex + 1);
  Serial.print("Sound sequence: ");
  Serial.println(alarms[alarmIndex].sound + 1);
}

void dismissActiveAlarm() {

  silenceAlarm();
  activeAlarmIndex = -1;
  activeSnoozeCount = 0;
  alarmRunState = ALARM_IDLE;
}


bool alarmMatchesCurrentMinute(int alarmIndex) {

  const Alarm& alarm = alarms[alarmIndex];

  return alarm.enabled &&
    alarm.hour == now.hour() &&
    alarm.minute == now.minute();
}


void startDueScheduledAlarm() {

  uint32_t currentMinute = now.unixtime() / 60UL;


  for (int i = 0; i < 3; i++) {

    if (alarmMatchesCurrentMinute(i) &&
        lastTriggeredMinute[i] != currentMinute) {

      lastTriggeredMinute[i] = currentMinute;
      activeSnoozeCount = 0;
      startRingingAlarm(i);
      return;
    }
  }
}


void updateAlarmController() {

  if (alarmRunState == ALARM_IDLE) {

    startDueScheduledAlarm();
  }


  if (alarmRunState == ALARM_SNOOZED) {

    if (resetPressed) {

      dismissActiveAlarm();
    }


    else if ((long)(millis() - snoozeEndsAt) >= 0) {

      startRingingAlarm(activeAlarmIndex);
    }

    return;
  }


  if (alarmRunState != ALARM_RINGING) {

    return;
  }


  if (resetPressed) {

    dismissActiveAlarm();
    return;
  }


  if (snoozePressed) {

    if (activeSnoozeCount < alarms[activeAlarmIndex].snoozeCount) {

      activeSnoozeCount++;
      silenceAlarm();
      snoozeEndsAt = millis() +
        (unsigned long)alarms[activeAlarmIndex].snoozeMinutes * 60UL * 1000UL;
      alarmRunState = ALARM_SNOOZED;
      Serial.println("Alarm snoozed");
    }


    else {

      // The configured number of snoozes has been used; Snooze now dismisses.
      dismissActiveAlarm();
    }

    return;
  }


  unsigned long maximumLength =
    alarmLengthMilliseconds(alarms[activeAlarmIndex].lengthOption);


  if (maximumLength != 0 &&
      millis() - alarmStartedAt >= maximumLength) {

    dismissActiveAlarm();
    return;
  }


  updateAlarmSound();
}

// -----------------------------------------------------
// HANDLE MENU NAVIGATION
// -----------------------------------------------------

void handleMenu() {

  // -----------------------------------------------------
  // HOME BUTTON OVERRIDES EVERYTHING
  // -----------------------------------------------------

  if (homePressed) {

    currentScreen = HOME_SCREEN;

    menuIndex = 0;
  }

  else {

    switch (currentScreen) {

      // -----------------------------------------------------
      // HOME SCREEN
      // -----------------------------------------------------

      case HOME_SCREEN:

        if (encoderPressed) {

          currentScreen = SETTINGS_MENU;

          menuIndex = 0;
        }

        break;


      // -----------------------------------------------------
      // SETTINGS MENU
      // -----------------------------------------------------

      case SETTINGS_MENU:


        // Move forward one option

        if (encoderTurned) {

          menuIndex++;
          if (menuIndex > 3) { menuIndex = 0;}
        }


        // Select current option

        if (encoderPressed) {

          if (menuIndex == 0) {currentScreen = SET_ALARMS_MENU;}

          else if (menuIndex == 1) {currentScreen = SET_TIME_MENU;}

          else if (menuIndex == 2) {

            currentScreen = SET_STD_MILITARY_MENU;   // When this page opens, put the selection box around the format currently used on the Home screen.
            timeFormatMenuIndex = timeFormat;
          }

          else if (menuIndex == 3) {
            currentScreen = SET_BRIGHTNESS_MENU;
            brightnessMenuIndex = brightnessSetting;
          }
        }

        if (backPressed) {currentScreen = HOME_SCREEN;}          // Back returns home

        break;

      // -----------------------------------------------------
      // SET ALARMS
      // -----------------------------------------------------

      case SET_ALARMS_MENU:

        // Choose one of the three alarms to view or edit.
        if (encoderTurned) { alarmListIndex = (alarmListIndex + 1) % 3;}

        if (encoderPressed) {
          selectedAlarmIndex = alarmListIndex;
          alarmOptionsIndex = 0;
          currentScreen = ALARM_OPTIONS_MENU;
        }

        if (backPressed) {currentScreen = SETTINGS_MENU;}

        break;

      // -----------------------------------------------------
      // SELECTED ALARM: ON/OFF OR EDIT
      // -----------------------------------------------------

      case ALARM_OPTIONS_MENU:

        if (encoderTurned) { alarmOptionsIndex = (alarmOptionsIndex + 1) % 2; }

        if (encoderPressed) {

          if (alarmOptionsIndex == 0) {

            alarms[selectedAlarmIndex].enabled =
              !alarms[selectedAlarmIndex].enabled;

            saveSettings();
          }

          else {
            editAlarmIndex = 0;
            currentScreen = EDIT_ALARM_MENU;
          }
        }
        if (backPressed) {currentScreen = SET_ALARMS_MENU;}

        break;


      // -----------------------------------------------------
      // EDIT ALARM: TIME OR SETTINGS
      // -----------------------------------------------------

      case EDIT_ALARM_MENU:

        if (encoderTurned) {editAlarmIndex = (editAlarmIndex + 1) % 2;}

        if (encoderPressed) {

          if (editAlarmIndex == 0) {

            // Copy the saved time so Back can cancel an unfinished edit.
            editingAlarmHour = alarms[selectedAlarmIndex].hour;
            editingAlarmMinute = alarms[selectedAlarmIndex].minute;
            alarmTimeField = 0;
            currentScreen = SET_ALARM_TIME_MENU;
          }

          else {

            alarmSettingsIndex = 0;
            currentScreen = ALARM_SETTINGS_MENU;
          }
        }

        if (backPressed) { currentScreen = ALARM_OPTIONS_MENU;}
        break;

      // -----------------------------------------------------
      // SET ALARM TIME
      // -----------------------------------------------------

      case SET_ALARM_TIME_MENU:

        // The dial changes the highlighted part of HH:MM.
        if (encoderTurned) {

          if (alarmTimeField == 0) { editingAlarmHour = (editingAlarmHour + 1) % 24;}

          else { editingAlarmMinute = (editingAlarmMinute + 1) % 60; }
        }


        // First press moves from hour to minute. Second press saves both.
        if (encoderPressed) {

          if (alarmTimeField == 0) { alarmTimeField = 1; }

          else {

            alarms[selectedAlarmIndex].hour = editingAlarmHour;
            alarms[selectedAlarmIndex].minute = editingAlarmMinute;
            saveSettings();
            currentScreen = EDIT_ALARM_MENU;
          }
        }

        if (backPressed) { currentScreen = EDIT_ALARM_MENU;}

        break;

      // -----------------------------------------------------
      // ALARM SETTINGS: SOUND, SNOOZE, OR LENGTH
      // -----------------------------------------------------

      case ALARM_SETTINGS_MENU:

        if (encoderTurned) {

          alarmSettingsIndex = (alarmSettingsIndex + 1) % 3;
        }


        if (encoderPressed) {

          if (alarmSettingsIndex == 0) {

            alarmSoundIndex = alarms[selectedAlarmIndex].sound;
            currentScreen = ALARM_SOUND_MENU;
          }


          else if (alarmSettingsIndex == 1) {

            snoozeSettingsIndex = 0;
            currentScreen = SNOOZE_SETTINGS_MENU;
          }


          else {

            alarmLengthIndex = alarms[selectedAlarmIndex].lengthOption;
            currentScreen = ALARM_LENGTH_MENU;
          }
        }


        if (backPressed) {

          currentScreen = EDIT_ALARM_MENU;
        }

        break;


      // -----------------------------------------------------
      // ALARM SOUND
      // -----------------------------------------------------

      case ALARM_SOUND_MENU:

        if (encoderTurned) { alarmSoundIndex = (alarmSoundIndex + 1) % 3; }

        if (encoderPressed) {

          alarms[selectedAlarmIndex].sound = alarmSoundIndex;
          saveSettings();
        }


        if (backPressed) { currentScreen = ALARM_SETTINGS_MENU;}

        break;


      // -----------------------------------------------------
      // SNOOZE SETTINGS: LENGTH OR COUNT
      // -----------------------------------------------------

      case SNOOZE_SETTINGS_MENU:

        if (encoderTurned) { snoozeSettingsIndex = (snoozeSettingsIndex + 1) % 2; }

        if (encoderPressed) {

          if (snoozeSettingsIndex == 0) {

            snoozeLengthIndex = alarms[selectedAlarmIndex].snoozeMinutes - 5;
            currentScreen = SNOOZE_LENGTH_MENU;
          }

          else {

            snoozeCountIndex = alarms[selectedAlarmIndex].snoozeCount - 1;
            currentScreen = SNOOZE_COUNT_MENU;
          }
        }

        if (backPressed) {

          currentScreen = ALARM_SETTINGS_MENU;
        }

        break;

      // -----------------------------------------------------
      // SNOOZE LENGTH: 5 THROUGH 15 MINUTES
      // -----------------------------------------------------

      case SNOOZE_LENGTH_MENU:

        if (encoderTurned) {

          snoozeLengthIndex = (snoozeLengthIndex + 1) % 11;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].snoozeMinutes = snoozeLengthIndex + 5;
          saveSettings();
        }


        if (backPressed) {

          currentScreen = SNOOZE_SETTINGS_MENU;
        }

        break;


      // -----------------------------------------------------
      // SNOOZE COUNT: 1 THROUGH 10 TIMES
      // -----------------------------------------------------

      case SNOOZE_COUNT_MENU:

        if (encoderTurned) {

          snoozeCountIndex = (snoozeCountIndex + 1) % 10;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].snoozeCount = snoozeCountIndex + 1;
          saveSettings();
        }


        if (backPressed) {

          currentScreen = SNOOZE_SETTINGS_MENU;
        }

        break;


      // -----------------------------------------------------
      // ALARM LENGTH: 15, 30, 60 MINUTES, OR INDEFINITE
      // -----------------------------------------------------

      case ALARM_LENGTH_MENU:

        if (encoderTurned) {

          alarmLengthIndex = (alarmLengthIndex + 1) % 4;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].lengthOption = alarmLengthIndex;
          saveSettings();
        }


        if (backPressed) {

          currentScreen = ALARM_SETTINGS_MENU;
        }

        break;


      // -----------------------------------------------------
      // SET TIME
      // -----------------------------------------------------

      case SET_TIME_MENU:

        // Select either the Set Time or Set Date page.
        if (encoderTurned) {

          setTimeDateIndex = (setTimeDateIndex + 1) % 2;
        }


        if (encoderPressed) {

          if (setTimeDateIndex == 0) {

            // Copy the current RTC time into temporary edit values.
            editingClockHour = now.hour();
            editingClockMinute = now.minute();
            clockTimeField = 0;
            currentScreen = SET_CLOCK_TIME_EDIT_MENU;
          }


          else {

            // Copy the current RTC date into temporary edit values.
            editingClockMonth = now.month();
            editingClockDay = now.day();
            editingClockYear = now.year();
            clockDateField = 0;
            currentScreen = SET_DATE_MENU;
          }
        }


        if (backPressed) {

          currentScreen = SETTINGS_MENU;
        }

        break;


      // -----------------------------------------------------
      // SET CLOCK TIME: HOUR, THEN MINUTE
      // -----------------------------------------------------

      case SET_CLOCK_TIME_EDIT_MENU:

        if (encoderTurned) {

          if (clockTimeField == 0) {

            editingClockHour = (editingClockHour + 1) % 24;
          }


          else {

            editingClockMinute = (editingClockMinute + 1) % 60;
          }
        }


        // First press selects minutes. Second press saves the time to the RTC.
        if (encoderPressed) {

          if (clockTimeField == 0) {

            clockTimeField = 1;
          }


          else {

            rtc.adjust(DateTime(
              now.year(),
              now.month(),
              now.day(),
              editingClockHour,
              editingClockMinute,
              now.second()
            ));

            // The RTC backup cell keeps time advancing without ESP32 power.
            // NVS also receives a recovery snapshot through saveSettings().
            now = rtc.now();
            saveSettings();
            currentScreen = SET_TIME_MENU;
          }
        }


        if (backPressed) {

          // Back discards unfinished edits because the RTC was not changed.
          currentScreen = SET_TIME_MENU;
        }

        break;


      // -----------------------------------------------------
      // SET CLOCK DATE: MONTH, DAY, THEN YEAR
      // -----------------------------------------------------

      case SET_DATE_MENU:

        if (encoderTurned) {

          if (clockDateField == 0) {

            editingClockMonth = (editingClockMonth % 12) + 1;

            // For example, March 31 becomes April 30 when month changes.
            int maxDay = daysInMonth(editingClockYear, editingClockMonth);

            if (editingClockDay > maxDay) {

              editingClockDay = maxDay;
            }
          }


          else if (clockDateField == 1) {

            int maxDay = daysInMonth(editingClockYear, editingClockMonth);
            editingClockDay = (editingClockDay % maxDay) + 1;
          }


          else {

            editingClockYear++;

            if (editingClockYear > 2099) {

              editingClockYear = 2000;
            }


            int maxDay = daysInMonth(editingClockYear, editingClockMonth);

            if (editingClockDay > maxDay) {

              editingClockDay = maxDay;
            }
          }
        }


        // Press moves through month, day, and year. The final press saves.
        if (encoderPressed) {

          if (clockDateField < 2) { clockDateField++; }

          else {
            rtc.adjust(DateTime( editingClockYear, editingClockMonth, editingClockDay, now.hour(), now.minute(), now.second()));
            now = rtc.now();
            saveSettings();
            currentScreen = SET_TIME_MENU;
          }
        }

        if (backPressed) { currentScreen = SET_TIME_MENU;}

        break;
      // -----------------------------------------------------
      // SET STANDARD / MILITARY
      // -----------------------------------------------------

      case SET_STD_MILITARY_MENU:

        // Rotating the encoder moves the selection box between the two
        // choices. It does not apply a new time format yet.
        if (encoderTurned) {

          timeFormatMenuIndex++;

          if (timeFormatMenuIndex > 1) { timeFormatMenuIndex = 0;}
        }

        // Pressing the encoder confirms the choice. This changes only how the
        // RTC time is displayed; the RTC's stored time remains unchanged.
        if (encoderPressed) {
          timeFormat = timeFormatMenuIndex;
          saveSettings();
        }
        if (backPressed) {currentScreen = SETTINGS_MENU;}

        break;

      // -----------------------------------------------------
      // SET BRIGHTNESS
      // -----------------------------------------------------

      case SET_BRIGHTNESS_MENU:

        if (encoderTurned) {
          brightnessMenuIndex++;                                     // Select current brightness option
          if (brightnessMenuIndex > 3) {brightnessMenuIndex = 0;}
        }

        if (encoderPressed) {                                       // Select current brightness option
          brightnessSetting =brightnessMenuIndex;
          saveSettings();
        }

        if (backPressed) {currentScreen = SETTINGS_MENU;}           // Back returns to settings

        break;
    }
  }


  // -----------------------------------------------------
  // CLEAR INPUT EVENTS
  // -----------------------------------------------------

  encoderTurned = false;
  encoderPressed = false;
  backPressed = false;
  homePressed = false;
  snoozePressed = false;
  resetPressed = false;
}

//Function to call the time on the corner of the screen when working in the settings menus 
void drawSmallCurrentTime(){
  int displayHour = now.hour();
  const char* suffix = "";
  
  if (timeFormat == 0){  //conversion to standard mode
    suffix = (now.hour() < 12) ? "AM" : "PM";
    displayHour = now.hour() %12;

    if (displayHour == 0) {
      displayHour = 12 ;
    }
  }
   String hour = String(displayHour);
    String minute = String(now.minute());
    String second = String(now.second());

    // Military time keeps a leading zero, such as 08:05:09.
    if (timeFormat == 1 && displayHour < 10) {
      hour = "0" + hour;
    }

    if (now.minute() < 10) { minute = "0" + minute;}

    if (now.second() < 10) { second = "0" + second; }

    String smallTime = hour + ":" + minute + ":" + second;

    if (timeFormat == 0) {smallTime = smallTime + " " + suffix;}

    u8g2.setFont(u8g2_font_6x12_tr);

    int x = 128 - u8g2.getStrWidth(smallTime.c_str());

    // X is calculated so the right edge aligns with the 128-pixel OLED edge.
    // Y = 63 places the text at the bottom our screen
    u8g2.drawStr(x, 63, smallTime.c_str());
  }





// -----------------------------------------------------
// UPDATE OLED DISPLAY
// -----------------------------------------------------

void updateDisplay() {

  switch (currentScreen) {


    case HOME_SCREEN:

      displayHome();

      break;


    case SETTINGS_MENU:

      displaySettingsMenu();

      break;


    case SET_ALARMS_MENU:

      displaySetAlarms();

      break;


    case ALARM_OPTIONS_MENU:

      displayAlarmOptions();

      break;


    case EDIT_ALARM_MENU:

      displayEditAlarm();

      break;


    case SET_ALARM_TIME_MENU:

      displaySetAlarmTime();

      break;


    case ALARM_SETTINGS_MENU:

      displayAlarmSettings();

      break;


    case ALARM_SOUND_MENU:

      displayAlarmSound();

      break;


    case SNOOZE_SETTINGS_MENU:

      displaySnoozeSettings();

      break;


    case SNOOZE_LENGTH_MENU:

      displaySnoozeLength();

      break;


    case SNOOZE_COUNT_MENU:

      displaySnoozeCount();

      break;


    case ALARM_LENGTH_MENU:

      displayAlarmLength();

      break;


    case SET_TIME_MENU:

      displaySetTime();

      break;


    case SET_CLOCK_TIME_EDIT_MENU:

      displaySetClockTime();

      break;


    case SET_DATE_MENU:

      displaySetClockDate();

      break;


    case SET_STD_MILITARY_MENU:

      displaySetStdMilitary();

      break;


    case SET_BRIGHTNESS_MENU:

      displaySetBrightness();

      break;
  }
}


// -----------------------------------------------------
// HOME DISPLAY
// -----------------------------------------------------

void displayHome() {

  u8g2.clearBuffer();


  // -----------------------------------------------------
  // BUILD TIME STRING HH:MM:SS
  // -----------------------------------------------------

  int displayHour = now.hour();

  const char* meridiem = "";


  // The RTC always remains in 24-hour time.  Convert only for the standard
  // display, so noon and midnight are shown as 12 PM and 12 AM respectively.
  if (timeFormat == 0) {

    meridiem = (now.hour() < 12) ? "AM" : "PM";

    displayHour = now.hour() % 12;

    if (displayHour == 0) {

      displayHour = 12;
    }
  }


  String hour = String(displayHour);

  String minute = String(now.minute());

  String second = String(now.second());


  if (timeFormat == 1 && displayHour < 10) {

    hour = "0" + hour;
  }


  if (now.minute() < 10) {

    minute = "0" + minute;
  }


  if (now.second() < 10) {

    second = "0" + second;
  }


  String timeString =
    hour + ":" + minute + ":" + second;


  // -----------------------------------------------------
  // BUILD DAY STRING
  // -----------------------------------------------------

  const char* days[] = {

    "SUN",
    "MON",
    "TUE",
    "WED",
    "THU",
    "FRI",
    "SAT"
  };


  String dayString =
    days[now.dayOfTheWeek()];


  // -----------------------------------------------------
  // BUILD DATE STRING
  // -----------------------------------------------------

  String month = String(now.month());

  String day = String(now.day());

  String year = String(now.year());


  if (now.month() < 10) {

    month = "0" + month;
  }


  if (now.day() < 10) {

    day = "0" + day;
  }


  String dateString =
    month + "/" + day + "/" + year;


  // -----------------------------------------------------
  // LARGE CLOCK
  // -----------------------------------------------------

  u8g2.setFont(u8g2_font_logisoso20_tn);


  int timeWidth =
    u8g2.getStrWidth(timeString.c_str());


  int meridiemWidth = 0;

  if (timeFormat == 0) {

    u8g2.setFont(u8g2_font_6x12_tr);

    meridiemWidth = u8g2.getStrWidth(meridiem) + 3;

    u8g2.setFont(u8g2_font_logisoso20_tn);
  }


  int timeX =
    (128 - timeWidth - meridiemWidth) / 2;


  u8g2.drawStr(
    timeX,
    34,
    timeString.c_str()
  );


  if (timeFormat == 0) {

    u8g2.setFont(u8g2_font_6x12_tr);

    u8g2.drawStr(
      timeX + timeWidth + 3,
      34,
      meridiem
    );
  }


  // -----------------------------------------------------
  // FLASHING GEAR     for our settings icon
  // -----------------------------------------------------

  bool showGear =
    ((millis() / 500) % 2 == 0);

  if (showGear) {

    int gearX = 8;

    int gearY = 54;

    u8g2.drawCircle(
      gearX,
      gearY,
      5
    );

    u8g2.drawCircle(
      gearX,
      gearY,
      2
    );

    u8g2.drawLine(
      gearX,
      gearY - 7,
      gearX,
      gearY - 5
    );

    u8g2.drawLine(
      gearX,
      gearY + 5,
      gearX,
      gearY + 7
    );

    u8g2.drawLine(
      gearX - 7,
      gearY,
      gearX - 5,
      gearY
    );

    u8g2.drawLine(
      gearX + 5,
      gearY,
      gearX + 7,
      gearY
    );

    u8g2.drawLine(
      gearX - 5,
      gearY - 5,
      gearX - 4,
      gearY - 4
    );

    u8g2.drawLine(
      gearX + 4,
      gearY - 4,
      gearX + 5,
      gearY - 5
    );

    u8g2.drawLine(
      gearX - 5,
      gearY + 5,
      gearX - 4,
      gearY + 4
    );

    u8g2.drawLine(
      gearX + 4,
      gearY + 4,
      gearX + 5,
      gearY + 5
    );
  }

  // -----------------------------------------------------
  // DAY + DATE
  // -----------------------------------------------------

  u8g2.setFont(u8g2_font_6x12_tr);

  String bottomString = dayString + "  " + dateString;

  int bottomWidth = u8g2.getStrWidth(bottomString.c_str());

  int bottomX = 128 - bottomWidth;
  u8g2.drawStr( bottomX, 59, bottomString.c_str() );

  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SETTINGS MENU
// -----------------------------------------------------

void displaySettingsMenu() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(37,11,"SETTINGS");

  u8g2.drawStr(12,25,"SET ALARMS");

  u8g2.drawStr(12,37,"SET TIME/DATE");

  u8g2.drawStr(12,49,"SET STD/MILITARY");

  u8g2.drawStr( 12,61,"SET BRIGHTNESS");

  // -----------------------------------------------------
  // CURSOR
  // -----------------------------------------------------

  if (menuIndex == 0) {u8g2.drawStr(0, 25, ">");}

  else if (menuIndex == 1) {u8g2.drawStr(0, 37, ">");}

  else if (menuIndex == 2) {u8g2.drawStr(0, 49, ">");}

  else if (menuIndex == 3) {u8g2.drawStr(0, 61, ">");}

  u8g2.sendBuffer();
}

// -----------------------------------------------------
// ALARM MENU DISPLAY HELPERS
// -----------------------------------------------------

// Convert an alarm's stored 24-hour time into a short string for the menu.
String alarmTimeString(int alarmIndex) {

  const Alarm& alarm = alarms[alarmIndex];

  String hour = String(alarm.hour);
  String minute = String(alarm.minute);

  if (alarm.hour < 10) {hour = "0" + hour;}

  if (alarm.minute < 10) {minute = "0" + minute;}

  return hour + ":" + minute;
}

// Draw the > cursor at one row in a vertical list.
void drawMenuCursor(int index, int firstRowY, int rowSpacing) {

  u8g2.drawStr(0, firstRowY + (index * rowSpacing), ">");
}

// -----------------------------------------------------
// ALARM LIST: ALARM 1, 2, OR 3
// -----------------------------------------------------

void displaySetAlarms() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(34, 11, "SET ALARMS");

  for (int i = 0; i < 3; i++) {

    String alarmLabel = String("ALARM ") + String(i + 1) + "  " +
      alarmTimeString(i) + "  " +
      (alarms[i].enabled ? "ON" : "OFF");

    u8g2.drawStr(12, 27 + (i * 16), alarmLabel.c_str());
  }

  drawMenuCursor(alarmListIndex, 27, 16);
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SELECTED ALARM: ON/OFF OR EDIT
// -----------------------------------------------------

void displayAlarmOptions() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String title = String("ALARM ") + String(selectedAlarmIndex + 1);
  // Show the action the button will take, not the alarm's current state.
  String state = alarms[selectedAlarmIndex].enabled ? "OFF" : "ON";

  u8g2.drawStr(43, 11, title.c_str());
  u8g2.drawStr(18, 31, (String("TURN ") + state).c_str());
  u8g2.drawStr(18, 51, "EDIT ALARM");

  drawMenuCursor(alarmOptionsIndex, 31, 20);
  drawSmallCurrentTime();// calling the corner time function 
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// EDIT ALARM: TIME OR SETTINGS
// -----------------------------------------------------

void displayEditAlarm() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(31, 11, "EDIT ALARM");
  u8g2.drawStr(18, 31, "SET ALARM TIME");
  u8g2.drawStr(18, 51, "ALARM SETTINGS");

  drawMenuCursor(editAlarmIndex, 31, 20);
  drawSmallCurrentTime();
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SET ALARM TIME: HOUR, THEN MINUTE
// -----------------------------------------------------

void displaySetAlarmTime() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String hour = String(editingAlarmHour);
  String minute = String(editingAlarmMinute);

  if (editingAlarmHour < 10) {

    hour = "0" + hour;
  }

  if (editingAlarmMinute < 10) {

    minute = "0" + minute;
  }

  String editTime = hour + ":" + minute;

  u8g2.drawStr(25, 11, "SET ALARM TIME");
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int timeX = (128 - u8g2.getStrWidth(editTime.c_str())) / 2;
  u8g2.drawStr(timeX, 40, editTime.c_str());

  // The arrow shows whether the dial edits the hour or minute.
  u8g2.setFont(u8g2_font_6x12_tr);

  if (alarmTimeField == 0) {

    u8g2.drawStr(timeX + 8, 56, "^");    //can be slighly shifted to the left (px6)
    u8g2.drawStr(25, 63, "HOUR: PRESS NEXT");
  }

  else {

    u8g2.drawStr(timeX + 38, 56, "^");  //may need to seperate the arrow and text to center better
    u8g2.drawStr(34, 63, "MIN: PRESS SAVE");
  }

  u8g2.sendBuffer();
}

// -----------------------------------------------------
// ALARM SETTINGS: SOUND, SNOOZE, OR LENGTH
// -----------------------------------------------------

void displayAlarmSettings() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(22, 11, "ALARM SETTINGS");
  u8g2.drawStr(12, 27, "ALARM SOUND");
  u8g2.drawStr(12, 43, "SNOOZE SETTINGS");
  u8g2.drawStr(12, 59, "ALARM LENGTH");

  drawMenuCursor(alarmSettingsIndex, 27, 16);
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// ALARM SOUND: SOUND 1, 2, OR 3
// -----------------------------------------------------

void displayAlarmSound() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(30, 11, "ALARM SOUND");
  u8g2.drawStr(18, 27, "SOUND 1");
  u8g2.drawStr(18, 43, "SOUND 2");
  u8g2.drawStr(18, 59, "SOUND 3");

  drawMenuCursor(alarmSoundIndex, 27, 16);
  u8g2.drawStr(7, 27 + (alarms[selectedAlarmIndex].sound * 16), "+");
  drawSmallCurrentTime();
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SNOOZE SETTINGS: LENGTH OR COUNT
// -----------------------------------------------------

void displaySnoozeSettings() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(23, 11, "SNOOZE SETTINGS");
  u8g2.drawStr(18, 31, "SNOOZE LENGTH");
  u8g2.drawStr(18, 51, "SNOOZE COUNT");

  drawMenuCursor(snoozeSettingsIndex, 31, 20);
  drawSmallCurrentTime();
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SNOOZE LENGTH: 5-15 MINUTES
// -----------------------------------------------------

void displaySnoozeLength() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String value = String(snoozeLengthIndex + 5);

  u8g2.drawStr(26, 11, "SNOOZE LENGTH");
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int valueWidth = u8g2.getStrWidth(value.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  const char* unit = "MINUTES";
  int contentWidth = valueWidth + 4 + u8g2.getStrWidth(unit);
  int valueX = (128 - contentWidth) / 2;

  u8g2.setFont(u8g2_font_logisoso20_tn);
  u8g2.drawStr(valueX, 42, value.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(valueX + valueWidth + 4, 42, unit);
  u8g2.drawStr(19, 62, "PRESS TO SAVE");
  u8g2.sendBuffer();
}

// -----------------------------------------------------
// SNOOZE COUNT: 1-10 TIMES
// -----------------------------------------------------

void displaySnoozeCount() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String value = String(snoozeCountIndex + 1);

  u8g2.drawStr(29, 11, "SNOOZE COUNT");
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int valueWidth = u8g2.getStrWidth(value.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  const char* unit = "TIMES";
  int contentWidth = valueWidth + 4 + u8g2.getStrWidth(unit);
  int valueX = (128 - contentWidth) / 2;

  u8g2.setFont(u8g2_font_logisoso20_tn);
  u8g2.drawStr(valueX, 42, value.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(valueX + valueWidth + 4, 42, unit);
  u8g2.drawStr(19, 62, "PRESS TO SAVE");
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// ALARM LENGTH: 15, 30, 60, OR INDEFINITE
// -----------------------------------------------------

void displayAlarmLength() {

  const char* lengthNames[] = {
    "15 MINUTES",
    "30 MINUTES",
    "60 MINUTES",
    "INDEFINITE"
  };

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(28, 11, "ALARM LENGTH");

  for (int i = 0; i < 4; i++) {

    u8g2.drawStr(18, 25 + (i * 12), lengthNames[i]);
  }

  drawMenuCursor(alarmLengthIndex, 25, 12);
  u8g2.drawStr(7, 25 + (alarms[selectedAlarmIndex].lengthOption * 12), "+");
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SET TIME/DATE MENU
// -----------------------------------------------------

void displaySetTime() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(35, 11, "SET TIME/DATE");
  u8g2.drawStr(18, 31, "SET TIME");
  u8g2.drawStr(18, 48, "SET DATE");

  drawMenuCursor(setTimeDateIndex, 31, 17);
  drawSmallCurrentTime();
  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SET CLOCK TIME
// -----------------------------------------------------

void displaySetClockTime() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String hour = String(editingClockHour);
  String minute = String(editingClockMinute);

  if (editingClockHour < 10) {

    hour = "0" + hour;
  }


  if (editingClockMinute < 10) {

    minute = "0" + minute;
  }


  String editTime = hour + ":" + minute;

  u8g2.drawStr(35, 11, "SET TIME");
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int timeX = (128 - u8g2.getStrWidth(editTime.c_str())) / 2;
  u8g2.drawStr(timeX, 40, editTime.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);

  if (clockTimeField == 0) {

    u8g2.drawStr(timeX + 8, 56, "^");
    u8g2.drawStr(25, 63, "HOUR: PRESS NEXT");
  }


  else {

    u8g2.drawStr(timeX + 38, 56, "^");
    u8g2.drawStr(34, 63, "MIN: PRESS SAVE");
  }

  u8g2.sendBuffer();
}


// -----------------------------------------------------
// SET CLOCK DATE
// -----------------------------------------------------

void displaySetClockDate() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String month = String(editingClockMonth);
  String day = String(editingClockDay);
  String year = String(editingClockYear);

  if (editingClockMonth < 10) { month = "0" + month; }
  if (editingClockDay < 10) { day = "0" + day; }

  String editDate = month + "/" + day + "/" + year;

  u8g2.drawStr(35, 11, "SET DATE");

  // MM/DD/YYYY is ten characters wide. The large numeric font can exceed the
  // OLED width, so use the reliable fixed-width font for this edit screen.
  u8g2.setFont(u8g2_font_6x12_tr);

  int dateX = (128 - u8g2.getStrWidth(editDate.c_str())) / 2;
  u8g2.drawStr(dateX, 34, editDate.c_str());

  if (clockDateField == 0) {
    u8g2.drawStr(dateX + 3, 47, "^");
    u8g2.drawStr(24, 63, "MONTH: PRESS NEXT");
  }

  else if (clockDateField == 1) {
    u8g2.drawStr(dateX + 21, 47, "^");
    u8g2.drawStr(27, 63, "DAY: PRESS NEXT");
  }

  else {
    u8g2.drawStr(dateX + 45, 47, "^");
    u8g2.drawStr(31, 63, "YEAR: PRESS SAVE");
  }


  u8g2.sendBuffer();
}

// -----------------------------------------------------
// STANDARD / MILITARY MENU
// -----------------------------------------------------

void displaySetStdMilitary() {  //Screen function to select between standard and military time 
                                //The time will only update when the encoder is pressed again 
  u8g2.clearBuffer();

  // The preview changes only when the encoder button is pressed and the new choice is saved following flowchart
  int previewHour = now.hour();
  const char* previewMeridiem = "";

  // For Standard time, convert the RTC's 0-23 hour into a 1-12 hour and add
  // AM or PM. For Military time, previewHour remains the original 0-23 hour.
  if (timeFormat == 0) {

    previewMeridiem = (now.hour() < 12) ? "AM" : "PM";
    previewHour = now.hour() % 12;

    if (previewHour == 0) {

      previewHour = 12;
    }
  }

  String previewHourString = String(previewHour);
  String previewMinute = String(now.minute());
  String previewSecond = String(now.second());

  // Military time always uses two hour digits, for example 08:30:00.
  if (timeFormat == 1 && previewHour < 10) {previewHourString = "0" + previewHourString;}  //hour
  if (now.minute() < 10) {previewMinute = "0" + previewMinute;} //minute
  if (now.second() < 10) {previewSecond = "0" + previewSecond;} //second


  String previewTime =
    previewHourString + ":" + previewMinute + ":" + previewSecond;

  // Center the time preview at the top of the page. AM/PM needs additional
  // width, so include it in the centering calculation when needed.
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int previewTimeWidth = u8g2.getStrWidth(previewTime.c_str());
  int previewMeridiemWidth = 0;

  if (timeFormat == 0) {

    u8g2.setFont(u8g2_font_6x12_tr);
    previewMeridiemWidth = u8g2.getStrWidth(previewMeridiem) + 3;
    u8g2.setFont(u8g2_font_logisoso20_tn);
  }


  int previewTimeX =
    (128 - previewTimeWidth - previewMeridiemWidth) / 2;


  u8g2.drawStr(previewTimeX, 31, previewTime.c_str());


  if (timeFormat == 0) {

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(
      previewTimeX + previewTimeWidth + 3,
      31,
      previewMeridiem
    );
  }


  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(13, 54, "STANDARD");   //Standard display
  u8g2.drawStr(71, 54, "MILITARY");  // Military display

  //replacing the boxing with an arrow to keep it simpler
  if (timeFormatMenuIndex == 0) {u8g2.drawStr(33, 63, "^");}

  else {u8g2.drawStr(91, 63, "^");}

  u8g2.sendBuffer();
}

// -----------------------------------------------------
// BRIGHTNESS MENU
// -----------------------------------------------------

void displaySetBrightness() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);

  // -----------------------------------------------------
  // TITLE
  // -----------------------------------------------------

  u8g2.drawStr(20,11,"OLED BRIGHTNESS");

  // -----------------------------------------------------
  // OPTIONS
  // -----------------------------------------------------

  u8g2.drawStr(30,25,"AUTOMATIC");
  u8g2.drawStr(30,37,"LOW");
  u8g2.drawStr(30,49,"MEDIUM");
  u8g2.drawStr(30,61,"HIGH");

  // -----------------------------------------------------
  // CURSOR >
  // -----------------------------------------------------

  if (brightnessMenuIndex == 0) {u8g2.drawStr(0,25,">");}

  else if (brightnessMenuIndex == 1) {u8g2.drawStr(0,37,">");}

  else if (brightnessMenuIndex == 2) {u8g2.drawStr(0,49,">");}

  else if (brightnessMenuIndex == 3) {u8g2.drawStr(0,61,">");}

  // -----------------------------------------------------
  // ACTIVE SETTING +
  // -----------------------------------------------------

  if (brightnessSetting == 0) {u8g2.drawStr(18,25,"+");}

  else if (brightnessSetting == 1) {u8g2.drawStr(18,37,"+");}

  else if (brightnessSetting == 2) {u8g2.drawStr(18,49,"+");}

  else if (brightnessSetting == 3) {u8g2.drawStr(18,61,"+");}

  u8g2.sendBuffer();
}
