#include <Servo.h>  // Include the Servo library

// Define analog pins for joystick axes
const int X_pin = A0;  // Joystick X-axis connected to analog pin A0
const int Y_pin = A1;  // Joystick Y-axis connected to analog pin A1

// Define digital pin for joystick button
const int button_pin = 2;  // Joystick button connected to digital pin 2

// Define digital pin for relay control
const int relay_pin = 4;  // Relay control connected to digital pin 4

// Create Servo objects
Servo servoX;  // Servo for X-axis control
Servo servoY;  // Servo for Y-axis control

// Variables to store current servo positions
int servoXPos = 90;  // Initialize to center position
int servoYPos = 90;  // Initialize to center position

// Define thresholds for joystick tilt
const int thresholdHigh = 1000;  // Threshold for right/up tilt
const int thresholdLow = 50;      // Threshold for left/down tilt

// Define step size and delay for gradual movement
const int stepSize = 5;      // Degrees to move per step
const int stepDelay = 5;    // Delay in milliseconds between steps

void setup() {
  // Initialize serial communication at 9600 bits per second
  Serial.begin(9600);

  // Set the button pin as input with internal pull-up resistor
  pinMode(button_pin, INPUT_PULLUP);

  // Set relay pin as output
  pinMode(relay_pin, OUTPUT);

  // Initialize relay to inactive state (HIGH for active LOW relay)
  digitalWrite(relay_pin, HIGH);

  // Attach the servos to digital pins
  servoX.attach(9);  // Servo X connected to pin 9
  servoY.attach(10); // Servo Y connected to pin 10

  // Move servos to initial (center) positions
  servoX.write(servoXPos);
  servoY.write(servoYPos);
}

void loop() {
  // Read the analog values from the joystick axes
  int X_value = analogRead(X_pin);
  int Y_value = analogRead(Y_pin);

  // Handle Servo X Movement Based on Joystick X-axis
  if (X_value >= thresholdHigh && servoXPos < 180) {
    servoXPos += stepSize;
    if (servoXPos > 180) {
      servoXPos = 180;
    }
    servoX.write(servoXPos);
    delay(stepDelay);
  }
  else if (X_value <= thresholdLow && servoXPos > 0) {
    servoXPos -= stepSize;
    if (servoXPos < 0) {
      servoXPos = 0;
    }
    servoX.write(servoXPos);
    delay(stepDelay);
  }

  // Handle Servo Y Movement Based on Joystick Y-axis
  if (Y_value >= thresholdHigh && servoYPos < 180) {
    servoYPos += stepSize;
    if (servoYPos > 180) {
      servoYPos = 180;
    }
    servoY.write(servoYPos);
    delay(stepDelay);
  }
  else if (Y_value <= thresholdLow && servoYPos > 0) {
    servoYPos -= stepSize;
    if (servoYPos < 0) {
      servoYPos = 0;
    }
    servoY.write(servoYPos);
    delay(stepDelay);
  }

  // Read the button state (LOW when pressed due to pull-up resistor)
  int button_state = digitalRead(button_pin);

  // Control the relay based on button state (active LOW)
  if (button_state == LOW) {
    // Button pressed, activate relay (LOW for active LOW relay)
    digitalWrite(relay_pin, LOW);
  } else {
    // Button not pressed, deactivate relay (HIGH for active LOW relay)
    digitalWrite(relay_pin, HIGH);
  }

  // Print the joystick values and servo positions to the Serial Monitor
  Serial.print("X_value: ");
  Serial.print(X_value);
  Serial.print(" | ServoXPos: ");
  Serial.print(servoXPos);
  Serial.print(" | Y_value: ");
  Serial.print(Y_value);
  Serial.print(" | ServoYPos: ");
  Serial.print(servoYPos);
  Serial.print(" | Button: ");
  Serial.println(button_state == LOW ? "Pressed" : "Not Pressed");

  // Short delay to prevent excessive serial output
  delay(20);
}
