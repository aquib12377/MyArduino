#include <AFMotor.h>
#include <Servo.h>
#include <SoftwareSerial.h>

SoftwareSerial bt(A2, A3);
// Define motors on the Adafruit Motor Shield
AF_DCMotor motorLeftFront(1);   // Motor connected to port M1
AF_DCMotor motorRightFront(3);  // Motor connected to port M3
AF_DCMotor motorLeftBack(2);    // Motor connected to port M2
AF_DCMotor motorRightBack(4);   // Motor connected to port M4

// HC-SR04 Ultrasonic Sensor Pins
const int trigPin = A0;
const int echoPin = A1;

// Bluetooth module HC-05 connected to RX, TX pins
char command;  // Stores Bluetooth commands

// Servo for obstacle avoidance
Servo myServo;
int servoPin = 10;

// Variable to store distance measurement
long duration;
int distance;

void setup() {
  bt.begin(9600);
  Serial.begin(9600);        // Start serial communication
  myServo.attach(servoPin);  // Attach servo to pin
  pinMode(trigPin, OUTPUT);  // Set trigPin as OUTPUT
  pinMode(echoPin, INPUT);   // Set echoPin as INPUT

  // Initialize motors and set speed
  motorLeftFront.setSpeed(150);
  motorRightFront.setSpeed(150);
  motorLeftBack.setSpeed(150);
  motorRightBack.setSpeed(150 );

  // Initial servo position
  myServo.write(0);  // Center position

  Serial.println("Bot is ready to receive commands via Bluetooth");
}

void loop() {
  // Check for Bluetooth command input
  if (bt.available() > 0) {
    char c = bt.read();
    Serial.println(c);
    executeCommand(c);
  }

  // Read obstacle distance
  distance = getDistance();
  //Serial.print("Ditance: ");
  //Serial.println(distance);
  if (distance < 35) {
    for (int pos = 0; pos <= 180; pos += 2) {
      Serial.println("Pos:"+String(pos));
      myServo.write(pos);
      delay(5);
    }
    for (int pos = 180; pos >= 0; pos -= 2) {
      Serial.println("Pos:"+String(pos));
      myServo.write(pos);
      delay(5);
    }
    stopBot();

    

    Serial.println("Obstacle detected!");
  }
}

// Function to execute commands based on Bluetooth input
void executeCommand(char cmd) {
  //md.trim();
  Serial.print("Received Command: ");
  Serial.println(cmd);
  if (cmd == 'F') {
    Serial.println("Moving Forward");
    moveForward();
  } else if (cmd == 'B') {
    Serial.println("Moving Backward");
    moveBackward();
  } else if (cmd == 'R') {
    Serial.println("Moving Right");
    turnRight();
  } else if (cmd == 'L') {
    Serial.println("Moving Left");
    turnLeft();
  } else if (cmd == 'S') {
    Serial.println("Stopping");
    stopBot();
  } else{
    Serial.println("No Command");
    stopBot();
  }
}

// Function to move the bot forward
void moveForward() {
  motorLeftFront.run(BACKWARD);
  motorRightFront.run(BACKWARD);
  motorLeftBack.run(BACKWARD);
  motorRightBack.run(BACKWARD);
  //Serial.println("Moving Forward");
}

// Function to move the bot backward
void moveBackward() {
  motorLeftFront.run(FORWARD);
  motorRightFront.run(FORWARD);
  motorLeftBack.run(FORWARD);
  motorRightBack.run(FORWARD);
  //Serial.println("Moving Backward");
}

// Function to turn the bot left
void turnLeft() {
  motorLeftFront.run(FORWARD);
  motorRightFront.run(BACKWARD);
  motorLeftBack.run(FORWARD);
  motorRightBack.run(BACKWARD);
  //Serial.println("Turning Left");
}

// Function to turn the bot right
void turnRight() {
  motorLeftFront.run(BACKWARD);
  motorRightFront.run(FORWARD);
  motorLeftBack.run(BACKWARD);
  motorRightBack.run(FORWARD);
  //Serial.println("Turning Right");
}

// Function to stop the bot
void stopBot() {
  motorLeftFront.run(RELEASE);
  motorRightFront.run(RELEASE);
  motorLeftBack.run(RELEASE);
  motorRightBack.run(RELEASE);
  Serial.println("Stopping");
}

// Function to measure distance using ultrasonic sensor
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;
  return distance == 0 ? 25 : distance;
}
