/*
  ============================================================
  2WD Fire-Fighting Bot  —  v2 (Clean Rewrite)
  ============================================================
  FIXES vs v1:
  ① Servo hiccup: original used analogWrite(9/10) which shares
    Timer1 with the Servo library on Uno → corrupted servo pulses.
    ENA/ENB are now hard-wired HIGH (digitalWRITE), and speed is
    controlled by software PWM on the IN pins instead.
  ② New state machine: aligns to center sensor before spraying.
  ③ Software PWM: smooth speed control without jerking.

  FLOW:
  • Flame on LEFT   → turn  left  until CENTER sensor fires
                    → stop, pump ON, servo sweeps 45-135°
  • Flame on RIGHT  → turn right  until CENTER sensor fires
                    → stop, pump ON, servo sweeps 45-135°
  • Flame on CENTER → drive forward 1 s
                    → stop, pump ON, servo sweeps 45-135°
  • Flame gone (past min-on & clear delay) → pump OFF,
    servo returns to 90°, back to PATROL
  ============================================================
*/

#include <Servo.h>

// =====================================================================
//  PIN MAP
// =====================================================================
const int FLAME_L = A1;   // Left   flame sensor  (ACTIVE-LOW)
const int FLAME_C = A0;   // Center flame sensor  (ACTIVE-LOW)
const int FLAME_R = A2;   // Right  flame sensor  (ACTIVE-LOW)

const int RELAY   = 12;   // Water pump relay     (ACTIVE-LOW)

const int IN1 = 5;        // Left  motor A
const int IN2 = 6;        // Left  motor B
const int IN3 = 7;        // Right motor A
const int IN4 = 8;        // Right motor B

// ENA/ENB are wired permanently HIGH (no PWM — see setup()).
// Speed is controlled purely via software PWM on the IN pins.
const int ENA = 9;
const int ENB = 10;

// IMPORTANT: Servo must NOT share a timer with ENA/ENB.
// On Uno, Servo library uses Timer1 (pins 9 & 10).
// Pin 11 uses Timer2 — safe to use with the Servo library.
const int SERVO_PIN = 11;

// =====================================================================
//  TUNING CONSTANTS
// =====================================================================

// --- Servo ---
const int   SERVO_MIN    = 45;          // leftmost spray angle
const int   SERVO_MAX    = 135;         // rightmost spray angle
const int   SERVO_CENTER = 90;          // parked / patrol angle
const int   SERVO_STEP   = 1;           // degrees per step (1 = smoothest)
const unsigned long SERVO_STEP_MS = 12; // ms between steps  (~90° in ~1 s)

// --- Flame debounce ---
const unsigned long FLAME_DEBOUNCE_MS = 60;   // ms signal must stay LOW

// --- Approach ---
const unsigned long APPROACH_FWD_MS = 1000;   // forward time on center flame
// --- Software PWM (motor speed without analogWrite) ---
const unsigned long PWM_ON_MS   = 18;   // normal speed  (~72% duty)
const unsigned long PWM_OFF_MS  = 7;

// Reduced speed used while aligning / approaching a flame
const unsigned long PWM_SLOW_ON_MS  = 10;  // ~40% duty — lower = slower
const unsigned long PWM_SLOW_OFF_MS = 15;
// --- Pump ---
const unsigned long PUMP_MIN_ON_MS  = 2000;   // minimum spray time
const unsigned long FLAME_CLEAR_MS  = 800;    // flame-free time to stop pump

// --- Software PWM (motor speed without analogWrite) ---
// Duty cycle = PWM_ON_MS / (PWM_ON_MS + PWM_OFF_MS)
// 18 / (18+7) ≈ 72%.  Raise PWM_ON_MS for more speed, lower for less.

// =====================================================================
//  STATE
// =====================================================================
Servo scanServo;
bool slowMotor = true;   // ← ADD THIS: true = use slow PWM timings
enum Mode { PATROL, ALIGN_LEFT, ALIGN_RIGHT, APPROACH, SPRAYING };
Mode mode     = PATROL;
unsigned long modeStart = 0;

// Flame debounce timestamps
unsigned long debL = 0, debC = 0, debR = 0;
bool actL = false, actC = false, actR = false;
unsigned long flameLastSeen = 0;   // last millis() any raw sensor was LOW

// Pump
bool          pumpActive = false;
unsigned long pumpOnAt   = 0;

// Servo
int  sAngle  = SERVO_CENTER;
int  sDir    = 1;              // +1 = sweeping toward MAX
bool sSweep  = false;
unsigned long lastServoStep = 0;

// Software PWM
enum MotorDir { M_STOP, M_FORWARD, M_LEFT, M_RIGHT };
MotorDir curDir    = M_STOP;
bool     pwmPhase  = false;    // true = motors ON phase
unsigned long pwmAt = 0;       // when current phase started

