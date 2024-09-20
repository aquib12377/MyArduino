// IR sensor pins
const int leftIRPin = 6;
const int rightIRPin = 7;

// Ultrasonic sensor pins
const int trigPin = A1;
const int echoPin = A0;

// Motor control pins
const int motorLeftForward = 2;
const int motorLeftBackward = 3;
const int motorRightForward = 4;
const int motorRightBackward = 5;

// Enable pins (for PWM speed control)
const int motorLeftEnable = 10;
const int motorRightEnable = 11;

// Distance threshold to stop the robot (in cm)
const int distanceThreshold = 15;

void setup() {
  // Set up IR sensor pins as inputs
  pinMode(leftIRPin, INPUT_PULLUP);
  pinMode(rightIRPin, INPUT_PULLUP);

  // Set up ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Set up motor control pins as outputs
  pinMode(motorLeftForward, OUTPUT);
  pinMode(motorLeftBackward, OUTPUT);
  pinMode(motorRightForward, OUTPUT);
  pinMode(motorRightBackward, OUTPUT);

  // Set up motor enable pins for PWM control
  pinMode(motorLeftEnable, OUTPUT);
  pinMode(motorRightEnable, OUTPUT);

  // Start motors with a medium speed
  analogWrite(motorLeftEnable, 60);
  analogWrite(motorRightEnable, 60);

  Serial.begin(9600);  // Start serial communication for debugging
  Serial.println("Line Follower with Obstacle Detection Initialized");
}

void loop() {
  // Read the IR sensors
  bool leftIR = digitalRead(leftIRPin) == LOW;  // Detecting black if LOW
  bool rightIR = digitalRead(rightIRPin) == LOW;  // Detecting black if LOW

  // Read the ultrasonic sensor
  int distance = getDistance();

  // Debugging - Print sensor states and distance


  // Stop if the obstacle is too close
  if (distance < distanceThreshold) {
    stopMotors();
    Serial.println("Obstacle detected! Stopping.");
    delay(500);  // Small delay to prevent overwhelming prints
    return;
  }

  // Line following logic
  if (leftIR && rightIR) {
    // Both sensors detect black line, move forward
    moveForward();
    Serial.println("Moving Forward");
  } else if (leftIR) {
    // Left sensor detects black, turn left
    turnLeft();
    Serial.println("Turning Left");
  } else if (rightIR) {
    // Right sensor detects black, turn right
    turnRight();
    Serial.println("Turning Right");
  } else {
    // No line detected, stop
    stopMotors();
    Serial.println("Line lost, stopping.");
  }

  delay(100);  // Small delay to make the Serial monitor easier to read
}

// Function to move the robot forward
void moveForward() {
  digitalWrite(motorLeftForward, HIGH);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, HIGH);
  digitalWrite(motorRightBackward, LOW);
}

// Function to turn left
void turnLeft() {
  digitalWrite(motorLeftForward, LOW);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, HIGH);
  digitalWrite(motorRightBackward, LOW);
}

// Function to turn right
void turnRight() {
  digitalWrite(motorLeftForward, HIGH);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, LOW);
  digitalWrite(motorRightBackward, LOW);
}

// Function to stop the motors
void stopMotors() {
  digitalWrite(motorLeftForward, LOW);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, LOW);
  digitalWrite(motorRightBackward, LOW);
}

// Function to get distance from ultrasonic sensor
int getDistance() {
  long duration, distance;
  
  // Send a 10us pulse to trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin
  duration = pulseIn(echoPin, HIGH);

  // Calculate distance in cm
  distance = (duration / 2) / 29.1;

  return distance;
}
