/*
 * ============================================================
 *  SMART TABLE — ESP32 Controller  v2
 * ============================================================
 *  Hardware:
 *    • WS2812B LED strip     – 6 m, 360 LEDs @ 60/m  → GPIO 5
 *    • WS2812B Ring (16 LED) – Temperature indicator  → GPIO 25
 *    • TTP223 Touch Sensor   – OUT → GPIO 27  (HIGH = touched)
 *    • DS18B20 Temp Sensor   – DATA → GPIO 4  (4.7 kΩ pull-up)
 *    • VL53L0X ToF Sensor    – SDA → GPIO 21, SCL → GPIO 22
 *
 *  Touch gestures:
 *    Single tap   → Soft Amber (255,179,102) — stays on
 *    Double tap   → Focus sequence:
 *                    Phase 1: Cyan           — 52 minutes
 *                    Phase 2: Fade → Gold    — 10 seconds
 *                    Phase 3: Sage Green     — 15 minutes
 *                    Then auto-off.
 *    Long press   → OFF
 *
 *  Temperature ring (always active when strip is on):
 *    Hot  (> 30 °C)  → Red-Orange  (255, 69,  0)
 *    Ideal(22–30 °C) → Gold-Orange (255,165,  0)
 *    Cold (< 22 °C)  → Deep Blue   (  0,  0,255)
 *
 *  VL53L0X controls strip brightness (50–500 mm → dim–full).
 *
 *  Libraries:  Adafruit_NeoPixel, OneWire, DallasTemperature,
 *              Adafruit_VL53L0X
 * ============================================================
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_VL53L0X.h>

// ─── PIN DEFINITIONS ────────────────────────────────────────
#define STRIP_PIN       5       // Main WS2812B strip data
#define RING_PIN        25      // Temperature ring data
#define TOUCH_OUT_PIN   27      // TTP223 OUT
#define DS18B20_PIN     4       // OneWire data
#define SDA_PIN         21      // I²C SDA (VL53L0X)
#define SCL_PIN         22      // I²C SCL (VL53L0X)

// ─── LED COUNTS ─────────────────────────────────────────────
#define NUM_STRIP       360     // 6 m × 60 LEDs/m
#define NUM_RING        16      // Circular temperature PCB

#define MAX_BRIGHTNESS  255
#define MIN_BRIGHTNESS  20

// ─── NeoPixel OBJECTS ───────────────────────────────────────
Adafruit_NeoPixel strip(NUM_STRIP, STRIP_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ring(NUM_RING,   RING_PIN,  NEO_GRB + NEO_KHZ800);

// ─── OPERATING MODES ───────────────────────────────────────
enum Mode {
  MODE_OFF,
  MODE_AMBIENT,     // Single tap  → solid soft amber
  MODE_FOCUS        // Double tap  → 3-phase timed sequence
};

Mode currentMode = MODE_OFF;

// ─── FOCUS SEQUENCE PHASES ──────────────────────────────────
enum FocusPhase {
  FOCUS_CYAN,       // Phase 1 — 52 minutes
  FOCUS_FADE,       // Phase 2 — 10 second cross-fade cyan → gold
  FOCUS_SAGE,       // Phase 3 — 15 minutes sage green
  FOCUS_DONE        // Auto-off
};

FocusPhase    focusPhase       = FOCUS_CYAN;
unsigned long focusPhaseStart  = 0;

#define FOCUS_CYAN_MS    (5UL * 60UL * 1000UL)   // 52 minutes
#define FOCUS_FADE_MS    (15UL * 1000UL)           // 10 seconds
#define FOCUS_SAGE_MS    (2UL * 60UL * 1000UL)    // 15 minutes

// ─── TOUCH DETECTION (TTP223) ───────────────────────────────
#define LONG_PRESS_MS       2000 //0.8
#define DOUBLE_TAP_GAP_MS   400
#define DEBOUNCE_MS         50

bool          prevTouchState   = false;
unsigned long touchStartTime   = 0;
unsigned long lastReleaseTime  = 0;
uint8_t       pendingTaps      = 0;
bool          longPressHandled = false;

// ─── FADE-OUT STATE ─────────────────────────────────────────
uint8_t fadeLevel = 0;

// ─── SENSOR OBJECTS ─────────────────────────────────────────
OneWire           oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);
Adafruit_VL53L0X  tofSensor = Adafruit_VL53L0X();

// ─── SENSOR STATE ───────────────────────────────────────────
float    currentTempC      = 25.0;
uint16_t currentDistanceMM = 400;
uint8_t  mappedBrightness  = MAX_BRIGHTNESS;

// ─── TIMING ─────────────────────────────────────────────────
unsigned long lastTempRead = 0;
unsigned long lastDistRead = 0;
#define TEMP_INTERVAL_MS   2000
#define DIST_INTERVAL_MS   100

/* ============================================================
 *  COLOUR HELPERS
 * ============================================================ */

