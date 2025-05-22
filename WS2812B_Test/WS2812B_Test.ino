/********************************************************************
 *  10-Strip “Up-and-Down” Runner – Adafruit_NeoPixel
 *  -------------------------------------------------
 *  • 10 independent WS2812B strips, 73 pixels each
 *  • A single pixel on every strip runs from start→end and back
 *  • Pixel hue is derived from its current position
 *  • Whole animation breathes (fades in/out) while it moves
 ********************************************************************/
#include <Adafruit_NeoPixel.h>

#define NUM_STRIPS   10
#define NUM_LEDS     73

// GPIOs for every strip                                   (2 … 11)
const uint8_t STRIP_PINS[NUM_STRIPS] = {2,3,4,5,6,7,8,9,10,11};

#define NEO_TYPE    (NEO_GRB + NEO_KHZ800)
#define MAX_BRIGHT  255          // upper limit for the breathing
#define MIN_BRIGHT   20          // lower limit for the breathing
#define FADE_STEP     3          // brightness step per frame
#define MOVE_DELAY   25          // ms between frames

Adafruit_NeoPixel strip0(NUM_LEDS, STRIP_PINS[0], NEO_TYPE);
Adafruit_NeoPixel strip1(NUM_LEDS, STRIP_PINS[1], NEO_TYPE);
Adafruit_NeoPixel strip2(NUM_LEDS, STRIP_PINS[2], NEO_TYPE);
Adafruit_NeoPixel strip3(NUM_LEDS, STRIP_PINS[3], NEO_TYPE);
Adafruit_NeoPixel strip4(NUM_LEDS, STRIP_PINS[4], NEO_TYPE);
Adafruit_NeoPixel strip5(NUM_LEDS, STRIP_PINS[5], NEO_TYPE);
Adafruit_NeoPixel strip6(NUM_LEDS, STRIP_PINS[6], NEO_TYPE);
Adafruit_NeoPixel strip7(NUM_LEDS, STRIP_PINS[7], NEO_TYPE);
Adafruit_NeoPixel strip8(NUM_LEDS, STRIP_PINS[8], NEO_TYPE);
Adafruit_NeoPixel strip9(NUM_LEDS, STRIP_PINS[9], NEO_TYPE);

Adafruit_NeoPixel* const strips[NUM_STRIPS] = {
  &strip0,&strip1,&strip2,&strip3,&strip4,
  &strip5,&strip6,&strip7,&strip8,&strip9
};

/* ---------------------------------------------------------------- */
static int      pos[NUM_STRIPS];     // current pixel position
static int8_t   dir[NUM_STRIPS];     // +1 or –1
static uint8_t  brightness = MIN_BRIGHT;
static int8_t   fadeDir    =  1;     // +1 → brighter, –1 → dimmer

/* ---------- helpers ---------- */
inline void clearAll()
{
  for (uint8_t s = 0; s < NUM_STRIPS; ++s) strips[s]->clear();
}
inline void showAll()
{
  for (uint8_t s = 0; s < NUM_STRIPS; ++s) strips[s]->show();
}

/* ---------------------------------------------------------------- */
void setup()
{
  for (uint8_t s = 0; s < NUM_STRIPS; ++s)
  {
    strips[s]->begin();
    strips[s]->setBrightness(brightness);
    strips[s]->show();
  }

  randomSeed(analogRead(A0));

  /* randomise starting position and direction for every strip */
  for (uint8_t s = 0; s < NUM_STRIPS; ++s)
  {
    pos[s] = random(NUM_LEDS);
    dir[s] = random(2) ?  1 : -1;
  }
}
/* ------------------------------------------------------------- */
/*  1. Up–Down single-pixel runner (already in earlier sketch)   */
/* ------------------------------------------------------------- */
void patternUpDown(uint16_t cycles = 2)
{
  static int      pos [NUM_STRIPS];
  static int8_t   dir [NUM_STRIPS];
  static bool     init = false;
  if (!init) {                    // one-time random init
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      pos[s] = random(NUM_LEDS);
      dir[s] = random(2) ?  1 : -1;
    }
    init = true;
  }

  for (uint16_t c = 0; c < cycles * NUM_LEDS * 2; ++c) {
    clearAll();
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      uint16_t hue = uint32_t(pos[s]) * 65535UL / NUM_LEDS;
      strips[s]->setPixelColor(pos[s],
                 Adafruit_NeoPixel::ColorHSV(hue, 255, 200));
      pos[s] += dir[s];
      if (pos[s] == NUM_LEDS - 1 || pos[s] == 0) dir[s] = -dir[s];
    }
    showAll();
    delay(25);
  }
}

/* ------------------------------------------------------------- */
/*  2. Rainbow runner (whole strip fades through rainbow)        */
/* ------------------------------------------------------------- */
void patternRainbowRunner(uint16_t cycles = 2)
{
  for (uint16_t shift = 0; shift < cycles * 65535UL; shift += 256) {
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      for (uint16_t p = 0; p < NUM_LEDS; ++p) {
        uint16_t hue = (shift + p * 512) & 0xFFFF;
        strips[s]->setPixelColor(p,
            Adafruit_NeoPixel::ColorHSV(hue, 255, 150));
      }
    }
    showAll();
    delay(20);
  }
}

/* ------------------------------------------------------------- */
/*  3. Two-way chase – dots from both ends meet at centre        */
/* ------------------------------------------------------------- */
void patternTwoWayChase(uint16_t cycles = 3)
{
  for (uint16_t step = 0; step < cycles * NUM_LEDS; ++step) {
    clearAll();
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      uint16_t a = step % NUM_LEDS;          // left→right
      uint16_t b = (NUM_LEDS - 1 - a);       // right→left
      strips[s]->setPixelColor(a, 0xFF0000); // red
      strips[s]->setPixelColor(b, 0x0000FF); // blue
    }
    showAll();
    delay(40);
  }
}

