#include <Servo.h>

/* =================== USER CONFIG =================== */
const byte PIN_TRIG = 8;
const byte PIN_ECHO = 9;

const byte PIN_LED_RED    = 6;
const byte PIN_LED_YELLOW = 7;
const byte PIN_LED_GREEN  = 5;
const byte PIN_SERVO      = 3;
const byte PIN_SERVO2     = 10;


// NEW: Buzzer
const byte PIN_BUZZER     = 4;      // Active or passive buzzer
const unsigned BUZZ_FREQ_HZ = 2000; // Beep frequency

const uint16_t TRAIN_NEAR_CM  = 25;
const uint16_t CLEAR_HYST_CM  = 50;
const uint32_t DETECT_HOLD_MS = 250;
const uint32_t CLEAR_HOLD_MS  = 400;
const uint32_t RED_WAIT_MS    = 15000; // 15s hold after train passes

const int SERVO_OPEN_DEG   = 92;
const int SERVO_CLOSED_DEG = 2;
const uint8_t SERVO_STEP   = 2;
const uint16_t SERVO_STEP_INTERVAL_MS = 15;

const uint16_t MEAS_INTERVAL_MS = 80;
const uint8_t  MEDIAN_WINDOW    = 5;
const uint8_t  MAX_BAD_READS    = 12;

enum State {
  IDLE_OPEN_GREEN,
  YELLOW_CLOSING,
  YELLOW_CLOSED,
  RED_WAIT_CLEAR,
  OPENING_GREEN,
  FAULT_CLOSED
};
State state = IDLE_OPEN_GREEN;

Servo gate;
Servo gate2;
int currentAngle = SERVO_OPEN_DEG;
uint32_t lastServoStepMs = 0;
uint32_t lastSampleMs = 0;
uint32_t nearSince = 0, clearSince = 0, redStartMs = 0;

uint16_t buf[MEDIAN_WINDOW];
uint8_t  bufFill = 0, bufIdx = 0;
uint8_t  badReadCount = 0;
bool yellowBlink = false;
uint32_t lastBlinkMs = 0;
const uint16_t BLINK_INTERVAL_MS = 350;

/* ======= BUZZER PATTERNS (non-blocking) ======= */
bool buzzerOn = false;
bool buzzerContinuous = false;
uint32_t lastBuzzToggleMs = 0;

const uint16_t BEEP_YELLOW_ON_MS = 200;
const uint16_t BEEP_YELLOW_OFF_MS = 200;

const uint16_t BEEP_FAULT_ON_MS = 100;
const uint16_t BEEP_FAULT_OFF_MS = 100;

void buzzerOff() {
  if (buzzerOn || buzzerContinuous) {
    noTone(PIN_BUZZER);
    buzzerOn = false;
    buzzerContinuous = false;
  }
}

void buzzerContinuousOn() {
  if (!buzzerContinuous) {
    tone(PIN_BUZZER, BUZZ_FREQ_HZ);
    buzzerContinuous = true;
    buzzerOn = true;
  }
}

void buzzerBeep(uint16_t onMs, uint16_t offMs) {
  buzzerContinuous = false; // ensure not in continuous mode
  uint32_t now = millis();
  if (buzzerOn) {
    if (now - lastBuzzToggleMs >= onMs) {
      noTone(PIN_BUZZER);
      buzzerOn = false;
      lastBuzzToggleMs = now;
    }
  } else {
    if (now - lastBuzzToggleMs >= offMs) {
      tone(PIN_BUZZER, BUZZ_FREQ_HZ);
      buzzerOn = true;
      lastBuzzToggleMs = now;
    }
  }
}

void updateBuzzer() {
  switch (state) {
    case IDLE_OPEN_GREEN:
    case OPENING_GREEN:
      buzzerOff();
      break;

    case YELLOW_CLOSING:
    case YELLOW_CLOSED:
      // Slow beeps while approaching/closing
      buzzerBeep(BEEP_YELLOW_ON_MS, BEEP_YELLOW_OFF_MS);
      break;

    case RED_WAIT_CLEAR:
      // Continuous during 15s red hold
      buzzerContinuousOn();
      break;

    case FAULT_CLOSED:
      // Fast beeps to indicate sensor fault
      buzzerBeep(BEEP_FAULT_ON_MS, BEEP_FAULT_OFF_MS);
      break;
  }
}

/* =================== HELPERS ======================= */
void leds(bool r, bool y, bool g) {
  digitalWrite(PIN_LED_RED,   r);
  digitalWrite(PIN_LED_YELLOW,y);
  digitalWrite(PIN_LED_GREEN, g);
}

void updateLeds() {
  switch (state) {
    case IDLE_OPEN_GREEN:   leds(false, false, true);  break;
    case YELLOW_CLOSING:
    case YELLOW_CLOSED: {
      if (millis() - lastBlinkMs >= BLINK_INTERVAL_MS) { lastBlinkMs = millis(); yellowBlink = !yellowBlink; }
      leds(false, yellowBlink, false);
    } break;
    case RED_WAIT_CLEAR:    leds(true, false, false);  break;
    case OPENING_GREEN:     leds(false, false, true);  break;
    case FAULT_CLOSED: {
      if (millis() - lastBlinkMs >= BLINK_INTERVAL_MS) { lastBlinkMs = millis(); yellowBlink = !yellowBlink; }
      leds(false, yellowBlink, false);
    } break;
  }
}

uint16_t measureDistanceCm() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 25000UL);
  if (dur == 0) return 0;
  return (uint16_t)(dur / 58UL);
}

