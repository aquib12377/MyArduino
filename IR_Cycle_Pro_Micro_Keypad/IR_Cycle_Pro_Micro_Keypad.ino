#include <Keyboard.h>  // Include the Keyboard library for keyboard emulation

const int inputPin = 2;  // Pin to which the digital input is connected
int lastInputState = LOW;  // Store the last state of the input
int currentState;          // Store the current state of the input
unsigned long lastChangeTime = 0;  // Store the last time the state changed
int stateChangeCount = 0;  // Counter for state changes

void setup() {
  pinMode(inputPin, INPUT_PULLUP);   // Set the pin as input with pull-up resistor
  Serial.begin(9600);               // Initialize serial communication for debugging
  Keyboard.begin();                 // Initialize the Keyboard library
}

void loop() {
  // Read the current state of the digital input
  currentState = digitalRead(inputPin);

  // Check if the state has changed
  if (currentState != lastInputState) {
    unsigned long currentTime = millis(); // Get the current time
    unsigned long timeDifference = currentTime - lastChangeTime; // Calculate time difference
    
    Serial.print("Input state changed to: ");
    Serial.println(currentState == HIGH ? "HIGH" : "LOW");
    Serial.print("Time since last change: ");
    Serial.print(timeDifference);
    Serial.println(" ms");
    
    // Increment state change count
    stateChangeCount++;
    
    // Send the number '8' as a keystroke
    
    // Check if 5 state changes have occurred
    if (stateChangeCount % 2 == 0) {
      Serial.println("5 state changes detected!");
      Keyboard.print('8');
    delay(5);
    Keyboard.release('8');
    }
    
    // Update the last state and last change time
    lastInputState = currentState;
    lastChangeTime = currentTime;
  }

  delay(10); // Debounce delay (adjust if needed)
}

void onInputChange(int newState) {
  // Add your custom actions for HIGH or LOW state here if needed
}
