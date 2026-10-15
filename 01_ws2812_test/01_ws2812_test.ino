/* ============================================================================
   01_ws2812_test.ino  —  WS2812 Strip Bring-Up Test
   Board   : Arduino Nano (ATmega328P, Old Bootloader on most clones)
   Library : FastLED 3.6.0+   (Library Manager -> "FastLED")

   PURPOSE
   Verify the strip BEFORE touching LCD / buttons / buzzer:
     1. Data pin wiring is correct
     2. NUM_LEDS matches your physical strip
     3. COLOR_ORDER is correct (GRB vs RGB vs BGR)
     4. Power supply can hold up under load

   WIRING
     Strip 5V   -> External 5V PSU (+)     <-- NOT the Nano's 5V pin
     Strip GND  -> External 5V PSU (-)  AND  Nano GND   (COMMON GROUND, mandatory)
     Strip DIN  -> 330-470 ohm resistor -> Nano D6
     1000uF/6.3V+ cap across strip 5V/GND, close to the first LED

   POWER MATH (read this before you blame the code)
     60-LED/m strip, pure blue at full brightness = 60 x 20mA  = 1.20 A
     60-LED/m strip, white  at full brightness    = 60 x 60mA  = 3.60 A
     Nano USB port can supply ~400-500mA TOTAL. Anything more browns out the
     Nano mid-animation and you get flicker, random colours, or resets.
   ========================================================================= */

#include <FastLED.h>

// ---------------------------------------------------------------- CONFIG ---
#define FW_NAME        "WS2812 Bring-Up Test"
#define FW_VERSION     "1.0.0"

#define LED_PIN        6            // Nano D6 -> strip DIN (via 330R)
#define NUM_LEDS       60           // 30 / 60 / 144 -- SET THIS TO YOUR STRIP
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB          // 99% of WS2812B are GRB. Test 3 confirms.

#define TEST_BRIGHTNESS 120          // 0-255. Keep LOW while testing on USB.
#define MAX_VOLTS       5
#define MAX_MILLIAMPS   450         // Safe ceiling for USB-only testing.
                                    // Raise to your PSU rating once external
                                    // power is wired (e.g. 2000 for a 3A PSU).

#define STEP_DELAY_MS   30          // Animation step pacing

CRGB leds[NUM_LEDS];                // 60 x 3 = 180 bytes SRAM

// ------------------------------------------------------------- UTILITIES ---
// Returns approximate free SRAM (gap between heap top and stack pointer)
static int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

static void banner() {
  Serial.println();
  Serial.println(F("========================================"));
  Serial.print (F("[BOOT] ")); Serial.print(F(FW_NAME));
  Serial.print (F(" v"));      Serial.println(F(FW_VERSION));
  Serial.println(F("[BOOT] Board        : Arduino Nano (ATmega328P)"));
  Serial.println(F("[BOOT] Baud         : 115200"));
  Serial.print  (F("[BOOT] LED data pin : D")); Serial.println(LED_PIN);
  Serial.print  (F("[BOOT] LED count    : ")); Serial.println(NUM_LEDS);
  Serial.print  (F("[BOOT] Colour order : ")); Serial.println(F("GRB"));
  Serial.print  (F("[BOOT] Brightness   : ")); Serial.println(TEST_BRIGHTNESS);
  Serial.print  (F("[BOOT] Power cap    : ")); Serial.print(MAX_MILLIAMPS);
  Serial.print  (F(" mA @ ")); Serial.print(MAX_VOLTS); Serial.println(F("V"));
  Serial.print  (F("[BOOT] Free SRAM    : ")); Serial.print(freeRam());
  Serial.println(F(" bytes"));
  Serial.println(F("========================================"));
}

static void clearStrip() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