uint16_t median5(uint16_t a[]) {
  uint16_t m[MEDIAN_WINDOW];
  for (uint8_t i = 0; i < MEDIAN_WINDOW; i++) m[i] = a[i];
  for (uint8_t i = 1; i < MEDIAN_WINDOW; i++) {
    uint16_t key = m[i]; int8_t j = i - 1;
    while (j >= 0 && m[j] > key) { m[j + 1] = m[j]; j--; }
    m[j + 1] = key;
  }
  return m[MEDIAN_WINDOW / 2];
}

bool haveBuf() { return bufFill >= MEDIAN_WINDOW; }

uint16_t filteredDistance() {
  uint16_t d = measureDistanceCm();
  Serial.println("Distance Raw: "+String(d));
  if (d == 0 || d > 1000) badReadCount++; else badReadCount = 0;
  buf[bufIdx] = (d == 0 ? 1000 : d);
  bufIdx = (bufIdx + 1) % MEDIAN_WINDOW;
  if (bufFill < MEDIAN_WINDOW) bufFill++;
  return haveBuf() ? median5(buf) : (d == 0 ? 1000 : d);
}

void moveServoTowards(int target) {
  uint32_t now = millis();
  if (now - lastServoStepMs < SERVO_STEP_INTERVAL_MS) return;
  lastServoStepMs = now;

  if (currentAngle < target) currentAngle = min(currentAngle + SERVO_STEP, target);
  else if (currentAngle > target) currentAngle = max(currentAngle - SERVO_STEP, target);
  gate.write(currentAngle);
  gate2.write(currentAngle);
}

void enter(State s) {
  state = s;
  yellowBlink = false;
  lastBlinkMs = 0;
  
  // Reset buzzer toggling timing on state change (patterns start clean)
  lastBuzzToggleMs = millis();

  Serial.print("State changed to: ");
  switch (s) {
    case IDLE_OPEN_GREEN: Serial.println("IDLE_OPEN_GREEN (Gate Open, Green LED)"); break;
    case YELLOW_CLOSING:  Serial.println("YELLOW_CLOSING (Train detected, closing gate)"); break;
    case YELLOW_CLOSED:   Serial.println("YELLOW_CLOSED (Gate closed, train still near sensor)"); break;
    case RED_WAIT_CLEAR:  Serial.println("RED_WAIT_CLEAR (Train passed sensor, Red LED, waiting 15s)"); break;
    case OPENING_GREEN:   Serial.println("OPENING_GREEN (Opening gate)"); break;
    case FAULT_CLOSED:    Serial.println("FAULT_CLOSED (Sensor fault - Gate closed)"); break;
  }
}

/* =================== SETUP/LOOP ==================== */
void setup() {
  Serial.begin(9600);
  Serial.println("=== Railway Automatic Gate System (with Buzzer) ===");

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  gate.attach(PIN_SERVO);
  gate2.attach(PIN_SERVO2);
  currentAngle = SERVO_OPEN_DEG;
  gate.write(currentAngle);
  gate2.write(currentAngle);

  for (uint8_t i = 0; i < MEDIAN_WINDOW; i++) buf[i] = 300;
  enter(IDLE_OPEN_GREEN);
  updateLeds();
}

void loop() {
  uint32_t now = millis();

  // Distance sampling
  static uint16_t dist = 999;
  if (now - lastSampleMs >= MEAS_INTERVAL_MS) {
    lastSampleMs = now;
    dist = filteredDistance();
    Serial.print("Distance: ");
    Serial.print(dist);
    Serial.print(" cm | BadReads: ");
    Serial.println(badReadCount);

    bool near = (dist <= TRAIN_NEAR_CM);
    bool clear = (dist >= CLEAR_HYST_CM);

    if (badReadCount >= MAX_BAD_READS) {
      if (state != FAULT_CLOSED) enter(FAULT_CLOSED);
    } else if (state == FAULT_CLOSED && badReadCount == 0) {
      enter(RED_WAIT_CLEAR);
      redStartMs = now;
    }

    if (near)  { if (!nearSince)  nearSince  = now; }  else nearSince  = 0;
    if (clear) { if (!clearSince) clearSince = now; }  else clearSince = 0;

    // === State Machine ===
    switch (state) {
      case IDLE_OPEN_GREEN:
        if (nearSince && (now - nearSince >= DETECT_HOLD_MS)) enter(YELLOW_CLOSING);
        break;

      case YELLOW_CLOSING:
        if (currentAngle == SERVO_CLOSED_DEG) enter(YELLOW_CLOSED);
        break;

      case YELLOW_CLOSED:
        if (clearSince && (now - clearSince >= CLEAR_HOLD_MS)) {
          enter(RED_WAIT_CLEAR);
          redStartMs = now;
        }
        break;

      case RED_WAIT_CLEAR:
        if (nearSince && (now - nearSince >= DETECT_HOLD_MS)) enter(YELLOW_CLOSED);
        break;

      case OPENING_GREEN:
        if (nearSince && (now - nearSince >= DETECT_HOLD_MS)) enter(YELLOW_CLOSING);
        break;

      case FAULT_CLOSED:
        // stay closed
        break;
    }
  }

  // RED_WAIT_CLEAR countdown
  if (state == RED_WAIT_CLEAR && millis() - redStartMs >= RED_WAIT_MS) {
    Serial.println("15s elapsed → Opening gate...");
    enter(OPENING_GREEN);
  }

  // Servo motion
  if (state == YELLOW_CLOSING || state == YELLOW_CLOSED || state == RED_WAIT_CLEAR || state == FAULT_CLOSED) {
    moveServoTowards(SERVO_CLOSED_DEG);
  } else if (state == OPENING_GREEN) {
    moveServoTowards(SERVO_OPEN_DEG);
    if (currentAngle == SERVO_OPEN_DEG) {
      enter(IDLE_OPEN_GREEN);
    }
  }

  // LEDs + Buzzer patterns (non-blocking)
  updateLeds();
  updateBuzzer();
}
