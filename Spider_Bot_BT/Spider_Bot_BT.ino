#include <Servo.h>

// Create eight Servo objects (4 legs × 2 servos each)
Servo myServo1;  // Leg A
Servo myServo2;  // Leg A
Servo myServo3;  // Leg B
Servo myServo4;  // Leg B
Servo myServo5;  // Leg C
Servo myServo6;  // Leg C
Servo myServo7;  // Leg D
Servo myServo8;  // Leg D

// Pins
const int SERVO_PIN1 = 2;
const int SERVO_PIN2 = 3;
const int SERVO_PIN3 = 4;
const int SERVO_PIN4 = 5;
const int SERVO_PIN5 = 6;
const int SERVO_PIN6 = 7;
const int SERVO_PIN7 = 8;
const int SERVO_PIN8 = 9;

// Movement speed (ms delay per 1-degree step)
const int STEP_DELAY = 2;
// How long to wait at end positions (ms)
const int WAIT_TIME = 500;

// -------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------
void setup() {
  // Attach each servo
  myServo1.attach(SERVO_PIN1);
  myServo2.attach(SERVO_PIN2);
  myServo3.attach(SERVO_PIN3);
  myServo4.attach(SERVO_PIN4);
  myServo5.attach(SERVO_PIN5);
  myServo6.attach(SERVO_PIN6);
  myServo7.attach(SERVO_PIN7);
  myServo8.attach(SERVO_PIN8);

  // Move to a known "neutral" position
  myServo1.write(45);
  myServo2.write(135);
  myServo3.write(45);
  myServo4.write(135);

  // last 4 servos at ~mid range for demonstration
  myServo5.write(45);
  myServo6.write(45);
  myServo7.write(45);
  myServo8.write(45);

  // Initialize Serial at 9600 baud for HC-05 comm
  Serial.begin(9600);

  // Give user a moment to see
  delay(3000);

  Serial.println("Spider Bot Initialized!");
  Serial.println("HC-05 is ready. Send commands (F/B/L/R/D)...");
}

// -------------------------------------------------------------------
//  Movement Functions
// -------------------------------------------------------------------

void moveForward() {
  Serial.println("Moving FORWARD...");
  // PHASE 1: (1,3) 90->0, (2,4) 180->90, (5..8) 0->90
  for (int t = 0; t <= 90; t++) {
    myServo1.write(90 - t);
    myServo3.write(90 - t);
    myServo2.write(180 - t);
    myServo4.write(180 - t);

    myServo5.write(t);
    myServo6.write(t);
    myServo7.write(t);
    myServo8.write(t);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);

  // PHASE 2: (1,3) 0->90, (2,4) 90->180, (5..8) 90->0
  for (int t = 0; t <= 90; t++) {
    myServo1.write(0 + t);
    myServo3.write(0 + t);
    myServo2.write(90 + t);
    myServo4.write(90 + t);

    myServo5.write(90 - t);
    myServo6.write(90 - t);
    myServo7.write(90 - t);
    myServo8.write(90 - t);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);
}

void moveBackward() {
  Serial.println("Moving BACKWARD...");
  // PHASE 1: (1,3) 0->90, (2,4) 90->180, (5..8) 90->0
  for (int t = 0; t <= 90; t++) {
    myServo1.write(0 + t);
    myServo3.write(0 + t);
    myServo2.write(90 + t);
    myServo4.write(90 + t);

    myServo5.write(90 - t);
    myServo6.write(90 - t);
    myServo7.write(90 - t);
    myServo8.write(90 - t);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);

  // PHASE 2: (1,3) 90->0, (2,4) 180->90, (5..8) 0->90
  for (int t = 0; t <= 90; t++) {
    myServo1.write(90 - t);
    myServo3.write(90 - t);
    myServo2.write(180 - t);
    myServo4.write(180 - t);

    myServo5.write(t);
    myServo6.write(t);
    myServo7.write(t);
    myServo8.write(t);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);
}

