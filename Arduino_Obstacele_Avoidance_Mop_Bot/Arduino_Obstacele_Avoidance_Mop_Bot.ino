/* ==========================================================
   Obstacle-Avoiding + Mopping Bot (ACTIVE-LOW mop relay)
   With Serial Debug Prints (toggle DEBUG below)
   ========================================================== */

// ---------------- Debug switch ----------------
#define DEBUG 1
#if DEBUG
  #define DBG_BEGIN()   Serial.begin(115200)
  #define DBG(...)      Serial.print(__VA_ARGS__)
  #define DBGLN(...)    Serial.println(__VA_ARGS__)
#else
  #define DBG_BEGIN()
  #define DBG(...)
  #define DBGLN(...)
#endif

// ---------------- Pins ----------------
#define IN1 4  // Left motor IN1
#define IN2 5  // Left motor IN2
#define IN3 6  // Right motor IN3
#define IN4 7  // Right motor IN4

#define ENL 9  // Left enable (PWM)
#define ENR 10 // Right enable (PWM)

#define MOP A0       // Mop relay (ACTIVE LOW)
#define TRIG_PIN 3
#define ECHO_PIN 2

/* -------------------- Tuning -------------------- */
const int  SPEED_FWD   = 150;  // 0..255 PWM
const int  SPEED_TURN  = 100;
const int  SPEED_BACK  = 100;

const unsigned long TURN_TIME_CHECK = 250;  // ms: small yaw to "peek" a side
const unsigned long TURN_SETTLE_MS  = 120;  // ms: let bot stop before measuring
const unsigned long BACK_TIME_MS    = 500;  // ms: back up when boxed in
const unsigned long DECEL_MS        = 80;   // ms: brief stop before new move

const int  DIST_NEAR   = 25;  // cm: treat as obstacle when ahead ≤ this
const int  DIST_CLEAR  = 28;  // cm: acceptable path when sampling sides
const int  DIST_MAX    = 300; // cm: cap for echo sanity
const int  SAMPLES_PER_MEAS = 3; // average multiple ultrasonic reads
/* ------------------------------------------------ */

unsigned long nowMs() { return millis(); }

/* -------------------- Mop control (ACTIVE LOW) -------------------- */
void mopOn()  { digitalWrite(MOP, LOW);  DBGLN(F("[MOP] ON (LOW)")); }
void mopOff() { digitalWrite(MOP, HIGH); DBGLN(F("[MOP] OFF (HIGH)")); }

/* -------------------- Motor primitives -------------------- */
void setSpeed(int l, int r) {
  l = constrain(l, 0, 255);
  r = constrain(r, 0, 255);
  analogWrite(ENL, l);
  analogWrite(ENR, r);
  DBG(F("[SPD] L=")); DBG(l); DBG(F(" R=")); DBGLN(r);
}

void motorsStop() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  setSpeed(0, 0);
  DBGLN(F("[MOVE] STOP"));
}

void forward(int pwm) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // Left forward
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // Right forward
  setSpeed(pwm, pwm);
  DBG(F("[MOVE] FWD pwm=")); DBGLN(pwm);
}

void backward(int pwm) {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // Left back
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // Right back
  setSpeed(pwm, pwm);
  DBG(F("[MOVE] BACK pwm=")); DBGLN(pwm);
}

void turnLeft(int pwm) {   // in-place left
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // Left back
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // Right fwd
  setSpeed(pwm, pwm);
  DBG(F("[MOVE] TURN L pwm=")); DBGLN(pwm);
}

void turnRight(int pwm) {  // in-place right
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // Left fwd
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // Right back
  setSpeed(pwm, pwm);
  DBG(F("[MOVE] TURN R pwm=")); DBGLN(pwm);
}

/* -------------------- Ultrasonic -------------------- */
long readDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long timeout = (unsigned long)(DIST_MAX * 58UL * 1.2); // 58us/cm
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, timeout);
  if (dur == 0) {
    DBGLN(F("[US] Timeout -> DIST_MAX"));
    return DIST_MAX;
  }
  long cm = (long)(dur / 58UL);
  if (cm < 2) cm = 2;
  if (cm > DIST_MAX) cm = DIST_MAX;
  DBG(F("[US] dur=")); DBG(dur); DBG(F("us -> ")); DBG(cm); DBGLN(F(" cm"));
  return cm;
}

long readDistanceAvg(uint8_t samples) {
  long acc = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    acc += readDistanceOnce();
    delay(10);
  }
  long avg = acc / (long)samples;
  DBG(F("[US] AVG(")); DBG(samples); DBG(F(") = ")); DBG(avg); DBGLN(F(" cm"));
  return avg;
}

