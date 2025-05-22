#include <FastLED.h>

#define NUM_LEDS           73
#define DATA_PIN           21
#define FLOORS             18
#define LEDS_PER_FLOOR      3
#define GAP                 1   // gap LEDs between floors

#define MAX_BRIGHTNESS    255   // peak brightness
#define FADE_DELAY         5   // ms between steps

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
}

void loop() {
  // Phase 1: even floors fade up 0→MAX, odd floors fade down MAX→0
  for (int level = 0; level <= MAX_BRIGHTNESS; ++level) {
    applyFade(0, MAX_BRIGHTNESS - level);
    FastLED.show();
    delay(FADE_DELAY);
  }
  delay(500);
for (int level = 0; level <= MAX_BRIGHTNESS; ++level) {
    applyFade(level, 0);
    FastLED.show();
    delay(FADE_DELAY);
  }
  delay(500);
  // Phase 2: odd floors fade up 0→MAX, even floors fade down MAX→0
  for (int level = 0; level <= MAX_BRIGHTNESS; ++level) {
    applyFade(MAX_BRIGHTNESS - level, 0);
    FastLED.show();
    delay(FADE_DELAY);
  }
  delay(500);
  for (int level = 0; level <= MAX_BRIGHTNESS; ++level) {
    applyFade(0, level);
    FastLED.show();
    delay(FADE_DELAY);
  }
  delay(500);

    fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (int f = 0; f < FLOORS; ++f) {
    // clear all LEDs

    // compute start index of this floor
    int start = f * (LEDS_PER_FLOOR + GAP);
    if (start + LEDS_PER_FLOOR > NUM_LEDS) break;

    // light up this floor in warm-yellow (hue=60, sat=128)
    for (int i = 0; i < LEDS_PER_FLOOR; ++i) {
      leds[start + i] = CHSV(60, 128, MAX_BRIGHTNESS);
    }

    FastLED.show();
    delay(250);
  }
  delay(500);
    fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (int f = FLOORS - 1; f >= FLOORS; --f) {
    // clear all LEDs

    // compute start index of this floor
    int start = f * (LEDS_PER_FLOOR + GAP);
    if (start + LEDS_PER_FLOOR > NUM_LEDS) break;

    // light up this floor in warm-yellow (hue=60, sat=128)
    for (int i = 0; i < LEDS_PER_FLOOR; ++i) {
      leds[start + i] = CHSV(60, 128, MAX_BRIGHTNESS);
    }

    FastLED.show();
    delay(250);
  }
  delay(500);

}

// evenB = brightness for even floors, oddB = for odd floors
void applyFade(uint8_t evenB, uint8_t oddB) {
  for (int floor = 0; floor < FLOORS; ++floor) {
    int start = floor * (LEDS_PER_FLOOR + GAP);
    if (start + LEDS_PER_FLOOR > NUM_LEDS) break;

    uint8_t b = (floor % 2 == 0) ? evenB : oddB;
    // CHSV(hue=42≈yellow, sat=128 for a warm‐white tint, val=b)
    for (int i = 0; i < LEDS_PER_FLOOR; ++i) {
      leds[start + i] = CHSV(60, 128, b);
    }
  }
}
