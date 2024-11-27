#include <SoftwareSerial.h>

SoftwareSerial BT(2, 3); // RX, TX for Bluetooth communication

// Motor pin definitions
const int motor1Pin1 = 4;
const int motor1Pin2 = 5;
const int motor2Pin1 = 6;
const int motor2Pin2 = 7;
const int motor3Pin1 = 8;
const int motor3Pin2 = 9;
const int motor4Pin1 = 10;
const int motor4Pin2 = 11;

void setup() {
  // Set motor pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  pinMode(motor3Pin1, OUTPUT);
  pinMode(motor3Pin2, OUTPUT);
  pinMode(motor4Pin1, OUTPUT);
  pinMode(motor4Pin2, OUTPUT);
  
  // Start Bluetooth communication
  BT.begin(9600);
  Serial.begin(9600);
}

void loop() {
  if (BT.available()) {
    String command = BT.readStringUntil('\n'); // Read full command as string
    Serial.println(command); // For debugging
    
    // Control motors based on command
    if (command == "F") {
      motor1Forward();
      delay(240);
      stopAllMotors();
      delay(500);
      motor2Backward();
      delay(240);
      stopAllMotors();
      delay(500);
      motor2Forward();
      delay(240);
      stopAllMotors();
      delay(500);
      motor1Backward();
      delay(240);
      stopAllMotors();
    } else if (command == "B") {
      moveBackward();
      delay(240);
      stopAllMotors();
    } else if (command == "L") {
      turnLeft();
      delay(240);
      stopAllMotors();
    } else if (command == "R") {
      turnRight();
      delay(240);
      stopAllMotors();
    } else if (command == "S") {
      stopAllMotors();
    } else if (command == "1F") {
      motor1Forward();
      delay(240);
      stopAllMotors();
    } else if (command == "1B") {
      motor1Backward();
      delay(240);
      stopAllMotors();
    } else if (command == "2F") {
      motor2Forward();
      delay(240);
      stopAllMotors();
    } else if (command == "2B") {
      motor2Backward();
      delay(240);
      stopAllMotors();
    } else if (command == "3F") {
      motor3Forward();
      delay(240);
      stopAllMotors();
    } else if (command == "3B") {
      motor3Backward();
      delay(240);
      stopAllMotors();
    } else if (command == "4F") {
      motor4Forward();
      delay(240);
      stopAllMotors();
      delay(500);
      motor4Backward();
      delay(240);
      stopAllMotors();
      delay(500);
      motor4Forward();
      delay(240);
      stopAllMotors();
      delay(500);
    } else if (command == "4B") {
      motor4Backward();
      delay(240);
      stopAllMotors();
    } else {
      stopAllMotors(); // Default case to stop motors
    }
  }
}

// Functions for each motor's individual movement
void motor1Forward() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
}

void motor1Backward() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
}

void motor2Forward() {
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
}

void motor2Backward() {
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void motor3Forward() {
  digitalWrite(motor3Pin1, HIGH);
  digitalWrite(motor3Pin2, LOW);
}

void motor3Backward() {
  digitalWrite(motor3Pin1, LOW);
  digitalWrite(motor3Pin2, HIGH);
}

void motor4Forward() {
  digitalWrite(motor4Pin1, HIGH);
  digitalWrite(motor4Pin2, LOW);
}

void motor4Backward() {
  digitalWrite(motor4Pin1, LOW);
  digitalWrite(motor4Pin2, HIGH);
}

// General movement functions
void moveForward() {
  motor1Forward();
  motor2Forward();
  motor3Forward();
  motor4Forward();
}

void moveBackward() {
  motor1Backward();
  motor2Backward();
  motor3Backward();
  motor4Backward();
}

void turnLeft() {
  motor1Backward();
  motor2Forward();
  motor3Backward();
  motor4Forward();
}

void turnRight() {
  motor1Forward();
  motor2Backward();
  motor3Forward();
  motor4Backward();
}

void stopAllMotors() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, LOW);
  digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW);
  digitalWrite(motor4Pin2, LOW);
}
