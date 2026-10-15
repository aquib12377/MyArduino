// ============================================================
//  Arduino Mega - WS2812B ALL PINS White Test
//  Adafruit NeoPixel | 62 Data Pins
//  ↳ Change NUM_LEDS_PER_STRIP to match your strip length
// ============================================================

#include <Adafruit_NeoPixel.h>

// ── Change this to match your LED count per strip ─────────
#define NUM_LEDS_PER_STRIP  130

#define BRIGHTNESS          200    // Start LOW! (0–255)
#define PIXEL_TYPE          (NEO_GRB + NEO_KHZ800)

// ── All 62 pins ───────────────────────────────────────────
// Analog pins A0–A15 = digital pins 54–69 on Mega
const uint8_t PINS[] = {
2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
  // Serial TX/RX pins (14–19) — disconnect Serial if using these
  14, 15, 16, 17, 18, 19,
  // Odd side (22–53)
  22, 23, 25, 27, 29, 31, 33, 35, 37, 39,
  41, 43, 45, 47, 49, 51, 53,
  // Even side (32–52)
  32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52,
  // Analog A0–A15 as digital 54–69
  54, 55, 56, 57, 58, 59, 60, 61,   // A0–A7
  62, 63, 64, 65, 66, 67, 68, 69    // A8–A15
};

const uint8_t NUM_STRIPS = sizeof(PINS) / sizeof(PINS[0]);

// ── Strip object pool ─────────────────────────────────────
Adafruit_NeoPixel* strips[62];

// ── Colors ────────────────────────────────────────────────
const uint32_t WHITE = Adafruit_NeoPixel::Color(255, 255, 255);
const uint32_t RED   = Adafruit_NeoPixel::Color(255,   0,   0);
const uint32_t GREEN = Adafruit_NeoPixel::Color(  0, 255,   0);
const uint32_t BLUE  = Adafruit_NeoPixel::Color(  0,   0, 255);
const uint32_t OFF   = 0;

// ── Globals ───────────────────────────────────────────────
uint32_t currentColor = WHITE;
uint8_t  brightness   = BRIGHTNESS;
bool     ledsOn       = true;

// ── Helper: set all strips to one color ───────────────────
void allColor(uint32_t color) {
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    strips[s]->fill(color);
    strips[s]->show();
  }
}

// ── Helper: set brightness on all strips ──────────────────
void allBrightness(uint8_t b) {
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    strips[s]->setBrightness(b);
    strips[s]->show();
  }
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("============================================"));
  Serial.println(F("  WS2812B 62-Pin WHITE Test — Arduino Mega"));
  Serial.println(F("============================================"));
  Serial.print(F("Strips       : ")); Serial.println(NUM_STRIPS);
  Serial.print(F("LEDs/strip   : ")); Serial.println(NUM_LEDS_PER_STRIP);
  Serial.print(F("Total LEDs   : ")); Serial.println((uint32_t)NUM_STRIPS * NUM_LEDS_PER_STRIP);
  Serial.print(F("Brightness   : ")); Serial.println(BRIGHTNESS);
  Serial.println();

  // Init all strips
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    strips[s] = new Adafruit_NeoPixel(
      NUM_LEDS_PER_STRIP,
      PINS[s],
      PIXEL_TYPE
    );
    strips[s]->begin();
    strips[s]->setBrightness(BRIGHTNESS);
    strips[s]->clear();
    strips[s]->show();
  }

  Serial.println(F("All strips initialized. Running blink test...\n"));
  delay(200);

  // ── 3× blink startup test ─────────────────────────────
  // for (uint8_t i = 0; i < 3; i++) {
  //   Serial.print(F("Blink ")); Serial.println(i + 1);
  //   allColor(WHITE);  delay(800);
  //   allColor(OFF);    delay(400);
  // }

  // ── Steady WHITE ──────────────────────────────────────
  allColor(WHITE);
  Serial.println(F("All LEDs → Steady WHITE\n"));
  //printHelp();
}

// ── Loop ──────────────────────────────────────────────────
void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'T': case 't':
      ledsOn = !ledsOn;
      allColor(ledsOn ? currentColor : OFF);
      Serial.println(ledsOn ? F("→ ON") : F("→ OFF"));
      break;

    case 'W': case 'w':
      currentColor = WHITE;
      allColor(WHITE);
      Serial.println(F("Color → WHITE"));
      break;

    case 'R': case 'r':
      currentColor = RED;
      allColor(RED);
      Serial.println(F("Color → RED"));
      break;

    case 'G': case 'g':
      currentColor = GREEN;
      allColor(GREEN);
      Serial.println(F("Color → GREEN"));
      break;

    case 'B': case 'b':
      currentColor = BLUE;
      allColor(BLUE);
      Serial.println(F("Color → BLUE"));
      break;

    case '+':
      brightness = (uint8_t)min(255, brightness + 10);
      allBrightness(brightness);
      Serial.print(F("Brightness → ")); Serial.println(brightness);
      break;

    case '-':
      brightness = (uint8_t)max(5, brightness - 10);
      allBrightness(brightness);
      Serial.print(F("Brightness → ")); Serial.println(brightness);
      break;

    case '?':
      printHelp();
      break;
  }
}

void printHelp() {
  Serial.println(F("── Serial Commands (115200 baud) ──────────"));
  Serial.println(F("  T  → Toggle all ON / OFF"));
  Serial.println(F("  W  → White"));
  Serial.println(F("  R  → Red"));
  Serial.println(F("  G  → Green"));
  Serial.println(F("  B  → Blue"));
  Serial.println(F("  +  → Brightness up   (+10)"));
  Serial.println(F("  -  → Brightness down (-10)"));
  Serial.println(F("  ?  → Show this help"));
  Serial.println(F("───────────────────────────────────────────"));
}