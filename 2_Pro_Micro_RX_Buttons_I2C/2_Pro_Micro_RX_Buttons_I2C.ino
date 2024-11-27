#include <Wire.h>

// Pins for 13 LEDs (since button 16 was removed)
const int ledPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, A0, A1};

void setup() {
  Serial.begin(9600);
  Wire.begin(9);  // Join I2C bus with address 9 (Slave)
  Wire.onReceive(receiveEvent);  // Register event handler for I2C communication
  
  // Initialize all LEDs as outputs and turn them off (HIGH state)
  for (int i = 0; i < 13; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);  // LEDs off initially
  }

  // Optional: Light each LED briefly on startup as a test
  Serial.println("Testing LEDs...");
  for (int i = 0; i < 13; i++) {
    delay(100);
    digitalWrite(ledPins[i], LOW);  // Turn LED on
    delay(200);
    digitalWrite(ledPins[i], HIGH);  // Turn LED off
    Serial.print("LED ");
    Serial.print(i);
    Serial.println(" tested.");
  }
  digitalWrite(ledPins[13], LOW);
  Serial.println("LED test complete.");
}

void loop() {
  // The LED control is handled in the receiveEvent function
}

void receiveEvent(int howMany) {
  if (Wire.available()) {
    int buttonIndex = Wire.read();  // Receive the button index from Pro Micro 1
    Serial.print("Received Data: ");
    Serial.println(buttonIndex);  // Print the received data for debugging

    if (buttonIndex == 21) {
      Serial.println("Reset command received.");
      resetLEDs();  // Reset all LEDs if reset command received
    } else if (buttonIndex >= 0 && buttonIndex < 13) {  // Adjusted to 13 LEDs
      Serial.print("Turning on LED ");
      Serial.println(buttonIndex);
      turnOnLED(buttonIndex);  // Turn on the corresponding LED
    } else {
      Serial.println("Invalid button index received.");
    }
  }
}

void turnOnLED(int index) {
  Serial.println("Turning off all LEDs...");
  // Turn off all LEDs first
  for (int i = 0; i < 13; i++) {
    digitalWrite(ledPins[i], HIGH);  // LEDs off
  }
  // Turn on the LED corresponding to the pressed button
  digitalWrite(ledPins[index], LOW);  // LED on
  Serial.print("LED ");
  Serial.print(index);
  Serial.println(" turned on.");
}

void resetLEDs() {
  Serial.println("Resetting all LEDs...");
  // Turn off all LEDs (Reset state)
  for (int i = 0; i < 13; i++) {
    digitalWrite(ledPins[i], HIGH);  // LEDs off
  }
  digitalWrite(ledPins[13], LOW);  // You can set one LED to stay on if desired
  Serial.println("All LEDs reset.");
}
