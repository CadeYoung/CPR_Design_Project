#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

// Define Constants
long encoderDebounce = 10; // 6 ms encoder rotation debounce
const int backButton = 4;
const int homeButton = 5;
const int snoozeButton = 6;
const int resetButton = 7;
const int encoderA = 9;
const int encoderB = 10;
const int encoderSelect = 11;

// Define Variables
RTC_DS3231 rtc;
int lastA; // used in readencoder function
long lastEncoderTime = 0; // last rotary encoder rotation timestamp
DateTime now; // create object

// OLED Display Voodoo Magic
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

void setup(){
  Serial.begin(115200); // enable serial comms
  Wire.begin(13,14); // enable I2C comms
  rtc.begin();
  rtc.adjust(DateTime(2026, 9, 10, 18, 7, 0));
  u8g2.begin();

  // configure buttons
  pinMode(backButton, INPUT_PULLUP); // GPIO 4
  pinMode(homeButton, INPUT_PULLUP); // GPIO 5
  pinMode(snoozeButton, INPUT_PULLUP); // GPIO 6
  pinMode(resetButton, INPUT_PULLUP); // GPIO 7
  pinMode(encoderA, INPUT_PULLUP); // GPIO 9
  pinMode(encoderB, INPUT_PULLUP); // GPIO 10
  pinMode(encoderSelect, INPUT_PULLUP); // GPIO 11
}

void loop(){
  readbuttons(); // continuously poll for push button input
  //readencoder(); // continuously poll for rotary encoder input
  readtime();
  updatemenu();
}

void readbuttons(){
  if(digitalRead(backButton) == LOW){ // wait for button press
      delay(50); // delay 50 ms

  if(digitalRead(backButton) == LOW){ // check to see if still pressed
      Serial.println("Back button pressed"); // output test message

      while(digitalRead(backButton) == LOW){ // wait for button release
        delay(10); // delay 10 ms
      }
    } 
  }

  if(digitalRead(homeButton) == LOW){ // wait for button press
      delay(50); // delay 50 ms

  if(digitalRead(homeButton) == LOW){ // check to see if still pressed
      Serial.println("Home button pressed"); // output test message

      while(digitalRead(homeButton) == LOW){ // wait for button release
        delay(10); // delay 10 ms
      }
    } 
  }

  if(digitalRead(snoozeButton) == LOW){ // wait for button press
      delay(50); // delay 50 ms

  if(digitalRead(snoozeButton) == LOW){ // check to see if still pressed
      Serial.println("Snooze button pressed"); // output test message

      while(digitalRead(snoozeButton) == LOW){ // wait for button release
        delay(10); // delay 10 ms
      }
    } 
  }

  if(digitalRead(resetButton) == LOW){ // wait for button press
      delay(50); // delay 50 ms

  if(digitalRead(resetButton) == LOW){ // check to see if still pressed
      Serial.println("Reset button pressed"); // output test message

      while(digitalRead(resetButton) == LOW){ // wait for button release
        delay(10); // delay 10 ms
      }
    } 
  }
}

void readencoder(){
  int currentA = digitalRead(encoderA); // read current channel A state
  
  if(currentA != lastA){ // detect rotation
    if((millis() - lastEncoderTime) >= encoderDebounce){ // proceed only if 6 ms have passed
      lastEncoderTime = millis(); // set last encoder time

      if(digitalRead(encoderB) != currentA){ // determine phase shift
        Serial.println("Dial forward (CW)");
      } else { // determine phase shift
        Serial.println("Dial backward (CCW)");
      }
    }
    lastA = currentA; // update last channel A state   

    if(digitalRead(encoderSelect) == LOW){ // wait for button press
        delay(50); // delay 50 ms

    if(digitalRead(encoderSelect) == LOW){ // check to see if still pressed
        Serial.println("Select button pressed"); // output test message

        while(digitalRead(encoderSelect) == LOW){ // wait for button release
          delay(10); // delay 10 ms
        }
      }
    }
  }
}

void readtime(){
  now = rtc.now();
  Serial.print("Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());
}

void updatemenu() {

  u8g2.clearBuffer();

  // ==================================================
  // BUILD TIME STRING HH:MM:SS
  // ==================================================

  String hour   = String(now.hour());
  String minute = String(now.minute());
  String second = String(now.second());

  if (now.hour() < 10)
    hour = "0" + hour;

  if (now.minute() < 10)
    minute = "0" + minute;

  if (now.second() < 10)
    second = "0" + second;

  String timeString = hour + ":" + minute + ":" + second;

  // ==================================================
  // BUILD DAY STRING
  // ==================================================

  const char* days[] = {
    "SUN", "MON", "TUE", "WED",
    "THU", "FRI", "SAT"
  };

  String dayString = days[now.dayOfTheWeek()];


  // ==================================================
  // BUILD DATE STRING MM/DD/YYYY
  // ==================================================

  String month = String(now.month());
  String day   = String(now.day());
  String year  = String(now.year());

  if (now.month() < 10)
    month = "0" + month;

  if (now.day() < 10)
    day = "0" + day;

  String dateString = month + "/" + day + "/" + year;


  // ==================================================
  // LARGE CLOCK
  // ==================================================

  u8g2.setFont(u8g2_font_logisoso20_tn);

  int timeWidth = u8g2.getStrWidth(timeString.c_str());
  int timeX = (128 - timeWidth) / 2;

  u8g2.drawStr(timeX, 34, timeString.c_str());

  // ==================================================
  // FLASHING GEAR ICON
  // ==================================================

  bool showGear = ((millis() / 500) % 2 == 0);

  if (showGear) {

    int gearX = 8;
    int gearY = 54;

    // Outer circle
    u8g2.drawCircle(gearX, gearY, 5);

    // Center hole
    u8g2.drawCircle(gearX, gearY, 2);

    // Small gear teeth
    u8g2.drawLine(gearX, gearY - 7, gearX, gearY - 5);
    u8g2.drawLine(gearX, gearY + 5, gearX, gearY + 7);

    u8g2.drawLine(gearX - 7, gearY, gearX - 5, gearY);
    u8g2.drawLine(gearX + 5, gearY, gearX + 7, gearY);

    u8g2.drawLine(gearX - 5, gearY - 5, gearX - 4, gearY - 4);
    u8g2.drawLine(gearX + 4, gearY - 4, gearX + 5, gearY - 5);

    u8g2.drawLine(gearX - 5, gearY + 5, gearX - 4, gearY + 4);
    u8g2.drawLine(gearX + 4, gearY + 4, gearX + 5, gearY + 5);
  }
  
    u8g2.setFont(u8g2_font_6x12_tr);

    String bottomString = dayString + "  " + dateString;

    int bottomWidth = u8g2.getStrWidth(bottomString.c_str());

    // Keep text to right of gear
    int bottomX = 128 - bottomWidth;

    u8g2.drawStr(bottomX, 59, bottomString.c_str());


  // ==================================================
  // SEND TO DISPLAY
  // ==================================================

  u8g2.sendBuffer();
}
