/* 
  Arduino Code to Detect "Crack" Using Ultrasonic Sensor
  and Stop Two Motors + Sound a Buzzer.
*/

// Pin Definitions
#define trigPin 7
#define echoPin 6

#define motor1Pin1 3
#define motor1Pin2 5
#define motor2Pin1 9
#define motor2Pin2 10

#define buzzerPin 11

// Set the distance threshold (in centimeters). If the measured
// distance is greater than this, we consider that a crack has been detected.
const int crackThreshold = 4;

// Speed for motors (0-255 for analogWrite)
const int motorSpeed = 100;  // Adjust as needed

// Function to measure distance using the HC-SR04
long measureDistance() {
  // Clear the trigger
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Trigger the sensor
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo
  long duration = pulseIn(echoPin, HIGH);

  // Calculate distance in cm
  // Speed of sound in air = ~343 m/s = 29.1 microseconds per centimeter round trip
  long distance = duration * 0.0343 / 2;
  
  return distance;
}

void setup() {
  // Initialize serial (optional for debugging)
  Serial.begin(9600);

  // Ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Motor pins
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Buzzer pin
  pinMode(buzzerPin, OUTPUT);

  // Initially, turn off buzzer
  digitalWrite(buzzerPin, LOW);

  // Start motors forward
  // For forward direction on a typical H-bridge:
  // motorPin1: HIGH, motorPin2: LOW (just an example; depends on wiring)
  analogWrite(motor1Pin1, motorSpeed);
  analogWrite(motor1Pin2, 0);

  analogWrite(motor2Pin1, motorSpeed);
  analogWrite(motor2Pin2, 0);

  Serial.println("System Initialized...");
}

void loop() {
  long distance = measureDistance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Check if distance exceeds threshold
  if (distance > crackThreshold) {
    // Stop the motors
    analogWrite(motor1Pin1, 0);
    analogWrite(motor1Pin2, 0);
    analogWrite(motor2Pin1, 0);
    analogWrite(motor2Pin2, 0);

    // Turn on the buzzer
    digitalWrite(buzzerPin, HIGH);
    Serial.println("Crack detected! Stopping motors and sounding buzzer.");
  }
  else {
    // If distance is within normal range, keep motors running and buzzer off
    analogWrite(motor1Pin1, motorSpeed);
    analogWrite(motor1Pin2, 0);
    analogWrite(motor2Pin1, motorSpeed);
    analogWrite(motor2Pin2, 0);
    
    digitalWrite(buzzerPin, LOW);
  }

  //delay(200); // Small delay for stability
}
  