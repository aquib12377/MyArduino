#include <Wire.h>
#include <MPU6050.h>
#include <BluetoothSerial.h>

// Bluetooth
BluetoothSerial SerialBT;
bool autoMode = false;  // Default to Auto Mode

// Motor Driver Pins (L298N)
#define ENA 14  //Speed Pins
#define IN1 27  //Motor Pin
#define IN2 26  //Motor Pin
#define ENB 12  //Speed Pins
#define IN3 25  //Motor Pin
#define IN4 33  //Motor Pin

// Ultrasonic Sensor Pins
#define TRIG_OBS 5   // Trigger pin for obstacle detection
#define ECHO_OBS 18  // Echo pin for obstacle detection
#define TRIG_POT 4   // Trigger pin for pothole detection
#define ECHO_POT 19  // Echo pin for pothole detection

// MPU6050 Accelerometer
MPU6050 mpu;
char command;
// Speed Levels
int baseSpeed = 150;  // Normal speed
int highSpeed = 200;  // Increased speed on forward slope
int lowSpeed = 100;   // Reduced speed on backward slope

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_BOT");  // Bluetooth Name
  Wire.begin();

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(TRIG_OBS, OUTPUT);
  pinMode(ECHO_OBS, INPUT);
  pinMode(TRIG_POT, OUTPUT);
  pinMode(ECHO_POT, INPUT);

  // Initialize MPU6050
  Serial.println("Initializing MPU6050...");
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (1)
      ;
  }
  Serial.println("MPU6050 Connected");

  Serial.println("ESP32 Bot Initialized");
}

void loop() {
  // Always read the sensors for logging or debug, regardless of auto or manual mode
  int obsDist = getDistance(TRIG_OBS, ECHO_OBS);
  int potholeDist = getDistance(TRIG_POT, ECHO_POT);
  float slope = getSlope();

  Serial.print("Mode: ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");

  Serial.print("Obstacle Distance: ");
  Serial.print(obsDist);
  Serial.print(" cm, Pothole Distance: ");
  Serial.print(potholeDist);
  Serial.print(" cm, Slope: ");
  Serial.print(slope);
  Serial.println("°");

  // Now either run auto or manual logic
  if (autoMode) {
    autoControl(obsDist, potholeDist, slope);
  } else {
    int speed = 0;
    if (slope > 5.0) {
    baseSpeed = highSpeed;
    Serial.println("Forward Slope Detected - Increasing Speed");
  } else if (slope < -5.0) {
    baseSpeed = lowSpeed;
    Serial.println("Backward Slope Detected - Reducing Speed");
  } else {
    baseSpeed = baseSpeed;
  }
    if (obsDist < 30) {
      Serial.println("Obstacle Detected - Stopping and Turning Right");
      stopBot();
      delay(500);
       moveBackward(baseSpeed);
      delay(1000);
      turnRight();
      delay(3000);
      stopBot();  // brief stop after turn
    } else if (potholeDist > 15) {
      Serial.println("Pothole Detected - Stopping and Turning Left");
      stopBot();
      delay(500);
      moveBackward(baseSpeed);
      delay(1000);
      turnLeft();
      delay(3000);
      stopBot();  // brief stop after turn
    } else {
      manualControl();
    }
  }

  // Some small delay to avoid too-frequent sensor prints
  delay(150);
}

void autoControl(int obsDist, int potholeDist, float slope) {
  int speed;
  if (SerialBT.available()) {
    command = (char)SerialBT.read();
    Serial.print("Bluetooth Command Received: ");
    Serial.println(command);

    if (command == 'M') {
      Serial.println("Switching to MANUAL Mode");
      autoMode = false;
    }
  }
  // Decide speed based on slope
  if (slope > 5.0) {
    speed = highSpeed;
    Serial.println("Forward Slope Detected - Increasing Speed");
  } else if (slope < -5.0) {
    speed = lowSpeed;
    Serial.println("Backward Slope Detected - Reducing Speed");
  } else {
    speed = baseSpeed;
  }

  // React to obstacles/potholes
  if (obsDist < 30) {
    Serial.println("Obstacle Detected - Stopping and Turning Right");
    stopBot();
    delay(500);
     moveBackward(baseSpeed);
      delay(1000);
    turnRight();
    delay(3000);
    stopBot();  // brief stop after turn
  } else if (potholeDist > 15) {
    Serial.println("Pothole Detected - Stopping and Turning Left");
    stopBot();
    delay(500);
    moveBackward(baseSpeed);
      delay(1000);
    turnLeft();
    delay(3000);
    stopBot();  // brief stop after turn
  } else {
    Serial.println("Path Clear - Moving Forward");
    moveForward(speed);
  }
}

