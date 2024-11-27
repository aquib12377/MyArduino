#include <BluetoothSerial.h>  // ESP32 Bluetooth Serial Library
#include <ESP32Servo.h>       // Servo Library for controlling the arm

BluetoothSerial SerialBT;  // Bluetooth Serial Object
// Declare Servo objects
Servo gripperServo;
Servo belowGripperServo;
Servo armServo1;  // Servo 14
Servo armServo2;  // Servo 27
Servo armServo3;  // Servo 26
Servo baseServo;  // Servo 25

// Variables to store the previous positions for each servo
int previousPositionGripper = 0;
int previousPositionBelowGripper = 0;
int previousPositionArm1 = 0;
int previousPositionArm2 = 0;
int previousPositionArm3 = 120;  // Starting at 120 degrees
int previousPositionBase = 120;  // Starting at 120 degrees         // Servo for arm control

// Motor pins (adjust to match your hardware)
const int motorLeftForward = 23;
const int motorLeftBackward = 22;
const int motorRightForward = 21;
const int motorRightBackward = 19;
const int motorLeftForward1 = 18;
const int motorLeftBackward1 = 5;
const int motorRightForward1 = 17;
const int motorRightBackward1 = 16;

// Actuator pins (adjust to match your hardware)
const int actuatorPin = 32;
const int actuatorPin1 = 33;

// Global variables
bool isHolding = false;
int armPosition = 90;  // Initial arm position (90 degrees)

// Function to stop all motors
// Function to stop all motors
void stopMotors() {
  digitalWrite(motorLeftForward, LOW);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, LOW);
  digitalWrite(motorRightBackward, LOW);
  digitalWrite(motorLeftForward1, LOW);
  digitalWrite(motorLeftBackward1, LOW);
  digitalWrite(motorRightForward1, LOW);
  digitalWrite(motorRightBackward1, LOW);
}

// Function to move the bot forward
void moveForward() {
  digitalWrite(motorLeftForward, LOW);
  digitalWrite(motorLeftBackward, HIGH);
  digitalWrite(motorRightForward, LOW);
  digitalWrite(motorRightBackward, HIGH);
  digitalWrite(motorLeftForward1, HIGH);
  digitalWrite(motorLeftBackward1, LOW);
  digitalWrite(motorRightForward1, HIGH);
  digitalWrite(motorRightBackward1, LOW);
}

// Function to move the bot backward
void moveBackward() {
  digitalWrite(motorLeftForward, HIGH);
  digitalWrite(motorLeftBackward, LOW);
  digitalWrite(motorRightForward, HIGH);
  digitalWrite(motorRightBackward, LOW);
  digitalWrite(motorLeftForward1, LOW);
  digitalWrite(motorLeftBackward1, HIGH);
  digitalWrite(motorRightForward1, LOW);
  digitalWrite(motorRightBackward1, HIGH);
}

// Function to move the bot left
void moveLeft() {
  digitalWrite(motorLeftForward, LOW);   // Stop the left motors
  digitalWrite(motorLeftBackward, HIGH); // Move the left motors backward
  digitalWrite(motorRightForward, HIGH); // Move the right motors forward
  digitalWrite(motorRightBackward, LOW); // Stop the right motors
  digitalWrite(motorLeftForward1, LOW);
  digitalWrite(motorLeftBackward1, HIGH);
  digitalWrite(motorRightForward1, HIGH);
  digitalWrite(motorRightBackward1, LOW);
}

// Function to move the bot right
void moveRight() {
  digitalWrite(motorLeftForward, HIGH);  // Move the left motors forward
  digitalWrite(motorLeftBackward, LOW);  // Stop the left motors
  digitalWrite(motorRightForward, LOW);  // Stop the right motors
  digitalWrite(motorRightBackward, HIGH); // Move the right motors backward
  digitalWrite(motorLeftForward1, HIGH);
  digitalWrite(motorLeftBackward1, LOW);
  digitalWrite(motorRightForward1, LOW);
  digitalWrite(motorRightBackward1, HIGH);
}


// Function to hold actuators
void holdActuators() {
  digitalWrite(actuatorPin, LOW);
  digitalWrite(actuatorPin1, HIGH);
  isHolding = true;
}

// Function to release actuators
void releaseActuators() {
  digitalWrite(actuatorPin, HIGH);
  digitalWrite(actuatorPin1, LOW);
  isHolding = false;
}

void holdActuators1() {
  digitalWrite(actuatorPin, LOW);
  digitalWrite(actuatorPin1, HIGH);
  delay(1000);
  digitalWrite(actuatorPin, HIGH);
  digitalWrite(actuatorPin1, LOW);
  isHolding = true;
}

// Function to release actuators
void releaseActuators1() {
  digitalWrite(actuatorPin, HIGH);
  digitalWrite(actuatorPin1, LOW);
  isHolding = false;
}


