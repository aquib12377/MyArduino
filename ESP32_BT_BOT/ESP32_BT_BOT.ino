#include "BluetoothSerial.h"

// Motor pins
const int motor1Pin1 = 32; // IN1 on Motor Driver
const int motor1Pin2 = 33; // IN2 on Motor Driver
const int motor2Pin1 = 25; // IN3 on Motor Driver
const int motor2Pin2 = 26; // IN4 on Motor Driver

BluetoothSerial SerialBT;

// Timeout settings
unsigned long lastCommandTime = 0; // Stores the time of the last received command
const unsigned long commandTimeout = 200; // Timeout in milliseconds

void setup() {
  // Initialize motor control pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Stop the motors initially
  stopMotors();

  // Start Bluetooth communication
  Serial.begin(115200);
  SerialBT.begin("Bluetooth Controlled Bot"); // Bluetooth device name
  Serial.println("Bluetooth Device Started. Waiting for commands...");
}

void loop() {
  // Check for Bluetooth data
  if (SerialBT.available()) {
    char command = SerialBT.read(); // Read the incoming command
    Serial.print("Command received: ");
    Serial.println(command);

    // Update the last command time
    lastCommandTime = millis();

    // Act on the received command
    switch (command) {
      case 'F': // Move Forward
        moveForward();
        break;
      case 'B': // Move Backward
        moveBackward();
        break;
      case 'L': // Turn Left
        turnLeft();
        break;
      case 'R': // Turn Right
        turnRight();
        break;
      case 'S': // Stop
        stopMotors();
        break;
      default:
        Serial.println("Invalid Command");
    }
  }

  // Stop motors if no command is received within the timeout
  if (millis() - lastCommandTime > commandTimeout) {
    stopMotors();
  }
}

// Function to move the bot forward
void moveForward() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
  Serial.println("Moving Forward");
}

// Function to move the bot backward
void moveBackward() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
  Serial.println("Moving Backward");
}

// Function to turn the bot left
void turnLeft() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
  Serial.println("Turning Left");
}

// Function to turn the bot right
void turnRight() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
  Serial.println("Turning Right");
}

// Function to stop all motors
void stopMotors() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);
  Serial.println("Motors Stopped");
}