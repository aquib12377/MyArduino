/*
  2x BTS7960 + 2x DC Motors + HC-05 Bluetooth control
  ---------------------------------------------------
  - Arduino UNO / Nano
  - BTS7960 R_EN / L_EN tied to 5V (always enabled)
  - We use RPWM / LPWM for direction + speed
  - HC-05 via SoftwareSerial on pins 2 (RX) and 3 (TX)

  Commands over Bluetooth:
    F = both motors forward
    B = both motors backward
    L = left turn (M1 back, M2 forward)
    R = right turn (M1 fwd, M2 back)
    S = stop
    0..9 = speed level (0..255)
*/

#include <SoftwareSerial.h>

// -------- Bluetooth (HC-05) pins --------
const uint8_t BT_RX_PIN = 3;  // Arduino RX  <- HC-05 TX
const uint8_t BT_TX_PIN = 2;  // Arduino TX  -> HC-05 RX

SoftwareSerial BT(BT_RX_PIN, BT_TX_PIN);

// -------- BTS7960 pins for Motor 1 --------
const uint8_t M1_RPWM = 5;    // PWM
const uint8_t M1_LPWM = 6;    // PWM
// R_EN1 / L_EN1 tied to 5V on the module

// -------- BTS7960 pins for Motor 2 --------
const uint8_t M2_RPWM = 9;    // PWM
const uint8_t M2_LPWM = 10;   // PWM
// R_EN2 / L_EN2 tied to 5V on the module

// -------- Current speed setting (0..255) --------
int currentSpeed = 180;   // default mid-high speed

// -------- Helpers to drive motors (signed speed) --------
// speed: -255..+255 (negative = reverse, positive = forward)
void setMotor1(int speed) {
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    // Forward
    analogWrite(M1_RPWM, speed);
    digitalWrite(M1_LPWM, LOW);
  } else if (speed < 0) {
    // Reverse
    analogWrite(M1_LPWM, -speed);
    digitalWrite(M1_RPWM, LOW);
  } else {
    // Stop (coast)
    digitalWrite(M1_RPWM, LOW);
    digitalWrite(M1_LPWM, LOW);
  }
}

void setMotor2(int speed) {
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    // Forward
    analogWrite(M2_RPWM, speed);
    digitalWrite(M2_LPWM, LOW);
  } else if (speed < 0) {
    // Reverse
    analogWrite(M2_LPWM, -speed);
    digitalWrite(M2_RPWM, LOW);
  } else {
    // Stop (coast)
    digitalWrite(M2_RPWM, LOW);
    digitalWrite(M2_LPWM, LOW);
  }
}

// Convenience: stop both motors
void stopAll() {
  setMotor1(0);
  setMotor2(0);
}

void setup() {
  // PWM pins
  pinMode(M1_RPWM, OUTPUT);
  pinMode(M1_LPWM, OUTPUT);
  pinMode(M2_RPWM, OUTPUT);
  pinMode(M2_LPWM, OUTPUT);

  stopAll();

  // Serial for debug
  Serial.begin(9600);
  Serial.println("BTS7960 Dual Motor + HC-05 Control");

  // Bluetooth serial
  BT.begin(9600);  // default HC-05 baud
  Serial.println("Waiting for Bluetooth commands...");
}

void loop() {
  // Check if something arrived via Bluetooth
  if (BT.available()) {
    char c = BT.read();

    // Echo to debug serial
    Serial.print("BT cmd: ");
    Serial.println(c);

    // Normalize to uppercase for letters
    if (c >= 'a' && c <= 'z') {
      c = c - 'a' + 'A';
    }

    switch (c) {
      // Direction commands
      case 'F':  // Forward
        setMotor1(currentSpeed);
        setMotor2(currentSpeed);
        Serial.print("Forward at speed ");
        Serial.println(currentSpeed);
        break;

      case 'B':  // Backward
        setMotor1(-currentSpeed);
        setMotor2(-currentSpeed);
        Serial.print("Backward at speed ");
        Serial.println(currentSpeed);
        break;

      case 'L':  // Left turn
        setMotor1(-currentSpeed);
        setMotor2(currentSpeed);
        Serial.print("Left at speed ");
        Serial.println(currentSpeed);
        break;

      case 'R':  // Right turn
        setMotor1(currentSpeed);
        setMotor2(-currentSpeed);
        Serial.print("Right at speed ");
        Serial.println(currentSpeed);
        break;

      case 'S':  // Stop
        stopAll();
        Serial.println("Stop");
        break;

      // Speed commands 0..9
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      {
        int level = c - '0';  // 0..9
        currentSpeed = map(level, 0, 9, 0, 255);
        Serial.print("Speed level ");
        Serial.print(level);
        Serial.print(" -> PWM ");
        Serial.println(currentSpeed);
        break;
      }

      default:
        // Ignore unknown commands
        Serial.println("Unknown command");
        break;
    }
  }

  // No blocking code here; loop stays responsive
}
