/*
 * ============================================================
 *  3-DIRECTION FLAME FIGHTING BOT — Arduino Nano
 * ============================================================
 *  HARDWARE:
 *    Flame Sensors  : Active LOW  → D2 (Left)  D3 (Mid)  D4 (Right)
 *    L298N  IN1-IN4 : D5  D6  D7  D8
 *    L298N  ENA/ENB : D9  D10  (PWM speed control)
 *    Servo  (pump)  : D11
 *    Relay  (pump)  : D12  (Active LOW — LOW = ON)
 * ============================================================
 *  WIRING REMINDER:
 *    • Flame sensor OUT  → Nano digital pin  (10kΩ pull-up on board is fine)
 *    • L298N 12V rail    → separate 7-12V battery
 *    • L298N 5V out      → Nano VIN  (if board has on-board 5V regulator)
 *    • Relay IN          → D12  (LOW activates coil)
 *    • Relay COM/NO      → pump power circuit
 * ============================================================
 */

#include <Servo.h>

// ── Pin Definitions ──────────────────────────────────────────
// Flame Sensors (Active LOW: LOW = flame detected)
const uint8_t PIN_FLAME_LEFT   = A2;
const uint8_t PIN_FLAME_MIDDLE = A0;
const uint8_t PIN_FLAME_RIGHT  = A1;

// L298N Motor Driver — Left Motor
const uint8_t PIN_IN1  = 5;   // Left motor forward
const uint8_t PIN_IN2  = 6;   // Left motor backward
const uint8_t PIN_ENA  = 9;   // Left motor speed  (PWM)

// L298N Motor Driver — Right Motor
const uint8_t PIN_IN3  = 7;   // Right motor forward
const uint8_t PIN_IN4  = 8;   // Right motor backward
const uint8_t PIN_ENB  = 10;  // Right motor speed (PWM)

// Servo & Relay
const uint8_t PIN_SERVO = 11;
const uint8_t PIN_RELAY = 12; // Active LOW

// ── Tuning Constants ─────────────────────────────────────────
const uint8_t  SPEED_FORWARD   = 255; // 0-255  drive speed
const uint8_t  SPEED_TURN      = 255; // 0-255  turn speed (inner wheel)
const uint16_t SERVO_CENTER    = 90;  // degrees — pump resting position
const uint16_t SERVO_LEFT_MAX  = 45;  // degrees — sweep left limit
const uint16_t SERVO_RIGHT_MAX = 135; // degrees — sweep right limit
const uint8_t  SERVO_STEP      = 2;   // degrees per sweep step
const uint16_t SERVO_STEP_MS   = 30;  // ms delay between servo steps
const uint16_t EXTINGUISH_CONFIRM_MS = 800; // ms flame must be gone to confirm

// ── Objects ───────────────────────────────────────────────────
Servo pumpServo;

// ── Helper: read flame (returns true when flame detected) ─────
inline bool flameLeft()   { return digitalRead(PIN_FLAME_LEFT)   == LOW; }
inline bool flameMiddle() { return digitalRead(PIN_FLAME_MIDDLE) == LOW; }
inline bool flameRight()  { return digitalRead(PIN_FLAME_RIGHT)  == LOW; }
inline bool anyFlame()    { return flameLeft() || flameMiddle() || flameRight(); }

// ── Motor helpers ─────────────────────────────────────────────
void setMotors(int8_t leftDir, int8_t rightDir,
               uint8_t leftSpeed, uint8_t rightSpeed) {
  // leftDir / rightDir:  1 = forward,  -1 = backward,  0 = stop
  analogWrite(PIN_ENA, leftSpeed);
  analogWrite(PIN_ENB, rightSpeed);

  digitalWrite(PIN_IN1, leftDir  ==  1 ? HIGH : LOW);
  digitalWrite(PIN_IN2, leftDir  == -1 ? HIGH : LOW);
  digitalWrite(PIN_IN3, rightDir ==  1 ? HIGH : LOW);
  digitalWrite(PIN_IN4, rightDir == -1 ? HIGH : LOW);
}

