// Include the BluetoothSerial library
#include "BluetoothSerial.h"

// Create a BluetoothSerial object
BluetoothSerial SerialBT;

// Define the Bluetooth device name
const char* BT_DEVICE_NAME = "ESP32_BT_Sender";

void setup() {
  // Initialize Serial Monitor at 115200 baud rate
  Serial.begin(115200);
  Serial.println("ESP32 Bluetooth Serial Example");

  // Initialize Bluetooth Serial
  if (!SerialBT.begin(BT_DEVICE_NAME)) {  // Bluetooth device name
    Serial.println("An error occurred initializing Bluetooth");
  } else {
    Serial.print("Bluetooth initialized. Device name: ");
    Serial.println(BT_DEVICE_NAME);
  }

  // Optional: Display the Bluetooth address
  Serial.print("The device started, now you can pair it with bluetooth!");
  
}

void loop() {
  // Check if data is available on the Serial Monitor
  if (Serial.available()) {
    // Read the incoming byte
    String incomingData = Serial.readStringUntil('\n');  // Read until newline character
    incomingData.trim();  // Remove any leading/trailing whitespace

    // Check if any data was read
    if (incomingData.length() > 0) {
      Serial.print("Sending over Bluetooth: ");
      Serial.println(incomingData);

      // Send the data over Bluetooth
      SerialBT.println(incomingData);
    }
  }

  // Optional: Receive data from Bluetooth and print to Serial Monitor
  if (SerialBT.available()) {
    String btData = SerialBT.readStringUntil('\n');
    btData.trim();
    if (btData.length() > 0) {
      Serial.print("Received from Bluetooth: ");
      Serial.println(btData);
    }
  }

  delay(20);  // Small delay to avoid overwhelming the loop
}
