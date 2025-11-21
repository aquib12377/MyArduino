/*
   SPM Cycle Controller – Arduino MEGA
   -----------------------------------
   - One MEGA handles:
       * 7 solenoid relays for cylinders: H1, H2, L1, T1, E1, X1, X2
       * 1 status relay (green/red LED)
       * 14 limit switches (2 per cylinder): *_OFF (extend end), *_READY (retract end)
       * 1 stepper via DM1182 (STEP/DIR/ENA)
       * START and STOP buttons

   Motion sequence (one cycle):
     1) H1, H2 extend      → wait H1_OFF & H2_OFF
     2) L1 extend          → wait L1_OFF
     3) L1 retract         → wait L1_READY
     4) H1, H2 retract     → wait H1_READY & H2_READY
     5) T1 extend          → wait T1_OFF
     6) E1 extend          → wait E1_OFF
     7) Stepper 360° CW
     8) E1 retract         → wait E1_READY
     9) T1 retract         → wait T1_READY
    10) X1, X2 extend      → wait X1_OFF & X2_OFF
    11) X1, X2 retract     → wait X1_READY & X2_READY

   - This cycle repeats automatically until STOP is pressed.
   - STOP: graceful stop → all relays OFF, wait until all *_READY → go IDLE.
   - There is NO ESTOP in this version.
   - Stepper is NOT homed; no “initial angle” is enforced.
*/

#include <Arduino.h>

// =====================================================
//  USER CONFIG – PINS
// =====================================================

// ---------- Relays for cylinders ----------
const uint8_t H1_PIN = 4;
const uint8_t H2_PIN = 5;
const uint8_t L1_PIN = 6;
const uint8_t T1_PIN = 7;
const uint8_t E1_PIN = 8;
const uint8_t X1_PIN = 9;
const uint8_t X2_PIN = 10;

// ---------- Status relay (green/red LED) ----------
// Change this to whatever relay channel on your shield drives the tower light.
const uint8_t STATUS_RELAY_PIN = 3;  // example: D3, adjust to your shield

// Set to true if your relay boards are ACTIVE-HIGH, false if ACTIVE-LOW.
const bool RELAY_ACTIVE_HIGH = false;

// ---------- Stepper driver DM1182 ----------
const uint8_t STEP_PIN = 11;
const uint8_t DIR_PIN  = 12;
const uint8_t EN_PIN   = 13;   // set 255 if you don’t use ENA

// ---------- Buttons (INPUT_PULLUP, pressed = LOW) ----------
const uint8_t START_BTN = 46;  // Start / keep running
const uint8_t STOP_BTN  = 48;  // Graceful stop

// ---------- Limit switches (14 total) ----------
// Each LS is wired: LS_PIN → switch → GND, with INPUT_PULLUP.
// ACTIVE (1) = switch closed to GND (LOW).
//
// Index | Bit | Meaning
//   0   |  0  | H1_OFF
//   1   |  1  | H1_READY
//   2   |  2  | H2_OFF
//   3   |  3  | H2_READY
//   4   |  4  | L1_OFF
//   5   |  5  | L1_READY
//   6   |  6  | T1_OFF
//   7   |  7  | T1_READY
//   8   |  8  | E1_OFF
//   9   |  9  | E1_READY
//  10   | 10  | X1_OFF
//  11   | 11  | X1_READY
//  12   | 12  | X2_OFF
//  13   | 13  | X2_READY
//
// Already mapped to your analog pins on the Mega shield:
const uint8_t LS_PINS[14] = {
  35, 34, 32, 33,   // 0..3  H1_OFF, H1_READY, H2_OFF, H2_READY
  42,  43,  41,  40,    // 4..7  L1_OFF, L1_READY, T1_OFF, T1_READY
  30,  31,  38,  39,    // 8..11 E1_OFF, E1_READY, X1_OFF, X1_READY
  36,37 
};

// Enum to make LS() calls readable
enum LsIndex : uint8_t {
  H1_OFF = 0, H1_READY,
  H2_OFF,     H2_READY,
  L1_OFF,     L1_READY,
  T1_OFF,     T1_READY,
  E1_OFF,     E1_READY,
  X1_OFF,     X1_READY,
  X2_OFF,     X2_READY
};