void setup() {
  // Initialize Serial and Bluetooth
  Serial.begin(9600);
  SerialBT.begin("ESP32Bot");  // Bluetooth device name
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  // Set PWM frequency for the servos
  gripperServo.setPeriodHertz(50);
  belowGripperServo.setPeriodHertz(50);
  armServo1.setPeriodHertz(50);
  armServo2.setPeriodHertz(50);
  armServo3.setPeriodHertz(50);
  baseServo.setPeriodHertz(50);

  // Attach the servos to the correct pins
  gripperServo.attach(13);
  belowGripperServo.attach(12);
  armServo1.attach(14);
  armServo2.attach(27);
  armServo3.attach(26);
  baseServo.attach(25);
  // Initialize motor pins
  pinMode(motorLeftForward, OUTPUT);
  pinMode(motorLeftBackward, OUTPUT);
  pinMode(motorRightForward, OUTPUT);
  pinMode(motorRightBackward, OUTPUT);

  pinMode(motorLeftForward1, OUTPUT);
  pinMode(motorLeftBackward1, OUTPUT);
  pinMode(motorRightForward1, OUTPUT);
  pinMode(motorRightBackward1, OUTPUT);

  // Initialize actuator pin
  pinMode(actuatorPin, OUTPUT);
  pinMode(actuatorPin1, OUTPUT);
  // Stop motors and actuators
  stopMotors();
  releaseActuators();

  Serial.println("ESP32 Bot Control Ready");
}

void loop() {
  // Check if there's any incoming Bluetooth data
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');  // Read command
    Serial.println("Received command: " + command);

    if (command == "MOVE_FORWARD") {
      moveForward();
    } else if (command == "MOVE_BACKWARD") {
      moveBackward();
    } else if (command == "MOVE_LEFT") {
      moveLeft();
    } else if (command == "MOVE_RIGHT") {
      moveRight();
    } else if (command == "STOP") {
      stopMotors();
    } else if (command == "HOLD") {
      holdActuators();
    } else if (command == "RELEASE") {
      releaseActuators();
    } else if (command == "HOLD1") {
      holdActuators1();
    } else if (command == "RELEASE1") {
      releaseActuators1();
    } else if (command == "RAISE") {
      raiseArmUp();
    } else if (command == "DOWN") {
      raiseArmDown();
    } else if (command == "PICK") {
      pick();
    } else if (command == "RELEASE_GRIPPER") {
      release();
    }
  }
}

void moveServo(Servo &servo, int targetAngle, int &previousPosition) {
  int currentPosition = previousPosition;  // Get the last position of the servo

  if (targetAngle > currentPosition) {
    // Move clockwise (CW)
    for (int pos = currentPosition; pos <= targetAngle; pos++) {
      servo.write(pos);
      delay(15);  // Adjust delay for speed of the movement
    }
  } else if (targetAngle < currentPosition) {
    // Move counterclockwise (ACW)
    for (int pos = currentPosition; pos >= targetAngle; pos--) {
      servo.write(pos);
      delay(15);  // Adjust delay for speed of the movement
    }
  }

  // Update previous position after the movement
  previousPosition = targetAngle;
}

void raiseArmUp() {
  Serial.println("Raising arm up...");

  moveServo(gripperServo, 150, previousPositionGripper);  // Servo 14 bend (raise)
  Serial.println("Gripper Close");
  delay(500);

  moveServo(belowGripperServo, 0, previousPositionBelowGripper);  // Servo 14 bend (raise)
  Serial.println("Below Gripper Servo Straight");
  delay(500);

  moveServo(armServo1, 0, previousPositionArm1);  // Servo 14 bend (raise)
  Serial.println("Arm1 Servo Straight");
  delay(500);

  moveServo(armServo2, 130, previousPositionArm2);  // Servo 14 bend (raise)
  Serial.println("Arm2 Servo Straight");
  delay(500);

  moveServo(armServo3, 40, previousPositionArm3);  // Servo 26 straight
  Serial.println("Arm3 Servo Straight");
  delay(500);

  Serial.println("Arm fully raised.");
}

// Method to raise the arm down
void raiseArmDown() {
  Serial.println("Lowering arm down...");

  moveServo(gripperServo, 150, previousPositionGripper);  // Servo 14 bend (raise)
  Serial.println("Gripper Close");
  delay(500);

  moveServo(belowGripperServo, 0, previousPositionBelowGripper);  // Servo 14 bend (raise)
  Serial.println("Below Gripper Servo rotated");
  delay(500);

  moveServo(armServo1, 40, previousPositionArm1);  // Servo 14 bend (raise)
  Serial.println("Arm1 Servo Straight");
  delay(500);

  moveServo(armServo2, 80, previousPositionArm2);  // Servo 14 bend (raise)
  Serial.println("Arm2 Servo Straight");
  delay(500);

  moveServo(armServo3, 50, previousPositionArm3);  // Servo 26 straight
  Serial.println("Arm3 Servo Straight");
  delay(500);

  Serial.println("Arm fully lowered.");
}

// Method to pick an object
void pick() {
  Serial.println("Picking up object...");

  moveServo(belowGripperServo, 0, previousPositionBelowGripper);  // Servo 12 vertical
  Serial.println("Servo 12 (Below Gripper) set to vertical (1 degree).");
  delay(500);

  moveServo(gripperServo, 150, previousPositionGripper);  // Gripper close
  Serial.println("Gripper closing to 150 degrees.");
  delay(500);

  Serial.println("Object picked.");
}

// Method to release an object
void release() {
  Serial.println("Releasing object...");

  moveServo(gripperServo, 0, previousPositionGripper);  // Gripper open
  Serial.println("Gripper opening to 0 degrees.");
  delay(500);

  moveServo(belowGripperServo, 0, previousPositionBelowGripper);  // Servo 12 straight
  Serial.println("Servo 12 (Below Gripper) returning to straight (0 degrees).");
  delay(500);

  Serial.println("Object released.");
}
