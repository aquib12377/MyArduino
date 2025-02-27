/**************************************************************************
 * ESP32 Medicine Reminder with Start/Stop Times + Buzzer + IR Sensor
 *
 * Features:
 * 1) DS3231 RTC for current time
 * 2) 16x2 LCD for display
 * 3) Time Input widget (V0) sets start/stop times
 * 4) When current time >= Start time:
 *      -> Move servo to 90°
 *      -> Start buzzer beep
 * 5) When current time >= Stop time:
 *      -> Move servo to 0°
 *      -> Stop buzzer
 * 6) IR sensor (Active LOW) check:
 *      -> If time is in the active window (after start, before stop)
 *         and IR is LOW => rotate servo for 10 seconds
 * 7) Blynk Button (V1) to manually trigger continuous beep (until button is OFF)
 **************************************************************************/

// Blynk Template Info (replace if needed)
#define BLYNK_TEMPLATE_ID   "TMPL3I5rJNEz7"
#define BLYNK_TEMPLATE_NAME "Medicine Reminder"
#define BLYNK_AUTH_TOKEN    "M9zmgSEv40k0CO_T1rbRMTBUHAlfNWAb"

// Libraries
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <ESP32Servo.h>

/*******************/
/*  Blynk Settings */
/*******************/
char ssid[] = "MyProject";   // Replace with your WiFi SSID
char pass[] = "12345678";    // Replace with your WiFi password

/****************************************************
 * Time Input Variables (from Blynk widget V0)
 ****************************************************/
long startTimeInSec = -1;
long stopTimeInSec  = -1;
bool timesSet       = false; 
bool triggeredStart = false;
bool triggeredStop  = false;

/******************/
/*   RTC Module   */
/******************/
RTC_DS3231 rtc;

/******************/
/*   LCD Module   */
/******************/
LiquidCrystal_I2C lcd(0x27, 16, 2);

/******************/
/*   SG90 Servo   */
/******************/
Servo myServo;
const int servoPin = 13; // servo control pin

/******************/
/*   Buzzer       */
/******************/
const int buzzerPin = 27;  // choose any free GPIO
bool beepActive = false;   // is the buzzer *scheduled* to beep?
bool buzzerState = false;  // toggles HIGH/LOW in a pattern

/******************/
/*    IR Sensor   */
/******************/
// IR is ACTIVE LOW => LOW means "object detected"
const int irPin = 12;    // IR sensor output
// If your IR module has an onboard pull-up or uses an open-collector, 
// you might need pinMode(irPin, INPUT_PULLUP). Test as needed.

/******************/
/*   Blynk Timer  */
/******************/
BlynkTimer timer;

/**************************************************
 * beepTimerCallback()
 *
 * Called at a fixed interval (e.g. every 300 ms).
 * If 'beepActive' is true, toggles the buzzer pin
 * to produce an on/off beep pattern.
 **************************************************/
void beepTimerCallback() {
  if (beepActive) {
    // Toggle the buzzer output
    buzzerState = !buzzerState;
    digitalWrite(buzzerPin, buzzerState ? HIGH : LOW);
  } else {
    // Ensure buzzer is off
    digitalWrite(buzzerPin, LOW);
    buzzerState = false;
  }
}

/**************************************************
 * setup()
 **************************************************/