static uint8_t colR(uint32_t c) { return (c >> 16) & 0xFF; }
static uint8_t colG(uint32_t c) { return (c >>  8) & 0xFF; }
static uint8_t colB(uint32_t c) { return  c        & 0xFF; }

// Linear interpolate between two colours.  frac = 0→255
static uint32_t lerpColor(uint32_t a, uint32_t b, uint8_t frac) {
  uint8_t r  = colR(a) + (((int16_t)colR(b) - colR(a)) * frac / 255);
  uint8_t g  = colG(a) + (((int16_t)colG(b) - colG(a)) * frac / 255);
  uint8_t bl = colB(a) + (((int16_t)colB(b) - colB(a)) * frac / 255);
  return Adafruit_NeoPixel::Color(r, g, bl);
}

// Scale brightness of a packed colour (0–255)
static uint32_t scaleColor(uint32_t c, uint8_t s) {
  uint8_t r = (uint8_t)(((uint16_t)colR(c) * s) >> 8);
  uint8_t g = (uint8_t)(((uint16_t)colG(c) * s) >> 8);
  uint8_t b = (uint8_t)(((uint16_t)colB(c) * s) >> 8);
  return Adafruit_NeoPixel::Color(r, g, b);
}

static void fillStrip(uint32_t c) {
  for (int i = 0; i < NUM_STRIP; i++) strip.setPixelColor(i, c);
}

static void fillRing(uint32_t c) {
  for (int i = 0; i < NUM_RING; i++) ring.setPixelColor(i, c);
}

/* ============================================================
 *  SETUP
 * ============================================================ */
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[SmartTable] Booting…"));

  // — Main LED strip —
  strip.begin();
  strip.setBrightness(MAX_BRIGHTNESS);
  strip.clear();
  strip.show();

  // — Temperature ring —
  ring.begin();
  ring.setBrightness(MAX_BRIGHTNESS);
  ring.clear();
  ring.show();

  // — Touch sensor —
  pinMode(TOUCH_OUT_PIN, INPUT_PULLDOWN);

  // — DS18B20 —
  tempSensor.begin();
  tempSensor.setResolution(10);
  tempSensor.setWaitForConversion(false);
  tempSensor.requestTemperatures();

  // — VL53L0X —
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!tofSensor.begin()) {
    Serial.println(F("[VL53L0X] FAILED — check wiring!"));
  } else {
    Serial.println(F("[VL53L0X] OK"));
  }

  Serial.println(F("[SmartTable] Ready."));
}

/* ============================================================
 *  MAIN LOOP
 * ============================================================ */
void loop() {
  handleTouch();
  readSensors();
  updateBrightness();
  renderStrip();
  renderTempRing();

  strip.show();
  ring.show();
}

/* ────────────────────────────────────────────────────────────
 *  TOUCH GESTURE ENGINE  (TTP223)
 *
 *  Single tap   → Ambient (soft amber, stays on)
 *  Double tap   → Focus   (3-phase timed sequence)
 *  Long press   → OFF
 * ──────────────────────────────────────────────────────────── */