// Names for debugging (Serial prints)
const char* LS_NAMES[14] = {
  "H1_OFF","H1_READY","H2_OFF","H2_READY",
  "L1_OFF","L1_READY","T1_OFF","T1_READY",
  "E1_OFF","E1_READY","X1_OFF","X1_READY",
  "X2_OFF","X2_READY"
};

// =====================================================
//  MOTION / STEP CONFIG
// =====================================================
const int  MICROSTEPS      = 16;        // must match DM1182 DIP
const int  FULL_STEPS_REV  = 200;       // 1.8° motor
const long STEPS_PER_REV   = (long)FULL_STEPS_REV * MICROSTEPS;
const int  pulseDelay      = 250;       // µs between step edges (speed)

// Small delay after E1 fully-extended before rotating (not a retract timeout).
const uint16_t TIME_E1_EXT = 300;       // ms (set 0 if not needed)

// =====================================================
//  LIMIT SWITCH DEBOUNCE
// =====================================================

const uint8_t  DEBOUNCE_MS = 8;

// Debounced LS state (1 = ACTIVE)
uint16_t lsStableBits     = 0;
// Last raw sample
uint16_t lsSampleBits     = 0;
// Per-LS time when sample last changed
unsigned long lsLastChangeMs[14];

// Debug helpers (optional)
uint16_t lsLastPrinted = 0;
unsigned long lsLastPrintMs = 0;

inline bool LS(uint8_t idx) {
  return (lsStableBits >> idx) & 0x1;
}

// Read all LS pins into a 16-bit bitmap (raw)
uint16_t readRawLS() {
  uint16_t raw = 0;
  for (uint8_t i = 0; i < 14; i++) {
    // INPUT_PULLUP: LOW means switch closed → ACTIVE
    if (digitalRead(LS_PINS[i]) == LOW) {
      raw |= (1u << i);
    }
  }
  return raw;
}

// Debounce all limit switches
void updateLimitSwitches() {
  uint16_t raw = readRawLS();
  unsigned long now = millis();

  for (uint8_t i = 0; i < 14; i++) {
    bool prevSample = (lsSampleBits >> i) & 1;
    bool newSample  = (raw           >> i) & 1;

    if (prevSample != newSample) {
      // sample changed → reset debounce timer and update sample bit
      lsSampleBits ^= (1u << i);
      lsLastChangeMs[i] = now;
    } else {
      // sample stable: if stable long enough, commit to lsStableBits
      if ((now - lsLastChangeMs[i]) >= DEBOUNCE_MS) {
        bool prevStable = (lsStableBits >> i) & 1;
        if (prevStable != newSample) {
          // actual debounced change
          if (newSample) lsStableBits |=  (1u << i);
          else           lsStableBits &= ~(1u << i);

          // Debug: print change
          Serial.print(F("[LS-CHG] "));
          Serial.print(LS_NAMES[i]);
          Serial.print(F(" -> "));
          Serial.println(newSample ? F("ACTIVE") : F("idle"));
        }
      }
    }
  }

  // Periodic compact summary
  if (millis() - lsLastPrintMs >= 500) {
    lsLastPrintMs = millis();
    if (lsStableBits != lsLastPrinted) {
      lsLastPrinted = lsStableBits;
      Serial.print(F("[LS] 0x"));
      Serial.print(lsStableBits, HEX);
      Serial.print(F("  bits: "));
      for (int i = 13; i >= 0; --i) {
        Serial.print((lsStableBits >> i) & 1);
      }
      Serial.println();
    }
  }
}

// =====================================================
//  RELAY & STEPPER HELPERS
// =====================================================

inline void writeRelayPin(uint8_t pin, bool on) {
  if (RELAY_ACTIVE_HIGH) digitalWrite(pin, on ? HIGH : LOW);
  else                   digitalWrite(pin, on ? LOW  : HIGH);
}

// For cylinders: "extend" = energize valve
inline void setRelay(uint8_t pin, bool extend) {
  writeRelayPin(pin, extend);
}

// For status relay: "on" = machine running (green), "off" = stopped (red)
inline void setStatusRelay(bool on) {
  writeRelayPin(STATUS_RELAY_PIN, on);
}

