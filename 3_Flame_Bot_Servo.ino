#include <Servo.h>

/*
  2WD Fire-Fighting Bot + Scanning Servo
  - 3x Flame sensors (most modules are ACTIVE-LOW): Left, Center, Right
  - Relay (ACTIVE-LOW) to control pump
  - L298N/L293D driver: only IN pins used
  - Servo sweeps left-right until flame is detected

  Behavior:
  - No flame -> servo keeps scanning
  - Detect side:
      RIGHT  -> stop servo scan -> nudge right briefly -> STOP -> pump ON
      LEFT   -> stop servo scan -> nudge left  briefly -> STOP -> pump ON
      CENTER -> stop servo scan -> nudge forward briefly -> STOP -> pump ON
  - While spraying, the bot remains STOPPED.
  - Pump turns OFF after minimum ON time and a flame-clear window.
*/

/// -------------------- Pin Map --------------------
const int FLAME_L_PIN = 6;       // Flame Left   (ACTIVE-LOW on most modules)
const int FLAME_C_PIN = 7;       // Flame Center (ACTIVE-LOW)
const int FLAME_R_PIN = 8;       // Flame Right  (ACTIVE-LOW)

const int RELAY_PUMP_PIN = 10;   // Relay (ACTIVE-LOW): LOW=ON, HIGH=OFF

// Motor driver IN pins (ENA/ENB are hard-wired to +5V)
const int IN1 = 2;               // Left  motor IN1
const int IN2 = 3;               // Left  motor IN2
const int IN3 = 4;               // Right motor IN3
const int IN4 = 5;              // Right motor IN4

// Servo pin
const int SERVO_PIN = 9;

/// -------------------- Tuning --------------------
unsigned long FLAME_CONFIRM_MS = 60;     // Require flame signal this long to confirm
unsigned long NUDGE_MS         = 300;    // How long to nudge toward the flame
unsigned long PUMP_MIN_ON_MS   = 1500;   // Minimum pump ON time once triggered
unsigned long FLAME_CLEAR_MS   = 600;    // Flame must be gone this long to consider cleared

// Servo scan tuning
const int SERVO_MIN_ANGLE      = 20;
const int SERVO_MAX_ANGLE      = 160;
const int SERVO_START_ANGLE    = 90;
unsigned long SERVO_STEP_MS    = 20;     // lower = faster sweep
int SERVO_STEP_DEG             = 2;      // degrees per step

/// -------------------- Internal State --------------------
unsigned long lastLowL = 0, lastLowC = 0, lastLowR = 0; // last time each sensor went LOW
bool actL = false, actC = false, actR = false;          // debounced (confirmed) flame states

bool pumpOnState = false;
unsigned long pumpTurnOnAt = 0;
unsigned long flameLastSeenAt = 0;

enum Mode { PATROL, NUDGE_LEFT, NUDGE_RIGHT, NUDGE_FWD, SPRAYING };
Mode mode = PATROL;
unsigned long modeStartedAt = 0;

/// -------------------- Servo State --------------------
Servo scannerServo;
int servoAngle = SERVO_START_ANGLE;
int servoDir = 1; // +1 = increasing angle, -1 = decreasing angle
unsigned long lastServoStepAt = 0;
bool servoScanEnabled = true;

/// -------------------- Low-level helpers --------------------
void pumpOn() {
  if (!pumpOnState) {
    digitalWrite(RELAY_PUMP_PIN, LOW);   // ACTIVE-LOW -> ON
    pumpOnState = true;
    pumpTurnOnAt = millis();
  }
}

void pumpOff() {
  if (pumpOnState) {
    digitalWrite(RELAY_PUMP_PIN, HIGH);  // OFF
    pumpOnState = false;
  }
}

// Motor primitives (no PWM because EN pins are HIGH)
void leftForward()  { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  }
void leftReverse()  { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); }
void leftStop()     { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  }

