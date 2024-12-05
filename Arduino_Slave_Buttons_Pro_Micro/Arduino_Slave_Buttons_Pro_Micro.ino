#include <Wire.h>

const int buttonPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, A3, A2, A0, A1};  // 13 game buttons + reset
const int numButtons = sizeof(buttonPins) / sizeof(buttonPins[0]);
int buttonStates[numButtons];  // Array to store the current states of all buttons

void setup() {
  Serial.begin(9600);  // Initialize Serial for debugging
  Wire.begin(8);
  Wire.setClock(400000);  // Set I2C clock speed to 400 kHz (Fast Mode)
       // Initialize as I2C slave
  Wire.onRequest(sendButtonStates);  // Register event for master request

  // Configure buttons as input with pull-up resistors
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttonStates[i] = HIGH;  // Initialize all buttons to released state
  }

  Serial.println("Button Reader Initialized");
}

void loop() {
  // Continuously read and store the button states
  for (int i = 0; i < numButtons; i++) {
    buttonStates[i] = digitalRead(buttonPins[i]);
  }
}

void sendButtonStates() {
  // Send the stored button states to the master
  for (int i = 0; i < numButtons; i++) {
    Wire.write(buttonStates[i]);
  }
}
