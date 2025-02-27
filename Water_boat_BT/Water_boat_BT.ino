#include "BluetoothSerial.h"  // Library for Bluetooth on ESP32

// Create a BluetoothSerial object
BluetoothSerial SerialBT;

// Pin definitions for Motor A
const int ENA  = 5;   // Enable Pin for Motor A
const int IN1  = 13;  // Input 1 for Motor A
const int IN2  = 12;  // Input 2 for Motor A

// Pin definitions for Motor B
const int ENB  = 18;  // Enable Pin for Motor B
const int IN3  = 14;  // Input 3 for Motor B
const int IN4  = 27;  // Input 4 for Motor B

// Pin definition for Relay
const int relayPin = 15;

// Function to drive the motors
void forward() {
  // Motor A forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  // Motor B forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  // Motor A backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  // Motor B backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left() {
  // Motor A forward, Motor B backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void right() {
  // Motor A backward, Motor B forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void setup() {
  Serial.begin(115200);         // For debugging
  SerialBT.begin("ESP32_Motors"); // Name the Bluetooth device

  // Set pin modes for L293D inputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);


  // Set pin mode for Relay
  pinMode(relayPin, OUTPUT);

  // Initially stop motors and relay OFF
  stopMotors();
  digitalWrite(relayPin, HIGH);

  Serial.println("ESP32 Bluetooth: L293D & Relay Control");
}

void loop() {
  // Check if data is available on Bluetooth
  if (SerialBT.available()) {
    char cmd = SerialBT.read();
    Serial.print("Received Command: ");
    Serial.println(cmd);

    switch (cmd) {
      case 'F':
        forward();
        SerialBT.println("Motors -> FORWARD");
        break;
      case 'B':
        backward();
        SerialBT.println("Motors -> BACKWARD");
        break;
      case 'L':
        left();
        SerialBT.println("Motors -> LEFT");
        break;
      case 'R':
        right();
        SerialBT.println("Motors -> RIGHT");
        break;
      case 'S':
        stopMotors();
        SerialBT.println("Motors -> STOP");
        break;
      case '1': 
        // Turn the relay ON
        digitalWrite(relayPin, LOW);
        SerialBT.println("Relay -> ON");
        break;
      case '0': 
        // Turn the relay OFF
        digitalWrite(relayPin, HIGH);
        SerialBT.println("Relay -> OFF");
        break;
      default:
        SerialBT.println("Unknown Command");
        break;
    }
  }

  // Optional: do any other logic you need here
  delay(20); // Small delay for stability
}
