/*
 * Arduino Nano Bot Control with Ultrasonic Obstacle Detection
 * Features:
 * - Controls motors via L298N Motor Driver
 * - Obstacle detection using Ultrasonic Sensor (HC-SR04)
 * - Autonomous obstacle avoidance: Turn Right, Move Forward, Turn Left, Move Forward
 */

#include <Arduino.h>

// =====================
// Pin Definitions
// =====================

// Motor Control Pins
#define MOTOR1_IN1 7
#define MOTOR1_IN2 8
#define MOTOR2_IN1 9
#define MOTOR2_IN2 10

// Ultrasonic Sensor Pins
#define TRIG_PIN 11
#define ECHO_PIN 12

// =====================
// Constants
// =====================
const long OBSTACLE_DISTANCE = 20; // Distance in cm to trigger obstacle alert

// =====================
// Function Prototypes
// =====================
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMovement();
long readUltrasonicDistance();
void setupMotors();
void obstacleAvoidance();

// =====================
// Setup Function
// =====================
void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
  Serial.println("Bot Control Initialized.");

  // Initialize Motor Control Pins
  setupMotors();

  // Initialize Ultrasonic Sensor Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Ensure motors are stopped at startup
  stopMovement();

  delay(1000); // Short delay to ensure all components are ready

  // Start moving forward
  moveForward();
}

// =====================
// Main Loop Function
// =====================
void loop() {
  // Obstacle Detection
  long distance = readUltrasonicDistance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  moveForward();
  if (distance > 0 && distance < OBSTACLE_DISTANCE) {
    // Obstacle detected
    Serial.println("Obstacle Detected! Initiating Avoidance Maneuvers.");
    obstacleAvoidance();
  }

  delay(100); // Short delay for stability
}

// =====================
// Function Definitions
// =====================

// Function to initialize motor control pins
void setupMotors() {
  pinMode(MOTOR1_IN1, OUTPUT);
  pinMode(MOTOR1_IN2, OUTPUT);
  pinMode(MOTOR2_IN1, OUTPUT);
  pinMode(MOTOR2_IN2, OUTPUT);

  // Initialize all motors to stop
  stopMovement();
}

// Function to move the bot forward
void moveForward() {
  digitalWrite(MOTOR1_IN1, HIGH);
  digitalWrite(MOTOR1_IN2, LOW);
  digitalWrite(MOTOR2_IN1, HIGH);
  digitalWrite(MOTOR2_IN2, LOW);
  Serial.println("Moving Forward.");
}

// Function to move the bot backward
void moveBackward() {
  digitalWrite(MOTOR1_IN1, LOW);
  digitalWrite(MOTOR1_IN2, HIGH);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, HIGH);
  Serial.println("Moving Backward.");
}

// Function to turn the bot left
void turnLeft() {
  digitalWrite(MOTOR1_IN1, LOW);
  digitalWrite(MOTOR1_IN2, HIGH);
  digitalWrite(MOTOR2_IN1, HIGH);
  digitalWrite(MOTOR2_IN2, LOW);
  Serial.println("Turning Left.");
}

// Function to turn the bot right
void turnRight() {
  digitalWrite(MOTOR1_IN1, HIGH);
  digitalWrite(MOTOR1_IN2, LOW);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, HIGH);
  Serial.println("Turning Right.");
}

// Function to stop all motors
void stopMovement() {
  digitalWrite(MOTOR1_IN1, LOW);
  digitalWrite(MOTOR1_IN2, LOW);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, LOW);
  Serial.println("Stopping.");
}

// Function to read distance from ultrasonic sensor
long readUltrasonicDistance() {
  // Clear the trigPin by setting it LOW:
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Trigger the sensor by setting the trigPin high for 10 microseconds:
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read the echoPin, return the sound wave travel time in microseconds:
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms (~5 meters)

  // Calculate the distance:
  // Speed of sound wave divided by 2 (go and return)
  long distance = duration * 0.034 / 2;

  if (duration == 0) {
    // No echo received
    return -1;
  }

  return distance;
}

// Function for obstacle avoidance: Turn Right, Move Forward, Turn Left, Move Forward
void obstacleAvoidance() {
  stopMovement();
  delay(1000); // Wait for 0.5 seconds

  // Turn Right
  turnRight();
  delay(1000); // Turn for 1 second

  // After avoidance maneuver, continue moving forward
}
