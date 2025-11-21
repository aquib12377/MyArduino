/*
  2WD Fire-Fighting Bot (EN pins tied to +5V)
  - 3x Flame sensors (digital, ACTIVE-HIGH): Left, Center, Right
  - Relay (ACTIVE-LOW) to control pump
  - L298N/L293D driver: only IN pins used

  Behavior:
  - Detect side:
      RIGHT  -> nudge right briefly -> STOP -> pump ON
      LEFT   -> nudge left  briefly -> STOP -> pump ON
      CENTER -> nudge forward briefly -> STOP -> pump ON
  - While spraying, the bot remains STOPPED.
  - Pump turns OFF after minimum ON time and a flame-clear window.

  Pins below match your earlier sketch.
*/

/// -------------------- Pin Map --------------------
const int FLAME_L_PIN = 4;       // Flame Left   (ACTIVE-HIGH)
const int FLAME_C_PIN = 3;       // Flame Center (ACTIVE-HIGH)
const int FLAME_R_PIN = 2;       // Flame Right  (ACTIVE-HIGH)

const int RELAY_PUMP_PIN = 12;   // Relay (ACTIVE-LOW): LOW=ON, HIGH=OFF

// Motor driver IN pins (ENA/ENB are hard-wired to +5V)
const int IN1 = 7;               // Left  motor IN1
const int IN2 = 8;               // Left  motor IN2
const int IN3 = 9;               // Right motor IN3
const int IN4 = 10;              // Right motor IN4

/// -------------------- Tuning --------------------
unsigned long FLAME_CONFIRM_MS = 60;     // Require HIGH this long to confirm flame
unsigned long NUDGE_MS         = 300;    // How long to nudge toward the flame
unsigned long PUMP_MIN_ON_MS   = 1500;   // Minimum pump ON time once triggered
unsigned long FLAME_CLEAR_MS   = 600;    // Flame must be gone this long to consider cleared

/// -------------------- Internal State --------------------
unsigned long lastHighL = 0, lastHighC = 0, lastHighR = 0;  // last time each sensor went HIGH
bool          actL = false, actC = false, actR = false;     // debounced (confirmed) states

bool          pumpOnState = false;
unsigned long pumpTurnOnAt = 0;
unsigned long flameLastSeenAt = 0;

enum Mode { PATROL, NUDGE_LEFT, NUDGE_RIGHT, NUDGE_FWD, SPRAYING };
Mode mode = PATROL;
unsigned long modeStartedAt = 0;

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

/// Debounce/confirm each flame sensor (ACTIVE-HIGH)
bool flameActiveDebounced(int pin, unsigned long &lastHighTime) {
  int raw = digitalRead(pin); // HIGH means flame
  unsigned long now = millis();

  if (raw == LOW) {
    if (lastHighTime == 0) lastHighTime = now; // start timing HIGH
    return (now - lastHighTime) >= FLAME_CONFIRM_MS;
  } else {
    lastHighTime = 0; // reset when LOW
    return false;
  }
}

void updateFlames() {
  actL = flameActiveDebounced(FLAME_L_PIN, lastHighL);
  actC = flameActiveDebounced(FLAME_C_PIN, lastHighC);
  actR = flameActiveDebounced(FLAME_R_PIN, lastHighR);

  // track last time any sensor saw flame (even transiently)
  if (digitalRead(FLAME_L_PIN) == LOW ||
      digitalRead(FLAME_C_PIN) == LOW ||
      digitalRead(FLAME_R_PIN) == LOW) {
    flameLastSeenAt = millis();
  }
}

/// -------------------- Setup/Loop --------------------
void setup() {
  // Flame modules usually drive strongly; use INPUT (not pullups) for ACTIVE-HIGH.
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
  Serial.begin(115200);
  Serial.println(F("Fire Bot (ACTIVE-HIGH sensors, nudge->stop->spray) ready."));
}

void enterMode(Mode m) {
  mode = m;
  modeStartedAt = millis();
}

void loop() {
  updateFlames();
  unsigned long now = millis();

  switch (mode) {
    case PATROL: {
      // Default: gentle forward patrol (or stop if you prefer no roaming)
      //forward();

      // Decide which direction to engage:
      // Priority: CENTER > RIGHT > LEFT (so if center sees flame, go forward)
      if (actC) {
        enterMode(NUDGE_FWD);
        forward(); // start the nudge immediately
      } else if (actR && !actC) {
        enterMode(NUDGE_RIGHT);
        turnRight();
      } else if (actL && !actC) {
        enterMode(NUDGE_LEFT);
        turnLeft();
      }
    } break;

    case NUDGE_RIGHT: {
      // Briefly nudge right, then stop & spray
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
      // Stay stopped while spraying
      stopMotors();

      bool minOnSatisfied = (now - pumpTurnOnAt) >= PUMP_MIN_ON_MS;
      bool flameGoneLong  = (now - flameLastSeenAt) >= FLAME_CLEAR_MS;

      if (pumpOnState && minOnSatisfied && flameGoneLong) {
        pumpOff();
        enterMode(PATROL);
      }
    } break;
  }

  // Optional debug
  static unsigned long lastPrint = 0;
  if (now - lastPrint > 300) {
    lastPrint = now;
    Serial.print(F("raw LCR="));
    Serial.print(digitalRead(FLAME_L_PIN)==HIGH);
    Serial.print(",");
    Serial.print(digitalRead(FLAME_C_PIN)==HIGH);
    Serial.print(",");
    Serial.println(digitalRead(FLAME_R_PIN)==HIGH);

    Serial.print(F("act LCR="));
    Serial.print(actL); Serial.print(",");
    Serial.print(actC); Serial.print(",");
    Serial.print(actR); Serial.print(" | ");

    Serial.print(F("mode="));
    Serial.print(mode);
    Serial.print(F(" | pump="));
    Serial.println(pumpOnState ? "ON" : "OFF");
  }
}