inline void allRetractSafe() {
  setRelay(H1_PIN, false);
  setRelay(H2_PIN, false);
  setRelay(L1_PIN, false);
  setRelay(T1_PIN, false);
  setRelay(E1_PIN, false);
  setRelay(X1_PIN, false);
  setRelay(X2_PIN, false);
}

inline bool pressed(uint8_t pin) { return digitalRead(pin) == LOW; }

// Stepper / STOP integration
volatile bool stopRequested = false;

bool rotateAngle(float angleDeg, bool cw) {
  long steps = (long)((angleDeg / 360.0f) * (float)STEPS_PER_REV);
  if (steps <= 0) return true;

  digitalWrite(DIR_PIN, cw ? HIGH : LOW);
  delay(10);  // allow DIR settle

  for (long i = 0; i < steps; i++) {
    if (stopRequested) {
      // Abort rotation immediately on STOP
      Serial.println(F("[ROT] Aborted by STOP"));
      return false;
    }
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(pulseDelay);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(pulseDelay);
  }
  return true;
}

bool rotateOneRevCW() {
  return rotateAngle(183.2f, true);
}

// =====================================================
//  STATE MACHINE
// =====================================================

enum StepState : uint8_t {
  IDLE = 0,
  S1_EXTEND_H1H2,
  S2_EXTEND_L1,
  S3_RETRACT_L1,
  S4_RETRACT_H1H2,
  S5_EXTEND_T1,
  S6_EXTEND_E1,
  S7_ROTATE,
  S8_RETRACT_E1,
  S9_RETRACT_T1,
  S10_EXTEND_X12,
  S11_RETRACT_X12,
  GS_RETRACT_ALL    // graceful stop: wait until all *_READY
};

StepState state = IDLE;
bool running = false;   // when true, cycle auto-repeats until STOP

void requestStart() {
  if (state == IDLE) {
    running = true;
    stopRequested = false;

    // Turn on status relay (e.g. green lamp)
    setStatusRelay(true);

    // Begin from step 1
    setRelay(H1_PIN, true);
    setRelay(H2_PIN, true);
    state = S1_EXTEND_H1H2;
    Serial.println(F("[S1] H1,H2 -> EXTEND (wait H1_OFF & H2_OFF)"));
  }
}

void requestStop() {
  // Graceful stop: stop auto looping, retract all,
  // and go IDLE once all *_READY are seen.
  running = false;
  stopRequested = true;

  // Turn OFF status relay (e.g. red lamp ON, green OFF)
  setStatusRelay(false);

  // Remove power from all valves (assume springs/air retract them)
  allRetractSafe();
  state = GS_RETRACT_ALL;
  Serial.println(F("[STOP] Requested → all relays OFF, waiting all *_READY → IDLE"));
}

// =====================================================
//  SETUP & LOOP
// =====================================================

void setup() {
  // Relays
  pinMode(H1_PIN, OUTPUT);
  pinMode(H2_PIN, OUTPUT);
  pinMode(L1_PIN, OUTPUT);
  pinMode(T1_PIN, OUTPUT);
  pinMode(E1_PIN, OUTPUT);
  pinMode(X1_PIN, OUTPUT);
  pinMode(X2_PIN, OUTPUT);
//   digitalWrite(H1_PIN, LOW);delay(1000);digitalWrite(H1_PIN, HIGH);
// digitalWrite(H2_PIN, LOW);delay(1000);digitalWrite(H2_PIN, HIGH);
// digitalWrite(L1_PIN, LOW);delay(1000);digitalWrite(L1_PIN, HIGH);
// digitalWrite(T1_PIN, LOW);delay(1000);digitalWrite(T1_PIN, HIGH);
// digitalWrite(E1_PIN, LOW);delay(1000);digitalWrite(E1_PIN, HIGH);
// digitalWrite(X1_PIN, LOW);delay(1000);digitalWrite(X1_PIN, HIGH);
// digitalWrite(X2_PIN, LOW);delay(1000);digitalWrite(X2_PIN, HIGH);
  // Status relay
  pinMode(STATUS_RELAY_PIN, OUTPUT);
  setStatusRelay(false);   // default: machine stopped → LOW

  // Make sure all cylinders are off initially
  allRetractSafe();

  // Stepper
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN,  OUTPUT);
  if (EN_PIN != 255) {
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW); // enable driver (adjust if your DM1182 is opposite)
  }

  // Buttons
  pinMode(START_BTN, INPUT_PULLUP);
  pinMode(STOP_BTN,  INPUT_PULLUP);

  // Limit switches
  for (uint8_t i = 0; i < 14; i++) {
    pinMode(LS_PINS[i], INPUT_PULLUP);
    lsLastChangeMs[i] = millis();
  }

  Serial.begin(115200);
  Serial.println(F("\nSPM Controller – Arduino Mega (single-board)."));
  Serial.println(F("START: begin/restart cycle (status relay ON)"));
  Serial.println(F("STOP:  graceful stop (retract all, status relay OFF, then IDLE)\n"));
}

