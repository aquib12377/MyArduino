char command = 'S';

// Motor driver pins
const int IN1 = 2;
const int IN2 = 3;
const int IN3 = 4;
const int IN4 = 5;

void setup() {
  Serial.begin(9600); // HC-12 connected on pins 0/1

  // Set motor pins as outputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Stop motors initially
  stopMotors();

  Serial.println("Receiver ready. Waiting for command...");
}

void loop() {
  if (Serial.available()) {
    command = Serial.read();

    switch (command) {
      case 'F':
        Serial.println("Command: Forward");
        moveForward();
        break;
      case 'B':
        Serial.println("Command: Backward");
        moveBackward();
        break;
      case 'L':
        Serial.println("Command: Left");
        turnLeft();
        break;
      case 'R':
        Serial.println("Command: Right");
        turnRight();
        break;
      case 'S':
        Serial.println("Command: Stop");
        stopMotors();
        break;
      default:
        // Ignore unknown commands
        break;
    }
  }
}

// ==== Motor Functions ====

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}
