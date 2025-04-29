#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Pin Definitions
#define TRIG_PIN A3
#define ECHO_PIN A2
#define BUZZER_PIN 2
#define SERVO_PIN 3

// Constants
const int DAM_FULL_DISTANCE_CM = 10; // Adjust as per your dam height threshold

// Initialize LCD: I2C address 0x27, 16 column and 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo damGateServo;

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  damGateServo.attach(SERVO_PIN);
  damGateServo.write(0); // Closed position initially

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dam Monitoring");
  delay(2000);
  lcd.clear();
}

void loop() {
  int waterLevel = measureWaterLevel();

  lcd.setCursor(0, 0);
  lcd.print("Level: ");
  lcd.print(waterLevel);
  lcd.print(" cm   ");

  if (waterLevel <= DAM_FULL_DISTANCE_CM) {
    lcd.setCursor(0, 1);
    lcd.print("Dam Full!      ");

    activateBuzzer();

    damGateServo.write(90); // Open gate
  }
  else {
    lcd.setCursor(0, 1);
    lcd.print("Dam Filling... ");

    digitalWrite(BUZZER_PIN, LOW);

    damGateServo.write(0); // Close gate
  }

  delay(500);
}

int measureWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);

  int distanceCm = duration * 0.034 / 2;

  return distanceCm;
}

void activateBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
  delay(500);
}