void driveForward()  { setMotors( 1,  1, SPEED_FORWARD, SPEED_FORWARD); }
void driveBackward() { setMotors(-1, -1, SPEED_FORWARD, SPEED_FORWARD); }
void turnLeft()      { setMotors(-1,  1, SPEED_TURN,    SPEED_TURN);    }
void turnRight()     { setMotors( 1, -1, SPEED_TURN,    SPEED_TURN);    }
void stopMotors()    { setMotors( 0,  0, 0, 0);                          }

// ── Pump helpers ──────────────────────────────────────────────
void pumpON()  { digitalWrite(PIN_RELAY, LOW);  } // Active LOW
void pumpOFF() { digitalWrite(PIN_RELAY, HIGH); }

// ── Servo sweep until flame is extinguished ───────────────────
void sweepUntilExtinguished() {
  int  servoPos   = SERVO_CENTER;
  int  direction  = 1;           // +1 sweep right, -1 sweep left
  bool sweeping   = true;
  unsigned long flameClearSince = 0;

  pumpServo.write(SERVO_CENTER);
  delay(300);

  while (sweeping) {
    // Move servo one step
    servoPos += direction * SERVO_STEP;

    // Bounce at limits
    if (servoPos >= SERVO_RIGHT_MAX) { servoPos = SERVO_RIGHT_MAX; direction = -1; }
    if (servoPos <= SERVO_LEFT_MAX)  { servoPos = SERVO_LEFT_MAX;  direction =  1; }

    pumpServo.write(servoPos);
    delay(SERVO_STEP_MS);

    // Check if flame is gone long enough to confirm
    if (!flameMiddle() && !anyFlame()) {
      if (flameClearSince == 0) {
        flameClearSince = millis();
      } else if (millis() - flameClearSince >= EXTINGUISH_CONFIRM_MS) {
        sweeping = false; // confirmed extinguished
      }
    } else {
      flameClearSince = 0; // flame still present — reset timer
    }
  }

  // Return servo to center and shut pump
  pumpServo.write(SERVO_CENTER);
  delay(300);
  pumpOFF();
}

// ═════════════════════════════════════════════════════════════
void setup() {
  // Flame sensor inputs with internal pull-ups (sensors are active LOW)
  pinMode(PIN_FLAME_LEFT,   INPUT_PULLUP);
  pinMode(PIN_FLAME_MIDDLE, INPUT_PULLUP);
  pinMode(PIN_FLAME_RIGHT,  INPUT_PULLUP);

  // Motor driver pins
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);

  // Relay — HIGH = pump OFF at startup (Active LOW)
  pinMode(PIN_RELAY, OUTPUT);
  pumpOFF();

  // Servo
  pumpServo.attach(PIN_SERVO);
  pumpServo.write(SERVO_CENTER);

  stopMotors();
  Serial.begin(9600);
  Serial.println(F("FlameBot ready."));
}

// ═════════════════════════════════════════════════════════════
void loop() {
  bool left   = flameLeft();
  bool middle = flameMiddle();
  bool right  = flameRight();

  // ── Priority 1: Middle flame → stop & extinguish ────────────
  if (middle) {
    stopMotors();
    Serial.println(F("FLAME AHEAD — Activating pump!"));
    pumpON();
    sweepUntilExtinguished();
    Serial.println(F("Flame extinguished. Resuming search."));
    return;
  }

  // ── Priority 2: Only left flame → turn left ─────────────────
  if (left && !right) {
    Serial.println(F("Flame LEFT — turning left."));
    turnLeft();
    return;
  }

  // ── Priority 3: Only right flame → turn right ───────────────
  if (right && !left) {
    Serial.println(F("Flame RIGHT — turning right."));
    turnRight();
    return;
  }

  // ── Priority 4: Both left & right (spread/wide flame) ────────
  //    Drive forward so middle sensor can get a fix
  if (left && right) {
    Serial.println(F("Flame BOTH SIDES — driving forward."));
    driveForward();
    return;
  }

  // ── No flame detected → stop and wait ───────────────────────
  stopMotors();
}
