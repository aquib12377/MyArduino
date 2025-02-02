#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Initialize LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Initialize Servos
Servo horizontalServo;
Servo verticalServo;

// LDR Module Pins
const int ldrRight = 4;
const int ldrTop = 5;
const int ldrLeft = 6;
const int ldrBottom = 7;

// Servo Pins
const int horizontalPin = 9;
const int verticalPin = 10;

// Servo angles
int horizontalAngle = 90; // Start at the center
int verticalAngle = 30;   // Start at the center

// Movement thresholds
const int stepSize = 2;   // Angle step size
const int delayTime = 500; // Delay for stability

void setup() {
  Serial.begin(9600);
  Serial.println("2 Axis Solar Tracker");
  // LCD Initialization
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("2-Axis Solar");
  lcd.setCursor(0, 1);
  lcd.print("Tracker System");
  delay(2000);
  lcd.clear();

  // Servo Initialization
  horizontalServo.attach(horizontalPin);
  verticalServo.attach(verticalPin);

  // Set initial positions
  horizontalServo.write(horizontalAngle);
  verticalServo.write(verticalAngle);

  // Set LDR pins as inputs
  pinMode(ldrRight, INPUT_PULLUP);
  pinMode(ldrTop, INPUT_PULLUP);
  pinMode(ldrLeft, INPUT_PULLUP);
  pinMode(ldrBottom, INPUT_PULLUP);

  lcd.setCursor(0, 0);
  lcd.print("Tracking...");
}

void loop() {
  // Read LDR sensors
  bool rightLight = digitalRead(ldrRight) == LOW;
  bool topLight = digitalRead(ldrTop) == LOW;
  bool leftLight = digitalRead(ldrLeft) == LOW;
  bool bottomLight = digitalRead(ldrBottom) == LOW;

  Serial.print("TOP: ");
  Serial.print(topLight);
  Serial.print(" | Bottom: ");
  Serial.print(bottomLight);
  Serial.print(" | Left: ");
  Serial.print(leftLight);
  Serial.print(" | Right: ");
  Serial.println(rightLight);

  // Determine horizontal movement
  if (rightLight && !leftLight) {
    horizontalAngle = constrain(horizontalAngle + stepSize, 0, 180);
    horizontalServo.write(horizontalAngle);
    updateLCD("East Right: "+String(horizontalAngle));
  } else if (leftLight && !rightLight) {
    horizontalAngle = constrain(horizontalAngle - stepSize, 0, 180);
    horizontalServo.write(horizontalAngle);
    updateLCD("West Left: "+String(horizontalAngle));
  }

  // Determine vertical movement
  if (topLight && !bottomLight) {
    verticalAngle = constrain(verticalAngle + stepSize, 0, 180);
    verticalServo.write(verticalAngle);
    updateLCD("North Up: "+String(verticalAngle));
  } else if (bottomLight && !topLight) {
    verticalAngle = constrain(verticalAngle - stepSize, 0, 180);
    verticalServo.write(verticalAngle);
    updateLCD("South Down: "+String(verticalAngle));
  }

  // Wait before next adjustment
  delay(30);
}

// Function to update LCD with direction
void updateLCD(String direction) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tracking...");
  lcd.setCursor(0, 1);
  lcd.print(direction);
}
