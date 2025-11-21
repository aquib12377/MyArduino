/*
  UNO-MASTER: LS-driven, repeats until STOP/ESTOP.
  STOP (A2) = graceful: turn all relays OFF, wait till all *_READY, go IDLE.
  START begins from S1 only after everything is retracted. Stepper is NOT homed.
*/

#include <Arduino.h>
#include <Wire.h>
#define DEBUG_LS 1

const char* LS_NAMES[14] = {
  "H1_OFF","H1_READY","H2_OFF","H2_READY",
  "L1_OFF","L1_READY","T1_OFF","T1_READY",
  "E1_OFF","E1_READY","X1_OFF","X1_READY",
  "X2_OFF","X2_READY"
};

uint16_t last_ls_bitmap = 0;
unsigned long last_dbg_ms = 0;
// -------------------- USER CONFIG --------------------
const uint8_t H1_PIN = 4;
const uint8_t H2_PIN = 5;
const uint8_t L1_PIN = 6;
const uint8_t T1_PIN = 7;
const uint8_t E1_PIN = 8;
const uint8_t X1_PIN = 9;
const uint8_t X2_PIN = 10;

const bool RELAY_ACTIVE_HIGH = false; // false = active-LOW relay boards

// Stepper pins (DM1182)
const uint8_t STEP_PIN = 11;
const uint8_t DIR_PIN  = 12;
const uint8_t EN_PIN   = 13;   // set 255 if unused

// Buttons
const uint8_t START_BTN = A0;   // pressed = LOW
const uint8_t ESTOP_BTN = A1;   // pressed = LOW (emergency)
const uint8_t STOP_BTN  = A2;   // pressed = LOW (graceful stop)

// Motion
const int  MICROSTEPS      = 16;
const int  FULL_STEPS_REV  = 200;
const long STEPS_PER_REV   = (long)FULL_STEPS_REV * MICROSTEPS;
const int  pulseDelay      = 250;  // µs
const uint16_t TIME_E1_EXT = 300;  // ms settle before spin (not a retract timeout)

// ----------------- I2C LS SLAVE -----------------
const uint8_t LS_I2C_ADDR = 0x42;

enum LsIndex : uint8_t {
  H1_OFF = 0, H1_READY,
  H2_OFF,     H2_READY,
  L1_OFF,     L1_READY,
  T1_OFF,     T1_READY,
  E1_OFF,     E1_READY,
  X1_OFF,     X1_READY,
  X2_OFF,     X2_READY
};

uint16_t ls_bitmap = 0;

bool i2cReadLS(uint16_t &out) {
  Wire.requestFrom((int)LS_I2C_ADDR, 2);  // stop=true by default
  if (Wire.available() < 2) {
    return false;
  }
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  out = (uint16_t)lo | ((uint16_t)hi << 8);
  return true;
}
inline bool LS(uint8_t idx) { return (ls_bitmap >> idx) & 0x1; }

// ----------------- Helpers -----------------
inline void setRelay(uint8_t pin, bool extend) {
  if (RELAY_ACTIVE_HIGH) digitalWrite(pin, extend ? HIGH : LOW);
  else                   digitalWrite(pin, extend ? LOW  : HIGH);
}
inline void allRetractSafe() {
  setRelay(H1_PIN, false); setRelay(H2_PIN, false); setRelay(L1_PIN, false);
  setRelay(T1_PIN, false); setRelay(E1_PIN, false); setRelay(X1_PIN, false); setRelay(X2_PIN, false);
}
inline bool pressed(uint8_t pin) { return digitalRead(pin) == LOW; }
inline bool estopPressed()       { return pressed(ESTOP_BTN); }

// ----------------- Stepper -----------------
volatile bool stopRequested = false;

bool rotateAngle(float angleDeg, bool cw) {
  long steps = (long)((angleDeg / 360.0f) * (float)STEPS_PER_REV);
  if (steps <= 0) return true;
  digitalWrite(DIR_PIN, cw ? HIGH : LOW);
  delay(10);
  for (long i = 0; i < steps; i++) {
    if (estopPressed() || stopRequested) return false;  // abort spin on ESTOP/STOP
    digitalWrite(STEP_PIN, HIGH); delayMicroseconds(pulseDelay);
    digitalWrite(STEP_PIN, LOW);  delayMicroseconds(pulseDelay);
  }
  return true;
}
bool rotateOneRevCW() { return rotateAngle(360.0f, true); }

