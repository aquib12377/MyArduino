#include <Adafruit_NeoPixel.h>

// Definitions
#define NUM_LEDS 32

// LED strip pins
  #define STRIP1_PIN 13
  #define STRIP2_PIN 12
  #define STRIP3_PIN 11
  #define STRIP4_PIN 10

// Create NeoPixel strip objects
Adafruit_NeoPixel strip1(NUM_LEDS, STRIP1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_LEDS, STRIP2_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUM_LEDS, STRIP3_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip4(NUM_LEDS, STRIP4_PIN, NEO_GRB + NEO_KHZ800);

// Warm white color
uint32_t warmWhite = Adafruit_NeoPixel::Color(255, 147, 41); // approximate warm white

void setup() {
  strip1.begin();
  strip2.begin();
  strip3.begin();
  strip4.begin();

  strip1.show();
  strip2.show();
  strip3.show();
  strip4.show();
}

void loop() {
  topToBottomFade(warmWhite, 30);
  delay(500);
  
  bottomToTopFade(warmWhite, 30);
  delay(500);

  breatheEffect(warmWhite, 10);
  delay(500);

  chaseEffect(warmWhite, 50);
  delay(500);
}

// ---------------------- Pattern Functions -------------------------

void topToBottomFade(uint32_t color, int delayTime) {
  for (int i = 0; i < NUM_LEDS; i++) {
    setPixelAllStrips(i, color);
    delay(delayTime);
  }
  clearAll();
}

void bottomToTopFade(uint32_t color, int delayTime) {
  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    setPixelAllStrips(i, color);
    delay(delayTime);
  }
  clearAll();
}

void breatheEffect(uint32_t color, int delayTime) {
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
  for (int i = 0; i < NUM_LEDS; i++) {
    clearAll();
    setPixelAllStrips(i, color);
    delay(delayTime);
  }
}

// ---------------------- Helper Functions -------------------------

void setPixelAllStrips(int index, uint32_t color) {
  strip1.setPixelColor(index, color);
  strip2.setPixelColor(index, color);
  strip3.setPixelColor(index, color);
  strip4.setPixelColor(index, color);
  strip1.show();
  strip2.show();
  strip3.show();
  strip4.show();
}

void clearAll() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip1.setPixelColor(i, 0);
    strip2.setPixelColor(i, 0);
    strip3.setPixelColor(i, 0);
    strip4.setPixelColor(i, 0);
  }
  strip1.show();
  strip2.show();
  strip3.show();
  strip4.show();
}

void setAllStripsColorDim(uint32_t color, uint8_t brightness) {
  uint8_t r = (uint8_t)((color >> 16) & 0xFF);
  uint8_t g = (uint8_t)((color >> 8) & 0xFF);
  uint8_t b = (uint8_t)(color & 0xFF);

  uint32_t dimColor = strip1.Color((r * brightness) / 255, (g * brightness) / 255, (b * brightness) / 255);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip1.setPixelColor(i, dimColor);
    strip2.setPixelColor(i, dimColor);
    strip3.setPixelColor(i, dimColor);
    strip4.setPixelColor(i, dimColor);
  }
  strip1.show();
  strip2.show();
  strip3.show();
  strip4.show();
}
