/*
  Arduino Mega -> Many WS2812B/NeoPixel strips (one per pin)
  Sets ALL strips to full-brightness WHITE and latches once.

  Library: Adafruit_NeoPixel (Install via Library Manager)
*/

#include <Adafruit_NeoPixel.h>

// ----- CONFIG -----
#define NUM_LEDS_PER_STRIP 150   // <-- set this to how many LEDs are on EACH strip
#define FULL_BRIGHTNESS    255  // 255 = max brightness (careful with power!)
#define PIXEL_TYPE (NEO_GRB + NEO_KHZ800)

// Pins to drive (skip 0-1 (USB), 20-21 (I2C)). Add/remove as needed.
const uint8_t STRIP_PINS[] = {
  // Digital 2..13
  2,3,4,5,6,7,8,9,10,11,12,13,
  // Digital 22..53
  22,23,24,25,26,27,28,29,30,31,32,33,
  34,35,36,37,38,39,40,41,42,43,44,45,
  46,47,48,49,50,51,52,53,
  // Analog pins usable as digital: A0..A15 => 54..69
  54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69
};

const uint8_t NUM_STRIPS = sizeof(STRIP_PINS) / sizeof(STRIP_PINS[0]);

Adafruit_NeoPixel* strips[NUM_STRIPS] = { nullptr };

void fillStripWhite(Adafruit_NeoPixel* s) {
  // Full white (R,G,B) = (255,255,255)
  for (uint16_t i = 0; i < s->numPixels(); i++) {
    s->setPixelColor(i, s->Color(255, 255, 255));
  }
}

void setup() {
  // If you use Serial, keep pins 0/1 free; this is safe to print.
  Serial.begin(115200);
  Serial.println(F("Setting all strips to full-brightness WHITE..."));

  // Create, init and set all strips
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    strips[i] = new Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, STRIP_PINS[i], PIXEL_TYPE);
    strips[i]->begin();
    strips[i]->setBrightness(FULL_BRIGHTNESS); // 255 = no dimming
    fillStripWhite(strips[i]);
    strips[i]->show(); // latch once
  }

  Serial.println(F("Done."));
}

void loop() {
  // Nothing needed — LEDs stay latched until you update them again
}