// ----------------- States -----------------
enum StepState : uint8_t {
  IDLE = 0,
  S1_EXTEND_H1H2, S2_EXTEND_L1, S3_RETRACT_L1, S4_RETRACT_H1H2,
  S5_EXTEND_T1, S6_EXTEND_E1, S7_ROTATE, S8_RETRACT_E1, S9_RETRACT_T1,
  S10_EXTEND_X12, S11_RETRACT_X12,
  GS_RETRACT_ALL,        // graceful stop: retract everything, wait *_READY
  ESTOP_STATE
};

StepState state = IDLE;
bool running = false;   // auto-loop when true

void requestStart() {
  if (state == IDLE) {
    running = true;
    stopRequested = false;
    setRelay(H1_PIN, true);
    setRelay(H2_PIN, true);
    state = S1_EXTEND_H1H2;
    Serial.println(F("[S1] H1,H2 -> EXTEND (wait H1_OFF & H2_OFF)"));
  }
}

void requestStop() {
  // Graceful: stop auto-loop, abort current action, retract all, wait READY, then IDLE
  running = false;
  stopRequested = true;           // causes rotate to exit if spinning
  allRetractSafe();               // de-energize all (spring-return retract)
  state = GS_RETRACT_ALL;
  Serial.println(F("[STOP] Requested. Retracting all → wait all *_READY, then IDLE."));
}

void goESTOP() {
  running = false;
  stopRequested = true;
  state = ESTOP_STATE;
  Serial.println(F("EMERGENCY STOP!"));
  allRetractSafe();
  if (EN_PIN != 255) digitalWrite(EN_PIN, HIGH);
}

void setup() {
  pinMode(H1_PIN, OUTPUT); pinMode(H2_PIN, OUTPUT); pinMode(L1_PIN, OUTPUT);
  pinMode(T1_PIN, OUTPUT); pinMode(E1_PIN, OUTPUT); pinMode(X1_PIN, OUTPUT); pinMode(X2_PIN, OUTPUT);
  allRetractSafe();
Serial.begin(115200);
  Serial.println(F("SPM Controller (LS-driven). START=repeats, STOP=graceful retract to IDLE."));
  pinMode(STEP_PIN, OUTPUT); pinMode(DIR_PIN, OUTPUT);
  if (EN_PIN != 255) { pinMode(EN_PIN, OUTPUT); digitalWrite(EN_PIN, LOW); }

  pinMode(START_BTN, INPUT_PULLUP);
  pinMode(ESTOP_BTN, INPUT_PULLUP);
  pinMode(STOP_BTN,  INPUT_PULLUP);

  Wire.begin(); // I2C master
  Wire.setClock(100000);
  Wire.requestFrom(0x42, 2);
  if (Wire.available()==2){
    uint16_t v = Wire.read() | (Wire.read()<<8);
    Serial.print("0x"); Serial.println(v, HEX);
  } else {
    Serial.println("I2C read fail");
  }
  delay(200);
  
}

