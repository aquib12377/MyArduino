/*
 * Liquid dispenser: E18-D80NK proximity sensor -> 180° (positional) servo
 * Board: Arduino Nano
 *
 * On detection:  PRESS down -> HOLD 1000ms -> RELEASE up -> idle
 * Runs once per detection. Object must leave the beam before it re-triggers.
 *
 * IMPORTANT: servo on a SEPARATE 5V supply, common ground, 470-1000uF cap,
 * or stalls will RESET the Nano.
 */

#include <Servo.h>

// ---------------- Pins ----------------
const uint8_t SENSOR_PIN = 2;   // E18-D80NK signal (black wire)
const uint8_t SERVO_PIN  = 9;   // servo signal wire

// --------------- Sensor ---------------
const int OBJECT_LEVEL = LOW;  // HIGH = object present; use LOW if inverted

// ------- Servo positions (degrees) -------
// Set these to YOUR mechanism. HOME = resting/up, PRESS = knob fully pushed.
// If it moves the wrong way, swap the two values.
const int HOME_ANGLE  = 0;    // resting position (knob up)
const int PRESS_ANGLE = 120;   // pressed position (knob down)

// ------- Speed control -------
// The servo steps STEP_SIZE degrees, waiting STEP_DELAY_MS between steps.
// Bigger STEP_DELAY_MS or smaller STEP_SIZE = slower, smoother motion.
const int           STEP_SIZE     = 1;    // degrees per step
const unsigned long STEP_DELAY_MS = 10;   // ms between steps (raise = slower)

// ---------------- Timing ----------------
const unsigned long DWELL_MS  = 500;  // hold while pressed
const unsigned long WARMUP_MS = 5000;  // ignore sensor while it stabilizes

Servo servo;
bool armed = true;
int  lastPrinted = -1;

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  // Start at home position
  servo.attach(SERVO_PIN);
  servo.write(HOME_ANGLE);

  Serial.println("warming up sensor...");
  delay(WARMUP_MS);
  Serial.println("ready");
}

void loop() {
  bool objectDetected = (digitalRead(SENSOR_PIN) == OBJECT_LEVEL);

  if (objectDetected != lastPrinted) {
    Serial.println(objectDetected ? "DETECTED" : "clear");
    lastPrinted = objectDetected;
  }

  if (objectDetected && armed) {
    armed = false;
    dispense();
  }

  if (!objectDetected) {
    armed = true;
  }
}

// Move smoothly from the current angle to 'target'
void moveTo(int from, int target) {
  if (from < target) {
    for (int a = from; a <= target; a += STEP_SIZE) {
      servo.write(a);
      delay(STEP_DELAY_MS);
    }
  } else {
    for (int a = from; a >= target; a -= STEP_SIZE) {
      servo.write(a);
      delay(STEP_DELAY_MS);
    }
  }
  servo.write(target);   // land exactly on target
}

void dispense() {
  moveTo(HOME_ANGLE, PRESS_ANGLE);   // press down slowly
  delay(DWELL_MS);                   // hold
  moveTo(PRESS_ANGLE, HOME_ANGLE);   // release up slowly
}