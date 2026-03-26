#include <Servo.h>

/*
  2WD Fire-Fighting Bot + Scanning Servo
  - 3x Flame sensors (most modules are ACTIVE-LOW): Left, Center, Right
  - Relay (ACTIVE-LOW) to control pump
  - L298N/L293D driver: only IN pins used

  Behavior:
  - No flame   -> bot moves FORWARD, servo STOPPED
  - Flame detected -> nudge toward flame (servo stopped during approach)
  - SPRAYING   -> bot STOPPED, servo ROTATES to spray water around
  - Flame gone -> pump OFF, servo STOPS, returns to center, back to PATROL
*/

/// -------------------- Pin Map --------------------
const int FLAME_L_PIN = A1;       // Flame Left   (ACTIVE-LOW on most modules)
const int FLAME_C_PIN = A0;       // Flame Center (ACTIVE-LOW)
const int FLAME_R_PIN = A2;       // Flame Right  (ACTIVE-LOW)

const int RELAY_PUMP_PIN = 12;   // Relay (ACTIVE-LOW): LOW=ON, HIGH=OFF

// Motor driver IN pins (ENA/ENB are hard-wired to +5V)
const int IN1 = 5;               // Left  motor IN1
const int IN2 = 6;               // Left  motor IN2
const int IN3 = 7;               // Right motor IN3
const int IN4 = 8;               // Right motor IN4

// Servo pin
const int SERVO_PIN = 11;

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
unsigned long lastLowL = 0, lastLowC = 0, lastLowR = 0;
bool actL = false, actC = false, actR = false;

bool pumpOnState = false;
unsigned long pumpTurnOnAt = 0;
unsigned long flameLastSeenAt = 0;

enum Mode { PATROL, NUDGE_LEFT, NUDGE_RIGHT, NUDGE_FWD, SPRAYING };
Mode mode = PATROL;
unsigned long modeStartedAt = 0;
int speedLeft  = 255;   // adjust (0–255)
int speedRight = 255;
/// -------------------- Servo State --------------------
Servo scannerServo;
int servoAngle = SERVO_START_ANGLE;
int servoDir = 1;
unsigned long lastServoStepAt = 0;
bool servoScanEnabled = false;   // starts OFF (no flame on boot)

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
}

void startServoScan() {
  servoScanEnabled = true;
}

/// -------------------- Flame debounce --------------------
bool flameActiveDebounced(int pin, unsigned long &lastLowTime) {
  int raw = digitalRead(pin);
  unsigned long now = millis();

  if (raw == LOW) {
    if (lastLowTime == 0) lastLowTime = now;
    return (now - lastLowTime) >= FLAME_CONFIRM_MS;
  } else {
    lastLowTime = 0;
    return false;
  }
}

void updateFlames() {
  actL = flameActiveDebounced(FLAME_L_PIN, lastLowL);
  actC = flameActiveDebounced(FLAME_C_PIN, lastLowC);
  actR = flameActiveDebounced(FLAME_R_PIN, lastLowR);

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

  // ENA / ENB enable pins hard-wired HIGH via code
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  analogWrite(9, speedLeft);
  analogWrite(10, speedRight);

  stopMotors();

  scannerServo.attach(SERVO_PIN);
  scannerServo.write(SERVO_START_ANGLE);  // center servo on boot

  Serial.begin(115200);
  Serial.println(F("Fire Bot ready. Servo OFF until flame detected."));
}

void enterMode(Mode m) {
  mode = m;
  modeStartedAt = millis();
}

void loop() {
  updateFlames();
  updateServoScan();   // only ticks if servoScanEnabled == true

  unsigned long now = millis();

  switch (mode) {

    // ---- PATROL: bot moves forward, servo is STOPPED ----
    case PATROL: {
      stopServoScan();   // servo OFF while patrolling

      if (actC) {
        enterMode(NUDGE_FWD);
        forward();
      } else if (actR) {
        enterMode(NUDGE_RIGHT);
        turnRight();
      } else if (actL) {
        enterMode(NUDGE_LEFT);
        turnLeft();
      } else {
        //forward();       // no flame -> keep patrolling forward
      }
    } break;

    // ---- NUDGE modes: approach the flame, servo still STOPPED ----
    case NUDGE_RIGHT: {
      if (now - modeStartedAt >= NUDGE_MS) {
        stopMotors();
        pumpOn();
        startServoScan();   // flame reached -> servo STARTS rotating to spray
        enterMode(SPRAYING);
      } else {
        turnRight();
      }
    } break;

    case NUDGE_LEFT: {
      if (now - modeStartedAt >= NUDGE_MS) {
        stopMotors();
        pumpOn();
        startServoScan();   // flame reached -> servo STARTS rotating to spray
        enterMode(SPRAYING);
      } else {
        turnLeft();
      }
    } break;

    case NUDGE_FWD: {
      if (now - modeStartedAt >= NUDGE_MS) {
        stopMotors();
        pumpOn();
        startServoScan();   // flame reached -> servo STARTS rotating to spray
        enterMode(SPRAYING);
      } else {
        forward();
      }
    } break;

    // ---- SPRAYING: bot STOPPED, servo ROTATES to spray around ----
    case SPRAYING: {
      stopMotors();
      // servo keeps sweeping left/right (startServoScan called on entry)

      bool minOnSatisfied = (now - pumpTurnOnAt) >= PUMP_MIN_ON_MS;
      bool flameGoneLong  = (now - flameLastSeenAt) >= FLAME_CLEAR_MS;

      if (pumpOnState && minOnSatisfied && flameGoneLong) {
        pumpOff();
        stopServoScan();                        // flame out -> servo STOPS
        scannerServo.write(SERVO_START_ANGLE);  // return servo to center
        servoAngle = SERVO_START_ANGLE;
        enterMode(PATROL);
      }
    } break;
  }

  // Optional serial debug
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
