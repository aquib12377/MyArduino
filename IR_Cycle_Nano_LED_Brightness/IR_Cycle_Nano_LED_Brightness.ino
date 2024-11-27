#include <FastLED.h>

// Motor control pins
#define IN1_PIN 4    // Control pin 1 for motor direction
#define IN2_PIN 5    // Control pin 2 for motor direction
#define ENA_PIN 9    // PWM pin for motor speed control

#define IR_PIN 3

int lastVal = 0;
unsigned long lastChangeTime = 0;  // Track the last time the IR signal changed
const unsigned long timeout = 5000;  // 15 seconds

int currentSpeed = 0;  // Current motor speed (PWM value)
int targetSpeed = 0;   // Desired motor speed based on IR frequency
int rampStep = 1;      // Step size for smooth speed changes

void setup() {
  pinMode(IR_PIN, INPUT_PULLUP);
  
  // Motor control pins
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  
  // Set motor direction (e.g., forward)
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  
  // Initialize motor speed to 0 (stopped)
  analogWrite(ENA_PIN, 0);
  
  Serial.begin(9600);
  lastChangeTime = millis();  // Initialize the last change time
}

void loop() {
  int currentVal = digitalRead(IR_PIN);
  unsigned long currentTime = millis();

  if (currentVal != lastVal) {  // IR signal changed
    lastVal = currentVal;
    unsigned long interval = currentTime - lastChangeTime;  // Time since last change
    lastChangeTime = currentTime;  // Update last change time

    // Calculate frequency: since each cycle has two changes, frequency = 1000 / (interval * 2)
    float frequency = 0;
    if (interval > 0) {
      frequency = 1000.0 / (interval * 2.0);  // in Hz
    }

    Serial.print("Interval: ");
    Serial.print(interval);
    Serial.print(" ms, Frequency: ");
    Serial.print(frequency);
    Serial.println(" Hz");

    // Map frequency to PWM value (Adjust these values based on your requirements)
    if (frequency > 0 && frequency <= 100) {  // Adjust 100 Hz as maximum expected frequency
      targetSpeed = map(frequency, 0, 100, 0, 255);
      targetSpeed = constrain(targetSpeed, 0, 255);  // Ensure it's within PWM range
    } else if (frequency > 100) {
      targetSpeed = 255;  // Maximum speed
    }

    Serial.print("Target Motor Speed (PWM): ");
    Serial.println(targetSpeed);
  }

  // Smoothly adjust motor speed towards the target speed
  if (currentSpeed < targetSpeed) {
    currentSpeed += rampStep;  // Increase speed
    if (currentSpeed > targetSpeed) {
      currentSpeed = targetSpeed;  // Don't exceed the target speed
    }
  } else if (currentSpeed > targetSpeed) {
    currentSpeed -= rampStep;  // Decrease speed
    if (currentSpeed < targetSpeed) {
      currentSpeed = targetSpeed;  // Don't go below the target speed
    }
  }

  analogWrite(ENA_PIN, currentSpeed);  // Apply the current speed
  delay(5);  // Adjust delay to control ramping speed

  // Check if the IR signal hasn't changed for the timeout duration
  if (currentTime - lastChangeTime >= timeout) {
    Serial.println("Resetting motor speed to 0 due to timeout.");
    targetSpeed = 0;  // Stop the motor
    lastChangeTime = currentTime;  // Reset the timer
  }
}
