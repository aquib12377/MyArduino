#include <AccelStepper.h>

#define MOTOR_INTERFACE_TYPE 1

AccelStepper stepper1(MOTOR_INTERFACE_TYPE, 2, 3); // Motor 1: STEP pin 2, DIR pin 3
AccelStepper stepper2(MOTOR_INTERFACE_TYPE, 4, 5); // Motor 2: STEP pin 4, DIR pin 5
AccelStepper stepper3(MOTOR_INTERFACE_TYPE, 6, 7); // Motor 3: STEP pin 6, DIR pin 7

const int buttonPin = 8;

int buttonState = 0;
int lastButtonState = 0;
int pressCount = 0;

void setup() {
  stepper1.setMaxSpeed(3000);
  stepper1.setAcceleration(2000);
  
  stepper2.setMaxSpeed(3000);
  stepper2.setAcceleration(2000);
  
  stepper3.setMaxSpeed(3000);
  stepper3.setAcceleration(2000);

  // Initialize the button pin as an input
  pinMode(buttonPin, 0x2);

  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
}

void loop() {
  // Read the state of the button
  buttonState = digitalRead(buttonPin);

  // Check if the button is pressed
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) { // Button is pressed
      pressCount++;
      Serial.print(F("Button pressed: "));
      Serial.println(pressCount);
      stepper1.disableOutputs();
      stepper2.disableOutputs();
      stepper3.disableOutputs();
      // Control the stepper motors based on press count
      if (pressCount == 1) {
        stepper1.move(20000); // Move stepper1 200 steps
      } else if (pressCount == 2) {
        stepper2.move(20000); // Move stepper2 200 steps
      } else if (pressCount == 3) {
        stepper3.move(20000); // Move stepper3 200 steps
        pressCount = 0; // Reset the press count after the third motor
      }
    }
    // Delay for debouncing
    delay(50);
  }
  
  // Save the current button state
  lastButtonState = buttonState;

  // Run the steppers
  stepper1.run();
  stepper2.run();
  stepper3.run();
}
