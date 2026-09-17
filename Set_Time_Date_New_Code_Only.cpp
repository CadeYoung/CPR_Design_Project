/*
  Set Time/Date - NEW CODE ONLY

  This file contains only the code additions/sections associated with the
  Set Time/Date functionality. It is NOT a standalone replacement for the
  full alarm-clock program.

  Add the enum entries to the existing Screen enum, the variables near the
  existing global variables, the menu cases inside handleMenu(), and the
  display functions alongside the existing display functions.
*/


int setTimeDateIndex = 0;

// Time editing variables
int editingClockHour = 0;
int editingClockMinute = 0;
int clockTimeField = 0;

// Date editing variables
int editingClockMonth = 1;
int editingClockDay = 1;
int editingClockYear = 2026;
int clockDateField = 0;

// Function prototype used by the Set Date menu logic.
int daysInMonth(int year, int month);

    case SET_TIME_MENU:

        // Select between Set Time and Set Date.
        if (encoderTurned) {

          setTimeDateIndex = (setTimeDateIndex + 1) % 2;
        }


        if (encoderPressed) {

          if (setTimeDateIndex == 0) {

            // Copy the current RTC time into the temporary edit values.
            editingClockHour = now.hour();
            editingClockMinute = now.minute();
            clockTimeField = 0;
            currentScreen = SET_CLOCK_TIME_EDIT_MENU;
          }


          else {

            // Copy the current RTC date into the temporary edit values.
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


      // ==================================================
      // SET TIME
      // ==================================================

      case SET_CLOCK_TIME_EDIT_MENU:

        // The dial changes the highlighted part of HH:MM.
        if (encoderTurned) {

          if (clockTimeField == 0) {

            editingClockHour = (editingClockHour + 1) % 24;
          }


          else {

            editingClockMinute = (editingClockMinute + 1) % 60;
          }
        }


        // First press moves from hour to minute. Second press saves both.
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

            currentScreen = SET_TIME_MENU;
          }
        }


        if (backPressed) {

          currentScreen = SET_TIME_MENU;
        }

        break;


      // ==================================================
      // SET DATE
      // ==================================================

      case SET_DATE_MENU:

        // The dial changes the highlighted part of MM/DD/YYYY.
        if (encoderTurned) {

          if (clockDateField == 0) {

            editingClockMonth++;

            if (editingClockMonth > 12) {
              editingClockMonth = 1;
            }

            int maxDay = daysInMonth(editingClockYear, editingClockMonth);

            if (editingClockDay > maxDay) {
              editingClockDay = maxDay;
            }
          }


          else if (clockDateField == 1) {

            editingClockDay++;

            int maxDay = daysInMonth(editingClockYear, editingClockMonth);

            if (editingClockDay > maxDay) {
              editingClockDay = 1;
            }
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


        // Pressing the Selection button moves through the fields.
        // The final press saves the date to the RTC.
        if (encoderPressed) {

          if (clockDateField < 2) {

            clockDateField++;
          }


          else {

            rtc.adjust(DateTime(
              editingClockYear,
              editingClockMonth,
              editingClockDay,
              now.hour(),
              now.minute(),
              now.second()
            ));

            currentScreen = SET_TIME_MENU;
          }
        }


        if (backPressed) {

          currentScreen = SET_TIME_MENU;
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
        }

        if (backPressed) {

          currentScreen = SETTINGS_MENU;
        }

        break;


      // ==================================================
      // SET BRIGHTNESS
      // ==================================================

void displaySetTime() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.drawStr(35, 11, "SET TIME/DATE");
  u8g2.drawStr(18, 31, "SET TIME");
  u8g2.drawStr(18, 51, "SET DATE");

  drawMenuCursor(setTimeDateIndex, 31, 20);

  drawSmallCurrentTime();

  u8g2.sendBuffer();
}


// ==================================================
// SET CLOCK TIME
// ==================================================

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

  u8g2.drawStr(25, 11, "SET TIME");

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


// ==================================================
// SET CLOCK DATE
// ==================================================

int daysInMonth(int year, int month) {

  if (month == 2) {

    if ((year % 4 == 0 && year % 100 != 0) ||
        (year % 400 == 0)) {

      return 29;
    }

    return 28;
  }

  if (month == 4 || month == 6 ||
      month == 9 || month == 11) {

    return 30;
  }

  return 31;
}

void displaySetClockDate() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);

  String month = String(editingClockMonth);
  String day = String(editingClockDay);
  String year = String(editingClockYear);

  if (editingClockMonth < 10) {
    month = "0" + month;
  }

  if (editingClockDay < 10) {
    day = "0" + day;
  }

  String editDate = month + "/" + day + "/" + year;

  u8g2.drawStr(32, 11, "SET DATE");

  u8g2.setFont(u8g2_font_logisoso20_tn);

  int dateX = (128 - u8g2.getStrWidth(editDate.c_str())) / 2;
  u8g2.drawStr(dateX, 40, editDate.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);

  if (clockDateField == 0) {
    u8g2.drawStr(dateX + 13, 56, "^");
    u8g2.drawStr(24, 63, "MONTH: PRESS NEXT");
  }

  else if (clockDateField == 1) {
    u8g2.drawStr(dateX + 42, 56, "^");
    u8g2.drawStr(27, 63, "DAY: PRESS NEXT");
  }

  else {
    u8g2.drawStr(dateX + 73, 56, "^");
    u8g2.drawStr(31, 63, "YEAR: PRESS SAVE");
  }

  u8g2.sendBuffer();
}
