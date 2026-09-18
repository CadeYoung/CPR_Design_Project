#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Preferences.h>

// ==================================================
// PIN DEFINITIONS
// ==================================================

const int backButton = 4;
const int homeButton = 5;
const int snoozeButton = 6;
const int resetButton = 7;

const int photoPin = 8;

const int encoderA = 9;
const int encoderB = 10;
const int encoderSW = 11;

const int buzzerPin = 12;


// ==================================================
// RTC
// ==================================================

RTC_DS3231 rtc;
DateTime now;

// Preferences object used to store user settings in ESP32 non-volatile memory
Preferences preferences;


// ==================================================
// OLED
// ==================================================

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);


// ==================================================
// SCREEN STATES
// ==================================================

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

  SET_STD_MILITARY_MENU,

  SET_BRIGHTNESS_MENU
};

Screen currentScreen = HOME_SCREEN;


// ==================================================
// MENU VARIABLES
// ==================================================

int menuIndex = 0;


// Brightness submenu cursor
int brightnessMenuIndex = 0;


// Active brightness setting
// 0 = Automatic
// 1 = Low
// 2 = Medium
// 3 = High

int brightnessSetting = 0;


// Photoresistor-determined brightness
// 0 = Low
// 1 = Medium
// 2 = High

int automaticBrightness = 0;


// TIME FORMAT SETTINGS
//
// The RTC always stores time in 24-hour format. This variable only changes
// how that same time is shown on the OLED:
//   0 = Standard 12-hour time with AM or PM
//   1 = Military 24-hour time
// Separating the display choice from the RTC prevents a format change from
// accidentally changing the actual time.
int timeFormat = 0;


// This is the temporary selection box on the Time Format page. Turning the
// rotary encoder changes this value, but it does NOT change the active format
// until the encoder button is clicked.
int timeFormatMenuIndex = 0;


// ==================================================
// ALARM SETTINGS
// ==================================================