/* -------------------- Direction sampling -------------------- */
long sampleForward() {
  DBGLN(F("[SAMPLE] Forward"));
  motorsStop(); mopOff();
  delay(DECEL_MS);
  long d = readDistanceAvg(SAMPLES_PER_MEAS);
  DBG(F("[SAMPLE] F = ")); DBG(d); DBGLN(F(" cm"));
  return d;
}

long sampleLeft() {
  DBGLN(F("[SAMPLE] Left"));
  mopOff();
  motorsStop(); delay(DECEL_MS);
  turnLeft(SPEED_TURN);
  delay(TURN_TIME_CHECK);
  motorsStop(); delay(TURN_SETTLE_MS);
  long d = readDistanceAvg(SAMPLES_PER_MEAS);

  // return to center
  turnRight(SPEED_TURN);
  delay(TURN_TIME_CHECK);
  motorsStop(); delay(DECEL_MS);

  DBG(F("[SAMPLE] L = ")); DBG(d); DBGLN(F(" cm"));
  return d;
}

long sampleRight() {
  DBGLN(F("[SAMPLE] Right"));
  mopOff();
  motorsStop(); delay(DECEL_MS);
  turnRight(SPEED_TURN);
  delay(TURN_TIME_CHECK);
  motorsStop(); delay(TURN_SETTLE_MS);
  long d = readDistanceAvg(SAMPLES_PER_MEAS);

  // return to center
  turnLeft(SPEED_TURN);
  delay(TURN_TIME_CHECK);
  motorsStop(); delay(DECEL_MS);

  DBG(F("[SAMPLE] R = ")); DBG(d); DBGLN(F(" cm"));
  return d;
}

/* -------------------- Decision logic -------------------- */
void decideAndMove() {
  DBGLN(F("[DECIDE] Sampling F/L/R..."));
  long dF = sampleForward();
  long dL = sampleLeft();
  long dR = sampleRight();

  // Choose the direction with the largest distance
  long best = dF;
  char bestDir = 'F'; // F/L/R
  if (dL > best) { best = dL; bestDir = 'L'; }
  if (dR > best) { best = dR; bestDir = 'R'; }

  DBG(F("[DECIDE] F=")); DBG(dF); DBG(F(" L=")); DBG(dL); DBG(F(" R=")); DBG(dR);
  DBG(F(" | best=")); DBG(bestDir); DBG(F(" ")); DBG(best); DBGLN(F(" cm"));

  if (best >= DIST_CLEAR) {
    if (bestDir == 'L') {
      DBGLN(F("[DECIDE] Turning LEFT to clear path"));
      mopOff();
      turnLeft(SPEED_TURN);
      delay(TURN_TIME_CHECK * 2);
      motorsStop(); delay(DECEL_MS);
    } else if (bestDir == 'R') {
      DBGLN(F("[DECIDE] Turning RIGHT to clear path"));
      mopOff();
      turnRight(SPEED_TURN);
      delay(TURN_TIME_CHECK * 2);
      motorsStop(); delay(DECEL_MS);
    } else {
      DBGLN(F("[DECIDE] Forward is best"));
    }
    mopOn();
    forward(SPEED_FWD);
    return;
  }

  // Boxed in: back up then try again
  DBGLN(F("[DECIDE] Boxed in -> BACKUP"));
  mopOff();
  backward(SPEED_BACK);
  delay(BACK_TIME_MS);
  motorsStop(); delay(DECEL_MS);

  if (dL >= dR) {
    DBGLN(F("[DECIDE] Bias turn LEFT after backup"));
    turnLeft(SPEED_TURN);
  } else {
    DBGLN(F("[DECIDE] Bias turn RIGHT after backup"));
    turnRight(SPEED_TURN);
  }
  delay(TURN_TIME_CHECK * 2);
  motorsStop(); delay(DECEL_MS);
}

/* -------------------- Setup/Loop -------------------- */
void setup() {
  DBG_BEGIN();
  DBGLN(F("\n[BOOT] Obstacle/Mop bot starting..."));

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENL, OUTPUT); pinMode(ENR, OUTPUT);

  pinMode(MOP, OUTPUT);
  mopOff(); // default OFF (ACTIVE LOW)

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  motorsStop();
  setSpeed(0,0);

  delay(500);
  DBGLN(F("[BOOT] Ready"));
}

void loop() {
  // Drive forward until obstacle is near
  mopOn();
  forward(SPEED_FWD);

  // Check ahead periodically
  long d = readDistanceAvg(2);
  DBG(F("[LOOP] Ahead=")); DBG(d); DBGLN(F(" cm"));
  if (d <= DIST_NEAR) {
    DBGLN(F("[LOOP] Obstacle detected -> maneuver"));
    motorsStop(); delay(DECEL_MS);
    mopOff(); // off during maneuver
    decideAndMove();
  }

  delay(100); // avoid hammering sensors
}
