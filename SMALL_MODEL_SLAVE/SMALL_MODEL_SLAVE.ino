/*********************************************************************
 *  Arduino Mega 2560  |  Four-Strip Building Lights  |  I²C slave
 *  Debug build with Serial prints + fixed switch/break logic
 *********************************************************************/
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define NUM_STRIPS       4
#define LEDS_PER_STRIP  32
const uint8_t STRIP_PINS[NUM_STRIPS] = { 13, 12, 11, 10 };

const uint32_t COLOR_WARM  = Adafruit_NeoPixel::Color(255, 147, 41);
const uint32_t COLOR_GREEN = Adafruit_NeoPixel::Color(0, 120, 20);
const uint32_t COLOR_RED   = Adafruit_NeoPixel::Color(255,   0,  0);
const uint32_t COLOR_OFF   = 0;

Adafruit_NeoPixel strips[NUM_STRIPS] = {
  Adafruit_NeoPixel(LEDS_PER_STRIP, STRIP_PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_STRIP, STRIP_PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_STRIP, STRIP_PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_STRIP, STRIP_PINS[3], NEO_GRB + NEO_KHZ800)
};

/* ---- I²C ---- */
constexpr uint8_t I2C_ADDR = 0x08;

/* ---- mode state ---- */
enum Mode : uint8_t { MODE_AVAILABLE = 0, MODE_PATTERNS, MODE_WARM };
volatile Mode curMode = MODE_WARM;

// “done” flags for one‐time inits
bool doneAvail    = false;
bool doneWarm     = false;
bool donePatterns = false;

/* ---- timers ---- */
unsigned long tPrev    = 0;
const unsigned PAT_MS  = 40;
const unsigned AVAIL_MS=1000;

/* ---- pattern helpers ---- */
uint16_t patPos = 0;
int8_t   patDir = 1;

