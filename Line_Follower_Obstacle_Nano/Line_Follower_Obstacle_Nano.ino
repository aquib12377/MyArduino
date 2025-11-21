/*
  Nano Line Follower + Obstacle Avoider (HC-SR04 + 2x IR ACTIVE-LOW + L298N)
  - ENA/ENB on L298N are jumpered to 5V (full speed, no PWM).
  - IR sensors output LOW when over the black line (ACTIVE-LOW).
  - Gentle steering: stop one side, drive the other.

  ---------- WIRING (this sketch) ----------
  IR_L (DO) -> D7
  IR_R (DO) -> D8

  HC-SR04  : TRIG -> D3,  ECHO -> D2
  L298N    : IN1 -> A0, IN2 -> A1   (Left motor)
             IN3 -> A2, IN4 -> A3   (Right motor)
             ENA -> 5V, ENB -> 5V, GND common with Nano
*/
enum Motion : uint8_t { M_STOP, M_FORWARD, M_BACK, M_STEER_LEFT, M_STEER_RIGHT };


//////////////////// USER CONFIG ////////////////////
#define IR_LEFT_PIN   7
#define IR_RIGHT_PIN  8

#define TRIG_PIN      3 
#define ECHO_PIN      2

// L298N pins
#define IN1           A0   // Left motor
#define IN2           A1
#define IN3           A2   // Right motor
#define IN4           A3

// Behavior tuning
const uint8_t  OBSTACLE_CM       = 18;   // Stop/avoid if object is nearer
const uint16_t BACK_UP_MS        = 250;
const uint16_t AVOID_TURN_MS     = 350;
const uint16_t SEEK_BURST_MS     = 60;
const uint16_t SEEK_PAUSE_MS     = 20;
const uint16_t SEEK_TIMEOUT_MS   = 1500;

// IR debouncing (majority of N samples)
const uint8_t  IR_SAMPLES        = 3;
const uint8_t  US_SAMPLES        = 3;    // ultrasonic median-of-3
/////////////////////////////////////////////////////

// ---------- DEBUG CONTROL ----------
#define STATUS_EVERY_MS 200
unsigned long _lastStatusMs = 0;

// --------- Helpers: Motor control (full speed) ---------
inline void leftForward()  { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  }
inline void leftBackward() { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); }
inline void leftStop()     { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  } // coast

inline void rightForward()  { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  }
inline void rightBackward() { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
inline void rightStop()     { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  } // coast

Motion _lastMotion = M_STOP;

void _logMotion(Motion m) {
  if (m == _lastMotion) return;
  _lastMotion = m;
  switch (m) {
    case M_STOP:        Serial.println(F("[MOTION] STOP")); break;
    case M_FORWARD:     Serial.println(F("[MOTION] FORWARD")); break;
    case M_BACK:        Serial.println(F("[MOTION] BACK")); break;
    case M_STEER_LEFT:  Serial.println(F("[MOTION] STEER_LEFT")); break;
    case M_STEER_RIGHT: Serial.println(F("[MOTION] STEER_RIGHT")); break;
  }
}

inline void motorsStop()    { leftStop();     rightStop();     _logMotion(M_STOP);    }
inline void motorsForward() { leftForward();  rightForward();  _logMotion(M_FORWARD); }
inline void motorsBack()    { leftBackward(); rightBackward(); _logMotion(M_BACK);    }

// Gentle steer: one side coast, the other forward
inline void steerLeft()  { leftStop();   rightForward(); _logMotion(M_STEER_LEFT);  }
inline void steerRight() { leftForward(); rightStop();    _logMotion(M_STEER_RIGHT); }

// --------- IR reading (ACTIVE-LOW) with simple majority filter ---------
bool readIRRaw(uint8_t pin) {
  // ACTIVE-LOW: LOW means on line
  return digitalRead(pin) == LOW;
}

bool readIRFiltered(uint8_t pin) {
  uint8_t hits = 0;
  for (uint8_t i = 0; i < IR_SAMPLES; i++) {
    if (readIRRaw(pin)) hits++;
    delayMicroseconds(500);
  }
  return (hits >= (IR_SAMPLES + 1) / 2);
}

// --------- Ultrasonic (HC-SR04) median-of-3 (cm) ---------
unsigned long readEchoUs() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout ~20ms to avoid lockups (max ~3.4m range)
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 20000UL);
  return dur; // microseconds
}

unsigned int microToCm(unsigned long us) {
  if (us == 0) return 999;  // timeout
  return (unsigned int)(us * 0.0343 / 2.0);
}

unsigned int readDistanceMedianCm() {
  unsigned int a[US_SAMPLES];
  for (uint8_t i = 0; i < US_SAMPLES; i++) {
    unsigned long u = readEchoUs();
    a[i] = microToCm(u);
    if (a[i] == 999) Serial.println(F("[ULTRA] timeout pulseIn()"));
    delay(5);
  }
  // simple sort for tiny N
  for (uint8_t i = 0; i < US_SAMPLES; i++)
    for (uint8_t j = i + 1; j < US_SAMPLES; j++)
      if (a[j] < a[i]) { unsigned int t = a[i]; a[i] = a[j]; a[j] = t; }
  return a[US_SAMPLES / 2];
}