void loop() {
  // Update LS debounced state
  updateLimitSwitches();

  // Buttons
  if (pressed(STOP_BTN) && state != IDLE && state != GS_RETRACT_ALL) {
    requestStop();
  }
  if (pressed(START_BTN)) {
    if (state == IDLE) {
      requestStart();
    }
  }

  // State machine
  switch (state) {
    case IDLE:
      // Wait for START (requestStart sets H1,H2 relays ON and state = S1)
      break;

    // --------------------------------------------------
    // 1) H1 & H2 EXTEND → wait H1_OFF & H2_OFF
    // --------------------------------------------------
    case S1_EXTEND_H1H2: {
      // H1_PIN and H2_PIN were set TRUE when we entered the cycle.
      // Do NOT turn them off here; we only watch the OFF LS.
      if (LS(H1_OFF) && LS(H2_OFF)) {
        // Both fully extended; now start L1 extend.
        setRelay(L1_PIN, true);
        state = S2_EXTEND_L1;
        Serial.println(F("[S2] L1 -> EXTEND (wait L1_OFF)"));
      }
      break;
    }

    // --------------------------------------------------
    // 2) L1 EXTEND → wait L1_OFF
    // --------------------------------------------------
    case S2_EXTEND_L1:
      if (LS(L1_OFF)) {
        // Stop extending L1 and let it retract (spring/air).
        setRelay(L1_PIN, false);
        state = S3_RETRACT_L1;
        Serial.println(F("[S3] L1 -> RETRACT (wait L1_READY)"));
      }
      break;

    // --------------------------------------------------
    // 3) L1 RETRACT → wait L1_READY
    // --------------------------------------------------
    case S3_RETRACT_L1:
      if (LS(L1_READY)) {
        // L1 fully retracted; now retract H1 & H2.
        state = S4_RETRACT_H1H2;
        Serial.println(F("[S4] H1,H2 -> RETRACT (wait H1_READY & H2_READY)"));
      }
      break;

    // --------------------------------------------------
    // 4) H1 & H2 RETRACT → wait H1_READY & H2_READY
    // --------------------------------------------------
    case S4_RETRACT_H1H2:
      // Now we actually command retract: turn off H1 & H2 valves.
      setRelay(H1_PIN, false);
      setRelay(H2_PIN, false);

      if (LS(H1_READY) && LS(H2_READY)) {
        // H1 & H2 fully retracted → extend T1.
        setRelay(T1_PIN, true);
        state = S5_EXTEND_T1;
        Serial.println(F("[S5] T1 -> EXTEND (wait T1_OFF)"));
      }
      break;

    // --------------------------------------------------
    // 5) T1 EXTEND → wait T1_OFF
    // --------------------------------------------------
    case S5_EXTEND_T1:
      // T1 stays extended (relay ON) while we wait for T1_OFF.
      if (LS(T1_OFF)) {
        // Do NOT turn T1 off here – it must remain extended until after E1 + rotation.
        // Move to E1 extend.
        setRelay(E1_PIN, true);
        state = S6_EXTEND_E1;
        Serial.println(F("[S6] E1 -> EXTEND (wait E1_OFF)"));
      }
      break;

    // --------------------------------------------------
    // 6) E1 EXTEND → wait E1_OFF
    // --------------------------------------------------
    case S6_EXTEND_E1:
      // E1 stays extended (relay ON) while we wait for E1_OFF.
      if (LS(E1_OFF)) {
        // E1 fully extended. Keep E1 & T1 extended; now rotate.
        if (TIME_E1_EXT) delay(TIME_E1_EXT);
        state = S7_ROTATE;
        Serial.print(F("[S7] Stepper rotate 360° ("));
        Serial.print(STEPS_PER_REV);
        Serial.println(F(" steps)"));
      }
      break;

    // --------------------------------------------------
    // 7) STEPPER ROTATE 360°
    // --------------------------------------------------
    case S7_ROTATE: {
      bool ok = rotateOneRevCW();
      if (!ok) {
        // If aborted by STOP, higher-level logic moves to GS_RETRACT_ALL.
        if (stopRequested) {
          state = GS_RETRACT_ALL;
        }
        break;
      }
      // After rotation, keep T1 extended; now retract only E1.
      setRelay(E1_PIN, false);
      state = S8_RETRACT_E1;
      Serial.println(F("[S8] E1 -> RETRACT (wait E1_READY)"));
      break;
    }

    // --------------------------------------------------
    // 8) E1 RETRACT → wait E1_READY
    // --------------------------------------------------
    case S8_RETRACT_E1:
      if (LS(E1_READY)) {
        // E1 fully retracted; now retract T1.
        setRelay(T1_PIN, false);
        state = S9_RETRACT_T1;
        Serial.println(F("[S9] T1 -> RETRACT (wait T1_READY)"));
      }
      break;

    // --------------------------------------------------
    // 9) T1 RETRACT → wait T1_READY
    // --------------------------------------------------
    case S9_RETRACT_T1:
      if (LS(T1_READY)) {
        // T1 fully retracted; now extend ejectors.
        setRelay(X1_PIN, true);
        setRelay(X2_PIN, true);
        state = S10_EXTEND_X12;
        Serial.println(F("[S10] X1,X2 -> EXTEND (wait X1_OFF & X2_OFF)"));
      }
      break;

    // --------------------------------------------------
    // 10) X1 & X2 EXTEND → wait X1_OFF & X2_OFF
    // --------------------------------------------------
    case S10_EXTEND_X12: {
      if (LS(X1_OFF) && LS(X2_OFF)) {
        // Both fully extended; now retract them.
        setRelay(X1_PIN, false);
        setRelay(X2_PIN, false);
        state = S11_RETRACT_X12;
        Serial.println(F("[S11] X1,X2 -> RETRACT (wait X1_READY & X2_READY)"));
      }
      break;
    }

    // --------------------------------------------------
    // 11) X1 & X2 RETRACT → wait X1_READY & X2_READY → loop or stop
    // --------------------------------------------------
    
    case S11_RETRACT_X12:
      if (LS(X1_READY) && LS(X2_READY)) {
        Serial.println(F("[DONE] Cycle complete."));
        if (running && !stopRequested) {
          // Loop again from S1
          setRelay(H1_PIN, true);
          setRelay(H2_PIN, true);
          state = S1_EXTEND_H1H2;
          Serial.println(F("[LOOP] Restarting cycle: H1,H2 -> EXTEND"));
        } else if (stopRequested) {
          // Ensure everything is retracted and go IDLE
          state = GS_RETRACT_ALL;
          Serial.println(F("[STOP] Finishing with retract-all → IDLE."));
        } else {
          state = IDLE;
          setStatusRelay(false); // extra safety: ensure status relay OFF when idle
        }
      }
      break;

    // --------------------------------------------------
    // Graceful stop: all valves OFF, wait until all *_READY
    // --------------------------------------------------
    case GS_RETRACT_ALL: {
      allRetractSafe();  // all relays OFF

      bool allReady =
        LS(H1_READY) && LS(H2_READY) &&
        LS(L1_READY) &&
        LS(T1_READY) &&
        LS(E1_READY) &&
        LS(X1_READY) && LS(X2_READY);

      if (allReady) {
        stopRequested = false;
        state = IDLE;
        setStatusRelay(false);
        Serial.println(F("[STOP] All cylinders retracted. State -> IDLE. Ready for fresh START."));
      }
      break;
    }
  }
}
