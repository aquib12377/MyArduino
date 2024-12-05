#include <Servo.h>

// Define pins for ultrasonic sensor
const int trigPin = 9;
const int echoPin = 10;

// Define distance threshold in cm
const int distanceThreshold = 15;

// Servo object to control the dustbin lid
Servo dustbinServo;

// Variables for distance measurement
long duration;
int distance;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Initialize ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Attach the servo to pin 6
  dustbinServo.attach(3);

  // Start with the dustbin lid closed
  dustbinServo.write(0);
}

void loop() {
  // Trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Calculate distance based on echo duration
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2; // Convert to cm

  // Print distance for debugging
  Serial.print("Distance: ");
  Serial.println(distance);

  // Open the lid if an object is detected within the threshold
  if (distance > 0 && distance <= distanceThreshold) {
    dustbinServo.write(150); // Open lid
    delay(3000); // Keep it open for 3 seconds
  } else {
    dustbinServo.write(0); // Close lid
  }

  delay(100); // Short delay for stability
}
