#include <FastLED.h>

#define NUM_STRIPS         9
#define NUM_LEDS          73

// literal data pins for each strip:
const uint8_t STRIP_PINS[NUM_STRIPS] = { 2,3,4,5,6,7,8,9,10 };
#define ROOMS_PER_CYCLE  8

#define FLOORS            18
#define LEDS_PER_FLOOR     2
#define GAP                0   // LEDs between floors

#define ROOMS_PER_STRIP  8          // how many floors per strip
#define FADE_STEP        2          // smaller = slower/smoother

#define MAX_BRIGHTNESS   255
#define FADE_DELAY         5
#define RUN_DELAY         75
#define RANDOM_CYCLES      8
#define RELAY_PIN         5

CRGB leds[NUM_STRIPS][NUM_LEDS];

void setup() {
  FastLED.setBrightness(255);

  // Unroll the addLeds calls with literal pins:
  FastLED.addLeds<WS2812, 2,  GRB>(leds[0], NUM_LEDS);
  FastLED.addLeds<WS2812, 3,  GRB>(leds[1], NUM_LEDS);
  FastLED.addLeds<WS2812, 4,  GRB>(leds[2], NUM_LEDS);
  FastLED.addLeds<WS2812, 5, GRB>(leds[3], NUM_LEDS);
  FastLED.addLeds<WS2812, 6, GRB>(leds[4], NUM_LEDS);
  FastLED.addLeds<WS2812, 7, GRB>(leds[5], NUM_LEDS);
  FastLED.addLeds<WS2812, 8, GRB>(leds[6], NUM_LEDS);
  FastLED.addLeds<WS2812, 9, GRB>(leds[7], NUM_LEDS);
  FastLED.addLeds<WS2812, 10, GRB>(leds[8], NUM_LEDS);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  randomSeed(analogRead(A0));
}

void loop() {
  //allLightsOn();
  //delay(1000);

  patternAlternateFade();
  patternChase();
stepRandomRoomsFade();   // smooth random‑room animation
  delay(5);
  //  for (int s = 0; s < NUM_STRIPS; ++s) {
  //   clearAll();
  //   fill_solid(leds[s], NUM_LEDS, CRGB::DarkCyan);
  //   delay(100);
  // }

  //patternRandomRoomsFade();

  // Relay ON pulse
  // digitalWrite(RELAY_PIN, LOW);
  // delay(1000);
  // digitalWrite(RELAY_PIN, HIGH);
  // delay(1000);
}
void lightFloorStrip(uint8_t strip, uint8_t floor, uint8_t val) {
  int start = floor * (LEDS_PER_FLOOR + GAP);
  if (start + LEDS_PER_FLOOR > NUM_LEDS) return;
  for (int i = 0; i < LEDS_PER_FLOOR; ++i)
    leds[strip][start + i] = CHSV(60, 128, val);   // warm white
}

void stepRandomRoomsFade() {
  static uint8_t rooms[NUM_STRIPS][ROOMS_PER_STRIP];
  static uint8_t brightness = 0;   // current fade level
  static int8_t  dir = 1;          // +1 up, –1 down
  static bool    needNew = true;   // pick new rooms?

  // --- pick fresh random floors per strip once per full cycle ---
  if (needNew) {
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      bool used[FLOORS] = { false };
      for (uint8_t i = 0; i < ROOMS_PER_STRIP; ++i) {
        uint8_t f;
        do { f = random(FLOORS); } while (used[f]);
        used[f] = true;
        rooms[s][i] = f;
      }
    }
    needNew = false;
  }

  // --- render this frame ---
  clearAll();
  for (uint8_t s = 0; s < NUM_STRIPS; ++s)
    for (uint8_t i = 0; i < ROOMS_PER_STRIP; ++i)
      lightFloorStrip(s, rooms[s][i], brightness);
  FastLED.show();

  // --- advance brightness ---
  brightness += dir * FADE_STEP;

  // turn‑around points
  if (brightness == 0 || brightness >= MAX_BRIGHTNESS) {
    dir = -dir;                     // reverse direction
    if (brightness == 0)            // finished fade‑out
      needNew = true;               // pick new rooms next frame
  }
}
void allLightsOn() {
  for (int s = 0; s < NUM_STRIPS; ++s) {
    fill_solid(leds[s], NUM_LEDS, CHSV(60, 128, MAX_BRIGHTNESS));
  }
  FastLED.show();
}

