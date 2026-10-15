/*
  Arduino Mega -> Many WS2812B/NeoPixel strips (one per pin)
  Cycles through shades of BLUE only, then shades of YELLOW only.
  No fusion/blending between the two colors.
  Each shade is shown solid across ALL LEDs, line by line.

  Library: Adafruit_NeoPixel (Library Manager)
*/

#include <Adafruit_NeoPixel.h>

// ===== CONFIG =====
#define NUM_LEDS_PER_STRIP  110
#define FULL_BRIGHTNESS     200
#define PIXEL_TYPE          (NEO_GRB + NEO_KHZ800)

#define USE_SERIAL          1
#define INCLUDE_EXTRA_PINS  1

// ===== TIMING =====
#define SHADE_HOLD_MS   400   // how long each shade is displayed (ms)
#define FADE_STEPS       5   // interpolation steps between two shades of SAME color
#define STEP_DELAY_MS    15   // ms per interpolation step

// ===== BLUE SHADES (no yellow at all) =====
struct Shade { uint8_t r, g, b; const char* name; };

const Shade BLUE_SHADES[] = {
  {   0, 120, 255, "Dodger Blue"     },
  {   0, 160, 255, "Sky Blue"        },
  {   0, 200, 255, "Deep Sky Blue"   },
  {   0, 220, 255, "Light Blue"      },
};

// ===== YELLOW SHADES (no blue at all) =====
const Shade YELLOW_SHADES[] = {
  {  30,  20,   0, "Dark Brown-Gold" },
  {  80,  50,   0, "Dark Goldenrod"  },
  { 130,  90,   0, "Olive Gold"      },
  { 180, 130,   0, "Golden"          },
  { 210, 160,   0, "Amber"           },
  { 230, 190,   0, "Deep Yellow"     },
  { 240, 210,   0, "Yellow"          },
  { 248, 225,   0, "Bright Yellow"   },
  { 255, 240,  20, "Pure Yellow"     },
  { 255, 255,  80, "Pale Yellow"     },
};

const size_t NUM_BLUE   = sizeof(BLUE_SHADES)   / sizeof(BLUE_SHADES[0]);
const size_t NUM_YELLOW = sizeof(YELLOW_SHADES) / sizeof(YELLOW_SHADES[0]);

template<typename T, size_t N>
constexpr size_t CountOf(const T (&)[N]) { return N; }

const uint8_t SAFE_PINS[] = {
  2,3,4,5,6,7,8,9,10,11,12,13,
  14,15,16,17,18,19,
  22,23,24,25,26,27,28,29,30,31,32,33,
  34,35,36,37,38,39,40,41,42,43,44,45,
  46,47,48,49,50,51,52,53,
  54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69
};

const uint8_t EXTRA_PINS[] = { 0, 1, 20, 21 };

// ===== Fill ALL strips with one solid color =====
static void setAllSolid(uint8_t r, uint8_t g, uint8_t b) {
  for (size_t i = 0; i < CountOf(SAFE_PINS); i++) {
    Adafruit_NeoPixel strip(NUM_LEDS_PER_STRIP, SAFE_PINS[i], PIXEL_TYPE);
    strip.begin();
    strip.setBrightness(FULL_BRIGHTNESS);
    for (uint16_t p = 0; p < strip.numPixels(); p++)
      strip.setPixelColor(p, strip.Color(r, g, b));
    strip.show();
  }
#if INCLUDE_EXTRA_PINS && !USE_SERIAL
  for (size_t i = 0; i < CountOf(EXTRA_PINS); i++) {
    Adafruit_NeoPixel strip(NUM_LEDS_PER_STRIP, EXTRA_PINS[i], PIXEL_TYPE);
    strip.begin();
    strip.setBrightness(FULL_BRIGHTNESS);
    for (uint16_t p = 0; p < strip.numPixels(); p++)
      strip.setPixelColor(p, strip.Color(r, g, b));
    strip.show();
  }
#endif
}

// ===== Smooth fade between two shades of the SAME color family =====
static void fadeBetween(const Shade& from, const Shade& to) {
  for (int step = 0; step <= FADE_STEPS; step++) {
    float   t = (float)step / FADE_STEPS;
    uint8_t r = (uint8_t)(from.r + t * ((int)to.r - from.r));
    uint8_t g = (uint8_t)(from.g + t * ((int)to.g - from.g));
    uint8_t b = (uint8_t)(from.b + t * ((int)to.b - from.b));

    setAllSolid(r, g, b);

#if USE_SERIAL
    Serial.print(F("  Step "));
    Serial.print(step);
    Serial.print(F("/"));
    Serial.print(FADE_STEPS);
    Serial.print(F("  RGB("));
    Serial.print(r); Serial.print(F(", "));
    Serial.print(g); Serial.print(F(", "));
    Serial.print(b);
    Serial.println(F(")"));
#endif

    delay(STEP_DELAY_MS);
  }
}

// ===== Hard CUT to black before switching color families =====
static void blackout() {
  setAllSolid(0, 0, 0);
#if USE_SERIAL
  Serial.println(F("\n--- BLACKOUT ---\n"));
#endif
  delay(300);  // brief black pause between blue and yellow cycles
}

void setup() {
#if USE_SERIAL
  Serial.begin(115200);
  while (!Serial) {;}
  Serial.println(F("========================================="));
  Serial.println(F("  BLUE shades  →  BLACK  →  YELLOW shades"));
  Serial.println(F("  No fusion. Pure color families only.   "));
  Serial.println(F("========================================="));
  Serial.print(F("Blue shades  : ")); Serial.println(NUM_BLUE);
  Serial.print(F("Yellow shades: ")); Serial.println(NUM_YELLOW);
  Serial.print(F("Fade steps   : ")); Serial.println(FADE_STEPS);
  Serial.println();
#endif

  setAllSolid(0, 0, 0);
  delay(300);
}

void loop() {

  // ── BLUE CYCLE ──────────────────────────────────────────────
#if USE_SERIAL
  Serial.println(F("========== BLUE CYCLE =========="));
#endif

  for (size_t i = 0; i < NUM_BLUE; i++) {
    const Shade& from = BLUE_SHADES[i];
    const Shade& to   = BLUE_SHADES[(i + 1) % NUM_BLUE];

#if USE_SERIAL
    Serial.print(F("\n>>> ["));
    Serial.print(from.name);
    Serial.print(F("] -> ["));
    Serial.print(to.name);
    Serial.println(F("]"));
#endif

    fadeBetween(from, to);
  }

  // ── HARD CUT: black before switching to yellow ───────────────
  blackout();

  // ── YELLOW CYCLE ────────────────────────────────────────────
#if USE_SERIAL
  Serial.println(F("========== YELLOW CYCLE =========="));
#endif

  for (size_t i = 0; i < NUM_YELLOW; i++) {
    const Shade& from = YELLOW_SHADES[i];
    const Shade& to   = YELLOW_SHADES[(i + 1) % NUM_YELLOW];

#if USE_SERIAL
    Serial.print(F("\n>>> ["));
    Serial.print(from.name);
    Serial.print(F("] -> ["));
    Serial.print(to.name);
    Serial.println(F("]"));
#endif

    fadeBetween(from, to);
  }

  // ── HARD CUT: black before going back to blue ────────────────
  blackout();
}