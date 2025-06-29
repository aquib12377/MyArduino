// Arduino line follower bot using Adafruit Motor Shield (AFMotor library)
// IR modules output HIGH on black, LOW elsewhere

#include <AFMotor.h>

// Connect two DC motors on ports M1 and M2 of the Motor Shield
AF_DCMotor leftMotor(1);
AF_DCMotor rightMotor(4);

// IR sensor pins
const int leftSensorPin  = A1;
const int rightSensorPin = A0;

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  Serial.println("Line Follower with AFMotor Shield");

  // Set initial speed for both motors (0-255)
  leftMotor.setSpeed(150);
  rightMotor.setSpeed(150);

  // Sensor pins
  pinMode(leftSensorPin, INPUT);
  pinMode(rightSensorPin, INPUT);
}

void loop() {
  bool leftDetected  = digitalRead(leftSensorPin)  == HIGH;
  bool rightDetected = digitalRead(rightSensorPin) == HIGH;

  Serial.print("L="); Serial.print(leftDetected);
  Serial.print(" R="); Serial.println(rightDetected);

  if (leftDetected && rightDetected) {
    moveForward();
  } else if (leftDetected && !rightDetected) {
    turnLeft();
  } else if (!leftDetected && rightDetected) {
    turnRight();
  } else {
    stopMotors();
  }

  delay(50);
}

// Movement functions
void moveForward() {
  leftMotor.run(FORWARD);
  rightMotor.run(FORWARD);
}

void turnLeft() {
  leftMotor.run(RELEASE);  // stop left motor
  rightMotor.run(FORWARD);
}

void turnRight() {
  leftMotor.run(FORWARD);
  rightMotor.run(RELEASE); // stop right motor
}

void stopMotors() {
  leftMotor.run(RELEASE);
  rightMotor.run(RELEASE);
}