// Every alarm keeps its own settings.  The hour is stored in 24-hour time
// (0-23), even when the Home screen is set to Standard time.
struct Alarm {
  bool enabled;
  int hour;
  int minute;
  int sound;          // 0 = Sound 1, 1 = Sound 2, 2 = Sound 3
  int snoozeMinutes;  // 5 through 15 minutes
  int snoozeCount;    // 1 through 10 snoozes
  int lengthOption;   // 0 = 15m, 1 = 30m, 2 = 60m, 3 = Indefinite
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


// ==================================================
// ROTARY ENCODER VARIABLES
// ==================================================

int lastA = HIGH;

unsigned long lastEncoderTime = 0;


// Encoder push button debounce

int lastEncoderButtonReading = HIGH;
int encoderButtonState = HIGH;

unsigned long encoderButtonDebounceTime = 0;


// ==================================================
// BACK BUTTON VARIABLES
// ==================================================

int lastBackReading = HIGH;
int backButtonState = HIGH;

unsigned long backDebounceTime = 0;


// ==================================================
// HOME BUTTON VARIABLES
// ==================================================

int lastHomeReading = HIGH;
int homeButtonState = HIGH;

unsigned long homeDebounceTime = 0;


// ==================================================
// INPUT EVENTS
// ==================================================

bool encoderTurned = false;
bool encoderPressed = false;

bool backPressed = false;
bool homePressed = false;


// ==================================================
// BUZZER VARIABLES
// ==================================================

bool buzzerActive = false;
bool buzzerFinished = false;

unsigned long buzzerStartTime = 0;



// ==================================================
// PREFERENCES / NON-VOLATILE MEMORY
// ==================================================
//
// The DS3231 RTC stores the actual current time and date.
// Preferences stores user settings so they survive a ESP32 reset
// and power loss.
//
// ==================================================


// ==================================================
// LOAD SETTINGS FROM NON-VOLATILE MEMORY
// ==================================================

void loadPreferences() {

  // Open the "alarmclock" namespace in read/write mode.
  preferences.begin("alarmclock", false);


  // ==================================================
  // GENERAL SETTINGS
  // ==================================================

  // The second argument is the default value used if
  // this setting has never been saved before.
  timeFormat =
    preferences.getInt("timeFormat", 0);

  brightnessSetting =
    preferences.getInt("brightness", 0);


  // ==================================================
  // ALARM 1
  // ==================================================

  alarms[0].enabled =
    preferences.getBool("a1enabled", false);

  alarms[0].hour =
    preferences.getInt("a1hour", 7);

  alarms[0].minute =
    preferences.getInt("a1minute", 0);

  alarms[0].sound =
    preferences.getInt("a1sound", 0);

  alarms[0].snoozeMinutes =
    preferences.getInt("a1snooze", 5);

  alarms[0].snoozeCount =
    preferences.getInt("a1count", 3);

  alarms[0].lengthOption =
    preferences.getInt("a1length", 1);


  // ==================================================
  // ALARM 2
  // ==================================================

  alarms[1].enabled =
    preferences.getBool("a2enabled", false);

  alarms[1].hour =
    preferences.getInt("a2hour", 8);

  alarms[1].minute =
    preferences.getInt("a2minute", 0);

  alarms[1].sound =
    preferences.getInt("a2sound", 0);

  alarms[1].snoozeMinutes =
    preferences.getInt("a2snooze", 5);

  alarms[1].snoozeCount =
    preferences.getInt("a2count", 3);

  alarms[1].lengthOption =
    preferences.getInt("a2length", 1);


  // ==================================================
  // ALARM 3
  // ==================================================

  alarms[2].enabled =
    preferences.getBool("a3enabled", false);

  alarms[2].hour =
    preferences.getInt("a3hour", 9);

  alarms[2].minute =
    preferences.getInt("a3minute", 0);

  alarms[2].sound =
    preferences.getInt("a3sound", 0);

  alarms[2].snoozeMinutes =
    preferences.getInt("a3snooze", 5);

  alarms[2].snoozeCount =
    preferences.getInt("a3count", 3);

  alarms[2].lengthOption =
    preferences.getInt("a3length", 1);


  // Close the Preferences namespace.
  preferences.end();
}


// ==================================================
// SAVE SETTINGS TO NON-VOLATILE MEMORY
// ==================================================

void savePreferences() {

  // Open the "alarmclock" namespace in read/write mode.
  preferences.begin("alarmclock", false);


  // ==================================================
  // GENERAL SETTINGS
  // ==================================================

  preferences.putInt("timeFormat", timeFormat);
  preferences.putInt("brightness", brightnessSetting);


  // ==================================================
  // ALARM 1
  // ==================================================

  preferences.putBool("a1enabled", alarms[0].enabled);
  preferences.putInt("a1hour", alarms[0].hour);
  preferences.putInt("a1minute", alarms[0].minute);
  preferences.putInt("a1sound", alarms[0].sound);
  preferences.putInt("a1snooze", alarms[0].snoozeMinutes);
  preferences.putInt("a1count", alarms[0].snoozeCount);
  preferences.putInt("a1length", alarms[0].lengthOption);


  // ==================================================
  // ALARM 2
  // ==================================================

  preferences.putBool("a2enabled", alarms[1].enabled);
  preferences.putInt("a2hour", alarms[1].hour);
  preferences.putInt("a2minute", alarms[1].minute);
  preferences.putInt("a2sound", alarms[1].sound);
  preferences.putInt("a2snooze", alarms[1].snoozeMinutes);
  preferences.putInt("a2count", alarms[1].snoozeCount);
  preferences.putInt("a2length", alarms[1].lengthOption);


  // ==================================================
  // ALARM 3
  // ==================================================

  preferences.putBool("a3enabled", alarms[2].enabled);
  preferences.putInt("a3hour", alarms[2].hour);
  preferences.putInt("a3minute", alarms[2].minute);
  preferences.putInt("a3sound", alarms[2].sound);
  preferences.putInt("a3snooze", alarms[2].snoozeMinutes);
  preferences.putInt("a3count", alarms[2].snoozeCount);
  preferences.putInt("a3length", alarms[2].lengthOption);


  // Close the Preferences namespace.
  preferences.end();
}


// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);


  // ==================================================
  // I2C
  // ==================================================

