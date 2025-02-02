#include <Stepper.h>

// ---------------------------------
// Stepper Setup
// ---------------------------------
const int stepsPerRevolution = 200; // Adjust if your motor is different
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);

// ---------------------------------
// Single Push Button for Short/Long Press
// ---------------------------------
const int pinAction = 2; // The push button

// ---------------------------------
// Switches for Selecting Step Count
// ---------------------------------
const int pinSteps1 = A0; // Press to select 1 step
const int pinSteps2 = A1; // Press to select 2 steps
const int pinSteps3 = A2; // Press to select 3 steps

// ---------------------------------
// Press Timing & Logic for pinAction
// ---------------------------------
bool prevButtonState      = HIGH; 
bool isButtonPressed      = false; 
bool longPressDetected    = false;
unsigned long pressStartTime   = 0;
const unsigned long LONG_PRESS_DURATION = 3000; // 5 seconds

// ---------------------------------
// Movement Settings
// ---------------------------------
// We'll use a variable instead of a fixed constant:
int stepsToMove      = 3;  // Default is 3 steps
const int STEPPER_SPEED = 60;  // RPM

void setup() {
  Serial.begin(9600);

  // Set up the stepper speed
  myStepper.setSpeed(STEPPER_SPEED);

  // Button for short/long press
  pinMode(pinAction, INPUT_PULLUP);

  // Switches for selecting the step count
  pinMode(pinSteps1, INPUT_PULLUP);
  pinMode(pinSteps2, INPUT_PULLUP);
  pinMode(pinSteps3, INPUT_PULLUP);

  Serial.println("Ready. Short press => move forward. Long press (>=5s) => move backward.");
  Serial.println("Use A0/A1/A2 switches to set steps to 1, 2, or 3.");
}

void loop() {
  // ----------------------------------------------------------------------
  // 1) Read the state of the three step-selection switches (A0, A1, A2)
  //    to decide how many steps to move for short/long press.
  // ----------------------------------------------------------------------
  if (digitalRead(pinSteps1) == LOW) {
    // If A0 is pressed, steps = 1
    stepsToMove = 1;
  }
  else if (digitalRead(pinSteps2) == LOW) {
    // If A1 is pressed, steps = 2
    stepsToMove = 2;
  }
  else if (digitalRead(pinSteps3) == LOW) {
    // If A2 is pressed, steps = 3
    stepsToMove = 3;
  }
  else {
    // If none of the switches are pressed, default to 3
    stepsToMove = 3;
  }

  // ----------------------------------------------------------------------
  // 2) Handle the push button on pinAction for short/long press
  // ----------------------------------------------------------------------
  bool buttonState = digitalRead(pinAction);

  // Detect PRESS (HIGH -> LOW)
  if (buttonState == LOW && prevButtonState == HIGH) {
    // Button just went down
    isButtonPressed   = true;
    longPressDetected = false;
    pressStartTime    = millis();
    Serial.println("Button Pressed");
  }

  // Detect RELEASE (LOW -> HIGH)
  if (buttonState == HIGH && prevButtonState == LOW) {
    // Button just went up (released)
    isButtonPressed = false;
    unsigned long pressDuration = millis() - pressStartTime;

    // If we haven't already triggered a long press by the time of release:
    if (!longPressDetected) {
      if (pressDuration < LONG_PRESS_DURATION) {
        // SHORT press action => move forward
        Serial.print("Short press => Moving forward ");
        Serial.print(stepsToMove);
        Serial.println(" steps...");
        myStepper.step(stepsToMove);  // Blocking move forward
      } else {
        // If for some reason it wasn't triggered earlier, treat as long press
        Serial.print("Long press => Moving backward ");
        Serial.print(stepsToMove);
        Serial.println(" steps...");
        myStepper.step(-stepsToMove); // Blocking move backward
      }
    }
  }

  // If the button is still pressed, check for LONG press
  if (isButtonPressed && !longPressDetected) {
    unsigned long pressDuration = millis() - pressStartTime;
    if (pressDuration >= LONG_PRESS_DURATION) {
      // We’ve hit 5 seconds => LONG press
      longPressDetected = true;
      Serial.print("Long press => Moving backward ");
      Serial.print(stepsToMove);
      Serial.println(" steps (while still held)...");
      myStepper.step(-stepsToMove);  // Blocking move backward
    }
  }

  // Update previous button state for next loop
  prevButtonState = buttonState;
}
