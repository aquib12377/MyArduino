#include <SoftwareSerial.h>

// Define pins for SoftwareSerial
SoftwareSerial BTSerial(2, 3);  // RX on pin 2, TX on pin 3

// Define the pin for the relay
int relayPin = 12;

void setup() {
  // Set the relay pin as output
  pinMode(relayPin, OUTPUT);
  
  // Initially turn off the relay
  digitalWrite(relayPin, LOW);

  // Begin serial communication with HC-05
  BTSerial.begin(9600);  // Baud rate for HC-05

  // Optionally, start regular Serial communication for debugging
  Serial.begin(9600);
}

void loop() {
  // Check if data is available to read from Bluetooth
  if (BTSerial.available() > 0) {
    char data = BTSerial.read();  // Read the incoming data

    if (data == '1') {
      // Turn ON the relay if '1' is received
      digitalWrite(relayPin, HIGH);
      BTSerial.println("Relay ON");  // Send feedback via Bluetooth
      Serial.println("Relay ON");    // Send feedback via Serial for debugging
    }
    else if (data == '0') {
      // Turn OFF the relay if '0' is received
      digitalWrite(relayPin, LOW);
      BTSerial.println("Relay OFF");  // Send feedback via Bluetooth
      Serial.println("Relay OFF");    // Send feedback via Serial for debugging
    }
  }
}