// --------- Utility ---------
bool anyLineDetected(bool L, bool R) { return L || R; }

void seekLineRight(unsigned long timeoutMs) {
  Serial.println(F("[SEEK] Right start"));
  unsigned long t0 = millis();
  uint16_t ticks = 0;
  while (millis() - t0 < timeoutMs) {
    steerRight(); delay(SEEK_BURST_MS);
    motorsStop(); delay(SEEK_PAUSE_MS);

    bool L = readIRFiltered(IR_LEFT_PIN);
    bool R = readIRFiltered(IR_RIGHT_PIN);
    if ((++ticks % 8) == 0) {
      Serial.print(F("[SEEK] Right tick, L=")); Serial.print(L);
      Serial.print(F(" R=")); Serial.println(R);
    }
    if (anyLineDetected(L, R)) {
      Serial.print(F("[SEEK] Right found line: L=")); Serial.print(L);
      Serial.print(F(" R=")); Serial.println(R);
      return;
    }
  }
  Serial.println(F("[SEEK] Right timeout"));
}

void seekLineLeft(unsigned long timeoutMs) {
  Serial.println(F("[SEEK] Left start"));
  unsigned long t0 = millis();
  uint16_t ticks = 0;
  while (millis() - t0 < timeoutMs) {
    steerLeft(); delay(SEEK_BURST_MS);
    motorsStop(); delay(SEEK_PAUSE_MS);

    bool L = readIRFiltered(IR_LEFT_PIN);
    bool R = readIRFiltered(IR_RIGHT_PIN);
    if ((++ticks % 8) == 0) {
      Serial.print(F("[SEEK] Left tick, L=")); Serial.print(L);
      Serial.print(F(" R=")); Serial.println(R);
    }
    if (anyLineDetected(L, R)) {
      Serial.print(F("[SEEK] Left found line: L=")); Serial.print(L);
      Serial.print(F(" R=")); Serial.println(R);
      return;
    }
  }
  Serial.println(F("[SEEK] Left timeout"));
}

void avoidObstacle() {
  Serial.println(F("[AVOID] Obstacle -> back + turn right + reacquire"));
  motorsStop(); delay(50);
  motorsBack(); delay(BACK_UP_MS);
  steerRight(); delay(AVOID_TURN_MS);
  motorsStop(); delay(20);

  // Reacquire line
  seekLineRight(SEEK_TIMEOUT_MS);
  bool L = readIRFiltered(IR_LEFT_PIN);
  bool R = readIRFiltered(IR_RIGHT_PIN);
  if (!anyLineDetected(L, R)) {
    Serial.println(F("[AVOID] Not found to the right, trying left"));
    seekLineLeft(SEEK_TIMEOUT_MS);
  } else {
    Serial.println(F("[AVOID] Line reacquired (right)"));
  }
}

void setup() {
  pinMode(IR_LEFT_PIN,  INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  motorsStop();

  Serial.begin(9600);
  Serial.println(F("\n=== Boot: Line Follower + Obstacle Avoider (ACTIVE-LOW IR, EN=VCC) ==="));
  Serial.print(F("Pins: IR_L(D")); Serial.print(IR_LEFT_PIN);
  Serial.print(F("), IR_R(D")); Serial.print(IR_RIGHT_PIN);
  Serial.print(F("), TRIG(D"));   Serial.print(TRIG_PIN);
  Serial.print(F("), ECHO(D"));   Serial.print(ECHO_PIN);
  Serial.println(F(")"));
  Serial.println(F("Pins: L298N IN1(A0), IN2(A1), IN3(A2), IN4(A3)"));
  Serial.print(F("Thresholds: OBSTACLE_CM=")); Serial.println(OBSTACLE_CM);
}

void loop() {
  // 1) Obstacle check
  unsigned int d = readDistanceMedianCm();
  if (d <= OBSTACLE_CM) {
    Serial.print(F("[ULTRA] ")); Serial.print(d); Serial.println(F(" cm -> AVOID"));
    avoidObstacle();
    return; // start next loop
  }

  // 2) Line following
  bool L = readIRFiltered(IR_LEFT_PIN);   // true if on line (LOW sampled)
  bool R = readIRFiltered(IR_RIGHT_PIN);

  // Throttled status line
  if (millis() - _lastStatusMs >= STATUS_EVERY_MS) {
    Serial.print(F("[STATE] IR L=")); Serial.print(L);
    Serial.print(F(" R="));           Serial.print(R);
    Serial.print(F("  dist="));       Serial.print(d);
    Serial.println(F("cm"));
    _lastStatusMs = millis();
  }

  if (L && R) {
    // Centered on line
    motorsForward();
  } else if (L && !R) {
    // Left sensor on line -> steer left to center
    steerLeft();
  } else if (!L && R) {
    // Right sensor on line -> steer right to center
    steerRight();
  } else {
    // Both off line -> seek (spin right in bursts), with logs inside
    seekLineRight(400);
    // If still nothing, try left quickly
    L = readIRFiltered(IR_LEFT_PIN);
    R = readIRFiltered(IR_RIGHT_PIN);
    if (!anyLineDetected(L, R)) {
      seekLineLeft(400);
    }
  }

  delay(5); // small loop delay
}