  Wire.begin(13, 14);

  rtc.begin();

  u8g2.begin();


  // ==================================================
  // PUSH BUTTONS
  // ==================================================

  pinMode(backButton, INPUT_PULLUP);     // GPIO 4
  pinMode(homeButton, INPUT_PULLUP);     // GPIO 5
  pinMode(snoozeButton, INPUT_PULLUP);   // GPIO 6
  pinMode(resetButton, INPUT_PULLUP);    // GPIO 7


  // ==================================================
  // PHOTORESISTOR
  // ==================================================

  pinMode(photoPin, INPUT);              // GPIO 8

  analogReadResolution(12);


  // ==================================================
  // ROTARY ENCODER
  // ==================================================

  pinMode(encoderA, INPUT_PULLUP);        // GPIO 9
  pinMode(encoderB, INPUT_PULLUP);        // GPIO 10
  pinMode(encoderSW, INPUT_PULLUP);       // GPIO 11

  lastA = digitalRead(encoderA);


  // ==================================================
  // BUZZER
  // ==================================================

  pinMode(buzzerPin, OUTPUT);             // GPIO 12


  // ==================================================
  // LOAD SAVED SETTINGS
  // ==================================================

  loadPreferences();

  // Apply the saved brightness immediately after startup.
  updateBrightness();
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  readbuttons();

  readencoder();

  readLightLevel();

  readtime();

  handleMenu();

  updateDisplay();

  // buzzeroutput();
}


// ==================================================
// READ BACK + HOME BUTTONS
// ==================================================

void readbuttons() {

  // ==================================================
  // BACK BUTTON
  // ==================================================

  int backReading = digitalRead(backButton);


  if (backReading != lastBackReading) {

    backDebounceTime = millis();
  }


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


  // ==================================================
  // HOME BUTTON
  // ==================================================

  int homeReading = digitalRead(homeButton);


  if (homeReading != lastHomeReading) {

    homeDebounceTime = millis();
  }


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
}


// ==================================================
// READ ROTARY ENCODER
// ==================================================

