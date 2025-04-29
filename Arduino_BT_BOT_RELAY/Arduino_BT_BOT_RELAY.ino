// Relay pins
const int relay1 = 2; // Motor A Forward
const int relay2 = 3; // Motor A Backward
const int relay3 = 4; // Motor B Forward
const int relay4 = 5; // Motor B Backward

void setup() {
  Serial.begin(9600); // For HC-05

  // Set relay pins as output
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  // Turn off all relays initially (Active LOW)
  stopMotors();
}

void loop() {
  if (Serial.available()) {
    char command = Serial.read();
    Serial.println(command);
    switch (command) {
      case 'F': // Forward
        moveForward();
        break;
      case 'B': // Backward
        moveBackward();
        break;
        case 'L': // Forward
        turnLeft();
        break;
      case 'R': // Backward
        turnRight();
        break;
      
      case 'S': // Stop
        stopMotors();
        break;
    }
  }
}

void moveForward() {
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, HIGH);
}

void moveBackward() {
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, LOW);
}

void turnLeft() {
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, HIGH);
}

void turnRight() {
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, LOW);
}

void stopMotors() {
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
}
