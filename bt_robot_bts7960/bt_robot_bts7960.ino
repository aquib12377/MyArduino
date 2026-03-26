#include <SoftwareSerial.h>

// -------------------- HC-05 Pins --------------------
#define BT_RX A0  // Moved to analog pins (SoftwareSerial works here)
#define BT_TX A1
SoftwareSerial bluetooth(BT_RX, BT_TX);

// -------------------- BTS7960 Motor 1 Pins --------------------
#define RPWM1  9   // PWM → Forward
#define LPWM1  10  // PWM → Backward
#define EN1    8   // R_EN + L_EN tied together

// -------------------- BTS7960 Motor 2 Pins --------------------
#define RPWM2  5   // PWM → Forward
#define LPWM2  6   // PWM → Backward
#define EN2    11  // R_EN + L_EN tied together

// -------------------- Ultrasonic Sensor Pins --------------------
#define FRONT_TRIG 3
#define FRONT_ECHO 2
#define BACK_TRIG  13
#define BACK_ECHO  12

// -------------------- LED & Switch Pins --------------------
#define LED_PIN    7
#define SWITCH_PIN 4

// -------------------- Speed (0–255) --------------------
#define MOTOR_SPEED 40

char command;

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);

  // BTS7960 Enable pins
  pinMode(EN1, OUTPUT);
  pinMode(EN2, OUTPUT);
  digitalWrite(EN1, HIGH);  // Always enabled
  digitalWrite(EN2, HIGH);

  // PWM pins (analogWrite handles pinMode internally, but explicit is fine)
  pinMode(RPWM1, OUTPUT);
  pinMode(LPWM1, OUTPUT);
  pinMode(RPWM2, OUTPUT);
  pinMode(LPWM2, OUTPUT);

  // Ultrasonic
  pinMode(FRONT_TRIG, OUTPUT);
  pinMode(FRONT_ECHO, INPUT);
  pinMode(BACK_TRIG,  OUTPUT);
  pinMode(BACK_ECHO,  INPUT);

  // LED & Switch
  pinMode(LED_PIN,    OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  stopMotors();
  Serial.println("BTS7960 Bluetooth Control Ready");
  bluetooth.println("Robot Ready");
}

// -------------------- LOOP --------------------
void loop() {
  float frontDist = getDistance(FRONT_TRIG, FRONT_ECHO);
  float backDist  = getDistance(BACK_TRIG,  BACK_ECHO);
  bool switchPressed    = (digitalRead(SWITCH_PIN) == LOW);
  bool obstacleDetected = (frontDist < 15 || backDist < 15 || switchPressed);

  digitalWrite(LED_PIN, obstacleDetected ? HIGH : LOW);

  if (bluetooth.available()) {
    command = toupper(bluetooth.read());
    Serial.print("Command: ");
    Serial.println(command);

    if (!obstacleDetected) {
      switch (command) {
        case 'F':
          (frontDist < 15) ? stopMotors() : moveForward();
          break;
        case 'B':
          (backDist  < 15) ? stopMotors() : moveBackward();
          break;
        case 'L': turnLeft();   break;
        case 'R': turnRight();  break;
        case 'S': stopMotors(); break;
        default:  stopMotors(); break;
      }
    } else {
      stopMotors();
      bluetooth.println("Obstacle Detected!");
    }
  }

  if(obstacleDetected)
  {
    stopMotors();
      bluetooth.println("Obstacle Detected!");
  }


}

// -------------------- MOTOR FUNCTIONS --------------------
//
//  BTS7960 truth table:
//    Forward  → RPWM = PWM,  LPWM = 0
//    Backward → RPWM = 0,    LPWM = PWM
//    Stop     → RPWM = 0,    LPWM = 0
//
void moveForward() {
  analogWrite(RPWM1, MOTOR_SPEED); analogWrite(LPWM1, 0);
  analogWrite(RPWM2, MOTOR_SPEED); analogWrite(LPWM2, 0);
}

void moveBackward() {
  analogWrite(RPWM1, 0); analogWrite(LPWM1, MOTOR_SPEED);
  analogWrite(RPWM2, 0); analogWrite(LPWM2, MOTOR_SPEED);
}

void turnLeft() {
  // Motor 1 backward, Motor 2 forward
  analogWrite(RPWM1, 0);            analogWrite(LPWM1, MOTOR_SPEED);
  analogWrite(RPWM2, MOTOR_SPEED);  analogWrite(LPWM2, 0);
}

void turnRight() {
  // Motor 1 forward, Motor 2 backward
  analogWrite(RPWM1, MOTOR_SPEED);  analogWrite(LPWM1, 0);
  analogWrite(RPWM2, 0);            analogWrite(LPWM2, MOTOR_SPEED);
}

void stopMotors() {
  analogWrite(RPWM1, 0); analogWrite(LPWM1, 0);
  analogWrite(RPWM2, 0); analogWrite(LPWM2, 0);
}

// -------------------- ULTRASONIC FUNCTION --------------------
float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 20000);
  float distance = duration * 0.034 / 2.0;
  if (distance == 0 || distance > 400) distance = 400;
  return distance;
}