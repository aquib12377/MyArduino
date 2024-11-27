#include <FastLED.h>

// Motor control pins
#define IN1_PIN 4    // Control pin 1 for motor direction
#define IN2_PIN 5    // Control pin 2 for motor direction
#define ENA_PIN 9    // PWM pin for motor speed control

#define IR_PIN 2     // IR sensor pin

int lastVal = 0;         // Store the last state of the IR sensor
unsigned long lastChangeTime = 0;  // Track the last time the IR signal changed
const unsigned long timeout = 2000;  // 2 seconds timeout

int currentSpeed = 0;  // Current motor speed (PWM value)
int rampStep = 1;      // Step size for increasing speed
int maxSpeed = 255;    // Maximum PWM value for motor speed (0-255)
int count = 0;         // Count signal changes

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
  Serial.println("Started Project");
}

void loop() {
  int currentVal = digitalRead(IR_PIN);  // Read the current state of the IR sensor
  unsigned long currentTime = millis();  // Track current time for timeout

  if (currentVal != lastVal) {  // If the signal has changed
    Serial.println("Signal Change Detected: " + String(++count));
    
    lastVal = currentVal;  // Update the last signal state
    lastChangeTime = currentTime;  // Update the last change time

    // Increase the motor speed on signal change
    if (currentSpeed < maxSpeed) {
      currentSpeed += rampStep;  // Increase speed by the ramp step
      currentSpeed = constrain(currentSpeed, 0, maxSpeed);  // Constrain speed to max value
    }

    Serial.print("Current Motor Speed (PWM): ");
    Serial.println(currentSpeed);
  }

  // Apply the current motor speed
  analogWrite(ENA_PIN, currentSpeed);

   if (currentTime - lastChangeTime >= timeout && currentSpeed > 0) {
    Serial.println("No signal change for timeout period.");
    for(int i = currentSpeed; i > 0; i--)
    {
      analogWrite(ENA_PIN, i);
      //Serial.println("Decreasing Speed: "+String(i));
      delay(1);
    }
    Serial.println("End");
    currentSpeed = 0;
  }
  if (currentTime - lastChangeTime >= 200 && currentSpeed > 0) {
    Serial.println("No signal change for timeout period.");
    for(int i = currentSpeed; i > 0; i--)
    {
      analogWrite(ENA_PIN, i);
      //Serial.println("Decreasing Speed: "+String(i));
      delay(1);
    }
    Serial.println("End");
    currentSpeed = 0;
  }
}
