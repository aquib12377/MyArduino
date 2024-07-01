// Pin definitions
const int flameSensorLeft = 8;
const int flameSensorFront = 9;
const int flameSensorRight = 10;

const int motorLeftForward = 2;
const int motorLeftBackward = 3;
const int motorRightForward = 4;
const int motorRightBackward = 5;

const int relayPin = 6;

// Flame sensor threshold
const int flameThreshold = 500;

void setup() {
  // Initialize pins
  pinMode(flameSensorLeft, INPUT_PULLUP);
  pinMode(flameSensorFront, INPUT_PULLUP);
  pinMode(flameSensorRight, INPUT_PULLUP);
  
  pinMode(motorLeftForward, OUTPUT);
  pinMode(motorLeftBackward, OUTPUT);
  pinMode(motorRightForward, OUTPUT);
  pinMode(motorRightBackward, OUTPUT);
  
  pinMode(relayPin, OUTPUT);
  pinMode(7, OUTPUT);
  digitalWrite(relayPin, HIGH);
  digitalWrite(7, LOW);
  // Initialize Serial Monitor
  Serial.begin(9600);
}

void loop() {
  // Read flame sensor values
  int leftValue = digitalRead(flameSensorLeft);
  int frontValue = digitalRead(flameSensorFront);
  int rightValue = digitalRead(flameSensorRight);
  
  // Check if fire is detected
  if (leftValue == LOW) {
    Serial.println("Fire detected on the left!");
    digitalWrite(7, HIGH);
    moveLeft();
    delay(500);
    activateRelay();
  } else if (frontValue == LOW) {
    Serial.println("Fire detected in the front!");
    digitalWrite(7, HIGH);
    moveForward();
    delay(500);
    activateRelay();
  } else if (rightValue == LOW) {
    Serial.println("Fire detected on the right!");
    digitalWrite(7, HIGH);
    moveRight();
    delay(500);
    activateRelay();
  } else {
    stopMotors();
  }
  
  // Delay before next reading
  delay(100);
}

void moveLeft() {
  digitalWrite(motorLeftForward, LOW);
  digitalWrite(motorLeftBackward, HIGH);
  digitalWrite(motorRightForward, HIGH);
  digitalWrite(motorRightBackward, LOW);
  delay(2000);  // Move for 2 seconds
  stopMotors();
}

void moveForward() {
  digitalWrite(motorLeftForward, HIGH);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, HIGH);
  digitalWrite(motorRightBackward, LOW);
  delay(2000);  // Move for 2 seconds
  stopMotors();
}

void moveRight() {
  digitalWrite(motorLeftForward, HIGH);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, LOW);
  digitalWrite(motorRightBackward, HIGH);
  delay(2000);  // Move for 2 seconds
  stopMotors();
}

void stopMotors() {
  digitalWrite(motorLeftForward, LOW);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, LOW);
  digitalWrite(motorRightBackward, LOW);  digitalWrite(7, LOW);
}

void activateRelay() {
  digitalWrite(relayPin, LOW);
  delay(5000);  
  digitalWrite(relayPin, HIGH);
}