void readencoder() {

  // ==================================================
  // ROTATION
  // ==================================================

  int currentA = digitalRead(encoderA);


  if (lastA == HIGH && currentA == LOW) {

    if (millis() - lastEncoderTime > 5) {

      encoderTurned = true;

      Serial.println("DIAL TURNED");

      lastEncoderTime = millis();
    }
  }


  lastA = currentA;


  // ==================================================
  // ENCODER PUSH BUTTON
  // ==================================================

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


// ==================================================
// READ RTC
// ==================================================

void readtime() {

  now = rtc.now();
}


// ==================================================
// PHOTORESISTOR
// ==================================================

void readLightLevel() {

  int adcValue = analogRead(photoPin);

  // Convert ADC reading to approximate voltage

  float voltage =
    (adcValue / 4095.0) * 3.3;

    Serial.println(voltage);


  // ==================================================
  // DETERMINE AUTOMATIC BRIGHTNESS CATEGORY
  // ==================================================

  if (voltage < 0.95) {

    automaticBrightness = 2;
  }


  else if (voltage < 1.7) {

    automaticBrightness = 1;
  }


  else {

    automaticBrightness = 0;
  }


  // Apply brightness setting

  updateBrightness();
}


// ==================================================
// UPDATE OLED BRIGHTNESS
// ==================================================

void updateBrightness() {

  // ==================================================
  // AUTOMATIC
  // ==================================================

  if (brightnessSetting == 0) {

    if (automaticBrightness == 0) {

      u8g2.setContrast(1);
    }


    else if (automaticBrightness == 1) {

      u8g2.setContrast(130);
    }


    else {

      u8g2.setContrast(255);
    }
  }


  // ==================================================
  // LOW
  // ==================================================

  else if (brightnessSetting == 1) {

    u8g2.setContrast(1);
  }


  // ==================================================
  // MEDIUM
  // ==================================================

  else if (brightnessSetting == 2) {

    u8g2.setContrast(130);
  }


  // ==================================================
  // HIGH
  // ==================================================

  else if (brightnessSetting == 3) {

    u8g2.setContrast(255);
  }
}


// ==================================================
// BUZZER
// ==================================================

void buzzeroutput() {

  // ==================================================
  // START BUZZER ONCE
  // ==================================================

  if (buzzerActive == false &&
      buzzerFinished == false) {

    tone(buzzerPin, 2000);

    buzzerStartTime = millis();

    buzzerActive = true;
  }


  // ==================================================
  // STOP AFTER 1 SECOND
  // ==================================================

  if (buzzerActive == true &&
      millis() - buzzerStartTime >= 1000) {

    noTone(buzzerPin);

    buzzerActive = false;

    buzzerFinished = true;
  }
}


// ==================================================
// HANDLE MENU NAVIGATION
// ==================================================

void handleMenu() {

  // ==================================================
  // HOME BUTTON OVERRIDES EVERYTHING
  // ==================================================

  if (homePressed) {

    currentScreen = HOME_SCREEN;

    menuIndex = 0;
  }


  else {

    switch (currentScreen) {


      // ==================================================
      // HOME SCREEN
      // ==================================================

      case HOME_SCREEN:

        if (encoderPressed) {

          currentScreen = SETTINGS_MENU;

          menuIndex = 0;
        }

        break;


      // ==================================================
      // SETTINGS MENU
      // ==================================================

      case SETTINGS_MENU:


        // Move forward one option

        if (encoderTurned) {

          menuIndex++;


          if (menuIndex > 3) {

            menuIndex = 0;
          }
        }


        // Select current option

        if (encoderPressed) {

          if (menuIndex == 0) {

            currentScreen = SET_ALARMS_MENU;
          }


          else if (menuIndex == 1) {

            currentScreen = SET_TIME_MENU;
          }


          else if (menuIndex == 2) {

            currentScreen = SET_STD_MILITARY_MENU;

            // When this page opens, put the selection box around the format
            // currently used on the Home screen.
            timeFormatMenuIndex = timeFormat;
          }


          else if (menuIndex == 3) {

            currentScreen = SET_BRIGHTNESS_MENU;

            // Start cursor on active brightness setting

            brightnessMenuIndex =
              brightnessSetting;
          }
        }


        // Back returns home

        if (backPressed) {

          currentScreen = HOME_SCREEN;
        }

        break;


      // ==================================================
      // SET ALARMS
      // ==================================================

      case SET_ALARMS_MENU:

        // Choose one of the three alarms to view or edit.
        if (encoderTurned) {

          alarmListIndex = (alarmListIndex + 1) % 3;
        }


        if (encoderPressed) {

          selectedAlarmIndex = alarmListIndex;
          alarmOptionsIndex = 0;
          currentScreen = ALARM_OPTIONS_MENU;
        }


        if (backPressed) {

          currentScreen = SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SELECTED ALARM: ON/OFF OR EDIT
      // ==================================================

      case ALARM_OPTIONS_MENU:

        if (encoderTurned) {

          alarmOptionsIndex = (alarmOptionsIndex + 1) % 2;
        }


        if (encoderPressed) {

          if (alarmOptionsIndex == 0) {

            alarms[selectedAlarmIndex].enabled =
              !alarms[selectedAlarmIndex].enabled;

            // Save the ON/OFF state immediately.
            savePreferences();
          }


          else {

            editAlarmIndex = 0;
            currentScreen = EDIT_ALARM_MENU;
          }
        }


        if (backPressed) {

          currentScreen = SET_ALARMS_MENU;
        }

        break;


      // ==================================================
      // EDIT ALARM: TIME OR SETTINGS
      // ==================================================

      case EDIT_ALARM_MENU:

        if (encoderTurned) {

          editAlarmIndex = (editAlarmIndex + 1) % 2;
        }


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


        if (backPressed) {

          currentScreen = ALARM_OPTIONS_MENU;
        }

        break;


      // ==================================================
      // SET ALARM TIME
      // ==================================================

      case SET_ALARM_TIME_MENU:

        // The dial changes the highlighted part of HH:MM.
        if (encoderTurned) {

          if (alarmTimeField == 0) {

            editingAlarmHour = (editingAlarmHour + 1) % 24;
          }


          else {

            editingAlarmMinute = (editingAlarmMinute + 1) % 60;
          }
        }


        // First press moves from hour to minute. Second press saves both.
        if (encoderPressed) {

          if (alarmTimeField == 0) {

            alarmTimeField = 1;
          }


          else {

            alarms[selectedAlarmIndex].hour = editingAlarmHour;
            alarms[selectedAlarmIndex].minute = editingAlarmMinute;

            // Save the new alarm time.
            savePreferences();

            currentScreen = EDIT_ALARM_MENU;
          }
        }


        if (backPressed) {

          currentScreen = EDIT_ALARM_MENU;
        }

        break;


      // ==================================================
      // ALARM SETTINGS: SOUND, SNOOZE, OR LENGTH
      // ==================================================

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


      // ==================================================
      // ALARM SOUND
      // ==================================================

      case ALARM_SOUND_MENU:

        if (encoderTurned) {

          alarmSoundIndex = (alarmSoundIndex + 1) % 3;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].sound = alarmSoundIndex;

          // Save the new alarm sound.
          savePreferences();
        }


        if (backPressed) {

          currentScreen = ALARM_SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SNOOZE SETTINGS: LENGTH OR COUNT
      // ==================================================

      case SNOOZE_SETTINGS_MENU:

        if (encoderTurned) {

          snoozeSettingsIndex = (snoozeSettingsIndex + 1) % 2;
        }


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


      // ==================================================
      // SNOOZE LENGTH: 5 THROUGH 15 MINUTES
      // ==================================================

      case SNOOZE_LENGTH_MENU:

        if (encoderTurned) {

          snoozeLengthIndex = (snoozeLengthIndex + 1) % 11;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].snoozeMinutes = snoozeLengthIndex + 5;

          // Save the new snooze length.
          savePreferences();
        }


        if (backPressed) {

          currentScreen = SNOOZE_SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SNOOZE COUNT: 1 THROUGH 10 TIMES
      // ==================================================

      case SNOOZE_COUNT_MENU:

        if (encoderTurned) {

          snoozeCountIndex = (snoozeCountIndex + 1) % 10;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].snoozeCount = snoozeCountIndex + 1;

          // Save the new snooze count.
          savePreferences();
        }


        if (backPressed) {

          currentScreen = SNOOZE_SETTINGS_MENU;
        }

        break;


      // ==================================================
      // ALARM LENGTH: 15, 30, 60 MINUTES, OR INDEFINITE
      // ==================================================

      case ALARM_LENGTH_MENU:

        if (encoderTurned) {

          alarmLengthIndex = (alarmLengthIndex + 1) % 4;
        }


        if (encoderPressed) {

          alarms[selectedAlarmIndex].lengthOption = alarmLengthIndex;

          // Save the new alarm length.
          savePreferences();
        }


        if (backPressed) {

          currentScreen = ALARM_SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SET TIME
      // ==================================================

      case SET_TIME_MENU:

        if (backPressed) {

          currentScreen = SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SET STANDARD / MILITARY
      // ==================================================

      case SET_STD_MILITARY_MENU:

        // Rotating the encoder moves the selection box between the two
        // choices. It does not apply a new time format yet.
        if (encoderTurned) {

          timeFormatMenuIndex++;

          if (timeFormatMenuIndex > 1) {

            timeFormatMenuIndex = 0;
          }
        }


        // Pressing the encoder confirms the choice. This changes only how the
        // RTC time is displayed; the RTC's stored time remains unchanged.
        if (encoderPressed) {

          timeFormat = timeFormatMenuIndex;

          // Save the selected 12-hour / 24-hour display format.
          savePreferences();
        }

        if (backPressed) {

          currentScreen = SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SET BRIGHTNESS
      // ==================================================

      case SET_BRIGHTNESS_MENU:


        // Move cursor forward

        if (encoderTurned) {

          brightnessMenuIndex++;


          if (brightnessMenuIndex > 3) {

            brightnessMenuIndex = 0;
          }
        }


        // Select current brightness option

        if (encoderPressed) {

          brightnessSetting =
            brightnessMenuIndex;

          // Save the selected brightness setting.
          savePreferences();

          // Apply the new brightness immediately.
          updateBrightness();
        }


        // Back returns to settings

        if (backPressed) {

          currentScreen = SETTINGS_MENU;
        }

        break;
    }
  }


  // ==================================================
  // CLEAR INPUT EVENTS
  // ==================================================

  encoderTurned = false;

  encoderPressed = false;

  backPressed = false;

  homePressed = false;
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





// ==================================================
// UPDATE OLED DISPLAY
// ==================================================

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


    case SET_STD_MILITARY_MENU:

      displaySetStdMilitary();

      break;


    case SET_BRIGHTNESS_MENU:

      displaySetBrightness();

      break;
  }
}


// ==================================================
// HOME DISPLAY
// ==================================================

void displayHome() {

  u8g2.clearBuffer();


  // ==================================================
  // BUILD TIME STRING HH:MM:SS
  // ==================================================

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


  // ==================================================
  // BUILD DAY STRING
  // ==================================================

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


  // ==================================================
  // BUILD DATE STRING
  // ==================================================

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


  // ==================================================
  // LARGE CLOCK
  // ==================================================

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


  // ==================================================
  // FLASHING GEAR
  // ==================================================

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


  // ==================================================
  // DAY + DATE
  // ==================================================

  u8g2.setFont(u8g2_font_6x12_tr);


  String bottomString =
    dayString + "  " + dateString;


  int bottomWidth =
    u8g2.getStrWidth(bottomString.c_str());


  int bottomX =
    128 - bottomWidth;


  u8g2.drawStr(
    bottomX,
    59,
    bottomString.c_str()
  );


  u8g2.sendBuffer();
}


// ==================================================
// SETTINGS MENU
// ==================================================

void displaySettingsMenu() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);


  u8g2.drawStr(
    37,
    11,
    "SETTINGS"
  );


  u8g2.drawStr(
    12,
    25,
    "SET ALARMS"
  );


  u8g2.drawStr(
    12,
    37,
    "SET TIME"
  );


  u8g2.drawStr(
    12,
    49,
    "SET STD/MILITARY"
  );


  u8g2.drawStr(
    12,
    61,
    "SET BRIGHTNESS"
  );


  // ==================================================
  // CURSOR
  // ==================================================

  if (menuIndex == 0) {

    u8g2.drawStr(0, 25, ">");
  }


  else if (menuIndex == 1) {

    u8g2.drawStr(0, 37, ">");
  }


  else if (menuIndex == 2) {

    u8g2.drawStr(0, 49, ">");
  }


  else if (menuIndex == 3) {

    u8g2.drawStr(0, 61, ">");
  }


  u8g2.sendBuffer();
}


// ==================================================
// ALARM MENU DISPLAY HELPERS
// ==================================================

// Convert an alarm's stored 24-hour time into a short string for the menu.
String alarmTimeString(const Alarm& alarm) {

  String hour = String(alarm.hour);
  String minute = String(alarm.minute);

  if (alarm.hour < 10) {

    hour = "0" + hour;
  }


  if (alarm.minute < 10) {

    minute = "0" + minute;
  }


  return hour + ":" + minute;
}


// Draw the familiar > cursor at one row in a vertical list.
void drawMenuCursor(int index, int firstRowY, int rowSpacing) {

  u8g2.drawStr(0, firstRowY + (index * rowSpacing), ">");
}


// ==================================================
// ALARM LIST: ALARM 1, 2, OR 3
// ==================================================

void displaySetAlarms() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(34, 11, "SET ALARMS");

  for (int i = 0; i < 3; i++) {

    String alarmLabel = String("ALARM ") + String(i + 1) + "  " +
      alarmTimeString(alarms[i]) + "  " +
      (alarms[i].enabled ? "ON" : "OFF");

    u8g2.drawStr(12, 27 + (i * 16), alarmLabel.c_str());
  }

  drawMenuCursor(alarmListIndex, 27, 16);
  u8g2.sendBuffer();
}


