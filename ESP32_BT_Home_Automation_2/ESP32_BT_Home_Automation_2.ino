/* 
   Modified Code to Control 4 Relays and a Gate Servo via ESP32 BluetoothSerial
   Author: [Your Name]
   Date: [Current Date]

   Features:
   - Control 4 relays (ON/OFF) using numeric commands
   - Control a gate servo with two positions (Open/Close) using numeric commands
   - Bluetooth serial communication for remote control
*/

#include <ESP32Servo.h>
#include <BluetoothSerial.h>

// Check if Bluetooth is enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

// Relay pins
const int relayPins[4] = {32, 33, 25, 26}; // GPIO pins for the 4 relays

// Servo pin and angles
const int gateServoPin = 13; // GPIO pin for gate servo
const int GATE_OPEN_ANGLE = 0;    // Define as per your requirement
const int GATE_CLOSE_ANGLE = 90;  // Define as per your requirement

Servo gateServo;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  SerialBT.begin("ESP32_Control"); // Bluetooth device name

  // Initialize relay pins
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); // Initialize relays to OFF
  }

  // Attach servo
  gateServo.attach(gateServoPin); // Gate servo

  // Set initial servo position to closed
  gateServo.write(GATE_CLOSE_ANGLE);

  Serial.println("ESP32 is ready to receive Bluetooth commands.");
}

void loop() {
  // Handle Bluetooth commands
  if (SerialBT.available()) {
    char command = SerialBT.read();
    Serial.print("Received command: ");
    Serial.println(command);

    switch (command) {
      case '1':
        digitalWrite(relayPins[0], HIGH);
        Serial.println("Relay 1 turned ON.");
        SerialBT.println("Relay 1 is ON.");
        break;

      case '2':
        digitalWrite(relayPins[0], LOW);
        Serial.println("Relay 1 turned OFF.");
        SerialBT.println("Relay 1 is OFF.");
        break;

      case '3':
        digitalWrite(relayPins[1], HIGH);
        Serial.println("Relay 2 turned ON.");
        SerialBT.println("Relay 2 is ON.");
        break;

      case '4':
        digitalWrite(relayPins[1], LOW);
        Serial.println("Relay 2 turned OFF.");
        SerialBT.println("Relay 2 is OFF.");
        break;

      case '5':
        digitalWrite(relayPins[2], HIGH);
        Serial.println("Relay 3 turned ON.");
        SerialBT.println("Relay 3 is ON.");
        break;

      case '6':
        digitalWrite(relayPins[2], LOW);
        Serial.println("Relay 3 turned OFF.");
        SerialBT.println("Relay 3 is OFF.");
        break;

      case '7':
        digitalWrite(relayPins[3], HIGH);
        Serial.println("Relay 4 turned ON.");
        SerialBT.println("Relay 4 is ON.");
        break;

      case '8':
        digitalWrite(relayPins[3], LOW);
        Serial.println("Relay 4 turned OFF.");
        SerialBT.println("Relay 4 is OFF.");
        break;

      case 'D ':
        gateServo.write(GATE_OPEN_ANGLE);
        Serial.println("Gate opened.");
        SerialBT.println("Gate is OPEN.");
        break;

      case 'd':
        gateServo.write(GATE_CLOSE_ANGLE);
        Serial.println("Gate closed.");
        SerialBT.println("Gate is CLOSED.");
        break;

      default:
        Serial.println("Unknown command.");
        SerialBT.println("Error: Unknown command.");
        break;
    }
  }

  // Optional: Add a small delay to prevent overwhelming the loop
  delay(10);
}