void patternAlternateFade() {
  // fade even ↑ while odd ↓, then reverse
  for (int lvl = 0; lvl <= MAX_BRIGHTNESS; ++lvl) {
    applyFade(0, MAX_BRIGHTNESS - lvl);
    FastLED.show();
    delay(FADE_DELAY);
  }
  clearAll();
  for (int lvl = 0; lvl <= MAX_BRIGHTNESS; ++lvl) {
    applyFade(lvl, 0);
    FastLED.show();
    delay(FADE_DELAY);
  }
    clearAll();

  for (int lvl = MAX_BRIGHTNESS; lvl >= 0; --lvl) {
    applyFade(lvl,0);
    FastLED.show();
    delay(FADE_DELAY);
  }
  clearAll();

    for (int lvl = MAX_BRIGHTNESS; lvl >= 0; --lvl) {
    applyFade(0, MAX_BRIGHTNESS - lvl);
    FastLED.show();
    delay(FADE_DELAY);
  }
    clearAll();

}

void patternChase() {
    clearAll();

  // bottom→top
  for (int f = 0; f < FLOORS; ++f) {
    lightFloor(f, MAX_BRIGHTNESS);
    FastLED.show();
    delay(RUN_DELAY);
  }
    clearAll();

  // top→bottom
  for (int f = FLOORS - 1; f >= 0; --f) {
    lightFloor(f, MAX_BRIGHTNESS);
    FastLED.show();
    delay(RUN_DELAY);
  }
}

// ──────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────
// Each strip picks its own 8 random rooms and fades them
void patternRandomRoomsFade() {
  for (int cycle = 0; cycle < RANDOM_CYCLES; ++cycle) {

    // --- choose 8 unique random floors PER STRIP ---
    uint8_t rooms[NUM_STRIPS][ROOMS_PER_CYCLE];
    for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
      bool used[FLOORS] = { false };
      for (uint8_t i = 0; i < ROOMS_PER_CYCLE; ++i) {
        uint8_t r;
        do { r = random(FLOORS); } while (used[r]);
        used[r] = true;
        rooms[s][i] = r;
      }
    }

    // --- fade IN ---
    for (uint8_t b = 0; b <= MAX_BRIGHTNESS; ++b) {
      clearAll();
      for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
        for (uint8_t i = 0; i < ROOMS_PER_CYCLE; ++i) {
          lightFloorStrip(s, rooms[s][i], b);
        }
      }
      FastLED.show();
      delay(FADE_DELAY);
    }

    // --- fade OUT ---
    for (int b = MAX_BRIGHTNESS; b >= 0; --b) {
      clearAll();
      for (uint8_t s = 0; s < NUM_STRIPS; ++s) {
        for (uint8_t i = 0; i < ROOMS_PER_CYCLE; ++i) {
          lightFloorStrip(s, rooms[s][i], b);
        }
      }
      FastLED.show();
      delay(FADE_DELAY);
    }
  }
}


// ---------------------------------------------------------------------
// Give each strip its *own* random set of "even" floors.
// The map is regenerated the very first time this function is called
// after you change patterns (or you can reset it whenever you like).
// ---------------------------------------------------------------------
// evenB = brightness for the “highlighted” floors
// oddB  = brightness for the other floors
void applyFade(uint8_t evenB, uint8_t oddB)
{
  for (int s = 0; s < NUM_STRIPS; ++s) {              // each strip
    bool stripEven = (s % 2 == 0);                    // true for 0,2,4…

    for (int f = 0; f < FLOORS; ++f) {                // each floor
      int start = f * (LEDS_PER_FLOOR + GAP);
      if (start + LEDS_PER_FLOOR > NUM_LEDS) break;

      bool floorEven = (f % 2 == 0);
      // highlight = (even floor on even‑indexed strip) OR (odd floor on odd‑indexed strip)
      uint8_t b = (stripEven == floorEven) ? evenB : oddB;

      for (int i = 0; i < LEDS_PER_FLOOR; ++i) {
        leds[s][start + i] = CHSV(60, 128, b);        // warm white / yellow tint
      }
    }
  }
}


void clearAll() {
  for (int s = 0; s < NUM_STRIPS; ++s) {
    fill_solid(leds[s], NUM_LEDS, CRGB::Black);
  }
}

void lightFloor(int floor, uint8_t brightness) {
  int start = floor * (LEDS_PER_FLOOR + GAP);
  if (start + LEDS_PER_FLOOR > NUM_LEDS) return;
  for (int s = 0; s < NUM_STRIPS; ++s) {
    for (int i = 0; i < LEDS_PER_FLOOR; ++i) {
      leds[s][start + i] = CHSV(60, 128, brightness);
    }
  }
}
