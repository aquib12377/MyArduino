// Define pins for Motor 1
const int IN1_M1 = 2;  // Motor 1 input pin 1
const int IN2_M1 = 3;  // Motor 1 input pin 2

// Define pins for Motor 2
const int IN1_M2 = 4; // Motor 2 input pin 1
const int IN2_M2 = 5; // Motor 2 input pin 2

// Define pins for limit switches
const int limitSwitch1_M1 = A5; // Limit switch 1 for Motor 1
const int limitSwitch2_M1 = A4; // Limit switch 2 for Motor 1
const int limitSwitch1_M2 = A3; // Limit switch 1 for Motor 2
const int limitSwitch2_M2 = A2; // Limit switch 2 for Motor 2

// Variables to store the current direction of the motors
bool motorDirection_M1 = true; // true for one direction, false for the opposite direction (Motor 1)
bool motorDirection_M2 = true; // true for one direction, false for the opposite direction (Motor 2)

void setup() {
  // Start Serial communication for debugging
  Serial.begin(9600);

  // Set Motor 1 pins as outputs
  pinMode(IN1_M1, OUTPUT);
  pinMode(IN2_M1, OUTPUT);

  // Set Motor 2 pins as outputs
  pinMode(IN1_M2, OUTPUT);
  pinMode(IN2_M2, OUTPUT);

  // Set limit switch pins as inputs
  pinMode(limitSwitch1_M1, INPUT_PULLUP);
  pinMode(limitSwitch2_M1, INPUT_PULLUP);
  pinMode(limitSwitch1_M2, INPUT_PULLUP);
  pinMode(limitSwitch2_M2, INPUT_PULLUP);
}

void loop() {
  // Check if limit switch 1 for Motor 1 is pressed
  if (digitalRead(limitSwitch1_M1) == LOW) {
    motorDirection_M1 = false; // Change direction for Motor 1
    Serial.println("Motor 1: Limit Switch 1 pressed, changing direction to reverse.");
    delay(500); // Debounce delay
  }

  // Check if limit switch 2 for Motor 1 is pressed
  if (digitalRead(limitSwitch2_M1) == LOW) {
    motorDirection_M1 = true; // Change direction for Motor 1
    Serial.println("Motor 1: Limit Switch 2 pressed, changing direction to forward.");
    delay(500); // Debounce delay
  }

  // Control Motor 1 direction
  if (motorDirection_M1) {
    digitalWrite(IN1_M1, HIGH);
    digitalWrite(IN2_M1, LOW);
    Serial.println("Motor 1 running in forward direction.");
  } else {
    digitalWrite(IN1_M1, LOW);
    digitalWrite(IN2_M1, HIGH);
    Serial.println("Motor 1 running in reverse direction.");
  }

  // Check if limit switch 1 for Motor 2 is pressed
  if (digitalRead(limitSwitch1_M2) == LOW) {
    motorDirection_M2 = false; // Change direction for Motor 2
    Serial.println("Motor 2: Limit Switch 1 pressed, changing direction to reverse.");
    delay(500); // Debounce delay
  }

  // Check if limit switch 2 for Motor 2 is pressed
  if (digitalRead(limitSwitch2_M2) == LOW) {
    motorDirection_M2 = true; // Change direction for Motor 2
    Serial.println("Motor 2: Limit Switch 2 pressed, changing direction to forward.");
    delay(500); // Debounce delay
  }

  // Control Motor 2 direction
  if (motorDirection_M2) {
    digitalWrite(IN1_M2, HIGH);
    digitalWrite(IN2_M2, LOW);
    Serial.println("Motor 2 running in forward direction.");
  } else {
    digitalWrite(IN1_M2, LOW);
    digitalWrite(IN2_M2, HIGH);
    Serial.println("Motor 2 running in reverse direction.");
  }

  // Add a small delay to avoid flooding the serial monitor
  delay(100);
}