// ==================================================
// SELECTED ALARM: ON/OFF OR EDIT
// ==================================================

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


// ==================================================
// EDIT ALARM: TIME OR SETTINGS
// ==================================================

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


// ==================================================
// SET ALARM TIME: HOUR, THEN MINUTE
// ==================================================

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


// ==================================================
// ALARM SETTINGS: SOUND, SNOOZE, OR LENGTH
// ==================================================

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


// ==================================================
// ALARM SOUND: SOUND 1, 2, OR 3
// ==================================================

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


// ==================================================
// SNOOZE SETTINGS: LENGTH OR COUNT
// ==================================================

void displaySnoozeSettings() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(23, 11, "SNOOZE SETTINGS");
  u8g2.drawStr(18, 31, "SNOOZE LENGTH");
  u8g2.drawStr(18, 51, "SNOOZE COUNT");

  drawMenuCursor(snoozeSettingsIndex, 31, 20);
  u8g2.sendBuffer();
}


// ==================================================
// SNOOZE LENGTH: 5-15 MINUTES
// ==================================================

void displaySnoozeLength() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String value = String(snoozeLengthIndex + 5);

  u8g2.drawStr(26, 11, "SNOOZE LENGTH");
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int valueX = (128 - u8g2.getStrWidth(value.c_str())) / 2;
  u8g2.drawStr(valueX, 42, value.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(valueX + 25, 42, "MINUTES");
  u8g2.drawStr(19, 62, "PRESS TO SAVE");
  u8g2.sendBuffer();
}


