#include <Wire.h>

const int ledPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0,A1};  // 13 LEDs

void setup() {
  Serial.begin(9600);
  Wire.begin(9);  // Initialize as I2C slave
  Wire.setClock(400000);  // Set I2C clock speed to 400 kHz (Fast Mode)

  Wire.onReceive(receiveLedCommand);  // Register event for master command
  for (int i = 0; i < 14; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);  // Turn off all LEDs
  }
  digitalWrite(13,LOW);
}

void loop() {
  // Nothing to do in loop; LEDs controlled on command
}

void receiveLedCommand(int howMany) {
  if (Wire.available()) {
    int command = Wire.read();  // Read command
    Serial.println(command);
    if (command >= 0 && command < 14) {  // Activate specified LED
      for (int i = 0; i < 14; i++) {
        digitalWrite(ledPins[i], i == command ? LOW : HIGH);
      }
    } else if (command == 14) {  // Turn off all LEDs
      for (int i = 0; i < 13; i++) {
        digitalWrite(ledPins[i], HIGH);
      }
    }
  }
}
