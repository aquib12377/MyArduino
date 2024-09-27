#include <Wire.h>

// Pins for 13 LEDs (since button 16 was removed)
const int ledPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, A0,A1};

void setup() {
  Serial.begin(9600);
  Wire.begin(8);  // Join I2C bus with address 8 (Slave)
  Wire.onReceive(receiveEvent);  // Register event handler for I2C communication
  
  // Initialize all LEDs as outputs and turn them off (HIGH state)
  for (int i = 0; i < 13; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);  // LEDs off initially
  }

  // Optional: Light each LED briefly on startup as a test
  for (int i = 0; i < 13; i++) {
    delay(100);
    digitalWrite(ledPins[i], LOW);  // Turn LED on
    delay(200);
    digitalWrite(ledPins[i], HIGH);  // Turn LED off
  }
}

void loop() {
  // The LED control is handled in the receiveEvent function
}

void receiveEvent(int howMany) {
  if (Wire.available()) {
    int buttonIndex = Wire.read();  // Receive the button index from Pro Micro 1
    Serial.println("Received Data: " + String(buttonIndex));

    if (buttonIndex == 13) {
      resetLEDs();  // Reset all LEDs if reset command received
    } else if (buttonIndex >= 0 && buttonIndex < 12) {  // Adjusted to 13 LEDs
      turnOnLED(buttonIndex);  // Turn on the corresponding LED
    }
  }
}

void turnOnLED(int index) {
  // Turn off all LEDs first
  for (int i = 0; i < 13; i++) {
    digitalWrite(ledPins[i], HIGH);  // LEDs off
  }
  // Turn on the LED corresponding to the pressed button
  digitalWrite(ledPins[index], LOW);  // LED on
}

void resetLEDs() {
  // Turn off all LEDs (Reset state)
  for (int i = 0; i < 13; i++) {
    digitalWrite(ledPins[i], HIGH);  // LEDs off
  }
  digitalWrite(ledPins[12], LOW);  // You can set one LED to stay on if desired
}