void turnLeft() {
  Serial.println("Turning LEFT...");
  for (int angle = 0; angle <= 30; angle++) {
    // front legs
    myServo1.write(90 - angle);
    myServo2.write(180 - angle);
    myServo3.write(90 + angle);
    myServo4.write(180 - angle);

    // last legs
    myServo5.write(angle);
    myServo6.write(0);
    myServo7.write(angle);
    myServo8.write(0);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);

  // Return to neutral
  for (int angle = 30; angle >= 0; angle--) {
    myServo1.write(90 - angle);
    myServo2.write(180 - angle);
    myServo3.write(90 + angle);
    myServo4.write(180 - angle);

    myServo5.write(angle);
    myServo6.write(0);
    myServo7.write(angle);
    myServo8.write(0);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);
}

void turnRight() {
  Serial.println("Turning RIGHT...");
  for (int angle = 0; angle <= 30; angle++) {
    myServo1.write(90 + angle);
    myServo2.write(180 - angle);
    myServo3.write(90 - angle);
    myServo4.write(180 - angle);

    myServo5.write(0);
    myServo6.write(angle);
    myServo7.write(0);
    myServo8.write(angle);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);

  // Return to neutral
  for (int angle = 30; angle >= 0; angle--) {
    myServo1.write(90 + angle);
    myServo2.write(180 - angle);
    myServo3.write(90 - angle);
    myServo4.write(180 - angle);

    myServo5.write(0);
    myServo6.write(angle);
    myServo7.write(0);
    myServo8.write(angle);
    delay(STEP_DELAY);
  }
  delay(WAIT_TIME);
}

void dance() {
  Serial.println("Dancing...");
  for (int cycle = 0; cycle < 3; cycle++) {
    // Lift all legs
    for (int angle = 0; angle < 90; angle++) {
      myServo1.write(45);
      myServo3.write(45);
      myServo2.write(135);
      myServo4.write(135);

      myServo5.write(angle);
      myServo6.write(angle);
      myServo7.write(angle);
      myServo8.write(angle);
      delay(STEP_DELAY);
    }
    // Then put them back
    for (int angle = 90; angle >= 0; angle--) {
      myServo1.write(45);
      myServo3.write(45);
      myServo2.write(135);
      myServo4.write(135);

      myServo5.write(angle);
      myServo6.write(angle);
      myServo7.write(angle);
      myServo8.write(angle);
      delay(STEP_DELAY);
    }
  }
  delay(WAIT_TIME);
}

// -------------------------------------------------------------------
// Parse incoming Bluetooth commands (single letters):
//   'F' => moveForward()
//   'B' => moveBackward()
//   'L' => turnLeft()
//   'R' => turnRight()
//   'D' => dance()
//   'S' => Stop / Neutral
// -------------------------------------------------------------------
void parseBTCommand(char cmd) {
  cmd = toupper(cmd); // handle uppercase for simplicity
  int i = 0;
  switch (cmd) {
    case 'F':
      while(i < 5){
        i++;
      moveForward();}
      break;

    case 'B':
    while(i < 5){
        i++;
      moveBackward();}
      break;

    case 'L':
    while(i < 5){
        i++;
      turnLeft();}
      break;

    case 'R':
    while(i < 5){
        i++;
      turnRight();}
      break;

    case 'D':
    while(i < 5){
        i++;
      dance();}
      break;

    case 'S':
      // Stop/neutral
      Serial.println("Stopping...");
      myServo1.write(45);
      myServo2.write(135);
      myServo3.write(45);
      myServo4.write(135);
      myServo5.write(45);
      myServo6.write(45);
      myServo7.write(45);
      myServo8.write(45);
      delay(500);
      break;

    default:
      Serial.print("Unknown cmd: ");
      Serial.println(cmd);
      break;
  }
}

// -------------------------------------------------------------------
// Main Loop
// -------------------------------------------------------------------
void loop() {
  // 1) Check if the HC-05 (or Serial) has new data
  if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    parseBTCommand(incomingByte);
  }

  // 2) You could also do any "autonomous" code here if desired,
  //    but for pure HC-05 remote control, we just wait for commands.
}
