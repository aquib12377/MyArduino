/*
  Pro Micro (ATmega32U4) – 2x Ultrasonic → Keyboard
  Sensor1: send 'A' once per 1-inch step closer (from 50")
  Sensor2: send 'B' once per 1-inch step closer (from 50")
*/

#include <Arduino.h>
#include <Keyboard.h>

/* ----------------- CONFIG ----------------- */
const uint8_t S1_TRIG = 9;
const uint8_t S1_ECHO = 8;
const uint8_t S2_TRIG = 7;
const uint8_t S2_ECHO = 6;

const char KEY_S1 = 'A';
const char KEY_S2 = 'B';

const int START_INCH       = 50;     // start counting from 50"
const int MIN_VALID_INCH   = 2;      // ignore ultra-close noise
const int MAX_VALID_INCH   = 200;    // ignore far echoes
const unsigned long PULSE_TIMEOUT_US = 25000UL; // pulseIn timeout
const uint8_t SAMPLES = 5;           // median-of-N for stability

/* ------------- TYPES ------------- */
struct SensorState {
  uint8_t trigPin;
  uint8_t echoPin;
  char key;
  int lastBoundary;     // last whole inch boundary we sent at
  int lastSeenInch;     // last measured distance
  unsigned long lastSendMs;
};

/* ------------- FORWARD DECLS ------------- */
static inline void pingOnce(uint8_t trigPin);
static unsigned long echoTime(uint8_t echoPin);
static int usToInches(unsigned long us);
static int readInches(uint8_t trigPin, uint8_t echoPin);
static void setupSensorPins(const SensorState& s);
static void sendKey(char c);
static void processSensor(SensorState &s);

/* ------------- GLOBALS ------------- */
SensorState s1{S1_TRIG, S1_ECHO, KEY_S1, START_INCH, START_INCH, 0};
SensorState s2{S2_TRIG, S2_ECHO, KEY_S2, START_INCH, START_INCH, 0};

/* ------------- IMPL ------------- */
static inline void pingOnce(uint8_t trigPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
}

static unsigned long echoTime(uint8_t echoPin) {
  return pulseIn(echoPin, HIGH, PULSE_TIMEOUT_US);
}

static int usToInches(unsigned long us) {
  if (us == 0) return -1;
  // HC-SR04: inches ≈ us / 148 (round-trip); +74 for rounding
  return (int)((us + 74) / 148);
}

static int readInches(uint8_t trigPin, uint8_t echoPin) {
  unsigned long buf[SAMPLES];
  for (uint8_t i = 0; i < SAMPLES; i++) {
    pingOnce(trigPin);
    buf[i] = echoTime(echoPin);
    delay(5);
  }
  // insertion sort (small array)
  for (uint8_t i = 1; i < SAMPLES; i++) {
    unsigned long key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = key;
  }
  int inches = usToInches(buf[SAMPLES / 2]);
  if (inches < MIN_VALID_INCH || inches > MAX_VALID_INCH) return -1;
  return inches;
}

static void setupSensorPins(const SensorState& s) {
  pinMode(s.trigPin, OUTPUT);
  pinMode(s.echoPin, INPUT);
  digitalWrite(s.trigPin, LOW);
}

static void sendKey(char c) {
  Keyboard.press(c);
  delay(5);
  Keyboard.release(c);
}

static void processSensor(SensorState &s) {
  int dIn = readInches(s.trigPin, s.echoPin);
  if (dIn == -1) return;

  s.lastSeenInch = dIn;
  int cur = min(dIn, START_INCH);

  if (cur < s.lastBoundary) {
    for (int boundary = s.lastBoundary - 1; boundary >= cur; --boundary) {
      sendKey(s.key);
      s.lastSendMs = millis();
      delay(15);
    }
    s.lastBoundary = cur;
    return;
  }

  if (cur > s.lastBoundary + 1) {
    s.lastBoundary = cur; // re-arm when moved away ≥2"
  }
}

void setup() {
  setupSensorPins(s1);
  setupSensorPins(s2);
  Keyboard.begin();
  delay(500);
}

void loop() {
  processSensor(s1);
  processSensor(s2);
  delay(25);
}
