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
  myServo5.write(45);
  myServo6.write(45);
  myServo7.write(45);
  myServo8.write(45);

  delay(5000);
  Serial.begin(9600);
  Serial.println("Spider Bot Initialized!");
}

// ------------------------------------------------------------
// FORWARD MOVEMENT
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// BACKWARD MOVEMENT
// (reverse of forward angles)
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// EXAMPLE "TRICK" #1: TURN LEFT
// (Basic example - adjust angles & logic for your actual bot)
// ------------------------------------------------------------
void turnLeft() {
  Serial.println("Turning LEFT...");
  // Very rough example: pivot front-left legs differently than front-right
  for (int angle = 0; angle <= 30; angle++) {
    myServo1.write(90 - angle);   // Leg A moves slightly
    myServo2.write(180 - angle);  // Leg A joint
    myServo3.write(90 + angle);   // Leg B moves opposite
    myServo4.write(180 - angle);  // Leg B joint
    myServo5.write(angle);        // Leg C
    myServo6.write(0);            // Leg C joint
    myServo7.write(angle);        // Leg D
    myServo8.write(0);            // Leg D joint
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

// ------------------------------------------------------------
// EXAMPLE "TRICK" #2: TURN RIGHT
// ------------------------------------------------------------
void turnRight() {
  Serial.println("Turning RIGHT...");
  // Mirror of turnLeft logic
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

// ------------------------------------------------------------
// EXAMPLE "TRICK" #3: DANCE
// (Lift all legs up/down - purely a silly demonstration)
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// MAIN LOOP
// ------------------------------------------------------------
void loop() {
  //1) Move forward 5 times
  // for (int i = 0; i < 5; i++) {
  //   moveForward();
  // }

  // // 2) Move backward 5 times
  // for(int i = 0; i < 3; i++) {
  //   moveBackward();
  // }

  // 3) Turn left 5 times
  // for(int i = 0; i < 3; i++) {
  //   turnLeft();
  // }

  // // 4) Turn right 5 times
  // for(int i = 0; i < 3; i++) {
  //   turnRight();
  // }

  //5) Dance 5 times
  // for (int i = 0; i < 6; i++) {
  //   dance();
  // }

  // myServo1.write(45);
  // myServo3.write(45);
  // myServo2.write(135);
  // myServo4.write(135);
  // int lastAngle = 135;
  // // --- Going "Up" ---
  // for(int i = 0; i < 6; i++)
  // {
  //   for (int angle = 0; angle < lastAngle; angle++) {
  //   myServo5.write(lastAngle - angle);
  //   myServo6.write(lastAngle - angle);
  //   myServo7.write(angle);
  //   myServo8.write(angle);
  //   delay(5);
  // }
  // delay(1000);

  // // --- Going "Down" ---
  // for (int angle = lastAngle; angle > 0; angle--) {
  //   myServo5.write(lastAngle - angle);
  //   myServo6.write(lastAngle - angle);
  //   myServo7.write(angle);
  //   myServo8.write(angle);
    
  //   delay(5);
  // }
  // delay(1000);
  // }


}