// ------------------------------------------------------------- TEST 1 -----
// Walks a single WHITE pixel down the strip. Count the pixels that light up.
// If the walk ends early -> NUM_LEDS is set too high.
// If pixels remain dark at the tail -> NUM_LEDS is set too low.
static void test1_pixelWalk() {
  Serial.println(F("[TEST 1] Single-pixel walk (WHITE). Count your LEDs."));
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    clearStrip();
    leds[i] = CRGB::White;
    FastLED.show();
    if (i % 10 == 0) {
      Serial.print(F("[DEBUG]   index = ")); Serial.println(i);
    }
    delay(STEP_DELAY_MS);
  }
  clearStrip();
  Serial.println(F("[TEST 1] Done."));
}

// ------------------------------------------------------------- TEST 2 -----
// Solid RED, then GREEN, then BLUE. Serial announces the EXPECTED colour.
// If the strip shows GREEN when serial says RED -> COLOR_ORDER is wrong.
//   Serial RED  / strip GREEN -> you have RGB, change COLOR_ORDER to RGB
//   Serial RED  / strip BLUE  -> you have BRG or GBR, try each
static void test2_colourOrder() {
  Serial.println(F("[TEST 2] Colour order check."));

  const CRGB colours[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };
  const char* names[3]  = { "RED", "GREEN", "BLUE" };

  for (uint8_t c = 0; c < 3; c++) {
    Serial.print(F("[INFO]   Strip should now be: ")); Serial.println(names[c]);
    fill_solid(leds, NUM_LEDS, colours[c]);
    FastLED.show();
    delay(1500);
  }
  clearStrip();
  Serial.println(F("[TEST 2] Done. Mismatch => fix COLOR_ORDER at top of file."));
}

// ------------------------------------------------------------- TEST 3 -----
// The exact animation used by the main project: bright blue running fill.
static void test3_blueRunningFill() {
  Serial.println(F("[TEST 3] Blue running fill (project animation preview)."));
  clearStrip();
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue;
    FastLED.show();
    delay(STEP_DELAY_MS);
  }
  Serial.println(F("[TEST 3] Fill complete, holding 1500 ms."));
  delay(1500);
  clearStrip();
  Serial.println(F("[TEST 3] Done."));
}

// ------------------------------------------------------------- TEST 4 -----
// Brightness ramp on solid blue. This is the POWER STRESS test.
// Watch for: flicker, colour corruption, or the Nano resetting (banner
// reprints in serial). Any of those = supply cannot hold the load.
static void test4_brightnessRamp() {
  Serial.println(F("[TEST 4] Brightness ramp on BLUE (power stress)."));
  fill_solid(leds, NUM_LEDS, CRGB::Blue);

  for (uint16_t b = 10; b <= 255; b += 5) {
    FastLED.setBrightness((uint8_t)b);
    FastLED.show();
    if (b % 50 < 5) {
      Serial.print(F("[DEBUG]   brightness = ")); Serial.println(b);
    }
    delay(40);
  }
  FastLED.setBrightness(TEST_BRIGHTNESS);
  clearStrip();
  Serial.println(F("[TEST 4] Done. Reset / flicker here => power problem."));
}

// ----------------------------------------------------------------- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(300);                       // Let USB CDC / serial monitor attach
  banner();

  Serial.println(F("[INIT] Configuring FastLED..."));
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
         .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(TEST_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(MAX_VOLTS, MAX_MILLIAMPS);
  clearStrip();
  Serial.println(F("[INIT] FastLED OK. Strip cleared."));
  Serial.println();
}

// Smoothly fade between two colors
void fadeToColor(CRGB from, CRGB to, uint16_t steps, uint16_t delayMs)
{
  for (uint16_t i = 0; i <= steps; i++)
  {
    uint8_t amount = map(i, 0, steps, 0, 255);

    CRGB color = blend(from, to, amount);

    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();

    delay(delayMs);
  }
}

void loop()
{
  // Red -> Green
  fadeToColor(CRGB::Red, CRGB::Green, 255, 10);

  // Green -> Blue
  fadeToColor(CRGB::Green, CRGB::Blue, 255, 10);

  // Blue -> Red
  fadeToColor(CRGB::Blue, CRGB::Red, 255, 10);
}
