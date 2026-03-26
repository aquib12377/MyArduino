/*
 * ============================================================
 *  BUILDING LED CONTROLLER — Arduino Mega 2560
 *  Adafruit NeoPixel Edition
 * ============================================================
 *  Both buildings share same data pins (Y-split/daisy-chained)
 *  40 strips × 100 LEDs = 4000 LEDs per building pair
 *
 *  Memory: ONE Adafruit_NeoPixel object with setPin() swap.
 *  Only 300 bytes pixel buffer for everything.
 *
 *  I2C Slave (address 0x08) — receives commands from ESP32-S3
 * ============================================================
 *  COMMANDS:
 *    0x01 = Pattern Mode (cycles 5 patterns continuously)
 *    0x02 = All ON  (warm white)
 *    0x03 = All OFF
 * ============================================================
 *  PATTERNS (optimized for vertical strips):
 *    0 = Warm Wave        — sine wave ripples upward across facade
 *    1 = Rain Drop        — warm droplets fall down random strips
 *    2 = Breathing Columns— each strip pulses with staggered phase
 *    3 = Rising Fill      — building fills bottom-up then drains
 *    4 = Sparkle Twinkle  — random floors twinkle like city windows
 * ============================================================
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ── Geometry ──────────────────────────────────────────────
#define NUM_LEDS_PER_STRIP   100
#define TOTAL_STRIPS          40
#define BRIGHTNESS           255

// ── City floor geometry ───────────────────────────────────
#define CITY_FLOORS           50
#define CITY_LEDS_PER_FLOOR    2

// ── I2C ──────────────────────────────────────────────────
#define I2C_SLAVE_ADDR       0x08

// ── Commands ─────────────────────────────────────────────
#define CMD_PATTERNS         0x01
#define CMD_ALL_ON           0x02
#define CMD_ALL_OFF          0x03

// ── Idle timeout ─────────────────────────────────────────
uint32_t lastCommandTime = 0;
const uint32_t IDLE_TIMEOUT = 5000;
const uint8_t NUM_PATTERNS = 6;   // was 5
// ── Single NeoPixel object — pin is swapped via setPin() ─
Adafruit_NeoPixel strip(NUM_LEDS_PER_STRIP, 4, NEO_GRB + NEO_KHZ800);

// ── Pin map ───────────────────────────────────────────────
const uint8_t STRIP_PINS[TOTAL_STRIPS] = {
    13, 12, 11, 10,  9,  8,  7,  6,  5,  4,
    22, 23, 25, 27, 29, 31, 33, 35, 37, 39,
    A15, A14, A13, A12, A11, A10, A9, A8, A7, A6,
    50, 48, 46, 44, 42, 40, 38, 36, 34, 32
};

// ── Global control state ──────────────────────────────────
volatile uint8_t currentCommand  = CMD_ALL_OFF;
volatile bool    commandChanged  = false;

uint8_t  activePattern       = 0;
uint32_t patternCycleTimer   = 0;
const uint32_t PATTERN_DURATION = 15000;

uint32_t frameTimer          = 0;
const uint16_t FRAME_INTERVAL = 30;

uint16_t animOffset          = 0;


// ═══════════════════════════════════════════════════════════
//  PATTERN STATE — Rain Drop
// ═══════════════════════════════════════════════════════════
#define MAX_DROPS  12
struct RainDrop {
    uint8_t  stripIdx;      // which strip this drop is on
    int8_t   floorPos;      // current floor position (top=49, bottom=0)
    uint8_t  brightness;    // peak brightness
    uint8_t  speed;         // floors to move per update (1-3)
    uint8_t  tailLen;       // how many floors trail behind
    bool     active;
};
static RainDrop drops[MAX_DROPS];
static bool     dropsInitialized = false;
static uint8_t  dropSpawnTimer   = 0;


// ═══════════════════════════════════════════════════════════
//  PATTERN STATE — Sparkle Twinkle
// ═══════════════════════════════════════════════════════════
#define MAX_SPARKLES 60
struct Sparkle {
    uint8_t stripIdx;
    uint8_t floor;
    uint8_t brightness;
    int8_t  dir;           // +1 brightening, -1 dimming
    uint8_t speed;         // fade speed
    bool    active;
};
static Sparkle sparkles[MAX_SPARKLES];
static bool    sparklesInitialized = false;


// ═══════════════════════════════════════════════════════════
//  PATTERN STATE — Rising Fill
// ═══════════════════════════════════════════════════════════
static int8_t  fillLevel   = 0;     // current fill floor
static int8_t  fillDir     = 1;     // +1 filling, -1 draining
static uint8_t fillDelay   = 0;     // pause counter at top/bottom
static uint8_t fillSubStep = 0;     // sub-frame counter for speed


// ═══════════════════════════════════════════════════════════
//  SINE TABLE (PROGMEM)
// ═══════════════════════════════════════════════════════════
static const uint8_t PROGMEM _sineTable[256] = {
    128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
    176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
    218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
    245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
    255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
    245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
    218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
    176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
    128,125,122,119,116,113,110,107,104,101, 98, 94, 91, 89, 86, 83,
     80, 77, 74, 71, 68, 66, 63, 60, 58, 55, 53, 50, 48, 45, 43, 41,
     38, 36, 34, 32, 30, 28, 26, 24, 22, 21, 19, 18, 16, 15, 13, 12,
     11, 10,  8,  7,  6,  6,  5,  4,  3,  3,  2,  2,  2,  1,  1,  1,
      1,  1,  1,  1,  2,  2,  2,  3,  3,  4,  5,  6,  6,  7,  8, 10,
     11, 12, 13, 15, 16, 18, 19, 21, 22, 24, 26, 28, 30, 32, 34, 36,
     38, 41, 43, 45, 48, 50, 53, 55, 58, 60, 63, 66, 68, 71, 74, 77,
     80, 83, 86, 89, 91, 94, 98,101,104,107,110,113,116,119,122,125
};

static inline uint8_t sin8(uint8_t x) {
    return pgm_read_byte(&_sineTable[x]);
}


// ═══════════════════════════════════════════════════════════
//  WARM COLOR HELPERS
// ═══════════════════════════════════════════════════════════
//  All patterns use warm amber/yellow tones.

// Warm white with controllable brightness (R slightly dominant)
inline uint32_t warmWhite(uint8_t b) {
    uint8_t r = b;
    uint8_t g = (uint16_t)b * 160 / 255;   // ~63%
    uint8_t bl = (uint16_t)b * 60 / 255;   // ~24%  → amber/warm
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

// Deeper amber for accents
inline uint32_t deepAmber(uint8_t b) {
    uint8_t r = b;
    uint8_t g = (uint16_t)b * 100 / 255;   // ~39%
    uint8_t bl = (uint16_t)b * 20 / 255;   // ~8%
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

// Soft golden for subtle glow
inline uint32_t softGold(uint8_t b) {
    uint8_t r = b;
    uint8_t g = (uint16_t)b * 180 / 255;   // ~71%
    uint8_t bl = (uint16_t)b * 40 / 255;   // ~16%
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

// HSV yellow (NeoPixel built-in) for rainbow-ish warm
static const uint16_t YELLOW_HUE = (uint16_t)(65535UL * 60 / 360);

inline uint32_t hsvYellow(uint8_t v, uint8_t s = 128) {
    return Adafruit_NeoPixel::ColorHSV(YELLOW_HUE, s, v);
}


// ═══════════════════════════════════════════════════════════
//  FLOOR DRAW HELPER
// ═══════════════════════════════════════════════════════════
inline void drawFloor(uint8_t floor, uint32_t col) {
    uint16_t start = (uint16_t)floor * CITY_LEDS_PER_FLOOR;
    if (start + CITY_LEDS_PER_FLOOR > NUM_LEDS_PER_STRIP) return;
    for (uint8_t i = 0; i < CITY_LEDS_PER_FLOOR; i++) {
        strip.setPixelColor(start + i, col);
    }
}


// ═══════════════════════════════════════════════════════════
//  PUSH BUFFER TO ONE STRIP
// ═══════════════════════════════════════════════════════════
void pushStrip(uint8_t stripIndex) {
    strip.setPin(STRIP_PINS[stripIndex]);
    pinMode(STRIP_PINS[stripIndex], OUTPUT);
    strip.show();
}


// ═══════════════════════════════════════════════════════════
//  RESET ALL PATTERN STATE
// ═══════════════════════════════════════════════════════════
void resetPatternState() {
    dropsInitialized    = false;
    sparklesInitialized = false;
    fillLevel           = 0;
    fillDir             = 1;
    fillDelay           = 0;
    fillSubStep         = 0;
}


// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println(F("=== Building LED Controller (NeoPixel) ==="));
    Serial.print(F("Strips: "));   Serial.print(TOTAL_STRIPS);
    Serial.print(F("  LEDs/strip: ")); Serial.print(NUM_LEDS_PER_STRIP);
    Serial.print(F("  Total: "));  Serial.println((uint16_t)TOTAL_STRIPS * NUM_LEDS_PER_STRIP);

    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onReceive(onI2CReceive);

    strip.begin();
    strip.setBrightness(BRIGHTNESS);

    for (uint8_t i = 0; i < TOTAL_STRIPS; i++) {
        pinMode(STRIP_PINS[i], OUTPUT);
    }

    randomSeed(analogRead(A0));
    allOff();

    lastCommandTime = millis();

    Serial.println(F("Ready. Waiting for I2C commands..."));
    Serial.print(F("Free RAM: "));
    Serial.print(freeRam());
    Serial.println(F(" bytes"));
}

int freeRam() {
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0
           ? (int)&__heap_start : (int)__brkval);
}


// ═══════════════════════════════════════════════════════════
//  I2C RECEIVE HANDLER
// ═══════════════════════════════════════════════════════════
void onI2CReceive(int numBytes) {
    while (Wire.available()) {
        uint8_t cmd = Wire.read();
        if (cmd >= CMD_PATTERNS && cmd <= CMD_ALL_OFF) {
            currentCommand  = cmd;
            commandChanged  = true;
            lastCommandTime = millis();
        }
    }
}


// ═══════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
    uint32_t now = millis();

    if (commandChanged) {
        commandChanged = false;
        Serial.print(F("CMD: 0x"));
        Serial.println(currentCommand, HEX);

        if (currentCommand == CMD_ALL_OFF) { allOff(); return; }
        if (currentCommand == CMD_ALL_ON)  { allOn();  return; }
        if (currentCommand == CMD_PATTERNS) {
            activePattern     = 0;
            patternCycleTimer = now;
            animOffset        = 0;
            resetPatternState();
        }
    }

    // ── Idle timeout → auto-start patterns ───────────────
    if (currentCommand != CMD_PATTERNS &&
        now - lastCommandTime >= IDLE_TIMEOUT) {
        currentCommand    = CMD_PATTERNS;
        activePattern     = 0;
        patternCycleTimer = now;
        animOffset        = 0;
        resetPatternState();
        Serial.println(F("Idle timeout → auto-starting patterns"));
    }

    if (currentCommand == CMD_ALL_OFF || currentCommand == CMD_ALL_ON) {
        delay(50);
        return;
    }

    if (now - frameTimer < FRAME_INTERVAL) return;
    frameTimer = now;
    animOffset++;

    // ── Auto-cycle patterns ──────────────────────────────
    if (now - patternCycleTimer >= PATTERN_DURATION) {
        patternCycleTimer = now;
        activePattern = (activePattern + 1) % NUM_PATTERNS;
        resetPatternState();
        Serial.print(F("Pattern → "));
        Serial.println(activePattern);
    }

    // ── Per-frame global updates (rain, sparkle) ─────────
    if (activePattern == 1) updateRainDrops();
    if (activePattern == 3) updateFillLevel();
    if (activePattern == 4) updateSparkles();

    // ── Render & push every strip ────────────────────────
    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) {
        renderPattern(s);
        pushStrip(s);
    }
}


// ═══════════════════════════════════════════════════════════
//  RENDER DISPATCH
// ═══════════════════════════════════════════════════════════
void renderPattern(uint8_t s) {
    switch (activePattern) {
        case 0: patternWarmWave(s);        break;
        case 1: patternRainDrop(s);        break;
        case 2: patternBreathingColumns(s);break;
        case 3: patternRisingFill(s);      break;
        case 4: patternSparkleTwinkle(s);  break;
        case 5: patternStripPairChase(s);  break;   // ← new
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 0 — WARM WAVE
//  -------------------------------------------------------
//  A horizontal sine wave rolls upward through the building.
//  Each strip has a phase offset so the wave visibly travels
//  across the facade left-to-right AND bottom-to-top.
//
//  Floors near the wave peak glow bright warm white;
//  floors away from the peak dim to deep amber.
// ═══════════════════════════════════════════════════════════
void patternWarmWave(uint8_t stripIndex) {
    strip.clear();

    // Phase offset per strip creates horizontal sweep
    uint8_t stripPhase = (uint8_t)((uint16_t)stripIndex * 180 / TOTAL_STRIPS);

    for (uint8_t f = 0; f < CITY_FLOORS; f++) {
        // Vertical wave component — floor position creates upward motion
        uint8_t floorPhase = (uint8_t)((uint16_t)f * 128 / CITY_FLOORS);

        // Combined phase: animOffset drives time, strip+floor drive space
        uint8_t phase = (uint8_t)(animOffset * 2) + floorPhase + stripPhase;
        uint8_t val   = sin8(phase);

        // Map sine (0-255) to brightness with a warm floor
        // Minimum brightness 15 so building is never fully dark
        uint8_t b = 2 + (uint8_t)((uint16_t)val * 240 / 255);

        drawFloor(f, warmWhite(b));
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 1 — RAIN DROP
//  -------------------------------------------------------
//  Warm "droplets" fall from top to bottom on random strips.
//  Each drop has a bright head and a fading tail.
//  New drops spawn periodically on random strips.
//  Global update runs once per frame, render is per-strip.
// ═══════════════════════════════════════════════════════════
void updateRainDrops() {
    if (!dropsInitialized) {
        for (uint8_t i = 0; i < MAX_DROPS; i++) {
            drops[i].active = false;
        }
        dropsInitialized = true;
    }

    // Move existing drops downward
    for (uint8_t i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;
        drops[i].floorPos -= drops[i].speed;
        if (drops[i].floorPos < -(int8_t)drops[i].tailLen) {
            drops[i].active = false;
        }
    }

    // Spawn new drops
    dropSpawnTimer++;
    if (dropSpawnTimer >= 3) {      // spawn attempt every 3 frames
        dropSpawnTimer = 0;
        // Try to spawn 1-2 new drops
        uint8_t toSpawn = 1 + (random(100) < 40 ? 1 : 0);
        for (uint8_t n = 0; n < toSpawn; n++) {
            for (uint8_t i = 0; i < MAX_DROPS; i++) {
                if (!drops[i].active) {
                    drops[i].stripIdx   = random(TOTAL_STRIPS);
                    drops[i].floorPos   = CITY_FLOORS - 1;
                    drops[i].brightness = 180 + random(76);   // 180–255
                    drops[i].speed      = 1 + random(3);      // 1–3
                    drops[i].tailLen    = 3 + random(5);       // 3–7
                    drops[i].active     = true;
                    break;
                }
            }
        }
    }
}

void patternRainDrop(uint8_t stripIndex) {
    strip.clear();

    // Subtle ambient glow on all floors
    // for (uint8_t f = 0; f < CITY_FLOORS; f++) {
    //     drawFloor(f, deepAmber(8));
    // }

    // Draw drops that belong to this strip
    for (uint8_t i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;
        if (drops[i].stripIdx != stripIndex) continue;

        // Draw head + tail
        for (uint8_t t = 0; t <= drops[i].tailLen; t++) {
            int8_t flr = drops[i].floorPos + t;   // tail goes upward
            if (flr < 0 || flr >= CITY_FLOORS) continue;

            // Brightness fades along tail
            uint8_t tailFade = drops[i].brightness -
                               (uint8_t)((uint16_t)t * drops[i].brightness / (drops[i].tailLen + 1));
            if (tailFade < 10) tailFade = 10;

            // Head is warm white, tail transitions to deep amber
            if (t == 0) {
                drawFloor(flr, warmWhite(tailFade));
            } else {
                drawFloor(flr, deepAmber(tailFade));
            }
        }
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 2 — BREATHING COLUMNS
//  -------------------------------------------------------
//  Each strip (column) breathes independently with a
//  staggered sine phase. The entire column pulses together,
//  creating a organic living-building effect.
//  Alternates between warm white and soft gold per strip.
// ═══════════════════════════════════════════════════════════
void patternBreathingColumns(uint8_t stripIndex) {
    strip.clear();

    // Each strip gets a unique phase offset — spread across full cycle
    uint8_t phase = (uint8_t)(animOffset) +
                    (uint8_t)((uint16_t)stripIndex * 256 / TOTAL_STRIPS);

    // Slow sine breath
    uint8_t breath = sin8(phase);

    // Map to brightness: minimum 5 so strips don't fully vanish
    uint8_t b = 5 + (uint8_t)((uint16_t)breath * 250 / 255);

    // Alternate color between strips for visual depth
    bool useGold = (stripIndex & 1);

    for (uint8_t f = 0; f < CITY_FLOORS; f++) {
        // Add slight floor-level variation for texture
        uint8_t floorVar = (uint8_t)((uint16_t)f * 30 / CITY_FLOORS);
        uint8_t finalB   = b;

        // Top floors slightly brighter, bottom slightly dimmer
        if (f > CITY_FLOORS - 10) {
            finalB = (uint8_t)min(255, (uint16_t)b + floorVar);
        } else if (f < 10) {
            finalB = (b > floorVar) ? b - floorVar / 2 : 5;
        }

        drawFloor(f, useGold ? softGold(finalB) : warmWhite(finalB));
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 3 — RISING FILL
//  -------------------------------------------------------
//  The building fills with warm light from bottom to top,
//  pauses briefly when full, then drains back down.
//  The "waterline" floor glows extra bright.
//  Global update advances fillLevel once per frame.
// ═══════════════════════════════════════════════════════════
void updateFillLevel() {
    // Pause at top and bottom
    if (fillDelay > 0) {
        fillDelay--;
        return;
    }

    fillSubStep++;
    if (fillSubStep < 2) return;   // advance every 2nd frame → slower fill
    fillSubStep = 0;

    fillLevel += fillDir;

    if (fillLevel >= CITY_FLOORS) {
        fillLevel = CITY_FLOORS - 1;
        fillDir   = -1;
        fillDelay = 30;   // pause ~1s at top
    } else if (fillLevel < 0) {
        fillLevel = 0;
        fillDir   = 1;
        fillDelay = 20;   // pause ~0.6s at bottom
    }
}

void patternRisingFill(uint8_t stripIndex) {
    strip.clear();

    // Slight per-strip variation in fill height for organic look
    int8_t stripVar   = (int8_t)((int16_t)sin8((uint8_t)(stripIndex * 19)) - 128) / 32;
    int8_t localLevel = fillLevel + stripVar;
    if (localLevel < 0) localLevel = 0;
    if (localLevel >= CITY_FLOORS) localLevel = CITY_FLOORS - 1;

    for (uint8_t f = 0; f < CITY_FLOORS; f++) {
        if (f <= localLevel) {
            // Filled portion — gradient: brighter near the waterline
            uint8_t distFromTop = localLevel - f;
            uint8_t b;
            if (distFromTop == 0) {
                b = 255;   // waterline — brightest
            } else if (distFromTop < 4) {
                b = 200 - distFromTop * 25;   // bright zone near waterline
            } else {
                b = 100;   // base fill brightness
            }
            drawFloor(f, warmWhite(b));
        } else {
            // Unfilled — dim ambient
            drawFloor(f, deepAmber(6));
        }
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 4 — SPARKLE TWINKLE
//  -------------------------------------------------------
//  Random individual "windows" across the building twinkle
//  on and off independently. Each sparkle fades in then out
//  at its own speed, like city apartment lights.
//  Global update manages all sparkle lifetimes.
// ═══════════════════════════════════════════════════════════
void updateSparkles() {
    if (!sparklesInitialized) {
        for (uint8_t i = 0; i < MAX_SPARKLES; i++) {
            sparkles[i].active = false;
        }
        sparklesInitialized = true;
    }

    // Update existing sparkles
    for (uint8_t i = 0; i < MAX_SPARKLES; i++) {
        if (!sparkles[i].active) continue;

        int16_t next = (int16_t)sparkles[i].brightness +
                       (int16_t)sparkles[i].dir * sparkles[i].speed;

        if (next >= 255) {
            sparkles[i].brightness = 255;
            sparkles[i].dir = -1;
        } else if (next <= 0) {
            sparkles[i].active = false;   // done, slot freed
        } else {
            sparkles[i].brightness = (uint8_t)next;
        }
    }

    // Spawn new sparkles — try to keep ~40 active
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < MAX_SPARKLES; i++) {
        if (sparkles[i].active) activeCount++;
    }

    uint8_t toSpawn = 0;
    if (activeCount < 35) toSpawn = 3;
    else if (activeCount < 45) toSpawn = 1;

    for (uint8_t n = 0; n < toSpawn; n++) {
        for (uint8_t i = 0; i < MAX_SPARKLES; i++) {
            if (!sparkles[i].active) {
                sparkles[i].stripIdx   = random(TOTAL_STRIPS);
                sparkles[i].floor      = random(CITY_FLOORS);
                sparkles[i].brightness = 1;
                sparkles[i].dir        = 1;
                sparkles[i].speed      = 2 + random(6);   // 2–7
                sparkles[i].active     = true;
                break;
            }
        }
    }
}

void patternSparkleTwinkle(uint8_t stripIndex) {
    strip.clear();

    // Very dim base — like distant city ambient
    for (uint8_t f = 0; f < CITY_FLOORS; f++) {
        drawFloor(f, deepAmber(4));
    }

    // Draw sparkles belonging to this strip
    for (uint8_t i = 0; i < MAX_SPARKLES; i++) {
        if (!sparkles[i].active) continue;
        if (sparkles[i].stripIdx != stripIndex) continue;

        // Mix warm white and soft gold randomly per sparkle
        uint32_t col = (i & 1) ? softGold(sparkles[i].brightness)
                                : warmWhite(sparkles[i].brightness);
        drawFloor(sparkles[i].floor, col);
    }
}

// ═══════════════════════════════════════════════════════════
//  PATTERN 5 — STRIP PAIR CHASE
//  -------------------------------------------------------
//  Two adjacent strips light up at a time, sweep left→right
//  then right→left (ping-pong). Previous pair turns off.
// ═══════════════════════════════════════════════════════════
void patternStripPairChase(uint8_t stripIndex) {
    // 20 pairs total (40 strips / 2)
    uint8_t totalPairs = TOTAL_STRIPS / 2;
    uint16_t step = (animOffset / 8) % ((uint16_t)totalPairs * 2);
    uint8_t activePair = (step < totalPairs)
                         ? (uint8_t)step
                         : (uint8_t)((uint16_t)totalPairs * 2 - 1 - step);

    uint8_t pairStart = activePair * 2;

    if (stripIndex == pairStart || stripIndex == pairStart + 1) {
        for (uint8_t f = 0; f < CITY_FLOORS; f++) {
            drawFloor(f, warmWhite(220));
        }
    } else {
        strip.clear();
    }
}
// ═══════════════════════════════════════════════════════════
//  ALL ON — warm white
// ═══════════════════════════════════════════════════════════
void allOn() {
    uint32_t ww = strip.Color(255, 200, 130);
    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        strip.setPixelColor(i, ww);
    }
    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) {
        pushStrip(s);
    }
    Serial.println(F("All ON — warm white"));
}


// ═══════════════════════════════════════════════════════════
//  ALL OFF
// ═══════════════════════════════════════════════════════════
void allOff() {
    strip.clear();
    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) {
        pushStrip(s);
    }
    Serial.println(F("All OFF"));
}
