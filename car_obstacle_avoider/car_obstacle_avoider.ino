/*
 * 2WD Obstacle-Avoiding Car
 * Board   : Arduino Nano (ATmega328P)
 * Driver  : L293D (EN1,2 and EN3,4 tied to 5V -> motors always enabled, no PWM speed control)
 * Sensor  : HC-SR04 ultrasonic, FIXED to the front (no servo)
 *
 * Behaviour:
 *   1. Drive forward, ping every ~50 ms.
 *   2. Obstacle confirmed -> stop, (optionally) back up a little.
 *   3. Spin LEFT 90 deg  -> stop -> measure distance.
 *   4. Spin RIGHT 180 deg -> stop -> measure distance.
 *   5. Pick the clearer side (must be >= CLEAR_CM):
 *        - Right clearer : already facing it -> drive.
 *        - Left clearer  : spin LEFT 180 deg -> drive.
 *        - Both blocked  : spin RIGHT 90 deg (now facing back) -> measure ->
 *                          drive if clear, otherwise stop (TRAPPED) and retry later.
 *
 * Because there is no servo, the whole car rotates to "look". Turns are TIME-based,
 * so TURN_90_MS must be calibrated for your car, floor and battery.
 *
 * Serial monitor: 115200 baud.
 */

#include <Arduino.h>

#define FW_VERSION "1.0"

// ======================= PIN MAP =======================
constexpr uint8_t PIN_IN1  = 2;   // Left motor  (L293D pin 2)
constexpr uint8_t PIN_IN2  = 3;   // Left motor  (L293D pin 7)
constexpr uint8_t PIN_IN3  = 4;   // Right motor (L293D pin 10)
constexpr uint8_t PIN_IN4  = 5;   // Right motor (L293D pin 15)
constexpr uint8_t PIN_TRIG = 8;
constexpr uint8_t PIN_ECHO = 9;
constexpr uint8_t PIN_LED  = LED_BUILTIN;  // D13

// ======================= CONFIG =======================
// Swap if a wheel spins the wrong way (or just swap the motor wires).
constexpr bool INVERT_LEFT  = false;
constexpr bool INVERT_RIGHT = false;

constexpr bool     MOTOR_TEST_ON_BOOT = false;  // true = wiggle wheels once at boot to verify directions
constexpr uint16_t START_DELAY_MS     = 3000;   // time to unplug USB and put the car on the floor

// Distances (cm)
constexpr uint16_t OBSTACLE_CM       = 25;      // closer than this = obstacle (increase if car is fast / heavy)
constexpr uint16_t CLEAR_CM          = 45;      // a path must be at least this far to be chosen (> OBSTACLE_CM)
constexpr uint16_t MAX_CM            = 400;
constexpr uint8_t  OBSTACLE_CONFIRMS = 2;       // consecutive close readings needed (filters noise spikes)

// Ultrasonic
constexpr unsigned long ECHO_TIMEOUT_US = 25000UL;  // ~4.3 m max
constexpr uint8_t  SCAN_SAMPLES         = 5;        // median of N pings when stopped
constexpr uint16_t SAMPLE_GAP_MS        = 30;       // gap between pings (avoids echo cross-talk)
constexpr uint16_t FORWARD_PING_MS      = 50;

// Motion timing (calibrate TURN_90_MS!)
constexpr uint16_t BRAKE_MS    = 250;   // pause between ANY direction change (protects L293D + battery)
constexpr uint16_t SETTLE_MS   = 300;   // let car stop rocking before measuring
constexpr uint16_t REVERSE_MS  = 300;   // back-up after obstacle; 0 = disabled (no rear sensor!)
constexpr uint16_t TURN_90_MS  = 450;   // time to spin ~90 degrees

// Fail-safes
constexpr uint8_t  SENSOR_FAULT_TIMEOUTS = 40;    // consecutive "no echo" pings while driving -> stop (0 = disable)
constexpr uint16_t FAULT_RETRY_MS        = 2000;
constexpr uint8_t  STUCK_EVENTS          = 6;     // this many quick re-blocks in a row -> TRAPPED
constexpr uint16_t STUCK_WINDOW_MS       = 700;   // forward run shorter than this = "quick re-block"
constexpr uint16_t TRAPPED_RETRY_MS      = 5000;

constexpr uint16_t LOG_INTERVAL_MS = 500;

// ======================= TYPES =======================
enum State : uint8_t {
  ST_FORWARD,
  ST_BRAKE,
  ST_REVERSE,
  ST_TURN_LEFT,       // 90 deg left
  ST_MEASURE_LEFT,
  ST_TURN_RIGHT,      // 180 deg right
  ST_MEASURE_RIGHT,
  ST_TURN_LEFT_180,   // go back to the left path
  ST_TURN_BACK,       // 90 deg right (from facing right -> facing back)
  ST_MEASURE_BACK,
  ST_TRAPPED,
  ST_SENSOR_FAULT
};

