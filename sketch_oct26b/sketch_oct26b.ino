#include <BluetoothSerial.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <MPU6050.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Initialize Bluetooth Serial
BluetoothSerial SerialBT;

// Initialize MPU6050
MPU6050 mpu;
float initialYaw = 0.0;
float desiredYaw = 0.0;
bool rotateToDesiredYaw = false;

// Servo motor
Servo servo;
const int servoPin = 14; // Adjust based on your wiring

// Actuator control pins (each actuator has two pins)
const int actuator100mmExtendPin = 32; // Extend pin for 100mm actuator
const int actuator100mmRetractPin = 33; // Retract pin for 100mm actuator

const int actuator500mmExtendPin = 25; // Extend pin for 500mm actuator
const int actuator500mmRetractPin = 26; // Retract pin for 500mm actuator

// L298N Motor Driver Pins for Bot Movement
// Motor A
const int motorAInput1Pin = 14; // IN1
const int motorAInput2Pin = 12; // IN2
// Motor B
const int motorBInput1Pin = 13; // IN3
const int motorBInput2Pin = 15; // IN4

// Variables to store desired positions
float desiredActuator100mmPosition = 0.0;
float desiredActuator500mmPosition = 0.0;
int desiredServoAngle = 0.0;

// Variables to store current positions (simulated)
float currentActuator100mmPosition = 0.0;
float currentActuator500mmPosition = 0.0;

// Actuator speeds (mm per second)
const float actuator100mmSpeed = 50.0; // 50mm/s
const float actuator500mmSpeed = 15.0; // 15mm/s

// Timing variables for actuators
unsigned long lastActuatorUpdateTime = 0;
const unsigned long actuatorUpdateInterval = 50; // Update every 50ms

// Timing variables for sending yaw value
unsigned long lastYawSendTime = 0;
const unsigned long yawSendInterval = 500; // Send yaw every 500ms

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  SerialBT.begin("ESP32_Bot"); // Bluetooth device name
  Serial.println("The device started, now you can pair it with Bluetooth!");

  // Initialize Servo
  servo.attach(servoPin);

  // Initialize actuator control pins
  pinMode(actuator100mmExtendPin, OUTPUT);
  pinMode(actuator100mmRetractPin, OUTPUT);
  pinMode(actuator500mmExtendPin, OUTPUT);
  pinMode(actuator500mmRetractPin, OUTPUT);

  // Turn off actuator controls
  stopActuatorMovement();

  // Initialize MPU6050
  Wire.begin();
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connection successful");
  } else {
    Serial.println("MPU6050 connection failed");
  }

  // Initialize motor control pins for bot movement
  pinMode(motorAInput1Pin, OUTPUT);
  pinMode(motorAInput2Pin, OUTPUT);
  pinMode(motorBInput1Pin, OUTPUT);
  pinMode(motorBInput2Pin, OUTPUT);

  // Stop all motors
  stopAllMotors();
}

void loop() {
  // Handle incoming Bluetooth data
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil(';');
    Serial.println("Received command: " + command);
    parseCommand(command);
  }

  // Maintain actuator positions
  maintainActuatorPositions();

  // Maintain servo position
  maintainServoPosition();

  // Maintain bot orientation
  maintainOrientation();

  // Send yaw value periodically
  unsigned long currentTime = millis();
  if (currentTime - lastYawSendTime >= yawSendInterval) {
    lastYawSendTime = currentTime;
    sendYawValue();
  }
}

void parseCommand(String command) {
  if (command.startsWith("100MM:")) {
    String valueStr = command.substring(6);
    float value = valueStr.toFloat();
    desiredActuator100mmPosition = constrain(value, 0.0, 100.0);
    Serial.println("Desired 100MM Actuator Position: " + String(desiredActuator100mmPosition));
  } else if (command.startsWith("500MM:")) {
    String valueStr = command.substring(6);
    float value = valueStr.toInt();
    desiredActuator500mmPosition = constrain(value, 0.0, 500.0);
    Serial.println("Desired 500MM Actuator Position: " + String(desiredActuator500mmPosition));
  } else if (command.startsWith("SERVO:")) {
    String valueStr = command.substring(6);
    float angle = valueStr.toFloat();
    desiredServoAngle = constrain(angle, 0.0, 180.0);
    Serial.println("Desired Servo Angle: " + String(desiredServoAngle));
  } else if (command == "CALIBRATE") {
    calibrateMPU6050();
  } else if (command == "SET_YAW") {
    setDesiredYaw();
  } else if (command == "ROTATE_YAW") {
    rotateToDesiredYaw = true;
  } else {
    Serial.println("Unknown command");
  }
}

void maintainActuatorPositions() {
  unsigned long currentTime = millis();
  if (currentTime - lastActuatorUpdateTime >= actuatorUpdateInterval) {
    lastActuatorUpdateTime = currentTime;

    // Control 100MM Actuator
    if (currentActuator100mmPosition != desiredActuator100mmPosition) {
      controlActuator100mm();
    }

    // Control 500MM Actuator
    if (currentActuator500mmPosition != desiredActuator500mmPosition) {
      controlActuator500mm();
    }

    // If both actuators have reached their positions, stop movement
    if (currentActuator100mmPosition == desiredActuator100mmPosition && currentActuator500mmPosition == desiredActuator500mmPosition) {
      stopActuatorMovement();
    }
  }
}