/* ------------------------------------------------------------- */
/*  4. Comet with fading tail                                    */
/* ------------------------------------------------------------- */
void patternComet(uint16_t cycles = 4)
{
  const uint8_t tail = 10;
  for (uint16_t head = 0; head < cycles * NUM_LEDS; ++head) {
    clearAll();
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      for (uint8_t t = 0; t < tail; ++t) {
        int16_t idx = head - t;
        if (idx < 0) idx += NUM_LEDS;
        uint8_t b = 200 - t * (200 / tail);
        strips[s]->setPixelColor(idx,
          Adafruit_NeoPixel::ColorHSV(0, 0, b)); // white tail
      }
    }
    showAll();
    delay(20);
  }
}

/* ------------------------------------------------------------- */
/*  5. Color sine-wave along each strip                          */
/* ------------------------------------------------------------- */
void patternColorWave(uint16_t cycles = 3)
{
  for (uint16_t phase = 0; phase < cycles * 360; ++phase) {
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      for (uint16_t p = 0; p < NUM_LEDS; ++p) {
        float angle = (phase + p * 360.0 / NUM_LEDS) * DEG_TO_RAD;
        uint8_t v   = (sin(angle) * 0.5 + 0.5) * 200;
        strips[s]->setPixelColor(p,
          Adafruit_NeoPixel::ColorHSV(32000, 200, v));
      }
    }
    showAll();
    delay(25);
  }
}

/* ------------------------------------------------------------- */
/*  6. Scanner / Cylon eye                                       */
/* ------------------------------------------------------------- */
void patternScanner(uint16_t cycles = 4)
{
  for (uint16_t pos = 0; pos < cycles * (NUM_LEDS * 2); ++pos) {
    int16_t idx = pos < NUM_LEDS ? pos : (NUM_LEDS * 2 - 2 - pos);
    clearAll();
    for (uint8_t s = 0; s < NUM_STRIPS; ++s)
      strips[s]->setPixelColor(idx, 0xFF0000);
    showAll();
    delay(15);
  }
}

/* ------------------------------------------------------------- */
/*  7. Sparkle & slow fade out                                   */
/* ------------------------------------------------------------- */
void patternSparkleFade(uint16_t cycles = 40)
{
  for (uint16_t i = 0; i < cycles; ++i) {
    uint8_t s = random(NUM_STRIPS);
    uint8_t p = random(NUM_LEDS);
    strips[s]->setPixelColor(p, 0xFFFFFF);
    showAll();
    delay(20);

    // global fade
    for (uint8_t ss = 0; ss < NUM_STRIPS; ++ss)
      for (uint16_t pp = 0; pp < NUM_LEDS; ++pp) {
        uint32_t c = strips[ss]->getPixelColor(pp);
        c = (c >> 1) & 0x7F7F7F;          // simple half-fade
        strips[ss]->setPixelColor(pp, c);
      }
  }
  showAll();
}

/* ------------------------------------------------------------- */
/*  8. Stack-rise (fills upward, then clear)                     */
/* ------------------------------------------------------------- */
void patternStackRise(uint16_t cycles = 1)
{
  for (uint8_t c = 0; c < cycles; ++c) {
    for (uint16_t p = 0; p < NUM_LEDS; ++p) {   // fill
      for (uint8_t s = 0; s < NUM_STRIPS; ++s)
        strips[s]->setPixelColor(p, 0x00FF00);
      showAll();
      delay(30);
    }
    clearAll(); showAll(); delay(400);
  }
}

/* ------------------------------------------------------------- */
/*  9. Theater-chase (3-pixel dashes)                            */
/* ------------------------------------------------------------- */
void patternTheaterChase(uint16_t cycles = 30)
{
  for (uint8_t step = 0; step < cycles; ++step) {
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      for (uint16_t p = 0; p < NUM_LEDS; ++p) {
        strips[s]->setPixelColor(p,
          (p + step) % 3 ? 0 : 0xFF00FF); // magenta dash
      }
    }
    showAll();
    delay(50);
  }
  clearAll(); showAll();
}

/* ------------------------------------------------------------- */
/* 10. Meteor rain (bright head with fading trail)               */
/* ------------------------------------------------------------- */
void patternMeteorRain(uint16_t cycles = 4)
{
  const uint8_t meteorSize = 8;
  for (uint16_t pos = 0; pos < cycles * (NUM_LEDS + meteorSize); ++pos) {
    // global fade
    for (uint8_t s = 0; s < NUM_STRIPS; ++s)
      for (uint16_t p = 0; p < NUM_LEDS; ++p) {
        uint32_t c = strips[s]->getPixelColor(p);
        c = (c >> 1) & 0x7F7F7F;
        strips[s]->setPixelColor(p, c);
      }

    // draw meteor head
    for (uint8_t s = 0; s < NUM_STRIPS; ++s)
      for (uint8_t m = 0; m < meteorSize; ++m) {
        int16_t idx = pos - m;
        if (idx >= 0 && idx < NUM_LEDS) {
          uint8_t fade = 255 - (m * (255 / meteorSize));
          strips[s]->setPixelColor(idx,
            Adafruit_NeoPixel::ColorHSV(12000, 255, fade));
        }
      }
    showAll();
    delay(30);
  }
}

/* ------------------------------------------------------------- */
/*  Call them in loop() …                                        */
/* ------------------------------------------------------------- */
void loop()
{
  patternUpDown();
  patternRainbowRunner();
  patternTwoWayChase();
  patternComet();
  patternColorWave();
  patternScanner();
  patternSparkleFade();
  patternStackRise();
  patternTheaterChase();
  patternMeteorRain();
}
