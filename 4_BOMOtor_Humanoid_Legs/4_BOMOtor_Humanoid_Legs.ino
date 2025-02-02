// Define motor control pins (L298N motor driver)
int ENA = 9;    // Enable pin for left motor (PWM pin)
int ENB = 10;   // Enable pin for right motor (PWM pin)
int IN1 = 5;    // Input pin 1 for left motor
int IN2 = 6;    // Input pin 2 for left motor
int IN3 = 7;    // Input pin 1 for right motor
int IN4 = 8;    // Input pin 2 for right motor

// Define button pins for speed control
int buttonLow = 2;    // Button to set speed to Low
int buttonMedium = 3; // Button to set speed to Medium
int buttonHigh = 4;   // Button to set speed to High

// Define motor speeds
int motorSpeed = 0;  // Initial speed (Low)

// Speed settings (Low, Medium, High)
int speeds[3] = {80, 100,80 }; // Low = 75, Medium = 100, High = 125

// Button debounce variables
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // debounce time in milliseconds

// Button states
int lastButtonStateLow = LOW;
int lastButtonStateMedium = LOW;
int lastButtonStateHigh = LOW;
int buttonStateLow = LOW;
int buttonStateMedium = LOW;
int buttonStateHigh = LOW;

void setup() {
  // Set motor control pins as output
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Set button pins as input
  pinMode(buttonLow, INPUT_PULLUP);
  pinMode(buttonMedium, INPUT_PULLUP);
  pinMode(buttonHigh, INPUT_PULLUP);
  
  // Initialize motors turned off
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  
  // Set initial direction (forward for both motors)
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  
  // Start serial communication
  Serial.begin(9600);
  Serial.println("Motor Speed Control Initialized.");
}

void loop() {
  // Read the button states
  buttonStateLow = digitalRead(buttonLow);
  buttonStateMedium = digitalRead(buttonMedium);
  buttonStateHigh = digitalRead(buttonHigh);
  
  // Handle Button 1 (Low speed)
  if (buttonStateLow != lastButtonStateLow) {
    lastDebounceTime = millis(); // reset debounce timer
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (buttonStateLow == LOW) {
      // Set speed to Low
      motorSpeed = 0;
      analogWrite(ENA, speeds[motorSpeed]);
      analogWrite(ENB, speeds[motorSpeed]);
      // Print the speed change
      Serial.println("Button 1 Pressed: Speed set to Low.");
    }
  }

  // Handle Button 2 (Medium speed)
  if (buttonStateMedium != lastButtonStateMedium) {
    lastDebounceTime = millis(); // reset debounce timer
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (buttonStateMedium == LOW) {
      // Set speed to Medium
      motorSpeed = 1;
      analogWrite(ENA, speeds[motorSpeed]);
      analogWrite(ENB, speeds[motorSpeed]);
      // Print the speed change
      Serial.println("Button 2 Pressed: Speed set to Medium.");
    }
  }

  // Handle Button 3 (High speed, only one motor runs)
  if (buttonStateHigh != lastButtonStateHigh) {
    lastDebounceTime = millis(); // reset debounce timer
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (buttonStateHigh == LOW) {
      // Set one motor on High speed, turn off the other motor
      motorSpeed = 2;
      analogWrite(ENA, speeds[motorSpeed]);  // Left motor runs
      analogWrite(ENB, 0);                  // Right motor off
      // Print the speed change
      Serial.println("Button 3 Pressed: Left motor set to High, Right motor turned off.");
    }
  }

  // Save the last button states
  lastButtonStateLow = buttonStateLow;
  lastButtonStateMedium = buttonStateMedium;
  lastButtonStateHigh = buttonStateHigh;
  if ((millis() - lastDebounceTime) > 3000 && (buttonStateHigh || buttonStateMedium || buttonStateLow)) {
    analogWrite(ENA, 0);
    analogWrite(ENB, 0);
    Serial.println("Motors turned off due to inactivity.");
  }
}
