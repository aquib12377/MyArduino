#include <Keyboard.h>

// Define the touch sensor pins
const int touchPins[9] = {2, 3, 4, 5, 6, 7, 8, 9, 10}; // Adjust these pin numbers as necessary

// Define the corresponding keys to be pressed for each sensor
const char keys[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '97'};

// Array to keep track of the previous state of each touch sensor
bool previousState[9] = {LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW};

void setup() {
  // Initialize the touch sensor pins
  for (int i = 0; i < 9; i++) {
    pinMode(touchPins[i], INPUT_PULLUP);
  }
  
  // Start the Keyboard library
  Keyboard.begin();
  Serial.begin(9600);
}

void loop() {
  for (int i = 0; i < 9; i++) {
    int sensorValue = digitalRead(touchPins[i]);
    Serial.println(sensorValue);
    // Detect state change
    if (sensorValue == HIGH && previousState[i] == LOW) {
      // If touch is detected (state changed from LOW to HIGH)
      Keyboard.press(keys[i]);
      delay(50); // Short delay to ensure key press is registered
      Keyboard.release(keys[i]);
      previousState[i] = HIGH;
    } else if (sensorValue == LOW) {
      // Update the previous state to LOW if the touch is not detected
      previousState[i] = LOW;
    }

    // Add a small delay to debounce and avoid multiple key presses
    delay(50);
  }
}
