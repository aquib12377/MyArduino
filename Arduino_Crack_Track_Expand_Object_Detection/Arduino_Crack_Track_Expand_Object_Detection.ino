#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define pins for IR Sensors
#define IR_CRACK_LEFT 8      // Left crack detection sensor
#define IR_CRACK_RIGHT 9     // Right crack detection sensor
#define IR_EXPANDED_TRACK 10  // Expanded track detection sensor
#define IR_OBJECT 11          // Object detection sensor

// Motor control pins
#define IN1 4  // Motor A pin 1
#define IN2 5  // Motor A pin 2
#define IN3 6  // Motor B pin 1
#define IN4 7 // Motor B pin 2

// Buzzer pin
#define BUZZER 12

// I2C LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Initialize LCD
  lcd.begin();
  lcd.backlight();

  // Initialize IR Sensor pins
  pinMode(IR_CRACK_LEFT, INPUT_PULLUP);
  pinMode(IR_CRACK_RIGHT, INPUT_PULLUP);
  pinMode(IR_EXPANDED_TRACK, INPUT_PULLUP);
  pinMode(IR_OBJECT, INPUT_PULLUP);

  // Initialize motor control pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Initialize buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();
}

void loop() {
  int crackLeft = digitalRead(IR_CRACK_LEFT);
  int crackRight = digitalRead(IR_CRACK_RIGHT);
  int expandedTrack = digitalRead(IR_EXPANDED_TRACK);
  int objectDetected = digitalRead(IR_OBJECT);

  // Crack detection logic
  if (crackLeft == HIGH || crackRight == HIGH) {
    lcd.setCursor(0, 0);
    lcd.print("Crack Detected");
    digitalWrite(BUZZER, HIGH);
    stopMotors();
    delay(1000);
    digitalWrite(BUZZER, LOW);
  } else if (expandedTrack == LOW) {
    // Expanded track detection logic
    lcd.setCursor(0, 0);
    lcd.print("Track Expanded");
    digitalWrite(BUZZER, HIGH);
    stopMotors();
    delay(1000);
    digitalWrite(BUZZER, LOW);
  } else if (objectDetected == LOW) {
    // Object detection logic
    lcd.setCursor(0, 0);
    lcd.print("Object Detected");
    digitalWrite(BUZZER, HIGH);
    stopMotors();
    delay(1000);
    digitalWrite(BUZZER, LOW);
  } else {
    // If no issues, move the car forward
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("All Clear");
    moveForward();
  }
  delay(100);
}

// Function to stop motors
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// Function to move car forward
void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}