// I²C receive – reset flags on mode change
void onReceive(int) {
  while (Wire.available()) {
    char c = Wire.read();
    Serial.print("I2C received: '"); Serial.print(c); Serial.println("'");
    // reset all done flags so that the new mode re‐initializes
    doneAvail = doneWarm = donePatterns = false;
    if (c == 'A')       curMode = MODE_AVAILABLE;
    else if (c == 'P')  curMode = MODE_PATTERNS;
    else if (c == 'W')  curMode = MODE_WARM;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Wire.begin(I2C_ADDR);
  Wire.setClock(400000UL);
  Wire.onReceive(onReceive);

  for (auto& s : strips) {
    s.begin();
    s.setBrightness(150);
    s.show();
  }

  Serial.println(F("=== Mega I²C-slave DEBUG READY ==="));
  Serial.print("Starting in mode: "); Serial.println(curMode);
}

void loop() {
  unsigned long now = millis();

  switch (curMode) {
    /* -------------------- MODE_WARM -------------------- */
    case MODE_WARM: {
      if (!doneWarm) {
        Serial.println(">>> Entering WARM mode");
        fillAll(COLOR_WARM);
        doneWarm = true;
      }
      break;
    }

    /* ----------------- MODE_AVAILABLE ----------------- */
    case MODE_AVAILABLE: {
      if (!doneAvail) {
        Serial.println(">>> Entering AVAILABLE mode");
        showVacancies();
        doneAvail = true;
      } 
      break;
    }

    /* ----------------- MODE_PATTERNS ------------------ */
        case MODE_PATTERNS: {
      // on first entry, log and clear
      if (!donePatterns) {
        Serial.println(">>> Entering PATTERNS mode");
        clearAll();
        donePatterns = true;
      }

      // Run up to 10 cycles, but abort immediately if mode changes
      for (int cycle = 0; cycle < 10; cycle++) {
        if (curMode != MODE_PATTERNS) {
          Serial.println("PATTERNS aborted – mode changed");
          break;
        }
        
        topToBottomFade(COLOR_WARM, 30);
        if (curMode != MODE_PATTERNS) break;

        bottomToTopFade(COLOR_WARM, 30);
        if (curMode != MODE_PATTERNS) break;

        breatheEffect(COLOR_WARM, 10);
        if (curMode != MODE_PATTERNS) break;

        chaseEffect(COLOR_WARM, 50);
      }
      break;
    }

  }
}

/* =============== LED helper functions ================== */
void fillAll(uint32_t c) {
  for (auto& s : strips) {
    for (uint16_t i = 0; i < LEDS_PER_STRIP; i++) {
      s.setPixelColor(i, c);
    }
    s.show();
  }
}

void showVacancies() {
  for (uint8_t idx = 0; idx < NUM_STRIPS; idx++) {
    //Serial.printf(" showVacancies: strip %u\n", idx);
    auto& s = strips[idx];
    for (uint16_t room = 0; room < LEDS_PER_STRIP / 4; room++) {
      bool vacant = random(0, 10) < 4;
      for (uint8_t k = 0; k < 4; k++) {
        s.setPixelColor(room * 4 + k, vacant ? COLOR_GREEN : COLOR_RED);
      }
    }
    s.show();
  }
}

void randomFadeFrame() {
  static uint8_t step = 0;
  step = (step + 1) & 0x1F;
  uint8_t bri = (step < 16) ? step * 16 : (31 - step) * 16;
  uint32_t c = Adafruit_NeoPixel::Color(0, 0, bri);

  uint16_t led = random(0, NUM_STRIPS * LEDS_PER_STRIP);
  uint8_t  si  = led / LEDS_PER_STRIP;
  uint16_t pi  = led % LEDS_PER_STRIP;
  //Serial.printf(" randomFade: strip %u pixel %u bri %u\n", si, pi, bri);

  strips[si].setPixelColor(pi, c);
  strips[si].show();
}

void topBottomFrame() {
  //Serial.printf(" topBottomFrame: pos=%u dir=%d\n", patPos, patDir);
  fillAll(COLOR_OFF);
  for (auto& s : strips) {
    s.setPixelColor(patPos, COLOR_WARM);
    s.show();
  }
  patPos += patDir;
  if (patPos == 0 || patPos == LEDS_PER_STRIP - 1) patDir = -patDir;
}

void topToBottomFade(uint32_t color, int delayTime) {
  Serial.println("  → topToBottomFade");
  for (int i = 0; i < LEDS_PER_STRIP; i++) {
    setPixelAllStrips(i, color);
    delay(delayTime);
  }
  clearAll();
}

void bottomToTopFade(uint32_t color, int delayTime) {
  Serial.println("  → bottomToTopFade");
  for (int i = LEDS_PER_STRIP - 1; i >= 0; i--) {
    setPixelAllStrips(i, color);
    delay(delayTime);
  }
  clearAll();
}

void breatheEffect(uint32_t color, int delayTime) {
  Serial.println("  → breatheEffect");
  for (int b = 0; b < 256; b++) {
    setAllStripsColorDim(color, b);
    delay(delayTime);
  }
  for (int b = 255; b >= 0; b--) {
    setAllStripsColorDim(color, b);
    delay(delayTime);
  }
}

void chaseEffect(uint32_t color, int delayTime) {
  Serial.println("  → chaseEffect");
  for (int i = 0; i < LEDS_PER_STRIP; i++) {
    clearAll();
    setPixelAllStrips(i, color);
    delay(delayTime);
  }
}

// Helpers now index strips[0]..strips[3] correctly
void setPixelAllStrips(int idx, uint32_t color) {
  for (uint8_t s=0; s<NUM_STRIPS; s++){
    strips[s].setPixelColor(idx, color);
    strips[s].show();
  }
}

void clearAll() {
  for (uint8_t s=0; s<NUM_STRIPS; s++){
    for (int i=0; i<LEDS_PER_STRIP; i++){
      strips[s].setPixelColor(i, COLOR_OFF);
    }
    strips[s].show();
  }
}

void setAllStripsColorDim(uint32_t color, uint8_t brightness) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >>  8) & 0xFF;
  uint8_t b =  color        & 0xFF;
  uint32_t dimColor = Adafruit_NeoPixel::Color(
    (r * brightness) / 255,
    (g * brightness) / 255,
    (b * brightness) / 255
  );
  for (uint8_t s=0; s<NUM_STRIPS; s++){
    for (int i=0; i<LEDS_PER_STRIP; i++){
      strips[s].setPixelColor(i, dimColor);
    }
    strips[s].show();
  }
}