// =====================================================================
//  LOW-LEVEL MOTOR HELPERS
// =====================================================================

// Write directly to IN pins (no PWM logic)
void rawMotors(bool l1, bool l2, bool r1, bool r2) {
  digitalWrite(IN1, l1);
  digitalWrite(IN2, l2);
  digitalWrite(IN3, r1);
  digitalWrite(IN4, r2);
}

void applyDir(MotorDir d) {
  switch (d) {
    case M_FORWARD: rawMotors(1,0,1,0); break;
    case M_LEFT:    rawMotors(0,1,1,0); break;  // left rev, right fwd → turn left
    case M_RIGHT:   rawMotors(1,0,0,1); break;  // left fwd, right rev → turn right
    default:        rawMotors(0,0,0,0); break;
  }
}

/*
  setMotor() — sets direction and (re)starts the PWM cycle.
  Only resets the timer when direction actually changes,
  so rapid repeated calls with the same direction are safe.
*/
void setMotor(MotorDir d) {
  if (d == curDir) return;        // no change → leave PWM cycle alone
  curDir = d;
  if (d == M_STOP) {
    rawMotors(0,0,0,0);
    pwmPhase = false;
  } else {
    pwmPhase = true;              // start in ON phase
    pwmAt    = millis();
    applyDir(d);
  }
}

/*
  updatePWM() — call every loop; toggles motors ON/OFF to implement
  software PWM for smooth speed control.  Must be called frequently
  (ideally < 1 ms latency) for smooth operation.
*/
void updatePWM() {
  if (curDir == M_STOP) return;

  unsigned long onTime  = slowMotor ? PWM_SLOW_ON_MS  : PWM_ON_MS;
  unsigned long offTime = slowMotor ? PWM_SLOW_OFF_MS : PWM_OFF_MS;

  unsigned long dt = millis() - pwmAt;

  if (pwmPhase && dt >= onTime) {
    pwmPhase = false;
    pwmAt    = millis();
    rawMotors(0,0,0,0);
  } else if (!pwmPhase && dt >= offTime) {
    pwmPhase = true;
    pwmAt    = millis();
    applyDir(curDir);
  }
}
// =====================================================================
//  PUMP HELPERS
// =====================================================================
void pumpON() {
  if (pumpActive) return;
  digitalWrite(RELAY, LOW);   // ACTIVE-LOW relay
  pumpActive = true;
  pumpOnAt   = millis();
}

void pumpOFF() {
  if (!pumpActive) return;
  digitalWrite(RELAY, HIGH);
  pumpActive = false;
}

// =====================================================================
//  SERVO HELPERS
// =====================================================================
void servoSweepStart() {
  sSweep = true;
}

/*
  updateServo() — non-blocking 1°-step sweep between SERVO_MIN and SERVO_MAX.
  Called every loop; only moves when SERVO_STEP_MS has elapsed.
*/
void updateServo() {
  if (!sSweep)     return;
  if (!pumpActive) return;   // ← ADD THIS: wait for pump before sweeping

  unsigned long now = millis();
  if (now - lastServoStep < (unsigned long)SERVO_STEP_MS) return;
  lastServoStep = now;

  sAngle += sDir * SERVO_STEP;

  if (sAngle >= SERVO_MAX) { sAngle = SERVO_MAX; sDir = -1; }
  if (sAngle <= SERVO_MIN) { sAngle = SERVO_MIN; sDir =  1; }

  scanServo.write(sAngle);
}

/*
  servoReset() — stop sweep, return to center, reset direction.
  Only writes to servo once.
*/
void servoReset() {
  sSweep = false;
  if (sAngle != SERVO_CENTER) {
    sAngle = SERVO_CENTER;
    sDir   = 1;
    scanServo.write(SERVO_CENTER);
  }
}

// =====================================================================
//  FLAME SENSOR (debounced)
// =====================================================================
bool debounce(int pin, unsigned long &ts) {
  unsigned long now = millis();
  if (digitalRead(pin) == LOW) {           // sensor active = LOW
    if (ts == 0) ts = now;
    return (now - ts) >= FLAME_DEBOUNCE_MS;
  }
  ts = 0;
  return false;
}

void readFlames() {
  actL = debounce(FLAME_L, debL);
  actC = debounce(FLAME_C, debC);
  actR = debounce(FLAME_R, debR);

  // Track last raw detection for pump-clear logic
  if (digitalRead(FLAME_L) == LOW ||
      digitalRead(FLAME_C) == LOW ||
      digitalRead(FLAME_R) == LOW) {
    flameLastSeen = millis();
  }
}