void handleTouch() {
  unsigned long now = millis();
  bool touched = digitalRead(TOUCH_OUT_PIN) == HIGH;

  // ── RISING EDGE ──
  if (touched && !prevTouchState) {
    touchStartTime   = now;
    longPressHandled = false;
  }

  // ── HELD → long press ──
  if (touched && !longPressHandled) {
    if (now - touchStartTime >= LONG_PRESS_MS) {
      Serial.println(F("[Touch] Long press → OFF"));
      currentMode      = MODE_OFF;
      longPressHandled = true;
      pendingTaps      = 0;
      fadeLevel         = 255;   // begin smooth fade-out
    }
  }

  // ── FALLING EDGE ──
  if (!touched && prevTouchState) {
    unsigned long dur = now - touchStartTime;
    if (!longPressHandled && dur >= DEBOUNCE_MS) {
      pendingTaps++;
      lastReleaseTime = now;
      Serial.printf("[Touch] Tap #%u  (%lu ms)\n", pendingTaps, dur);
    }
  }

  // ── EVALUATE after gap ──
  if (pendingTaps > 0 && !touched) {
    if (now - lastReleaseTime > DOUBLE_TAP_GAP_MS) {
      if (pendingTaps >= 2) {
        // ── DOUBLE TAP → FOCUS (start phase 1) ──
        Serial.println(F("[Touch] Double tap → FOCUS (Cyan)"));
        currentMode     = MODE_FOCUS;
        focusPhase      = FOCUS_CYAN;
        focusPhaseStart = now;
      } else {
        // ── SINGLE TAP → AMBIENT ──
        Serial.println(F("[Touch] Single tap → AMBIENT (Soft Amber)"));
        currentMode = MODE_AMBIENT;
      }
      pendingTaps = 0;
    }
  }

  prevTouchState = touched;
}

/* ────────────────────────────────────────────────────────────
 *  SENSOR READS  (non-blocking)
 * ──────────────────────────────────────────────────────────── */
void readSensors() {
  unsigned long now = millis();

  // — Temperature —
  if (now - lastTempRead >= TEMP_INTERVAL_MS) {
    lastTempRead = now;
    if (tempSensor.isConversionComplete()) {
      currentTempC = tempSensor.getTempCByIndex(0);
      if (currentTempC == DEVICE_DISCONNECTED_C) currentTempC = 25.0;
      tempSensor.requestTemperatures();
      Serial.printf("[DS18B20] %.1f °C\n", currentTempC);
    }
  }

  // — Distance —
  if (now - lastDistRead >= DIST_INTERVAL_MS) {
    lastDistRead = now;
    VL53L0X_RangingMeasurementData_t measure;
    tofSensor.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      currentDistanceMM = measure.RangeMilliMeter;
    }
  }
}

/* ────────────────────────────────────────────────────────────
 *  BRIGHTNESS — VL53L0X distance → strip brightness only
 *  (ring stays at full so the temperature indicator is
 *   always clearly visible regardless of hand position)
 * ──────────────────────────────────────────────────────────── */
void updateBrightness() {
  if (currentMode == MODE_OFF) return;

  uint16_t d = constrain(currentDistanceMM, 50, 500);
  mappedBrightness = map(d, 50, 500, MAX_BRIGHTNESS, MIN_BRIGHTNESS);
  strip.setBrightness(mappedBrightness);
}

/* ────────────────────────────────────────────────────────────
 *  MAIN STRIP RENDERING
 * ──────────────────────────────────────────────────────────── */
void renderStrip() {
  switch (currentMode) {

    case MODE_OFF:
      renderOff();
      break;

    case MODE_AMBIENT:
      // Solid soft amber — no animation, stays until next command
      fillStrip(strip.Color(255, 179, 102));
      break;

    case MODE_FOCUS:
      renderFocus();
      break;
  }
}

/* ── OFF: smooth fade-out then black ─────────────────────── */
void renderOff() {
  if (fadeLevel > 0) {
    fadeLevel = (fadeLevel > 8) ? fadeLevel - 8 : 0;
    for (int i = 0; i < NUM_STRIP; i++) {
      uint32_t c = strip.getPixelColor(i);
      strip.setPixelColor(i, scaleColor(c, fadeLevel));
    }
  } else {
    strip.clear();
  }
}

