/*
 * Arduino Nano Voice Recognition and Relay Control with Bluetooth Transmission
 * Features:
 * - Voice Recognition for 4 commands
 * - Controls 4 relays based on recognized commands
 * - Sends recognized command details via HC-05 Bluetooth module
 */

#include <SoftwareSerial.h>
#include <VoiceRecognitionV3.h> // Include the Voice Recognition library

// =====================
// Pin Definitions
// =====================
#define VR_RX_PIN 2   // Voice Recognition Module TX -> Arduino RX (Pin 2)
#define VR_TX_PIN 3   // Voice Recognition Module RX -> Arduino TX (Pin 3)
#define BT_RX_PIN 4   // HC-05 TX -> Arduino RX (Pin 4)
#define BT_TX_PIN 5   // HC-05 RX -> Arduino TX (Pin 5)

#define RELAY1_PIN 6
#define RELAY2_PIN 7
#define RELAY3_PIN 8
#define RELAY4_PIN 9

// =====================
// SoftwareSerial Definitions
// =====================
SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN); // RX, TX for HC-05 Bluetooth Module

// =====================
// Voice Recognition Module Setup
// =====================
VR myVR(VR_RX_PIN,VR_TX_PIN);

// =====================
// Relay States
// =====================
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

// =====================
// Command IDs
// =====================
// These IDs should correspond to the IDs assigned during the training of the Voice Recognition Module
#define CMD_RELAY1 1
#define CMD_RELAY2 2
#define CMD_RELAY3 3
#define CMD_RELAY4 4

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
  Serial.println("Voice Recognition Relay Control with Bluetooth");

  // Initialize SoftwareSerial for VR and BT
  btSerial.begin(9600);
  myVR.begin(9600);
  // Initialize Relay Pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  // Ensure all relays are OFF at startup
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);
]

  // Optionally, display available commands
  myVR.clear(); // Clear all trained commands (optional, if you want to re-train via code)
  // Load commands if necessary or ensure they are pre-trained
}

void loop() {
  uint8_t records[7]; // save record
  uint8_t buf[64];

  // Check if Voice Recognition Module has a command
  if (myVR.recognize(buf, 50) > 0) {
    uint8_t commandID = buf[1];
    Serial.print("Recognized Command ID: ");
    Serial.println(commandID);

    switch (commandID) {
      case CMD_RELAY1:
        relay1State = !relay1State; // Toggle Relay 1 state
        digitalWrite(RELAY1_PIN, !relay1State ? HIGH : LOW);
        Serial.print("Relay 1 ");
        Serial.println(relay1State ? "ON" : "OFF");
        sendBTMessage("Relay1 " + String(relay1State ? "ON" : "OFF"));
        break;

      case CMD_RELAY2:
        relay2State = !relay2State; // Toggle Relay 2 state
        digitalWrite(RELAY2_PIN, !relay2State ? HIGH : LOW);
        Serial.print("Relay 2 ");
        Serial.println(relay2State ? "ON" : "OFF");
        sendBTMessage("Relay2 " + String(relay2State ? "ON" : "OFF"));
        break;

      case CMD_RELAY3:
        relay3State = !relay3State; // Toggle Relay 3 state
        digitalWrite(RELAY3_PIN, !relay3State ? HIGH : LOW);
        Serial.print("Relay 3 ");
        Serial.println(relay3State ? "ON" : "OFF");
        sendBTMessage("Relay3 " + String(relay3State ? "ON" : "OFF"));
        break;

      case CMD_RELAY4:
        relay4State = !relay4State; // Toggle Relay 4 state
        digitalWrite(RELAY4_PIN, !relay4State ? HIGH : LOW);
        Serial.print("Relay 4 ");
        Serial.println(relay4State ? "ON" : "OFF");
        sendBTMessage("Relay4 " + String(relay4State ? "ON" : "OFF"));
        break;

      default:
        Serial.println("Unknown Command");
        sendBTMessage("Unknown Command ID: " + String(commandID));
        break;
    }
  }

  // Handle incoming Bluetooth messages if necessary
  // Example: Turn relays on/off via Bluetooth commands
  // Uncomment and modify the following if you want bi-directional control
  /*
  if (btSerial.available()) {
    String btMsg = btSerial.readStringUntil('\n');
    Serial.print("Received via BT: ");
    Serial.println(btMsg);

    if (btMsg == "Relay1 ON") {
      relay1State = true;
      digitalWrite(RELAY1_PIN, HIGH);
    } else if (btMsg == "Relay1 OFF") {
      relay1State = false;
      digitalWrite(RELAY1_PIN, LOW);
    }
    // Repeat for other relays as needed
  }
  */

  delay(100); // Small delay to stabilize
}

// =====================
// Function to Send Messages via Bluetooth
// =====================
void sendBTMessage(String message) {
  btSerial.println(message);
  Serial.print("Sent via Bluetooth: ");
  Serial.println(message);
}