void manualControl() {
  // In manual mode, we don't automatically react to slope or obstacles
  // (but we can still read them, as done in loop()).

  if (SerialBT.available()) {
    char command = (char)SerialBT.read();
    Serial.print("Bluetooth Command Received: ");
    Serial.println(command);

    switch (command) {
      case 'F':
        Serial.println("Moving Forward");
        moveForward(baseSpeed);
        break;
      case 'B':
        Serial.println("Moving Backward");
        moveBackward(baseSpeed);
        break;
      case 'L':
        Serial.println("Turning Left");
        turnLeft();
        break;
      case 'R':
        Serial.println("Turning Right");
        turnRight();
        break;
      case 'S':
        Serial.println("Stopping Bot");
        stopBot();
        break;
      case 'A':
        Serial.println("Switching to AUTO Mode");
        autoMode = true;
        break;
      case 'M':
        Serial.println("Switching to MANUAL Mode");
        autoMode = false;
        break;
      default:
        Serial.println("Invalid Command");
    }
  }
  else{
    switch (command) {
      case 'F':
        Serial.println("Moving Forward");
        moveForward(baseSpeed);
        break;
      case 'B':
        Serial.println("Moving Backward");
        moveBackward(baseSpeed);
        break;
      case 'L':
        Serial.println("Turning Left");
        turnLeft();
        break;
      case 'R':
        Serial.println("Turning Right");
        turnRight();
        break;
      case 'S':
        Serial.println("Stopping Bot");
        stopBot();
        break;
      case 'A':
        Serial.println("Switching to AUTO Mode");
        autoMode = true;
        break;
      case 'M':
        Serial.println("Switching to MANUAL Mode");
        autoMode = false;
        break;
      default:
        Serial.println("Invalid Command");
    }
  }
}

// Function to get distance from Ultrasonic Sensor
int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;  // Convert to cm

  // For debug, print which sensor is being measured
  Serial.print("Ultrasonic Sensor (");
  if (trigPin == TRIG_OBS) Serial.print("Obstacle");
  else Serial.print("Pothole");
  Serial.print(") Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

// Function to get slope from MPU6050
float getSlope() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float accelX = ax / 16384.0;  // Convert to g
  float accelY = ay / 16384.0;  // Convert to g
  float accelZ = az / 16384.0;  // Convert to g

  // For debug
  Serial.print("MPU6050 Data - X: ");
  Serial.print(accelX);
  Serial.print(" Y: ");
  Serial.print(accelY);
  Serial.print(" Z: ");
  Serial.print(accelZ);
  Serial.println(" g");

  // Calculate slope angle (e.g. around X or Y axis)
  float slopeAngle = atan2(accelY, sqrt(accelX * accelX + accelZ * accelZ)) * (180.0 / PI);

  Serial.print("Slope Angle: ");
  Serial.print(slopeAngle);
  Serial.println("°");

  return slopeAngle;
}

// --- Movement functions ---
void moveForward(int speed) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  Serial.println("Moving Forward");
}

void moveBackward(int speed) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  Serial.println("Moving Backward");
}

void turnLeft() {
  // Give some PWM speed for turning
  analogWrite(ENA, baseSpeed);
  analogWrite(ENB, baseSpeed);

  // Left wheel backward, right wheel forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  Serial.println("Turning Left");
}

void turnRight() {
  // Give some PWM speed for turning
  analogWrite(ENA, baseSpeed);
  analogWrite(ENB, baseSpeed);

  // Left wheel forward, right wheel backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  Serial.println("Turning Right");
}

void stopBot() {
  // Set enable pins to 0 or just stop direction pins
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  Serial.println("Bot Stopped");
}