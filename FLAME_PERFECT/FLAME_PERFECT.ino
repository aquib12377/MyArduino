#include <Servo.h>

// Connect the flame modules' DIGITAL outputs (DO), not AO.
// Uno A0/A1/A2 support digitalRead().
const byte RIGHT_SENSOR = A0;
const byte CENTER_SENSOR = A1;
const byte LEFT_SENSOR = A2;
const byte SERVO_PIN = 3;
const byte RELAY_PIN = 6;

// Assumed L298N wiring. Leave ENA and ENB jumpers fitted.
// OUT1/OUT2: left motor; OUT3/OUT4: right motor.
const byte IN1 = 11;
const byte IN2 = 10;
const byte IN3 = 9;
const byte IN4 = 8;

// Software PWM on IN1 and IN3; ENA/ENB jumpers stay fitted.
// 0 = stopped, 255 = full power. Increase if motors struggle to start.
const byte MOTOR_SPEED = 9O8J0;
const unsigned long MOTOR_PWM_PERIOD_US = 2000;  // 500 Hz
const unsigned long MOTOR_ON_US =
  (MOTOR_PWM_PERIOD_US * MOTOR_SPEED) / 255UL;

const bool RELAY_ACTIVE_LOW = false;  // Active-HIGH relay: HIGH=ON, LOW=OFF.
// Set true for stationary diagnostics if needed.
const bool CALIBRATION_MODE = false;
// Change to false if your module's DO is HIGH when flame is detected.
const bool FLAME_ACTIVE_LOW = true;

const int SERVO_MIN = 60;
const int SERVO_MAX = 120;
const int SERVO_HOME = 90;
const unsigned long SENSOR_INTERVAL = 20;
const unsigned long SERVO_INTERVAL = 20;
const unsigned long PRINT_INTERVAL = 250;

Servo nozzle;
bool rightFlame = false;
bool centerFlame = false;
bool leftFlame = false;
bool spraying = false;
int rightValue = HIGH, centerValue = HIGH, leftValue = HIGH;
int servoAngle = SERVO_HOME;
int servoStep = 2;
unsigned long lastSensor = 0, lastServo = 0, lastPrint = 0;
unsigned long pwmStarted = 0;
bool leftMotorEnabled = false;
bool rightMotorEnabled = false;

void pump(bool on) {
  digitalWrite(RELAY_PIN, (on == RELAY_ACTIVE_LOW) ? LOW : HIGH);
}

void stopCar() {
  leftMotorEnabled = false;
  rightMotorEnabled = false;
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  leftMotorEnabled = true;
  rightMotorEnabled = true;
}

void turnRight() {
  // Left wheel forward, right wheel stopped.
  leftMotorEnabled = true;
  rightMotorEnabled = false;
}

void turnLeft() {
  // Right wheel forward, left wheel stopped.
  leftMotorEnabled = false;
  rightMotorEnabled = true;
}

void updateMotors() {
  unsigned long nowUs = micros();
  unsigned long elapsed = nowUs - pwmStarted;
  if (elapsed >= MOTOR_PWM_PERIOD_US) {
    pwmStarted = nowUs;
    elapsed = 0;
  }

  bool pulseOn = (MOTOR_SPEED == 255) || (elapsed < MOTOR_ON_US);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(IN1, (leftMotorEnabled && pulseOn) ? HIGH : LOW);
  digitalWrite(IN3, (rightMotorEnabled && pulseOn) ? HIGH : LOW);
}

void setup() {
  // Preload OFF level before making relay pin an output.
  pump(false);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(RIGHT_SENSOR, INPUT_PULLUP);
  pinMode(CENTER_SENSOR, INPUT_PULLUP);
  pinMode(LEFT_SENSOR, INPUT_PULLUP);
  stopCar();

  nozzle.write(SERVO_HOME);
  nozzle.attach(SERVO_PIN);
  Serial.begin(9600);
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensor >= SENSOR_INTERVAL) {
    lastSensor = now;
    rightValue = digitalRead(RIGHT_SENSOR);
    centerValue = digitalRead(CENTER_SENSOR);
    leftValue = digitalRead(LEFT_SENSOR);
    int flameLevel = FLAME_ACTIVE_LOW ? LOW : HIGH;
    rightFlame = (rightValue == flameLevel);
    centerFlame = (centerValue == flameLevel);
    leftFlame = (leftValue == flameLevel);

    // Centre has priority. If both sides detect, move forward to align.
    if (CALIBRATION_MODE) {
      stopCar();
      pump(false);
      spraying = false;
    } else if (centerFlame) {
      stopCar();
      pump(true);
      if (!spraying) {
        spraying = true;
        lastServo = now;
        servoAngle = SERVO_HOME;
        servoStep = 2;
        nozzle.write(servoAngle);
      }
    } else {
      pump(false);
      if (spraying) {
        spraying = false;
        servoAngle = SERVO_HOME;
        nozzle.write(servoAngle);
      }

      if (rightFlame && leftFlame) {
        moveForward();
      } else if (rightFlame) {
        turnRight();
      } else if (leftFlame) {
        turnLeft();
      } else {
        // No flame detected: continue moving forward to search.
        moveForward();
      }
    }
  }

  // Call every loop so motor PWM never blocks flame detection or servo.
  updateMotors();

  // Servo sweep advances one step per interval; no delay() or wait loop.
  if (spraying && now - lastServo >= SERVO_INTERVAL) {
    lastServo = now;
    servoAngle += servoStep;
    if (servoAngle >= SERVO_MAX) {
      servoAngle = SERVO_MAX;
      servoStep = -2;
    } else if (servoAngle <= SERVO_MIN) {
      servoAngle = SERVO_MIN;
      servoStep = 2;
    }
    nozzle.write(servoAngle);
  }

  // Only write a short line when the transmit buffer has enough room.
  // This avoids blocking the software motor PWM with long Serial prints.
  if (now - lastPrint >= PRINT_INTERVAL && Serial.availableForWrite() >= 48) {
    lastPrint = now;
    Serial.print("DO R/C/L: ");
    Serial.print(rightValue); Serial.print('/');
    Serial.print(centerValue); Serial.print('/');
    Serial.print(leftValue);
    Serial.print(" | Pump: "); Serial.println(spraying ? "ON" : "OFF");
  }
}
