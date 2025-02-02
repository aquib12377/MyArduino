// Ultrasonic Sensor Pins
#define TRIG_PIN 2
#define ECHO_PIN 3

// Motor Pins
#define MOTOR1_IN1 A2
#define MOTOR1_IN2 A3
#define MOTOR2_IN1 A4
#define MOTOR2_IN2 A5

// Threshold distance in centimeters
#define DISTANCE_THRESHOLD 30

// Function to initialize pins
void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Motor driver pins
  pinMode(MOTOR1_IN1, OUTPUT);
  pinMode(MOTOR1_IN2, OUTPUT);
  pinMode(MOTOR2_IN1, OUTPUT);
  pinMode(MOTOR2_IN2, OUTPUT);

  // Stop motors initially
  stopMotors();

  Serial.println(F("Obstacle Avoider Robot Initialized."));
}

// Function to measure distance using the ultrasonic sensor
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo pin duration in microseconds
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Convert duration to distance in cm
  long distance = duration * 0.034 / 2;  // Speed of sound is ~340 m/s
  return distance;
}

// Function to move the robot forward
void moveForward() {
  digitalWrite(MOTOR1_IN1, HIGH);
  digitalWrite(MOTOR1_IN2, LOW);
  digitalWrite(MOTOR2_IN1, HIGH);
  digitalWrite(MOTOR2_IN2, LOW);
}

// Function to move the robot backward
void moveBackward() {
  digitalWrite(MOTOR1_IN1, LOW);
  digitalWrite(MOTOR1_IN2, HIGH);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, HIGH);
}

// Function to turn the robot left
void turnLeft() {
  digitalWrite(MOTOR1_IN1, LOW);
  digitalWrite(MOTOR1_IN2, HIGH);
  digitalWrite(MOTOR2_IN1, HIGH);
  digitalWrite(MOTOR2_IN2, LOW);
}

// Function to turn the robot right
void turnRight() {
  digitalWrite(MOTOR1_IN1, HIGH);
  digitalWrite(MOTOR1_IN2, LOW);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, HIGH);
}

// Function to stop the motors
void stopMotors() {
  digitalWrite(MOTOR1_IN1, LOW);
  digitalWrite(MOTOR1_IN2, LOW);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, LOW);
}

// Main loop
void loop() {
  long distance = getDistance();
  Serial.print(F("Distance: "));
  Serial.print(distance);
  Serial.println(F(" cm"));

  if (distance > DISTANCE_THRESHOLD || distance == 0) {
    // If no obstacle detected, move forward
    moveForward();
  } else {
    // If obstacle detected, stop and decide direction
    stopMotors();
    delay(500);

    // Check left distance
    turnLeft();
    delay(300);
    stopMotors();
    delay(500);
    long leftDistance = getDistance();

    // Check right distance
    turnRight();
    delay(600);  // More delay for turning right from the left position
    stopMotors();
    delay(500);
    long rightDistance = getDistance();

    // Return to forward position
    turnLeft();
    delay(300);
    stopMotors();
    delay(500);

    // Decide direction based on distances
    if (leftDistance > rightDistance) {
      turnLeft();
      delay(500);
    } else {
      turnRight();
      delay(500);
    }
  }
  delay(100);
}