// =====================================================================
//  MODE TRANSITION
// =====================================================================
void enterMode(Mode m) {
  mode      = m;
  modeStart = millis();
}

// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  // Flame sensors
  pinMode(FLAME_L, INPUT_PULLUP);
  pinMode(FLAME_C, INPUT_PULLUP);
  pinMode(FLAME_R, INPUT_PULLUP);

  // Pump relay
  pinMode(RELAY, OUTPUT);
  pumpOFF();

  // Motor IN pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  rawMotors(0,0,0,0);

  // ENA / ENB — digital HIGH only (no analogWrite, avoids Timer1 conflict).
  // Speed is handled entirely by the software PWM above.
  pinMode(ENA, OUTPUT); digitalWrite(ENA, HIGH);
  pinMode(ENB, OUTPUT); digitalWrite(ENB, HIGH);

  // Servo — parks at center on boot
  scanServo.attach(SERVO_PIN);
  scanServo.write(SERVO_CENTER);

  Serial.begin(115200);
  Serial.println(F("FireBot v2 ready."));
}

// =====================================================================
//  MAIN LOOP
// =====================================================================
void loop() {
  readFlames();       // update actL / actC / actR
  updateServo();      // advance sweep if active
  updatePWM();        // toggle motor IN pins for soft-PWM speed control

  unsigned long now = millis();

  switch (mode) {

    // ------------------------------------------------------------------
    
    case PATROL:
{
  servoReset();

  if (actC) {
    slowMotor = true;          // ← approach at reduced speed
    setMotor(M_FORWARD);
    enterMode(APPROACH);

  } else if (actL) {
    slowMotor = true;          // ← align at reduced speed
    setMotor(M_LEFT);
    enterMode(ALIGN_LEFT);

  } else if (actR) {
    slowMotor = true;          // ← align at reduced speed
    setMotor(M_RIGHT);
    enterMode(ALIGN_RIGHT);

  } else {
    slowMotor = false;         // ← normal speed if you re-enable patrol driving
    setMotor(M_STOP);
  }
}
break;
    // ------------------------------------------------------------------
    case ALIGN_LEFT:
    // ------------------------------------------------------------------
    // Turning left until the center sensor catches the flame.
    {
      if (actC) {
        // Aligned — stop and start spraying
        setMotor(M_STOP);
        pumpON();
        servoSweepStart();
        enterMode(SPRAYING);

      } else if (!actL && !actC && !actR) {
        // Flame disappeared while aligning — abort
        setMotor(M_STOP);
        enterMode(PATROL);
      }
      // else: still turning left (setMotor(M_LEFT) already in effect)
    }
    break;

    // ------------------------------------------------------------------
    case ALIGN_RIGHT:
    // ------------------------------------------------------------------
    // Turning right until the center sensor catches the flame.
    {
      if (actC) {
        setMotor(M_STOP);
        pumpON();
        servoSweepStart();
        enterMode(SPRAYING);

      } else if (!actL && !actC && !actR) {
        setMotor(M_STOP);
        enterMode(PATROL);
      }
    }
    break;

    // ------------------------------------------------------------------
    case APPROACH:
    // ------------------------------------------------------------------
    // Center flame detected — move forward for APPROACH_FWD_MS then spray.
    {
      if (now - modeStart >= APPROACH_FWD_MS) {
        setMotor(M_STOP);
        pumpON();
        servoSweepStart();
        enterMode(SPRAYING);
      }
      // else: keep driving forward (setMotor(M_FORWARD) already in effect)
    }
    break;

    // ------------------------------------------------------------------
    case SPRAYING:
{
  setMotor(M_STOP);
  bool minOnDone  = (now - pumpOnAt)      >= PUMP_MIN_ON_MS;
  bool flameClear = (now - flameLastSeen) >= FLAME_CLEAR_MS;

  if (pumpActive && minOnDone && flameClear) {
    pumpOFF();
    servoReset();
    slowMotor = false;         // ← restore normal speed for next patrol
    enterMode(PATROL);
  }
}
break;
  }

  // ---- Optional serial debug (every 300 ms) ----
  static unsigned long lastDbg = 0;
  if (now - lastDbg >= 300) {
    lastDbg = now;

    const char* modeStr[] = {"PATROL","ALIGN_L","ALIGN_R","APPROACH","SPRAYING"};
    Serial.print(F("Sensors L/C/R="));
    Serial.print(actL); Serial.print('/');
    Serial.print(actC); Serial.print('/');
    Serial.print(actR);
    Serial.print(F("  Mode="));  Serial.print(modeStr[mode]);
    Serial.print(F("  Servo=")); Serial.print(sAngle);
    Serial.print(F("  Pump="));  Serial.println(pumpActive ? F("ON") : F("OFF"));
  }
}
