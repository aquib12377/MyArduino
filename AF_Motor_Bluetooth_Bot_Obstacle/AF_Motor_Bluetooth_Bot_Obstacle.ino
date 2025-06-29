#include <AFMotor.h>
#include <Servo.h>
#include <SoftwareSerial.h>

SoftwareSerial bt(A5,A4);
// Define motors on the Adafruit Motor Shield
AF_DCMotor motorLeft(1);  // Motor connected to port M1
AF_DCMotor motorRight(3); // Motor connected to port M2

// HC-SR04 Ultrasonic Sensor Pins
const int trigPin = A3;
const int echoPin = A2;

// Bluetooth module HC-05 connected to RX, TX pins
char command; // Stores Bluetooth commands

// Servo for obstacle avoidance
Servo myServo;
int servoPin = 10;

// Variable to store distance measurement
long duration;
int distance;

void setup() {
  bt.begin(9600);
  Serial.begin(9600);           // Start serial communication
  myServo.attach(servoPin);     // Attach servo to pin
  pinMode(trigPin, OUTPUT);     // Set trigPin as OUTPUT
  pinMode(echoPin, INPUT);      // Set echoPin as INPUT
  
  // Initialize motors
  motorLeft.setSpeed(150); 
  motorRight.setSpeed(150);
  
  // Initial servo position
  myServo.write(90); // Center position
  
  Serial.println("Bot is ready to receive commands via Bluetooth");
}

void loop() {
  // Check for Bluetooth command input
  if(bt.available() > 0) {
    String S = bt.readString();
    executeCommand(S);
  }

  // Read obstacle distance
  distance = getDistance();
  Serial.print("Ditance: ");
  Serial.println(distance);
  if (distance < 20) {
    // If an obstacle is too close, stop the bot
    stopBot();
    //Serial.println("Obstacle detected!");
  }
}

// Function to execute commands based on Bluetooth input
void executeCommand(String cmd) {
  cmd.trim();
  Serial.print("Received Command: ");
  Serial.println(cmd);
  if(cmd == "F")
  {
    moveForward();
  }
  else if(cmd == "B")
  {
    moveBackward();
  }
  else if(cmd == "R")
  {
    turnRight();
  }
  else if(cmd == "L")
  {
    turnLeft();
  }
  else if(cmd == "S")
  {
    stopBot();
  }
  
  
}

// Function to move the bot forward
void moveForward() {
  motorLeft.run(BACKWARD);
  motorRight.run(BACKWARD);
  //Serial.println("Moving Forward");
}

// Function to move the bot backward
void moveBackward() {
  motorLeft.run(FORWARD);
  motorRight.run(FORWARD);
  //Serial.println("Moving Backward");
}

// Function to turn the bot left
void turnLeft() {
  motorLeft.run(FORWARD);
  motorRight.run(BACKWARD);
  //Serial.println("Turning Left");
}

// Function to turn the bot right
void turnRight() {
  motorLeft.run(BACKWARD);
  motorRight.run(FORWARD);
  //Serial.println("Turning Right");
}

// Function to stop the bot
void stopBot() {
  motorLeft.run(RELEASE);
  motorRight.run(RELEASE);
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

