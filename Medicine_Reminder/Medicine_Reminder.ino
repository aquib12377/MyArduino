#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Servo.h>

RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int buzzer = 12;
const int servoPin = 9;
Servo servo;

const int alarmHour[3] = { 19, 19, 19 };
const int alarmMinute[3] = { 1, 2, 3 };
const int servoAngles[3] = { 30, 90, 180 };
bool alarmTriggered[3] = { false, false, false };  // Track if alarms have been triggered

unsigned long previousMillis = 0;
const long interval = 1000;
int alarmIndex = 0;

void setup() {
  lcd.begin();
  lcd.backlight();
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

    Serial.println("RTC is not running, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  pinMode(buzzer, OUTPUT);
  servo.attach(servoPin);
  servo.write(0);
}

void loop() {
  for (int i = 0; i < 3; i++) {
    unsigned long currentMillis = millis();
    int k = 0;
    while (k < 40) {
      delay(500);
      displayTime();
      delay(1000);
      k++;
    }
    Serial.println("Time has arrived");
    alarmIndex = i;
    triggerAlarm();
    alarmTriggered[i] = true;  // Mark the alarm as triggered
  }
}

void triggerAlarm() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Take Medicine");
  lcd.setCursor(0, 1);
  lcd.print(alarmIndex + 1);

  ringBuzzer();
  delay(500);
  servo.write(90);
  delay(5000);
  servo.write(0);
}

void ringBuzzer() {
  for (int i = 0; i < 10; i++) {
    digitalWrite(buzzer, HIGH);
    delay(500);
    digitalWrite(buzzer, LOW);
    delay(500);
  }
}

void rotateServo(int angle) {
  servo.write(0);
  delay(1000);
  servo.write(angle);
  delay(5000);
  servo.write(0);
}

void displayTime() {
  DateTime now = rtc.now();

  static int lastHour = -1;
  static int lastMinute = -1;
  static int lastSecond = -1;

  if (now.hour() != lastHour || now.minute() != lastMinute || now.second() != lastSecond) {
    lcd.setCursor(0, 0);
    lcd.print("Medicine Reminder");

    lcd.setCursor(0, 1);
    if (now.hour() < 10) lcd.print('0');
    lcd.print(now.hour());
    lcd.print(':');
    if (now.minute() < 10) lcd.print('0');
    lcd.print(now.minute());
    lcd.print(':');
    if (now.second() < 10) lcd.print('0');
    lcd.print(now.second());

    Serial.print("Time: ");
    Serial.print(now.hour());
    Serial.print(':');
    Serial.print(now.minute());
    Serial.print(':');
    Serial.println(now.second());

    lastHour = now.hour();
    lastMinute = now.minute();
    lastSecond = now.second();
  }
}
