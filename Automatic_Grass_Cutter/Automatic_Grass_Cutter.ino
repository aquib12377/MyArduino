// Autonomous Grass Cutter – obstacle‑aware steering with scanning servo
// Components:
//   * HC‑SR04 ultrasonic sensor mounted on SG90 pan servo (pin 9)
//   * Drive motors on M3 (left) & M4 (right)
//   * Cutting motor on M1 (always on)
//   * Adafruit Motor Shield (V1) on Arduino Uno
//
// Libraries required:
//   Adafruit Motor Shield Library (AFMotor)
//   Servo.h (built‑in)
//   NewPing by Tim Eckel
// -------------------------------------------------------------

#include <AFMotor.h>
#include <Servo.h>
#include <NewPing.h>

// ---------------- Pin assignments ---------------------------
const uint8_t SERVO_PIN = 9;   // SG90 pan servo for sensor
const uint8_t TRIG_PIN  = A5;  // HC‑SR04 Trig
const uint8_t ECHO_PIN  = A4;  // HC‑SR04 Echo

// ---------------- Ultrasonic object -------------------------
const uint16_t MAX_DISTANCE_CM = 200;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);

// ---------------- Motor objects -----------------------------
AF_DCMotor leftMotor (3, MOTOR12_64KHZ);   // M3
AF_DCMotor rightMotor(4, MOTOR12_64KHZ);   // M4
AF_DCMotor bladeMotor(1, MOTOR12_64KHZ);   // M1 (cutter)

Servo panServo;

// ---------------- Parameters --------------------------------
const uint8_t  OBSTACLE_THRESHOLD_CM = 25; // If closer than this, treat as obstacle
const uint8_t  DRIVE_SPEED  = 200;         // 0‑255
const uint8_t  TURN_SPEED   = 180;

// Timing (ms)
const uint16_t REVERSE_TIME = 700;

// Servo scan angles
const uint8_t CENTER_ANGLE = 90;
const uint8_t LEFT_ANGLE   = 150;
const uint8_t RIGHT_ANGLE  = 30;
const uint8_t STEP_DELAY   = 15;  // ms between incremental moves for smooth sweep

// -------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  panServo.attach(SERVO_PIN);
  panServo.write(CENTER_ANGLE);

  bladeMotor.setSpeed(255);
  bladeMotor.run(BACKWARD);      // start cutter

  delay(1500); // startup pause
}

// -------------------------------------------------------------
void loop() {
  uint16_t distAhead = readDistanceCM();
  Serial.print("Ahead: ");
  Serial.print(distAhead);
  Serial.println(" cm");

  if (distAhead < OBSTACLE_THRESHOLD_CM) {
    avoidObstacle();
  } else {
    moveForward();
  }

  delay(40); // keep loop responsive (~25 Hz)
}

// ---------------- Movement helpers --------------------------
void moveForward() {
  leftMotor.setSpeed(DRIVE_SPEED);
  rightMotor.setSpeed(DRIVE_SPEED);
  leftMotor.run(BACKWARD);
  rightMotor.run(BACKWARD);
}

void reverseRobot() {
  leftMotor.setSpeed(DRIVE_SPEED);
  rightMotor.setSpeed(DRIVE_SPEED);
  leftMotor.run(FORWARD);
  rightMotor.run(FORWARD);
}

void turnLeft() {
  leftMotor.setSpeed(TURN_SPEED);
  rightMotor.setSpeed(TURN_SPEED);
  leftMotor.run(FORWARD);
  rightMotor.run(BACKWARD);
}

void turnRight() {
  leftMotor.setSpeed(TURN_SPEED);
  rightMotor.setSpeed(TURN_SPEED);
  leftMotor.run(BACKWARD);
  rightMotor.run(FORWARD);
}

void stopDrive() {
  leftMotor.run(RELEASE);
  rightMotor.run(RELEASE);
}

// ------------- Obstacle avoidance with scanning -------------
void avoidObstacle() {
  // 1) Stop and reverse a bit
  stopDrive();
  delay(100);
  reverseRobot();
  delay(REVERSE_TIME);
  stopDrive();

  // 2) Scan left and right to find clearer path
  uint16_t distLeft  = sweepAndMeasure(LEFT_ANGLE, CENTER_ANGLE);
  uint16_t distRight = sweepAndMeasure(RIGHT_ANGLE, CENTER_ANGLE);

  Serial.print("Left clear: ");  Serial.print(distLeft);  Serial.println(" cm");
  Serial.print("Right clear: "); Serial.print(distRight); Serial.println(" cm");

  // 3) Decide turn direction
  if (distLeft > distRight && distLeft > OBSTACLE_THRESHOLD_CM) {
    turnLeft();
  } else if (distRight > OBSTACLE_THRESHOLD_CM) {
    turnRight();
  } else {
    // Both blocked; turn 180° (spin right twice)
    turnRight();
    delay(1200);
  }

  delay(600); // complete the turn
  stopDrive();
}

// Sweep servo to targetAngle, measure, then return to homeAngle; returns measured distance
uint16_t sweepAndMeasure(uint8_t targetAngle, uint8_t homeAngle) {
  // Sweep slowly to target
  if (targetAngle > homeAngle) {
    for (uint8_t a = homeAngle; a <= targetAngle; a++) {
      panServo.write(a);
      delay(STEP_DELAY);
    }
  } else {
    for (int a = homeAngle; a >= targetAngle; a--) {
      panServo.write(a);
      delay(STEP_DELAY);
    }
  }

  delay(40); // settle
  uint16_t dist = readDistanceCM();

  // Return to center smoothly
  if (targetAngle > homeAngle) {
    for (int a = targetAngle; a >= homeAngle; a--) {
      panServo.write(a);
      delay(STEP_DELAY);
    }
  } else {
    for (uint8_t a = targetAngle; a <= homeAngle; a++) {
      panServo.write(a);
      delay(STEP_DELAY);
    }
  }
  return dist == 0 ? MAX_DISTANCE_CM : dist;
}

// ---------------- Ultrasonic helper -------------------------
uint16_t readDistanceCM() {
  uint16_t d = sonar.ping_cm();
  if (d == 0) d = MAX_DISTANCE_CM;
  return d;
}
