/*
  Arduino Mega -> Many WS2812B/NeoPixel strips (one per pin)
  Sets ALL strips to full-brightness WHITE (after a brief OFF pass).

  Library: Adafruit_NeoPixel (Library Manager)
*/

#include <Adafruit_NeoPixel.h>

// ===== CONFIG =====
#define NUM_LEDS_PER_STRIP 106        // LEDs per strip
#define FULL_BRIGHTNESS    255        // 255 = max (watch power!)
#define PIXEL_TYPE         (NEO_GRB + NEO_KHZ800)

// Toggle these according to your wiring/usage:
#define USE_SERIAL            1       // set 0 if you want to include D0/D1
#define INCLUDE_EXTRA_PINS    0       // set 1 to also use {D0,D1,D20,D21} (no Serial/I2C)

// Safe pins set:
// - D2..D13, D14..D19
// - D22..D53
// - A0..A15 => D54..D69
const uint8_t SAFE_PINS[] = {
  // D2..D13
  2,3,4,5,6,7,8,9,10,11,12,13,
  // D14..D19 (UART pins; OK if those UARTs unused)
  14,15,16,17,18,19,
  // D22..D53
  22,23,24,25,26,27,28,29,30,31,32,33,
  34,35,36,37,38,39,40,41,42,43,44,45,
  46,47,48,49,50,51,52,53,
  // A0..A15 => 54..69
  54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69
};

// Optional pins if not using Serial (D0/D1) or I2C (D20/D21)
const uint8_t EXTRA_PINS[] = { 0, 1, 20, 21 };

template<typename T, size_t N>
constexpr size_t CountOf(const T (&)[N]) { return N; }

static inline void fillWhite(Adafruit_NeoPixel &s) {
  for (uint16_t i = 0; i < s.numPixels(); i++) s.setPixelColor(i, s.Color(255,255,255));
}
static inline void fillOff(Adafruit_NeoPixel &s) {
  for (uint16_t i = 0; i < s.numPixels(); i++) s.setPixelColor(i, 0);
}

static void drivePin(uint8_t pin, bool turnOn) {
  // Create a temporary strip object for THIS pin only (RAM-safe).
  Adafruit_NeoPixel strip(NUM_LEDS_PER_STRIP, pin, PIXEL_TYPE);
  strip.begin();
  strip.setBrightness(FULL_BRIGHTNESS); // 255 = no dimming
  if (turnOn) fillWhite(strip); else fillOff(strip);
  strip.show(); // latch
}

void setup() {
#if USE_SERIAL
  Serial.begin(115200);
  while (!Serial) {;}
  Serial.println(F("Clearing all strips, then setting WHITE..."));
#endif

  // Pass 1: turn everything OFF (optional, for a clean start)
  for (size_t i = 0; i < CountOf(SAFE_PINS); i++) { drivePin(SAFE_PINS[i], false); delay(10); }
#if INCLUDE_EXTRA_PINS && !USE_SERIAL
  for (size_t i = 0; i < CountOf(EXTRA_PINS); i++) { drivePin(EXTRA_PINS[i], false); delay(10); }
#endif

  // Pass 2: set everything to WHITE at full brightness
  for (size_t i = 0; i < CountOf(SAFE_PINS); i++) { drivePin(SAFE_PINS[i], true); delay(10); }
#if INCLUDE_EXTRA_PINS && !USE_SERIAL
  for (size_t i = 0; i < CountOf(EXTRA_PINS); i++) { drivePin(EXTRA_PINS[i], true); delay(10); }
#endif

#if USE_SERIAL
  Serial.println(F("Done."));
#endif
}

void loop() {
  // Nothing needed — LEDs stay latched until updated again.
}
