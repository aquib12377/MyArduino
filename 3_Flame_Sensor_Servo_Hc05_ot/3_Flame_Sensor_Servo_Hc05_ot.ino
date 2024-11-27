#include <Servo.h>
#include <SoftwareSerial.h>

// Pins for Flame Sensors
#define FLAME_SENSOR_1 2
#define FLAME_SENSOR_2 3
#define FLAME_SENSOR_3 4

// Pump and Servo Pins
#define PUMP_PIN 13
#define SERVO_PIN 6

// Motor Pins
#define MOTOR_A1 A0
#define MOTOR_A2 A1
#define MOTOR_B1 A2
#define MOTOR_B2 A3

// Bluetooth Software Serial Pins
#define BT_TX 9
#define BT_RX 8
SoftwareSerial BTSerial(BT_RX, BT_TX);  // RX, TX

// Servo Object
Servo waterServo;

// Variables
bool autoMode = true;  // Default mode

void setup() {
  // Flame Sensor Inputs
  pinMode(FLAME_SENSOR_1, INPUT);
  pinMode(FLAME_SENSOR_2, INPUT);
  pinMode(FLAME_SENSOR_3, INPUT);

  // Pump Output
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH);  // Pump OFF

  // Motor Outputs
  pinMode(MOTOR_A1, OUTPUT);
  pinMode(MOTOR_A2, OUTPUT);
  pinMode(MOTOR_B1, OUTPUT);
  pinMode(MOTOR_B2, OUTPUT);

  // Servo Setup
  waterServo.attach(SERVO_PIN);

  // Bluetooth Setup
  BTSerial.begin(9600);  // HC-05 communication
  Serial.begin(9600);    // Serial Monitor
  Serial.println("Bot Ready! Send 'AUTO' or 'MANUAL' to select mode.");
  Serial.println("Manual Commands: F (Forward), B (Backward), L (Left), R (Right), S (Stop)");

  // Initial Setup
  stopMotors();
  waterServo.write(90);  // Center servo
}

void loop() {
  // Check Bluetooth Command
  if (BTSerial.available()) {
    char cmd = BTSerial.read();

    if (cmd == 'A') {  // Switch to Auto Mode
      autoMode = true;
      Serial.println("Switched to Auto Mode");
      BTSerial.println("Switched to Auto Mode");
    } else if (cmd == 'M') {  // Switch to Manual Mode
      autoMode = false;
      Serial.println("Switched to Manual Mode");
      BTSerial.println("Switched to Manual Mode");
    } else {
      // Handle Manual Mode Commands
      if (!autoMode) {
        handleManualCommands(cmd);
      }
    }
  }

  if (autoMode) {
    autoModeFunction();
  }
}

// Auto Mode Functionality
void autoModeFunction() {
  bool flame1 = digitalRead(FLAME_SENSOR_1) == LOW;
  bool flame2 = digitalRead(FLAME_SENSOR_2) == LOW;
  bool flame3 = digitalRead(FLAME_SENSOR_3) == LOW;

  if (flame1 || flame2 || flame3) {
    Serial.println("Flame Detected!");

    // Move towards the flame
    if (flame1) {
      moveForward();
    } else if (flame2) {
      turnLeft();
    } else if (flame3) {
      turnRight();
    }
    delay(500);
    stopMotors();
    // Start pump and rotate servo
    digitalWrite(PUMP_PIN, LOW);  // Pump ON
    for (int pos = 70; pos <= 110; pos += 5) {
      waterServo.write(pos);
      delay(5);
    }
    for (int pos = 110; pos >= 70; pos -= 5) {
      waterServo.write(pos);
      delay(5);
    }
  } else {
    Serial.println("No Flame Detected.");
    stopMotors();
    digitalWrite(PUMP_PIN, HIGH);  // Pump OFF
  }
}

// Manual Mode Commands
void handleManualCommands(char cmd) {
  switch (cmd) {
    case 'F':  // Forward
      moveForward();
      Serial.println("Moving Forward");
      break;
    case 'B':  // Backward
      moveBackward();
      Serial.println("Moving Backward");
      break;
    case 'L':  // Left
      turnLeft();
      Serial.println("Turning Left");
      break;
    case 'R':  // Right
      turnRight();
      Serial.println("Turning Right");
      break;
    case 'S':  // Stop
      stopMotors();
      Serial.println("Stopping");
      break;
    case 'P':  // Stop
      stopMotors();
      // Start pump and rotate servo
      digitalWrite(PUMP_PIN, LOW);  // Pump ON
      for (int pos = 70; pos <= 110; pos += 5) {
        waterServo.write(pos);
        delay(50);
      }
      for (int pos = 110; pos >= 70; pos -= 5) {
        waterServo.write(pos);
        delay(50);
      }
      digitalWrite(PUMP_PIN, HIGH);  // Pump ON
      Serial.println("Stopping");
      break;
    default:
      Serial.println("Invalid Command in Manual Mode. Use F, B, L, R, S.");
      break;
  }
}

// Motor Control Functions
void moveForward() {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
}

void moveBackward() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
}

void turnLeft() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
}

void turnRight() {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
}

void stopMotors() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, LOW);
}