void setup() {
  Serial.begin(115200);

  // Start Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Connecting to Blynk & WiFi...");

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    while (1);
  }
  // If RTC lost power, set it to compile time
  if (rtc.lostPower()) {
    Serial.println("RTC lost power; setting time from compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  // Attach the servo
  myServo.attach(servoPin);
  myServo.write(0);  // Start servo at 0°

  // Buzzer pin
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // IR pin
  pinMode(irPin, INPUT_PULLUP); 
  // If your IR module already pulls the line high internally,
  // you might use pinMode(irPin, INPUT) instead.

  // Check time every second
  timer.setInterval(1000L, checkTimes);

  // Timer for buzzer toggling every 300 ms (adjust for beep speed)
  timer.setInterval(300L, beepTimerCallback);
}

/**************************************************
 * loop()
 **************************************************/
void loop() {
  Blynk.run();
  timer.run();
}

/**************************************************
 * checkTimes()
 *
 * 1) Reads current time from RTC
 * 2) Updates LCD
 * 3) Checks start/stop times
 * 4) IR sensor check
 **************************************************/
void checkTimes() {
  DateTime now = rtc.now();

  // Print current time to LCD (HH:MM:SS)
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  lcd.setCursor(0, 0);
  lcd.print(timeStr);

  // Print current date (DD/MM/YYYY) on second row
  char dateStr[17];
  sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());
  lcd.setCursor(0, 1);
  lcd.print(dateStr);

  // If we have valid start/stop times from Blynk, compare
  if (timesSet) {
    // Convert current hour/min/sec to total seconds from midnight
    long currentSec = now.hour() * 3600L + now.minute() * 60L + now.second();

    // --- Check if we have reached the Start Time ---
    if (!triggeredStart && currentSec >= startTimeInSec) {
      Serial.println("Reached START time => Trigger action!");
      Blynk.logEvent("medicine_reminder", "Start Time Reached");

      // Display a quick message on LCD
      lcd.clear();
      lcd.print("Please take medicine!");
      delay(3000);
      lcd.clear();

      // Move servo to 90°
      myServo.write(90);

      // Start buzzer beeping
      beepActive = true;

      triggeredStart = true; // so we don't trigger again this day
    }

    // --- Check if we have reached the Stop Time ---
    if (!triggeredStop && currentSec >= stopTimeInSec) {
      Serial.println("Reached STOP time => Trigger action!");
      Blynk.logEvent("medicine_reminder", "Stop Time Reached");

      // Move servo back to 0°
      myServo.write(0);
      lcd.clear();
      lcd.print("Reminder ended...");

      // Stop buzzer
      beepActive = false;

      triggeredStop = true; // so we don't trigger again this day
    }

    // --- IR Sensor Check (Active only between Start & Stop) ---
    // If we've started but not stopped yet, check IR
    if (triggeredStart && !triggeredStop) {
      // If IR is LOW => object detected
      if (digitalRead(irPin) == LOW) {
        Serial.println("IR detected object => rotating servo for 10s...");
        lcd.clear();
        lcd.print("Object detected!");
        // Rotate servo for 10 seconds (blocking example)
        myServo.write(180);   // or 90, or whichever angle
        delay(10000);         // hold for 10 seconds
        myServo.write(90);    // return to 90 or wherever you want
        lcd.clear();
      }
    }

    // --- Reset triggers if the day changes (for daily repetition) ---
    // If you'd like this code to reset daily, check if it's after midnight:
    // We'll do it if the time is < than both start & stop time,
    // indicating we likely rolled over past midnight.
    if (currentSec < startTimeInSec && currentSec < stopTimeInSec) {
      // Re-arm the triggers for the new day
      triggeredStart = false;
      triggeredStop  = false;
    }
  }
}

/******************************************************
 * Blynk Write Handler for Time Input Widget (V0)
 *
 * param[0] => startTime (seconds from midnight)
 * param[1] => stopTime  (seconds from midnight)
 * param[2] => timezone offset
 * param[3] => DST
 * param[4..] => days of week bitmask if repeating
 ******************************************************/
BLYNK_WRITE(V0) {
  TimeInputParam t(param);

  if (t.hasStartTime()) {
    // Convert hours/minutes/seconds to total seconds from midnight
    startTimeInSec = t.getStartHour() * 3600L
                   + t.getStartMinute() * 60L
                   + t.getStartSecond();
    Serial.printf("Start Time: %02d:%02d:%02d => %ld sec\n",
                  t.getStartHour(), t.getStartMinute(), t.getStartSecond(),
                  startTimeInSec);
  } else {
    Serial.println("No Start Time set");
    startTimeInSec = -1; // invalid
  }

  if (t.hasStopTime()) {
    stopTimeInSec = t.getStopHour() * 3600L
                  + t.getStopMinute() * 60L
                  + t.getStopSecond();
    Serial.printf("Stop Time:  %02d:%02d:%02d => %ld sec\n",
                  t.getStopHour(), t.getStopMinute(), t.getStopSecond(),
                  stopTimeInSec);
  } else {
    Serial.println("No Stop Time set");
    stopTimeInSec = -1; // invalid
  }

  // Determine if both times are valid
  if (startTimeInSec >= 0 && stopTimeInSec >= 0) {
    timesSet       = true;
    triggeredStart = false;
    triggeredStop  = false;
    Serial.println("Start & Stop Times are set.");
  } else {
    timesSet = false;
    Serial.println("Invalid times. timesSet = false");
  }
}

/******************************************************
 * Blynk Write Handler for Manual Buzzer Trigger (V1)
 *
 * A simple button widget on V1:
 *    - ON => beepActive = true
 *    - OFF => beepActive = false
 ******************************************************/
BLYNK_WRITE(V1) {
  int buttonState = param.asInt();
  if (buttonState == 1) {
    // Button ON
    beepActive = true;
    Serial.println("Manual beep triggered (ON).");
  } else {
    // Button OFF
    beepActive = false;
    Serial.println("Manual beep stopped (OFF).");
  }
}