enum class Dir : int8_t { REV = -1, STOP = 0, FWD = 1 };

struct Reading {
  uint16_t cm;
  bool     timeout;
};

// ======================= GLOBALS =======================
State          state        = ST_BRAKE;
State          brakeNext    = ST_FORWARD;
uint16_t       brakeMs      = BRAKE_MS;
unsigned long  stateStart   = 0;
unsigned long  forwardStart = 0;
unsigned long  lastPing     = 0;
unsigned long  lastLog      = 0;

uint8_t        obstacleHits    = 0;
uint8_t        consecTimeouts  = 0;
uint8_t        quickBlocks     = 0;
uint16_t       leftCm = 0, rightCm = 0, backCm = 0;

// ======================= LOGGING =======================
const __FlashStringHelper* stateName(State s) {
  switch (s) {
    case ST_FORWARD:       return F("FORWARD");
    case ST_BRAKE:         return F("BRAKE");
    case ST_REVERSE:       return F("REVERSE");
    case ST_TURN_LEFT:     return F("TURN_LEFT_90");
    case ST_MEASURE_LEFT:  return F("MEASURE_LEFT");
    case ST_TURN_RIGHT:    return F("TURN_RIGHT_180");
    case ST_MEASURE_RIGHT: return F("MEASURE_RIGHT");
    case ST_TURN_LEFT_180: return F("TURN_LEFT_180");
    case ST_TURN_BACK:     return F("TURN_BACK_90");
    case ST_MEASURE_BACK:  return F("MEASURE_BACK");
    case ST_TRAPPED:       return F("TRAPPED");
    case ST_SENSOR_FAULT:  return F("SENSOR_FAULT");
    default:               return F("?");
  }
}

void logMsg(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  Serial.print(tag);
  Serial.print(' ');
  Serial.println(msg);
}

// ======================= MOTORS =======================
// Write the LOW pin first so both inputs are never HIGH together, even for a microsecond.
static void driveMotor(uint8_t a, uint8_t b, Dir d, bool invert) {
  if (invert && d != Dir::STOP) d = (d == Dir::FWD) ? Dir::REV : Dir::FWD;
  switch (d) {
    case Dir::FWD:  digitalWrite(b, LOW);  digitalWrite(a, HIGH); break;
    case Dir::REV:  digitalWrite(a, LOW);  digitalWrite(b, HIGH); break;
    default:        digitalWrite(a, LOW);  digitalWrite(b, LOW);  break;  // EN high + both LOW = brake
  }
}

void setMotors(Dir left, Dir right) {
  driveMotor(PIN_IN1, PIN_IN2, left,  INVERT_LEFT);
  driveMotor(PIN_IN3, PIN_IN4, right, INVERT_RIGHT);
}

void stopMotors()  { setMotors(Dir::STOP, Dir::STOP); }
void moveForward() { setMotors(Dir::FWD,  Dir::FWD);  }
void moveReverse() { setMotors(Dir::REV,  Dir::REV);  }
void spinLeft()    { setMotors(Dir::REV,  Dir::FWD);  }
void spinRight()   { setMotors(Dir::FWD,  Dir::REV);  }

// ======================= ULTRASONIC =======================
Reading pingOnce() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  const unsigned long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);

  Reading r;
  if (us == 0) {                    // no echo: nothing in range, soft/angled surface, or sensor fault
    r.cm = MAX_CM;
    r.timeout = true;
    if (consecTimeouts < 255) consecTimeouts++;
    return r;
  }
  consecTimeouts = 0;
  uint32_t cm = us / 58UL;
  if (cm < 2) cm = 2;               // below sensor minimum range -> treat as "touching"
  if (cm > MAX_CM) cm = MAX_CM;
  r.cm = (uint16_t)cm;
  r.timeout = false;
  return r;
}

// Median of several pings (only call while the car is stopped - it blocks ~150-300 ms).
uint16_t scanDistance(bool &allTimeout) {
  uint16_t v[SCAN_SAMPLES];
  uint8_t timeouts = 0;
  for (uint8_t i = 0; i < SCAN_SAMPLES; i++) {
    Reading r = pingOnce();
    if (r.timeout) timeouts++;
    // insertion sort as we go
    uint8_t j = i;
    while (j > 0 && v[j - 1] > r.cm) { v[j] = v[j - 1]; j--; }
    v[j] = r.cm;
    delay(SAMPLE_GAP_MS);
  }
  allTimeout = (timeouts == SCAN_SAMPLES);
  return v[SCAN_SAMPLES / 2];
}

