#include <Wire.h>

// Pins for 14 LEDs
const int ledPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0, A1};

void setup() {
  Serial.begin(9600);
  Wire.begin(8);  // Join I2C bus with address 8 (Slave)
  Wire.onReceive(receiveEvent);  // Register event handler for I2C communication
  for (int i = 0; i < 14; i++) {
    pinMode(ledPins[i], OUTPUT);  // Setup all LEDs as outputs
    digitalWrite(ledPins[i], HIGH);  // Initialize all LEDs as OFF
  }

  for (int i = 0; i < 14; i++) {
    delay(100);
    digitalWrite(ledPins[i], LOW);  // Initialize all LEDs as OFF
    delay(200);
    digitalWrite(ledPins[i], HIGH);
  }
}

void loop() {
  // The LED control is handled in the receiveEvent function
}

void receiveEvent(int howMany) {
  if (Wire.available()) {
    int buttonIndex = Wire.read();  // Receive the button index from Pro Micro 1
    Serial.println("Received Data: "+String(buttonIndex));
    if (buttonIndex == 14) {
      resetLEDs();  // Reset all LEDs if reset command received
    } else if (buttonIndex >= 0 && buttonIndex < 14) {
      turnOnLED(buttonIndex);  // Turn on the corresponding LED
    }
  }
}

void turnOnLED(int index) {
  // Turn off all LEDs first
  for (int i = 0; i < 14; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  // Turn on the LED corresponding to the pressed button
  digitalWrite(ledPins[index], LOW);
}

void resetLEDs() {
  // Turn off all LEDs (Reset state)
  for (int i = 0; i < 14; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  digitalWrite(ledPins[13], LOW);
}
