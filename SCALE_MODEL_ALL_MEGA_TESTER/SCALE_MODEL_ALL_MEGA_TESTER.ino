#include <FastLED.h>

// ── Config ────────────────────────────────────────────────────────────────────
#define NUM_LEDS    600
#define BRIGHTNESS  150
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

// ── Pin list (X-macro) ────────────────────────────────────────────────────────
#define STRIP_PIN_LIST \
  X(13)  X(12)  X(11)  X(10)  X(9)   X(8)   X(7)   X(6)   X(5)   X(4)   X(3)   X(2) 
  //   X(14)  X(15)  X(16)  X(17)  X(18)  X(19)  X(22)  X(23)  X(25)  X(27)  X(29)  X(31) \
  // X(33)  X(35)  X(37)  X(39)  X(41)  X(43)  X(45)  X(47)  X(49)  X(51)  X(53)  X(52) \
  // X(A15) X(A14) X(A13) X(A12) X(A11) X(A10) X(A9)  X(A8)  X(A7)  X(A6)  X(A5)  X(A4) \
  // X(A3)  X(A2)  X(A1)  X(A0)  \
  // X(50)  X(48)  X(46)  X(44)  X(42)  X(40)  X(38)  X(36)  X(34)  X(32)

// ── Derived constants ─────────────────────────────────────────────────────────
#define X(p) 1+
constexpr uint8_t TOTAL_STRIPS = (STRIP_PIN_LIST 0);
#undef X

#define X(p) p,
const uint8_t stripPins[TOTAL_STRIPS] = { STRIP_PIN_LIST };
#undef X

// ── Memory ────────────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];
CRGB dormant[1] = { CRGB::Black };

CLEDController* controllers[TOTAL_STRIPS];

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  FastLED.setBrightness(BRIGHTNESS);

  uint8_t idx = 0;
  #define X(PIN) \
    controllers[idx++] = &FastLED.addLeds<LED_TYPE, PIN, COLOR_ORDER>(dormant, 1);
  STRIP_PIN_LIST
  #undef X

  Serial.print(F("Ready — "));
  Serial.print(TOTAL_STRIPS);
  Serial.println(F(" strips registered."));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void activateStrip(uint8_t i) {
  controllers[i]->setLeds(leds, NUM_LEDS);
}

void parkStrip(uint8_t i) {
  controllers[i]->setLeds(dormant, 1);
}

void parkAllStrips() {
  for (uint8_t i = 0; i < TOTAL_STRIPS; i++) {
    parkStrip(i);
  }
}

// ── Phase 1: One strip at a time, 1 second each ───────────────────────────────
void phase1() {
  Serial.println(F("=== PHASE 1: One strip at a time ==="));

  for (uint8_t i = 0; i < TOTAL_STRIPS; i++) {
    Serial.print(F("Strip #"));
    Serial.print(i);
    Serial.print(F("  pin "));
    Serial.println(stripPins[i]);

    // Activate this strip and light it white
    activateStrip(i);
    fill_solid(leds, NUM_LEDS, CRGB::White);
    FastLED.show();

    delay(1000); // 1 second ON

    // Blank and park before moving on
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(30); // short settle gap between strips
    parkStrip(i);
  }
}

// ── Phase 2: All strips ON simultaneously for 20 seconds ─────────────────────
void phase2() {
  Serial.println(F("=== PHASE 2: All strips ON for 20 s ==="));

  // Point every controller at the shared leds[] buffer
  for (uint8_t i = 0; i < TOTAL_STRIPS; i++) {
    activateStrip(i);
  }

  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show(); // one show() drives all strips at once

  delay(20000); // 20 seconds

  // Blank everything
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(50);

  // Park all strips back on dormant buffer
  parkAllStrips();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  phase1();
  phase2();

  Serial.println(F("── Cycle complete. Restarting in 3 s ──"));
  delay(500);
}