/* ────────────────────────────────────────────────────────────
 *  FOCUS MODE — 3-phase timed sequence
 *
 *  Phase 1:  Cyan (0,255,255)           for 52 minutes
 *  Phase 2:  Cross-fade Cyan → Gold     for 10 seconds
 *  Phase 3:  Sage Green (143,188,143)   for 15 minutes
 *  Then:     Smooth fade to off
 * ──────────────────────────────────────────────────────────── */
void renderFocus() {
  unsigned long now     = millis();
  unsigned long elapsed = now - focusPhaseStart;

  // Colour constants (computed once per call, not per pixel)
  const uint32_t COL_CYAN  = strip.Color(  0, 255, 255);
  const uint32_t COL_GOLD  = strip.Color(255, 215,   0);
  const uint32_t COL_SAGE  = strip.Color(143, 188, 143);

  switch (focusPhase) {

    // ── Phase 1: solid cyan for 52 minutes ──
    case FOCUS_CYAN:
      fillStrip(COL_CYAN);
      if (elapsed >= FOCUS_CYAN_MS) {
        focusPhase      = FOCUS_FADE;
        focusPhaseStart = now;
        Serial.println(F("[Focus] → Phase 2: Fading to Gold"));
      }
      break;

    // ── Phase 2: 10-second smooth cross-fade cyan → gold ──
    case FOCUS_FADE: {
      uint8_t frac = (elapsed >= FOCUS_FADE_MS)
                      ? 255
                      : (uint8_t)(elapsed * 255UL / FOCUS_FADE_MS);
      uint32_t blended = lerpColor(COL_CYAN, COL_GOLD, frac);
      fillStrip(blended);
      if (elapsed >= FOCUS_FADE_MS) {
        focusPhase      = FOCUS_SAGE;
        focusPhaseStart = now;
        Serial.println(F("[Focus] → Phase 3: Sage Green"));
      }
      break;
    }

    // ── Phase 3: sage green for 15 minutes ──
    case FOCUS_SAGE:
      fillStrip(COL_SAGE);
      if (elapsed >= FOCUS_SAGE_MS) {
        focusPhase = FOCUS_CYAN;
        Serial.println(F("[Focus] Sequence complete → OFF"));
      }
      break;

    // ── Sequence finished: auto turn off ──
    case FOCUS_DONE:
      currentMode = MODE_OFF;
      fadeLevel   = 255;   // smooth fade-out
      break;
  }
}

/* ────────────────────────────────────────────────────────────
 *  TEMPERATURE RING  (16-LED WS2812B circular PCB, GPIO 25)
 *
 *  Active whenever the main strip is on (any mode).
 *  Shows a gentle breathing pulse in the temperature colour.
 *
 *    Hot  (> 30 °C)   →  Red-Orange  (255, 69,  0)
 *    Ideal (22–30 °C) →  Gold-Orange (255,165,  0)
 *    Cold (< 22 °C)   →  Deep Blue   (  0,  0,255)
 * ──────────────────────────────────────────────────────────── */
void renderTempRing() {
  // Ring fades out in sync when strip is off
  if (currentMode == MODE_OFF) {
    if (fadeLevel > 0) {
      for (int i = 0; i < NUM_RING; i++) {
        uint32_t c = ring.getPixelColor(i);
        ring.setPixelColor(i, scaleColor(c, fadeLevel));
      }
    } else {
      ring.clear();
    }
    return;
  }

  // Choose colour based on temperature
  uint32_t tempCol;
  if (currentTempC > 60.0) {
    tempCol = ring.Color(255,  69,  0);   // Hot  — Red-Orange
  } else if (currentTempC >= 45.0) {
    tempCol = ring.Color(255, 165,  0);   // Ideal — Gold-Orange
  } else {
    tempCol = ring.Color(  0,   0, 255);  // Cold — Deep Blue
  }

  // Gentle breathing pulse so the ring feels alive
  uint8_t phase  = (millis() / 15) & 0xFF;
  float   sine   = (sin(phase * 3.14159f * 2.0f / 256.0f) + 1.0f) * 0.5f;
  uint8_t breath = 180 + (uint8_t)(sine * 75.0f);   // range 180–255

  uint32_t col = scaleColor(tempCol, breath);
  fillRing(col);
}