void controlActuator100mm() {
  // Calculate the movement increment based on speed and interval
  float increment = (actuator100mmSpeed * (actuatorUpdateInterval / 1000.0)); // mm per update

  if (currentActuator100mmPosition < desiredActuator100mmPosition) {
    // Extend actuator
    digitalWrite(actuator100mmExtendPin, HIGH);
    digitalWrite(actuator100mmRetractPin, LOW);

    currentActuator100mmPosition += increment;
    if (currentActuator100mmPosition > desiredActuator100mmPosition) {
      currentActuator100mmPosition = desiredActuator100mmPosition;
      stopActuator100mm();
    }
  } else if (currentActuator100mmPosition > desiredActuator100mmPosition) {
    // Retract actuator
    digitalWrite(actuator100mmExtendPin, LOW);
    digitalWrite(actuator100mmRetractPin, HIGH);

    currentActuator100mmPosition -= increment;
    if (currentActuator100mmPosition < desiredActuator100mmPosition) {
      currentActuator100mmPosition = desiredActuator100mmPosition;
      stopActuator100mm();
    }
  } else {
    // Stop actuator
    stopActuator100mm();
  }

  // Ensure current position stays within bounds
  currentActuator100mmPosition = constrain(currentActuator100mmPosition, 0.0, 100.0);
}

void controlActuator500mm() {
  // Calculate the movement increment based on speed and interval
  float increment = (actuator500mmSpeed * (actuatorUpdateInterval / 1000.0)); // mm per update

  if (currentActuator500mmPosition < desiredActuator500mmPosition) {
    // Extend actuator
    digitalWrite(actuator500mmExtendPin, HIGH);
    digitalWrite(actuator500mmRetractPin, LOW);

    currentActuator500mmPosition += increment;
    if (currentActuator500mmPosition > desiredActuator500mmPosition) {
      currentActuator500mmPosition = desiredActuator500mmPosition;
      stopActuator500mm();
    }
  } else if (currentActuator500mmPosition > desiredActuator500mmPosition) {
    // Retract actuator
    digitalWrite(actuator500mmExtendPin, LOW);
    digitalWrite(actuator500mmRetractPin, HIGH);

    currentActuator500mmPosition -= increment;
    if (currentActuator500mmPosition < desiredActuator500mmPosition) {
      currentActuator500mmPosition = desiredActuator500mmPosition;
      stopActuator500mm();
    }
  } else {
    // Stop actuator
    stopActuator500mm();
  }

  // Ensure current position stays within bounds
  currentActuator500mmPosition = constrain(currentActuator500mmPosition, 0.0, 500.0);
}

void stopActuator100mm() {
  digitalWrite(actuator100mmExtendPin, LOW);
  digitalWrite(actuator100mmRetractPin, LOW);
}

void stopActuator500mm() {
  digitalWrite(actuator500mmExtendPin, LOW);
  digitalWrite(actuator500mmRetractPin, LOW);
}

void stopActuatorMovement() {
  stopActuator100mm();
  stopActuator500mm();
}

void maintainServoPosition() {
  // Continuously set the servo to the desired angle
  servo.write(desiredServoAngle);
}

void calibrateMPU6050() {
  // Read initial yaw angle for calibration
  initialYaw = getYaw();
  desiredYaw = initialYaw;
  Serial.println("MPU6050 calibrated. Initial Yaw: " + String(initialYaw));
}

void setDesiredYaw() {
  // Set desired yaw to current yaw
  desiredYaw = getYaw();
  Serial.println("Desired Yaw set to: " + String(desiredYaw));
}

void maintainOrientation() {
  // Read current yaw
  float currentYaw = getYaw();
  float yawDifference = currentYaw - desiredYaw;

  // Adjust bot orientation if necessary
  if (rotateToDesiredYaw && abs(yawDifference) > 1.0) { // Threshold of 1 degree
    controlMotors(yawDifference);
  } else {
    stopAllMotors();
    rotateToDesiredYaw = false;
  }
}

float getYaw() {
  int16_t gx, gy, gz;
  mpu.getRotation(&gx, &gy, &gz);
  // Convert gyroscope data to yaw angle
  float yaw = gz / 131.0; // For MPU6050, 131 LSB per °/s
  return yaw;
}

void controlMotors(float yawDifference) {
  // Implement motor control to adjust orientation
  // Since we don't have speed control, motors run at full speed

  if (yawDifference > 0) {
    // Rotate bot to the right
    Serial.println("Rotating right");
    moveBotRight();
  } else if (yawDifference < 0) {
    // Rotate bot to the left
    Serial.println("Rotating left");
    moveBotLeft();
  }
}

void moveBotLeft() {
  // Set motors to rotate bot to the left
  // Motor A forward, Motor B backward

  // Motor A
  digitalWrite(motorAInput1Pin, HIGH);
  digitalWrite(motorAInput2Pin, LOW);

  // Motor B
  digitalWrite(motorBInput1Pin, LOW);
  digitalWrite(motorBInput2Pin, HIGH);
}

void moveBotRight() {
  // Set motors to rotate bot to the right
  // Motor A backward, Motor B forward

  // Motor A
  digitalWrite(motorAInput1Pin, LOW);
  digitalWrite(motorAInput2Pin, HIGH);

  // Motor B
  digitalWrite(motorBInput1Pin, HIGH);
  digitalWrite(motorBInput2Pin, LOW);
}

void stopAllMotors() {
  // Stop all motors by setting both inputs to LOW
  // Motor A
  digitalWrite(motorAInput1Pin, LOW);
  digitalWrite(motorAInput2Pin, LOW);

  // Motor B
  digitalWrite(motorBInput1Pin, LOW);
  digitalWrite(motorBInput2Pin, LOW);
}

void sendYawValue() {
  // Read current yaw
  float currentYaw = getYaw();
  String yawMessage = "YAW:" + String(currentYaw) + "\n";
  SerialBT.print(yawMessage);
}