// ==================================================
// SNOOZE COUNT: 1-10 TIMES
// ==================================================

void displaySnoozeCount() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  String value = String(snoozeCountIndex + 1);

  u8g2.drawStr(29, 11, "SNOOZE COUNT");
  u8g2.setFont(u8g2_font_logisoso20_tn);

  int valueX = (128 - u8g2.getStrWidth(value.c_str())) / 2;
  u8g2.drawStr(valueX, 42, value.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(valueX + 25, 42, "TIMES");
  u8g2.drawStr(19, 62, "PRESS TO SAVE");
  u8g2.sendBuffer();
}


// ==================================================
// ALARM LENGTH: 15, 30, 60, OR INDEFINITE
// ==================================================

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


// ==================================================
// SET TIME PLACEHOLDER
// ==================================================

void displaySetTime() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);


  u8g2.drawStr(
    40,
    15,
    "SET TIME"
  );


  u8g2.drawStr(
    15,
    35,
    "Coming later..."
  );


  u8g2.sendBuffer();
}


// ==================================================
// STANDARD / MILITARY MENU
// ==================================================


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


// ==================================================
// BRIGHTNESS MENU
// ==================================================

void displaySetBrightness() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);


  // ==================================================
  // TITLE
  // ==================================================

  u8g2.drawStr(20,11,"OLED BRIGHTNESS");


  // ==================================================
  // OPTIONS
  // ==================================================

  u8g2.drawStr(30,25,"AUTOMATIC");

  u8g2.drawStr(30,37,"LOW");

  u8g2.drawStr(30,49,"MEDIUM");

  u8g2.drawStr(30,61,"HIGH");

  // ==================================================
  // CURSOR >
  // ==================================================

  if (brightnessMenuIndex == 0) {
    u8g2.drawStr(0,25,">");
  }


  else if (brightnessMenuIndex == 1) {
    u8g2.drawStr(0,37,">");
  }


  else if (brightnessMenuIndex == 2) {
    u8g2.drawStr(0,49,">");
  }


  else if (brightnessMenuIndex == 3) {
    u8g2.drawStr(0,61,">");
  }


  // ==================================================
  // ACTIVE SETTING +
  // ==================================================

  if (brightnessSetting == 0) {
    u8g2.drawStr(18,25,"+");
  }


  else if (brightnessSetting == 1) {
    u8g2.drawStr(18,37,"+");
  }


  else if (brightnessSetting == 2) {
    u8g2.drawStr(18,49,"+");
  }


  else if (brightnessSetting == 3) {
    u8g2.drawStr(18,61,"+");
  }


  u8g2.sendBuffer();
}
