#include <Servo.h>

Servo myServo;

// Pins
const int rainSensorPin = A0;  // digital input from rain sensor
const int servoPin = A1;       // servo control pin

// Positions (angles in degrees)
int servoDryPosition = 0;   // servo angle when no rain
int servoRainPosition = 90; // servo angle when rain detected

void setup() {
  Serial.begin(9600);
  
  pinMode(rainSensorPin, INPUT);
  
  myServo.attach(servoPin);
  
  // Initialize servo to a default position
  myServo.write(servoDryPosition);
  delay(1000);
}

void loop() {
  // Read the rain sensor digital output
  int rainState = digitalRead(rainSensorPin);

  // If the rain sensor goes LOW or HIGH, depending on your module
  // Typically "LOW" might indicate water detected, but check your module's behavior
  if (rainState == LOW) {
    // Move servo to rain position
    myServo.write(servoRainPosition);
    delay(5000);
    Serial.println("Rain Detected! Moving servo to Rain Position.");
  } else {
    // Move servo to dry position
    myServo.write(servoDryPosition);
    Serial.println("No Rain. Servo at Dry Position.");
  }

  delay(500); // small delay
}
