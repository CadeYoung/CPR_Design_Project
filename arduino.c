#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

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

        if (backPressed) {

          currentScreen = SETTINGS_MENU;
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

  String hour = String(now.hour());

  String minute = String(now.minute());

  String second = String(now.second());


  if (now.hour() < 10) {

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


  int timeX =
    (128 - timeWidth) / 2;


  u8g2.drawStr(
    timeX,
    34,
    timeString.c_str()
  );


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
// SET ALARMS PLACEHOLDER
// ==================================================

void displaySetAlarms() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);


  u8g2.drawStr(
    30,
    15,
    "SET ALARMS"
  );


  u8g2.drawStr(
    15,
    35,
    "Coming later..."
  );


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
// STANDARD / MILITARY PLACEHOLDER
// ==================================================

void displaySetStdMilitary() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);


  u8g2.drawStr(
    10,
    15,
    "SET STD/MILITARY"
  );


  u8g2.drawStr(
    15,
    35,
    "Coming later..."
  );


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

  u8g2.drawStr(
    20,
    11,
    "OLED BRIGHTNESS"
  );


  // ==================================================
  // OPTIONS
  // ==================================================

  u8g2.drawStr(
    30,
    25,
    "AUTOMATIC"
  );


  u8g2.drawStr(
    30,
    37,
    "LOW"
  );


  u8g2.drawStr(
    30,
    49,
    "MEDIUM"
  );


  u8g2.drawStr(
    30,
    61,
    "HIGH"
  );


  // ==================================================
  // CURSOR >
  // ==================================================

  if (brightnessMenuIndex == 0) {

    u8g2.drawStr(
      0,
      25,
      ">"
    );
  }


  else if (brightnessMenuIndex == 1) {

    u8g2.drawStr(
      0,
      37,
      ">"
    );
  }


  else if (brightnessMenuIndex == 2) {

    u8g2.drawStr(
      0,
      49,
      ">"
    );
  }


  else if (brightnessMenuIndex == 3) {

    u8g2.drawStr(
      0,
      61,
      ">"
    );
  }


  // ==================================================
  // ACTIVE SETTING +
  // ==================================================

  if (brightnessSetting == 0) {

    u8g2.drawStr(
      18,
      25,
      "+"
    );
  }


  else if (brightnessSetting == 1) {

    u8g2.drawStr(
      18,
      37,
      "+"
    );
  }


  else if (brightnessSetting == 2) {

    u8g2.drawStr(
      18,
      49,
      "+"
    );
  }


  else if (brightnessSetting == 3) {

    u8g2.drawStr(
      18,
      61,
      "+"
    );
  }


  u8g2.sendBuffer();
}