void rightForward() { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  }
void rightReverse() { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
void rightStop()    { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  }

void stopMotors()   { leftStop(); rightStop(); }
void forward()      { leftForward(); rightForward(); }
void turnLeft()     { leftReverse(); rightForward(); }
void turnRight()    { leftForward(); rightReverse(); }

/// -------------------- Servo Helpers --------------------
void updateServoScan() {
  if (!servoScanEnabled) return;

  unsigned long now = millis();
  if (now - lastServoStepAt < SERVO_STEP_MS) return;
  lastServoStepAt = now;

  servoAngle += servoDir * SERVO_STEP_DEG;

  if (servoAngle >= SERVO_MAX_ANGLE) {
    servoAngle = SERVO_MAX_ANGLE;
    servoDir = -1;
  } else if (servoAngle <= SERVO_MIN_ANGLE) {
    servoAngle = SERVO_MIN_ANGLE;
    servoDir = 1;
  }

  scannerServo.write(servoAngle);
}

void stopServoScan() {
  servoScanEnabled = false;
  // servo stays at current angle
}

void startServoScan() {
  servoScanEnabled = true;
}

/// -------------------- Flame debounce --------------------
/// Most flame modules output LOW when flame is detected.
bool flameActiveDebounced(int pin, unsigned long &lastLowTime) {
  int raw = digitalRead(pin);   // LOW means flame
  unsigned long now = millis();

  if (raw == LOW) {
    if (lastLowTime == 0) lastLowTime = now;   // start timing LOW
    return (now - lastLowTime) >= FLAME_CONFIRM_MS;
  } else {
    lastLowTime = 0; // reset when not detecting
    return false;
  }
}

void updateFlames() {
  actL = flameActiveDebounced(FLAME_L_PIN, lastLowL);
  actC = flameActiveDebounced(FLAME_C_PIN, lastLowC);
  actR = flameActiveDebounced(FLAME_R_PIN, lastLowR);

  // track last time any sensor saw flame (even transiently)
  if (digitalRead(FLAME_L_PIN) == LOW ||
      digitalRead(FLAME_C_PIN) == LOW ||
      digitalWrite? 0 : 0) {
    // placeholder removed below
  }

  if (digitalRead(FLAME_L_PIN) == LOW ||
      digitalRead(FLAME_C_PIN) == LOW ||
      digitalRead(FLAME_R_PIN) == LOW) {
    flameLastSeenAt = millis();
  }
}

/// -------------------- Setup/Loop --------------------
void setup() {
  pinMode(FLAME_L_PIN, INPUT_PULLUP);
  pinMode(FLAME_C_PIN, INPUT_PULLUP);
  pinMode(FLAME_R_PIN, INPUT_PULLUP);

  pinMode(RELAY_PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, HIGH); // default OFF (ACTIVE-LOW)

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();

  scannerServo.attach(SERVO_PIN);
  scannerServo.write(servoAngle);

  Serial.begin(115200);
  Serial.println(F("Fire Bot + scanning servo ready."));
}

void enterMode(Mode m) {
  mode = m;
  modeStartedAt = millis();
}

void loop() {
  updateFlames();
  updateServoScan();

  unsigned long now = millis();

  switch (mode) {
    case PATROL: {
      // Keep motors stopped unless flame is detected
      stopMotors();

      // Priority: CENTER > RIGHT > LEFT
      if (actC) {
        startServoScan();
        enterMode(NUDGE_FWD);
        forward();
      } else if (actR && !actC) {
        startServoScan();
        enterMode(NUDGE_RIGHT);
        turnRight();
      } else if (actL && !actC) {
        startServoScan();
        enterMode(NUDGE_LEFT);
        turnLeft();
      } else {
        // no flame -> keep scanning
        stopServoScan();
      }
    } break;

    case NUDGE_RIGHT: {
      if (now - modeStartedAt >= NUDGE_MS) {
        stopMotors();
        pumpOn();
        enterMode(SPRAYING);
      } else {
        turnRight();
      }
    } break;

    case NUDGE_LEFT: {
      if (now - modeStartedAt >= NUDGE_MS) {
        stopMotors();
        pumpOn();
        enterMode(SPRAYING);
      } else {
        turnLeft();
      }
    } break;

    case NUDGE_FWD: {
      if (now - modeStartedAt >= NUDGE_MS) {
        stopMotors();
        pumpOn();
        enterMode(SPRAYING);
      } else {
        forward();
      }
    } break;

    case SPRAYING: {
      stopMotors();

      bool minOnSatisfied = (now - pumpTurnOnAt) >= PUMP_MIN_ON_MS;
      bool flameGoneLong  = (now - flameLastSeenAt) >= FLAME_CLEAR_MS;

      if (pumpOnState && minOnSatisfied && flameGoneLong) {
        pumpOff();
        startServoScan();
        enterMode(PATROL);
      }
    } break;
  }

  /// Optional debug
  static unsigned long lastPrint = 0;
  if (now - lastPrint > 300) {
    lastPrint = now;

    Serial.print(F("raw LCR="));
    Serial.print(digitalRead(FLAME_L_PIN) == LOW);
    Serial.print(",");
    Serial.print(digitalRead(FLAME_C_PIN) == LOW);
    Serial.print(",");
    Serial.println(digitalRead(FLAME_R_PIN) == LOW);

    Serial.print(F("act LCR="));
    Serial.print(actL); Serial.print(",");
    Serial.print(actC); Serial.print(",");
    Serial.print(actR); Serial.print(" | ");

    Serial.print(F("servo="));
    Serial.print(servoAngle);
    Serial.print(F(" | scan="));
    Serial.print(servoScanEnabled ? "ON" : "OFF");
    Serial.print(F(" | mode="));
    Serial.print(mode);
    Serial.print(F(" | pump="));
    Serial.println(pumpOnState ? "ON" : "OFF");
  }
}