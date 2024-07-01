#include <Servo.h>

// Create servo objects
Servo servo1;
Servo servo2;

// Define the pins for the servos
const int servo1Pin = 5;
const int servo2Pin = 6;

// Define the pins for the LDR sensors
const int LDR1Pin = A0;
const int LDR2Pin = A1;
const int LDR3Pin = A2;
const int LDR4Pin = A3;

// Define the delay between each step in milliseconds
const int stepDelay = 5;

// Initialize servo positions
int servo1Pos = 0;
int servo2Pos = 0;

void setup() {
  Serial.begin(9600);
  // Attach the servos to their respective pins
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);

  // Initialize LDR sensor pins as input
  pinMode(LDR1Pin, INPUT_PULLUP);
  pinMode(LDR2Pin, INPUT_PULLUP);
  pinMode(LDR3Pin, INPUT_PULLUP);
  pinMode(LDR4Pin, INPUT_PULLUP);

  // Set servos to initial positions
  servo1.write(servo1Pos);
  servo2.write(servo2Pos);
}

void loop() {
  // Read the state of each LDR sensor
  int LDR1State = digitalRead(LDR1Pin);
  int LDR2State = digitalRead(LDR2Pin);
  int LDR3State = digitalRead(LDR3Pin);
  int LDR4State = digitalRead(LDR4Pin);
  Serial.println(LDR1State);
  Serial.println(LDR2State);
  Serial.println(LDR3State);
  Serial.println(LDR4State);
  // Determine the direction based on the LDR sensor values
  if (LDR1State == LOW) {
    
    // Move servo1 to the left
    if (servo1Pos > 0) {
      servo1Pos--;
      servo1.write(servo1Pos);
      delay(stepDelay);
    }
  } else if (LDR2State == LOW) {
    // Move servo1 to the right
    if (servo1Pos < 180) {
      servo1Pos++;
      servo1.write(servo1Pos);
      delay(stepDelay);
    }
  }

  else if (LDR3State == LOW) {
    // Move servo2 up
    if (servo2Pos > 0) {
      servo2Pos--;
      if(servo2Pos > 0)
        servo2.write(servo2Pos);
      delay(stepDelay);
    }
  } else if (LDR4State == LOW) {
    // Move servo2 down
    if (servo2Pos < 90) {
      servo2Pos++;
      if(servo2Pos < 90)
        servo2.write(servo2Pos);
      delay(stepDelay);
    }
  }

  // Print the positions to the serial monitor for debugging
  Serial.print("Servo1 Position: ");
  Serial.print(servo1.read());
  Serial.print(" | Servo2 Position: ");
  Serial.println(servo2.read());

  // Small delay to avoid rapid looping
  delay(5);
}
