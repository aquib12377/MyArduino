// Define pin connections
const int stepPin1 = 2; // Pulse pin for motor 1
const int dirPin1 = 3;  // Direction pin for motor 1
const int stepPin2 = 4; // Pulse pin for motor 2
const int dirPin2 = 5;  // Direction pin for motor 2
const int stepPin3 = 6; // Pulse pin for motor 3
const int dirPin3 = 7;  // Direction pin for motor 3
const int enablePin = 9; // Enable pin (optional)
const int buttonPin = 8; // Button pin

// Define step delay
const int stepDelay = 150; // Delay between steps in microseconds
const int steps = 80000;
// Button state variables
int buttonState = 0;
int lastButtonState = 0;
int pressCount = 0;

void setup() {
  // Set pin modes
  pinMode(stepPin1, OUTPUT);
  pinMode(dirPin1, OUTPUT);
  pinMode(stepPin2, OUTPUT);
  pinMode(dirPin2, OUTPUT);
  pinMode(stepPin3, OUTPUT);
  pinMode(dirPin3, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Enable the driver
  digitalWrite(enablePin, LOW); // Assuming LOW enables the driver, check TB6600 specs if different

  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
}

void loop() {
  // Read the state of the button
  buttonState = digitalRead(buttonPin);

  // Check if the button is pressed
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) { // Button is pressed

      rotateStepper(stepPin1, dirPin1, 1000, HIGH);
      delay(1000);
      rotateStepper(stepPin1, dirPin1, 500, HIGH);
      delay(1000);
      rotateStepper(stepPin1, dirPin1, 500, LOW);
      delay(500);

      rotateStepper(stepPin2, dirPin2, 500, HIGH);
delay(1000);
      rotateStepper(stepPin1, dirPin1, 500, HIGH);
      delay(1000);
      rotateStepper(stepPin1, dirPin1, 500, LOW);
      delay(500);
            rotateStepper(stepPin3, dirPin3, 1000, HIGH);

      pressCount++;
      Serial.print(F("Button pressed: "));
      Serial.println(pressCount);
      // if (pressCount == 1) {
      //   rotateStepper(stepPin1, dirPin1, steps, HIGH); // Rotate motor 1 clockwise
      // } else if (pressCount == 2) {
      //   rotateStepper(stepPin2, dirPin2, steps, HIGH); // Rotate motor 2 clockwise
      // } else if (pressCount == 3) {
      //   rotateStepper(stepPin3, dirPin3, steps, HIGH); // Rotate motor 3 clockwise
      //   pressCount = 0; // Reset the press count after the third motor
      // }
    }
    // Delay for debouncing
    delay(50);
  }

  // Save the current button state
  lastButtonState = buttonState;
}

void rotateStepper(int stepPin, int dirPin, int steps, int direction) {
  // Set direction
  digitalWrite(dirPin, direction);

  // Rotate stepper motor
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(stepDelay);
  }
}
