// ============================================================
//  WS2812B All-Pin Test  —  Arduino Mega 2560
//  Library: Adafruit NeoPixel  (install via Library Manager)
//
//  62 independent data pins, 1 LED per pin by default.
//  Change LEDS_PER_STRIP if each pin drives a longer strip.
//
//  Sequence:
//    1. All WHITE  (3 s)
//    2. Smooth blend → rainbow  (≈2 s, 256 steps × 8 ms)
//    3. Moving rainbow at 60 % brightness (loops forever)
//
//  Analog-pin mapping on Mega:
//    A0=54  A1=55  A2=56  A3=57  A4=58  A5=59  A6=60  A7=61
//    A8=62  A9=63  A10=64 A11=65 A12=66 A13=67 A14=68 A15=69
//
//  NOTE: Pins 14-19 share UART TX/RX lines; do NOT call
//        Serial1/2/3.begin() while driving LEDs on those pins.
// ============================================================

#include <Adafruit_NeoPixel.h>

// ── User-adjustable constants ────────────────────────────────
#define LEDS_PER_STRIP    250       // LEDs daisy-chained on each pin
#define NUM_STRIPS        10
#define BRIGHTNESS        250     // 60 % of 255  (0-255)
#define WHITE_HOLD_MS     15000    // duration of the white phase
#define TRANS_STEP_MS     8       // ms per transition frame (~2 s total)
#define RAINBOW_STEP_MS   20      // ms per rainbow frame (lower = faster)
// ────────────────────────────────────────────────────────────

// Pin list — order matches strip index 0…61
const uint8_t PINS[NUM_STRIPS] = {
  // Digital 13 → 2
  13, 12, 11, 10,  9,  8,  7,  6,  5,  4
};

// One NeoPixel object per strip
Adafruit_NeoPixel strips[NUM_STRIPS];

// ── State machine ────────────────────────────────────────────
enum Phase { PH_WHITE, PH_TRANSITION, PH_RAINBOW };
Phase    phase          = PH_WHITE;
uint32_t phaseStart     = 0;
uint32_t lastFrameMs    = 0;
uint8_t  transBlend     = 0;     // 0 → 255 during transition
uint16_t rainbowOffset  = 0;     // 16-bit hue offset (0-65535)

// ── Helpers ──────────────────────────────────────────────────

// Linear blend of two 8-bit values: 0 = 100 % 'from', 255 = 100 % 'to'
static inline uint8_t blend8(uint8_t from, uint8_t to, uint8_t amount) {
  return (uint8_t)(from + ((int16_t)(to - from) * amount / 255));
}

// Set the same packed color on every LED of every strip, then show all
void showAll(uint32_t color) {
  for (int s = 0; s < NUM_STRIPS; s++) {
    for (int p = 0; p < LEDS_PER_STRIP; p++) {
      strips[s].setPixelColor(p, color);
    }
    strips[s].show();
  }
}

// ── setup ────────────────────────────────────────────────────
void setup() {
  for (int s = 0; s < NUM_STRIPS; s++) {
    strips[s] = Adafruit_NeoPixel(LEDS_PER_STRIP, PINS[s],
                                  NEO_GRB + NEO_KHZ800);
    strips[s].begin();
    strips[s].setBrightness(BRIGHTNESS);
  }

  // Phase 1 — all white immediately
  showAll(Adafruit_NeoPixel::Color(255, 255, 255));

  phaseStart  = millis();
  lastFrameMs = millis();
}

// ── loop ─────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // ── Phase 1: hold white ────────────────────────────────────
  if (phase == PH_WHITE) {
    if (now - phaseStart >= WHITE_HOLD_MS) {
      phase       = PH_RAINBOW;
      transBlend  = 0;
      lastFrameMs = now;
    }
  }
  // ── Phase 3: moving rainbow (loops forever) ───────────────
  else if (phase == PH_RAINBOW) {
    if (now - lastFrameMs >= RAINBOW_STEP_MS) {
      lastFrameMs = now;

      // Advance the hue wheel by ~256 units per frame for smooth motion
      rainbowOffset += 256;

      uint16_t totalLeds = (uint16_t)NUM_STRIPS * LEDS_PER_STRIP;

      for (int s = 0; s < NUM_STRIPS; s++) {
        for (int p = 0; p < LEDS_PER_STRIP; p++) {
          uint16_t ledIndex = (uint16_t)s * LEDS_PER_STRIP + p;

          // Spread the full hue wheel across all LEDs, then roll with offset
          uint16_t hue = rainbowOffset
                       + (uint16_t)((uint32_t)ledIndex * 65535 / totalLeds);

          uint32_t color = Adafruit_NeoPixel::ColorHSV(hue, 255, 255);
          strips[s].setPixelColor(p, color);
        }
        strips[s].show();
      }
    }
  }
}