void loop() {
  if (pressed(ESTOP_BTN)) { goESTOP(); }
  if (pressed(STOP_BTN))  { requestStop(); }
  if (pressed(START_BTN)) { requestStart(); }

  i2cReadLS(ls_bitmap);
#if DEBUG_LS
  static bool first = true;
  if (millis() - last_dbg_ms >= 100) {
    last_dbg_ms = millis();
    Serial.print(F("[LS] 0x")); Serial.print(ls_bitmap, HEX); Serial.print(F("    "));
    for (int i = 13; i >= 0; --i) Serial.print( ((ls_bitmap>>i)&1) ? '1' : '0' );
    Serial.println();
  }
  if (first || ls_bitmap != last_ls_bitmap) {
    uint16_t diff = ls_bitmap ^ last_ls_bitmap;
    for (uint8_t i = 0; i < 14; i++) {
      if (diff & (1u << i)) {
        bool now = (ls_bitmap >> i) & 1;
        Serial.print(F("[LS-CHG] "));
        Serial.print(LS_NAMES[i]);
        Serial.print(F(" -> "));
        Serial.println(now ? F("ACTIVE") : F("idle"));
      }
    }
    last_ls_bitmap = ls_bitmap;
    first = false;
  }
#endif
  switch (state) {
    case IDLE:
      // wait for START
      break;

    case S1_EXTEND_H1H2: {
      if (LS(H1_OFF)) setRelay(H1_PIN, false);
      if (LS(H2_OFF)) setRelay(H2_PIN, false);
      if (LS(H1_OFF) && LS(H2_OFF)) {
        setRelay(L1_PIN, true);
        state = S2_EXTEND_L1;
        Serial.println(F("[S2] L1 -> EXTEND (wait L1_OFF)"));
      }
      break;
    }

    case S2_EXTEND_L1:
      if (LS(L1_OFF)) {
        setRelay(L1_PIN, false);
        state = S3_RETRACT_L1;
        Serial.println(F("[S3] L1 -> RETRACT (wait L1_READY)"));
      }
      break;

    case S3_RETRACT_L1:
      if (LS(L1_READY)) {
        state = S4_RETRACT_H1H2;
        Serial.println(F("[S4] H1,H2 -> RETRACT (wait H1_READY & H2_READY)"));
      }
      break;

    case S4_RETRACT_H1H2:
      if (LS(H1_READY) && LS(H2_READY)) {
        setRelay(T1_PIN, true);
        state = S5_EXTEND_T1;
        Serial.println(F("[S5] T1 -> EXTEND (wait T1_OFF)"));
      }
      break;

    case S5_EXTEND_T1:
      if (LS(T1_OFF)) {
        setRelay(T1_PIN, false);
        setRelay(E1_PIN, true);
        state = S6_EXTEND_E1;
        Serial.println(F("[S6] E1 -> EXTEND (wait E1_OFF)"));
      }
      break;

    case S6_EXTEND_E1:
      if (LS(E1_OFF)) {
        setRelay(E1_PIN, false);
        if (TIME_E1_EXT) delay(TIME_E1_EXT);
        state = S7_ROTATE;
        Serial.print(F("[S7] Stepper rotate 360° (")); Serial.print(STEPS_PER_REV); Serial.println(F(" steps)"));
      }
      break;

    case S7_ROTATE: {
      bool ok = rotateOneRevCW();
      if (!ok || estopPressed()) { if (state != ESTOP_STATE) requestStop(); break; }
      state = S8_RETRACT_E1;
      Serial.println(F("[S8] E1 -> RETRACT (wait E1_READY)"));
      break;
    }

    case S8_RETRACT_E1:
      if (LS(E1_READY)) {
        state = S9_RETRACT_T1;
        Serial.println(F("[S9] T1 -> RETRACT (wait T1_READY)"));
      }
      break;

    case S9_RETRACT_T1:
      if (LS(T1_READY)) {
        setRelay(X1_PIN, true);
        setRelay(X2_PIN, true);
        state = S10_EXTEND_X12;
        Serial.println(F("[S10] X1,X2 -> EXTEND (wait X1_OFF & X2_OFF)"));
      }
      break;

    case S10_EXTEND_X12: {
      if (LS(X1_OFF)) setRelay(X1_PIN, false);
      if (LS(X2_OFF)) setRelay(X2_PIN, false);
      if (LS(X1_OFF) && LS(X2_OFF)) {
        state = S11_RETRACT_X12;
        Serial.println(F("[S11] X1,X2 -> RETRACT (wait X1_READY & X2_READY)"));
      }
      break;
    }

    case S11_RETRACT_X12:
      if (LS(X1_READY) && LS(X2_READY)) {
        Serial.println(F("[DONE] Cycle complete."));
        if (running && !stopRequested) {
          // Restart automatically
          setRelay(H1_PIN, true);
          setRelay(H2_PIN, true);
          state = S1_EXTEND_H1H2;
          Serial.println(F("[LOOP] Restarting cycle: H1,H2 -> EXTEND"));
        } else {
          // If STOP was pressed near the end, go retract-all to be safe
          if (stopRequested) { state = GS_RETRACT_ALL; Serial.println(F("[STOP] Finishing with retract-all.")); }
          else state = IDLE;
        }
      }
      break;

    case GS_RETRACT_ALL: {
      // Make sure everything is de-energized; wait until ALL retracted ends are reached
      allRetractSafe();
      bool allReady =
        LS(H1_READY) && LS(H2_READY) &&
        LS(L1_READY) &&
        LS(T1_READY) &&
        LS(E1_READY) &&
        LS(X1_READY) && LS(X2_READY);

      if (allReady) {
        stopRequested = false;
        state = IDLE;
        Serial.println(F("[STOP] All cylinders retracted. State -> IDLE. Ready for fresh START."));
      }
      break;
    }

    case ESTOP_STATE:
      // Remain here until ESTOP released and START pressed -> resume from S1
      if (!estopPressed() && pressed(START_BTN)) {
        if (EN_PIN != 255) digitalWrite(EN_PIN, LOW);
        stopRequested = false;
        allRetractSafe();
        running = true;
        setRelay(H1_PIN, true);
        setRelay(H2_PIN, true);
        state = S1_EXTEND_H1H2;
        Serial.println(F("[RECOVER] ESTOP cleared -> restarting at S1"));
      }
      break;
  }
}