// ======================= STATE MACHINE =======================
void enter(State s) {
  if (s != state) {
    Serial.print(F("[STATE] "));
    Serial.print(stateName(state));
    Serial.print(F(" -> "));
    Serial.println(stateName(s));
  }
  state = s;
  stateStart = millis();

  switch (s) {
    case ST_FORWARD:
      obstacleHits = 0;
      consecTimeouts = 0;
      forwardStart = stateStart;
      lastPing = stateStart;
      moveForward();
      break;
    case ST_REVERSE:                          moveReverse(); break;
    case ST_TURN_LEFT:
    case ST_TURN_LEFT_180:                    spinLeft();    break;
    case ST_TURN_RIGHT:
    case ST_TURN_BACK:                        spinRight();   break;
    default:                                  stopMotors();  break;  // BRAKE, MEASURE_*, TRAPPED, FAULT
  }
}

void enterBrake(State next, uint16_t ms) {
  brakeNext = next;
  brakeMs = ms;
  enter(ST_BRAKE);
}

void handleObstacle(uint16_t cm) {
  Serial.print(F("[INFO] Obstacle at "));
  Serial.print(cm);
  Serial.println(F(" cm"));

  // Detect "bouncing around in a tight spot"
  if (millis() - forwardStart < STUCK_WINDOW_MS) quickBlocks++;
  else quickBlocks = 0;

  if (quickBlocks >= STUCK_EVENTS) {
    quickBlocks = 0;
    logMsg(F("[WARN]"), F("Repeated quick blocks - boxed in. Stopping."));
    enter(ST_TRAPPED);
    return;
  }
  enterBrake((REVERSE_MS > 0) ? ST_REVERSE : ST_TURN_LEFT, BRAKE_MS);
}

void runForward(unsigned long now) {
  if (now - lastPing < FORWARD_PING_MS) return;
  lastPing = now;

  Reading r = pingOnce();

  if (SENSOR_FAULT_TIMEOUTS > 0 && consecTimeouts >= SENSOR_FAULT_TIMEOUTS) {
    logMsg(F("[ERROR]"), F("No echo for too long - sensor wiring/power? Stopping."));
    enter(ST_SENSOR_FAULT);
    return;
  }

  if (now - lastLog >= LOG_INTERVAL_MS) {
    lastLog = now;
    Serial.print(F("[INFO] Front: "));
    if (r.timeout) Serial.println(F("no echo"));
    else { Serial.print(r.cm); Serial.println(F(" cm")); }
  }

  if (!r.timeout && r.cm <= OBSTACLE_CM) obstacleHits++;
  else obstacleHits = 0;

  if (obstacleHits >= OBSTACLE_CONFIRMS) handleObstacle(r.cm);
}

void decideAfterScan() {
  Serial.print(F("[INFO] Left: "));  Serial.print(leftCm);
  Serial.print(F(" cm | Right: "));  Serial.print(rightCm);
  Serial.println(F(" cm"));

  const uint16_t best = (leftCm > rightCm) ? leftCm : rightCm;

  if (best < CLEAR_CM) {
    logMsg(F("[WARN]"), F("Left and right both blocked - checking behind"));
    enter(ST_TURN_BACK);
  } else if (rightCm >= leftCm) {
    logMsg(F("[INFO]"), F("Going RIGHT (already facing it)"));
    enterBrake(ST_FORWARD, SETTLE_MS);
  } else {
    logMsg(F("[INFO]"), F("Going LEFT (turning back)"));
    enter(ST_TURN_LEFT_180);
  }
}

void retrySensor() {
  bool allTimeout;
  uint16_t cm = scanDistance(allTimeout);
  if (!allTimeout) {
    Serial.print(F("[RECOVERY] Sensor is back, reading "));
    Serial.print(cm);
    Serial.println(F(" cm"));
    enter(ST_FORWARD);
  } else {
    logMsg(F("[ERROR]"), F("Sensor still silent (or nothing within 4 m). Check Trig/Echo/5V/GND."));
    stateStart = millis();  // wait another FAULT_RETRY_MS
  }
}

// ======================= STATUS LED =======================
void updateLed(unsigned long now) {
  switch (state) {
    case ST_FORWARD:      digitalWrite(PIN_LED, HIGH); break;
    case ST_TRAPPED:      digitalWrite(PIN_LED, (now / 500) & 1); break;
    case ST_SENSOR_FAULT: digitalWrite(PIN_LED, (now / 100) & 1); break;
    default:              digitalWrite(PIN_LED, LOW); break;
  }
}

// ======================= SETUP / LOOP =======================
void motorTest() {
  logMsg(F("[INIT]"), F("Motor test: both FORWARD"));  moveForward(); delay(500); stopMotors(); delay(400);
  logMsg(F("[INIT]"), F("Motor test: both REVERSE"));  moveReverse(); delay(500); stopMotors(); delay(400);
  logMsg(F("[INIT]"), F("Motor test: spin LEFT"));     spinLeft();    delay(500); stopMotors(); delay(400);
  logMsg(F("[INIT]"), F("Motor test: spin RIGHT"));    spinRight();   delay(500); stopMotors(); delay(400);
}

void setup() {
  // Drive outputs LOW *before* making them outputs -> no motor twitch at boot.
  const uint8_t motorPins[] = {PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(motorPins[i], LOW);
    pinMode(motorPins[i], OUTPUT);
  }
  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);

  Serial.begin(115200);
  Serial.println();
  Serial.print(F("[BOOT] 2WD obstacle-avoider v")); Serial.println(F(FW_VERSION));
  Serial.println(F("[BOOT] Nano + L293D (EN on 5V) + HC-SR04, fixed sensor"));
  Serial.print(F("[BOOT] Obstacle<=")); Serial.print(OBSTACLE_CM);
  Serial.print(F("cm  Clear>="));       Serial.print(CLEAR_CM);
  Serial.print(F("cm  Turn90="));       Serial.print(TURN_90_MS);
  Serial.print(F("ms  Reverse="));      Serial.print(REVERSE_MS);
  Serial.println(F("ms"));

  Serial.print(F("[BOOT] Starting in ")); Serial.print(START_DELAY_MS / 1000);
  Serial.println(F(" s - put the car on the floor"));
  delay(START_DELAY_MS);

  if (MOTOR_TEST_ON_BOOT) motorTest();

  // Sensor sanity check before driving blind
  bool allTimeout;
  uint16_t cm = scanDistance(allTimeout);
  if (allTimeout) {
    logMsg(F("[INIT]"), F("Ultrasonic: NO ECHO - not driving until it responds"));
    enter(ST_SENSOR_FAULT);
  } else {
    Serial.print(F("[INIT] Ultrasonic OK, front = "));
    Serial.print(cm);
    Serial.println(F(" cm"));
    enter(ST_FORWARD);
  }
}

void loop() {
  const unsigned long now = millis();
  const unsigned long elapsed = now - stateStart;   // overflow-safe
  updateLed(now);

  bool allTimeout;

  switch (state) {
    case ST_FORWARD:
      runForward(now);
      break;

    case ST_BRAKE:
      if (elapsed >= brakeMs) enter(brakeNext);
      break;

    case ST_REVERSE:
      if (elapsed >= REVERSE_MS) enterBrake(ST_TURN_LEFT, BRAKE_MS);
      break;

    case ST_TURN_LEFT:
      if (elapsed >= TURN_90_MS) enterBrake(ST_MEASURE_LEFT, SETTLE_MS);
      break;

    case ST_MEASURE_LEFT:
      leftCm = scanDistance(allTimeout);
      enter(ST_TURN_RIGHT);
      break;

    case ST_TURN_RIGHT:
      if (elapsed >= 2UL * TURN_90_MS) enterBrake(ST_MEASURE_RIGHT, SETTLE_MS);
      break;

    case ST_MEASURE_RIGHT:
      rightCm = scanDistance(allTimeout);
      decideAfterScan();
      break;

    case ST_TURN_LEFT_180:
      if (elapsed >= 2UL * TURN_90_MS) enterBrake(ST_FORWARD, SETTLE_MS);
      break;

    case ST_TURN_BACK:
      if (elapsed >= TURN_90_MS) enterBrake(ST_MEASURE_BACK, SETTLE_MS);
      break;

    case ST_MEASURE_BACK:
      backCm = scanDistance(allTimeout);
      Serial.print(F("[INFO] Behind: ")); Serial.print(backCm); Serial.println(F(" cm"));
      if (backCm >= CLEAR_CM) {
        logMsg(F("[INFO]"), F("Going BACK the way we came"));
        enterBrake(ST_FORWARD, SETTLE_MS);
      } else {
        logMsg(F("[WARN]"), F("All four directions blocked - TRAPPED, waiting"));
        enter(ST_TRAPPED);
      }
      break;

    case ST_TRAPPED:
      if (elapsed >= TRAPPED_RETRY_MS) {
        logMsg(F("[RECOVERY]"), F("Retrying"));
        enter(ST_FORWARD);   // forward logic will see the obstacle and rescan if still blocked
      }
      break;

    case ST_SENSOR_FAULT:
      if (elapsed >= FAULT_RETRY_MS) retrySensor();
      break;
  }
}
