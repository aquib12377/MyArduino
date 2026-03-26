/*
 * ============================================================
 *  BUILDING LED CONTROLLER — Arduino Mega 2560
 *  Adafruit NeoPixel Edition
 * ============================================================
 *  Building 1 : Strips  0–19  (20 vertical strips)
 *  Building 2 : Strips 20–39  (20 vertical strips)
 *  Both buildings animate simultaneously and identically.
 *
 *  Memory: ONE Adafruit_NeoPixel object with setPin() swap.
 *  Only 300-byte pixel buffer shared for all strips.
 *
 *  I2C Slave (address 0x08) — receives commands from ESP32-S3
 * ============================================================
 *  COMMANDS:
 *    0x01 = Pattern Mode  (cycles 5 patterns automatically)
 *    0x02 = All ON        (soft warm white)
 *    0x03 = All OFF
 * ============================================================
 *  PATTERNS (5 total, both buildings simultaneously):
 *    0 = Warm Wave        — sine ripple rolls across facade
 *    1 = Rain Drop        — warm droplets fall on random strips
 *    2 = Vertical Chase   — strip pairs light up & stay ON
 *    3 = Raindrop Collect — drops fall & pool collects at bottom
 *    4 = RGB Smooth Fade  — soft pastel hues cycle through all
 * ============================================================
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ── Geometry ──────────────────────────────────────────────
#define NUM_LEDS_PER_STRIP    100
#define TOTAL_STRIPS           40
#define STRIPS_PER_BUILDING    20   // 20 strips per building
#define BRIGHTNESS            255   // global cap (0-255)

// ── City floor geometry ───────────────────────────────────
#define CITY_FLOORS            50   // 50 floors per strip
#define CITY_LEDS_PER_FLOOR     2   // 2 LEDs per floor

// ── I2C ──────────────────────────────────────────────────
#define I2C_SLAVE_ADDR        0x08

// ── Commands ─────────────────────────────────────────────
#define CMD_PATTERNS          0x01
#define CMD_ALL_ON            0x02
#define CMD_ALL_OFF           0x03

// ── Timing ───────────────────────────────────────────────
const uint8_t  NUM_PATTERNS     = 5;
const uint32_t PATTERN_DURATION = 15000;   // ms per pattern
const uint16_t FRAME_INTERVAL   = 30;      // ms per frame (~33 fps)
const uint32_t IDLE_TIMEOUT     = 5000;    // ms before auto-start

// ── Single NeoPixel object — pin swapped per strip ───────
Adafruit_NeoPixel strip(NUM_LEDS_PER_STRIP, 4, NEO_GRB + NEO_KHZ800);

// ── Pin map: Building 1 = [0..19], Building 2 = [20..39] ─
const uint8_t STRIP_PINS[TOTAL_STRIPS] = {
    13, 12, 11, 10,  9,  8,  7,  6,  5,  4,   // B1 strips  0– 9
    22, 23, 25, 27, 29, 31, 33, 35, 37, 39,   // B1 strips 10–19
    A15,A14,A13,A12,A11,A10, A9, A8, A7, A6,  // B2 strips 20–29
    50, 48, 46, 44, 42, 40, 38, 36, 34, 32    // B2 strips 30–39
};

// ── Global control state ──────────────────────────────────
volatile uint8_t currentCommand = CMD_ALL_OFF;
volatile bool    commandChanged = false;

uint8_t  activePattern     = 0;
uint32_t patternCycleTimer = 0;
uint32_t frameTimer        = 0;
uint32_t lastCommandTime   = 0;
uint16_t animOffset        = 0;


// ═══════════════════════════════════════════════════════════
//  SINE TABLE (PROGMEM) — fast 8-bit sine lookup
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
//  SOFT COLOR HELPERS
//  All colors are intentionally muted / pastel — no harsh
//  saturated primaries. Max component values kept ≤ 220.
// ═══════════════════════════════════════════════════════════

// Warm amber-white (building "candlelight")
inline uint32_t warmWhite(uint8_t b) {
    return strip.Color(b,
                       (uint16_t)b * 155 / 255,
                       (uint16_t)b *  55 / 255);
}

// Deep amber for tails / accents
inline uint32_t deepAmber(uint8_t b) {
    return strip.Color(b,
                       (uint16_t)b *  95 / 255,
                       (uint16_t)b *  18 / 255);
}

// Soft sage-mint: gentle green-teal  (Vertical Chase "kept-ON")
inline uint32_t softMint(uint8_t b) {
    return strip.Color((uint16_t)b *  55 / 255,
                       b,
                       (uint16_t)b * 160 / 255);
}

// Soft sky-blue: raindrop pool
inline uint32_t softBlue(uint8_t b) {
    return strip.Color((uint16_t)b *  70 / 255,
                       (uint16_t)b * 145 / 255,
                       b);
}

// Soft cyan: falling raindrop head
inline uint32_t softCyan(uint8_t b) {
    return strip.Color((uint16_t)b *  35 / 255,
                       b,
                       b);
}

// Pastel HSV — low saturation (120/255) keeps colours soft
inline uint32_t pastelHSV(uint16_t hue, uint8_t val) {
    return Adafruit_NeoPixel::ColorHSV(hue, 110, val);
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
//  PUSH BUFFER → ONE PHYSICAL STRIP
// ═══════════════════════════════════════════════════════════
void pushStrip(uint8_t idx) {
    strip.setPin(STRIP_PINS[idx]);
    pinMode(STRIP_PINS[idx], OUTPUT);
    strip.show();
}


// ═══════════════════════════════════════════════════════════
//  PATTERN STATE — Rain Drop (pattern 1)
// ═══════════════════════════════════════════════════════════
#define MAX_DROPS 14
struct RainDrop {
    uint8_t stripIdx;
    int8_t  floorPos;
    uint8_t brightness;
    uint8_t speed;
    uint8_t tailLen;
    bool    active;
};
static RainDrop drops[MAX_DROPS];
static bool     dropsInit     = false;
static uint8_t  dropSpawnTmr  = 0;


// ═══════════════════════════════════════════════════════════
//  PATTERN STATE — Vertical Chase (pattern 2)
// ═══════════════════════════════════════════════════════════
// Each building has 10 strip-pairs (strips 0-1, 2-3, … 18-19).
// chasePairPos = the pair index that is currently the "live front".
// All pairs with index < chasePairPos are kept ON (softMint dim).
// The live pair glows brighter with a gentle pulse.
// After all 10 pairs are ON, hold briefly then reset.
#define CHASE_PAIRS  10                 // STRIPS_PER_BUILDING / 2
static uint8_t chasePos      = 0;      // 0–9
static uint8_t chaseSubStep  = 0;
static bool    chaseHolding  = false;  // true when all strips are ON, waiting to reset


// ═══════════════════════════════════════════════════════════
//  PATTERN STATE — Raindrop Collect (pattern 3)
// ═══════════════════════════════════════════════════════════
// A 2-floor "drop" falls from the top of every strip.
// When it reaches the pool surface it merges: pool grows by 2.
// The drop then resets to the top.  Repeats until pool is full,
// then holds briefly and resets.
static int8_t  rcDrop     = CITY_FLOORS - 1;  // floor of drop head
static uint8_t rcPool     = 0;                 // floors collected (from floor 0 up)
static uint8_t rcSubStep  = 0;
static bool    rcInit     = false;
static uint8_t rcHoldTmr  = 0;                 // countdown after full fill


// ═══════════════════════════════════════════════════════════
//  RESET ALL PATTERN STATE
// ═══════════════════════════════════════════════════════════
void resetPatternState() {
    dropsInit    = false;
    dropSpawnTmr = 0;

    chasePos     = 0;
    chaseSubStep = 0;
    chaseHolding = false;

    rcDrop     = CITY_FLOORS - 1;
    rcPool     = 0;
    rcSubStep  = 0;
    rcInit     = false;
    rcHoldTmr  = 0;
}


// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println(F("=== City Building LED Controller ==="));
    Serial.print(F("Buildings: 2 × ")); Serial.print(STRIPS_PER_BUILDING);
    Serial.print(F(" strips, ")); Serial.print(CITY_FLOORS);
    Serial.println(F(" floors each"));

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
    Serial.println(F("Ready — waiting for I2C commands…"));
    Serial.print(F("Free RAM: ")); Serial.print(freeRam()); Serial.println(F(" bytes"));
}

int freeRam() {
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
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

    // ── Handle incoming command ───────────────────────────
    if (commandChanged) {
        commandChanged = false;
        Serial.print(F("CMD: 0x")); Serial.println(currentCommand, HEX);

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
        Serial.println(F("Idle → auto-starting patterns"));
    }

    if (currentCommand == CMD_ALL_OFF || currentCommand == CMD_ALL_ON) {
        delay(50);
        return;
    }

    // ── Frame-rate gate ───────────────────────────────────
    if (now - frameTimer < FRAME_INTERVAL) return;
    frameTimer = now;
    animOffset++;

    // ── Auto-cycle patterns ──────────────────────────────
    if (now - patternCycleTimer >= PATTERN_DURATION) {
        patternCycleTimer = now;
        activePattern     = (activePattern + 1) % NUM_PATTERNS;
        resetPatternState();
        Serial.print(F("Pattern → ")); Serial.println(activePattern);
    }

    // ── Per-frame global updates ─────────────────────────
    if (activePattern == 1) updateRainDrops();
    if (activePattern == 2) updateChase();
    if (activePattern == 3) updateRainCollect();

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
        case 0: patternWarmWave(s);       break;
        case 1: patternRainDrop(s);       break;
        case 2: patternVerticalChase(s);  break;
        case 3: patternRainCollect(s);    break;
        case 4: patternRGBFade(s);        break;
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 0 — WARM WAVE
//  -------------------------------------------------------
//  A sine-wave ripple rolls upward and across both building
//  facades simultaneously. Strip phase offset makes the wave
//  appear to travel horizontally as well as vertically.
//  All colours stay in the warm amber-white family.
// ═══════════════════════════════════════════════════════════
void patternWarmWave(uint8_t stripIndex) {
    strip.clear();

    // Phase spread across all 40 strips so both buildings look
    // like one continuous wave scrolling across the skyline.
    uint8_t stripPhase = (uint8_t)((uint16_t)stripIndex * 180 / TOTAL_STRIPS);

    for (uint8_t f = 0; f < CITY_FLOORS; f++) {
        uint8_t floorPhase = (uint8_t)((uint16_t)f * 128 / CITY_FLOORS);
        uint8_t phase      = (uint8_t)(animOffset * 2) + floorPhase + stripPhase;
        uint8_t val        = sin8(phase);
        // Map 0-255 → 8-220  (never fully dark, never blinding white)
        uint8_t b          = 8 + (uint8_t)((uint16_t)val * 212 / 255);
        drawFloor(f, warmWhite(b));
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 1 — RAIN DROP
//  -------------------------------------------------------
//  Warm amber droplets fall on random strips across BOTH
//  buildings. Each drop has a bright head and a fading tail.
//  The pattern runs identically across all 40 strips.
// ═══════════════════════════════════════════════════════════
void updateRainDrops() {
    if (!dropsInit) {
        for (uint8_t i = 0; i < MAX_DROPS; i++) drops[i].active = false;
        dropsInit = true;
    }

    // Move drops downward
    for (uint8_t i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;
        drops[i].floorPos -= drops[i].speed;
        if (drops[i].floorPos < -(int8_t)drops[i].tailLen)
            drops[i].active = false;
    }

    // Spawn new drops every ~3 frames
    dropSpawnTmr++;
    if (dropSpawnTmr >= 3) {
        dropSpawnTmr = 0;
        uint8_t toSpawn = 1 + (random(100) < 40 ? 1 : 0);
        for (uint8_t n = 0; n < toSpawn; n++) {
            for (uint8_t i = 0; i < MAX_DROPS; i++) {
                if (!drops[i].active) {
                    drops[i].stripIdx   = random(TOTAL_STRIPS);
                    drops[i].floorPos   = CITY_FLOORS - 1;
                    drops[i].brightness = 170 + random(70);   // 170–240, soft cap
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

    for (uint8_t i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active || drops[i].stripIdx != stripIndex) continue;

        for (uint8_t t = 0; t <= drops[i].tailLen; t++) {
            int8_t flr = drops[i].floorPos + (int8_t)t;
            if (flr < 0 || flr >= CITY_FLOORS) continue;

            uint8_t fade = drops[i].brightness -
                           (uint8_t)((uint16_t)t * drops[i].brightness /
                                     (drops[i].tailLen + 1));
            if (fade < 12) fade = 12;

            drawFloor((uint8_t)flr,
                      (t == 0) ? warmWhite(fade) : deepAmber(fade));
        }
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 2 — VERTICAL CHASE  (strips kept ON)
//  -------------------------------------------------------
//  Both buildings chase identically:
//  • 10 strip-pairs per building (pair 0 = strips 0-1/20-21,
//    pair 1 = strips 2-3/22-23, … pair 9 = strips 18-19/38-39)
//  • Each frame-cycle the "live" pair advances by one.
//  • All previous pairs stay ON at a steady soft-mint glow.
//  • The current live pair glows brighter with a soft pulse.
//  • When all 10 pairs are ON, hold ~2 s then reset to pair 0.
// ═══════════════════════════════════════════════════════════
void updateChase() {
    if (chaseHolding) {
        // Count down hold period (~2 s = 67 frames at 30 ms)
        chaseSubStep++;
        if (chaseSubStep >= 67) {
            chasePos     = 0;
            chaseSubStep = 0;
            chaseHolding = false;
        }
        return;
    }

    chaseSubStep++;
    if (chaseSubStep < 42) return;   // ~1.25 s per pair
    chaseSubStep = 0;

    chasePos++;
    if (chasePos >= CHASE_PAIRS) {
        chasePos     = CHASE_PAIRS - 1;  // stay at max while holding
        chaseHolding = true;
        chaseSubStep = 0;
    }
}

void patternVerticalChase(uint8_t stripIndex) {
    // Map any strip to its per-building index (0–19) then to a pair (0–9)
    uint8_t buildStrip = (stripIndex < STRIPS_PER_BUILDING)
                         ? stripIndex
                         : stripIndex - STRIPS_PER_BUILDING;
    uint8_t pair = buildStrip / 2;

    strip.clear();

    if (pair < chasePos) {
        // Previously lit pair — stays ON at relaxed brightness
        for (uint8_t f = 0; f < CITY_FLOORS; f++) {
            drawFloor(f, softMint(110));
        }
    } else if (pair == chasePos) {
        // Active "live" pair — gentle brightness pulse
        uint8_t pulse = 150 +
                        (uint8_t)((uint16_t)sin8((uint8_t)(animOffset * 3)) * 65 / 255);
        for (uint8_t f = 0; f < CITY_FLOORS; f++) {
            drawFloor(f, softMint(pulse));
        }
    }
    // else: pair > chasePos → strip remains off (already cleared)
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 3 — RAINDROP COLLECT
//  -------------------------------------------------------
//  Every strip on both buildings shows the same animation:
//  • A 2-floor cyan drop falls from the top.
//  • The bottom has a "pool" of soft-blue floors that grows
//    by 2 each time a drop lands.
//  • The drop restarts from the top after each landing.
//  • When the pool fills all 50 floors, hold ~1 s then reset.
//
//  Visual result: the building appears to fill with water
//  one raindrop at a time.
// ═══════════════════════════════════════════════════════════
void updateRainCollect() {
    if (!rcInit) {
        rcDrop    = CITY_FLOORS - 1;
        rcPool    = 0;
        rcSubStep = 0;
        rcHoldTmr = 0;
        rcInit    = true;
    }

    // Hold after full fill
    if (rcHoldTmr > 0) {
        rcHoldTmr--;
        if (rcHoldTmr == 0) {
            // Full reset
            rcDrop = CITY_FLOORS - 1;
            rcPool = 0;
        }
        return;
    }

    rcSubStep++;
    if (rcSubStep < 3) return;   // drop moves 1 floor every 3 frames (smooth)
    rcSubStep = 0;

    rcDrop--;

    // Check if drop has reached the pool surface
    if (rcDrop < (int8_t)rcPool) {
        rcPool += 2;                   // collect 2 more floors into the pool
        if (rcPool >= CITY_FLOORS) {
            rcPool    = CITY_FLOORS;   // clamp
            rcHoldTmr = 33;            // hold ~1 s then reset
        } else {
            rcDrop = CITY_FLOORS - 1;  // restart drop from top
        }
    }
}

void patternRainCollect(uint8_t stripIndex) {
    // Identical on every strip — same shared state drives both buildings
    strip.clear();

    // ── Pool (bottom, growing upward) ────────────────────
    for (uint8_t f = 0; f < rcPool; f++) {
        // Gradient: surface bright, depth dim
        uint8_t dist = rcPool - 1 - f;   // 0 at surface, grows downward
        uint8_t b;
        if      (dist == 0)  b = 190;
        else if (dist < 3)   b = 155 - dist * 20;
        else if (dist < 8)   b = 100 - dist * 5;
        else                 b = 55;
        drawFloor(f, softBlue(b));
    }

    // ── Falling drop (2-floor size) ──────────────────────
    if (rcDrop >= 0 && rcDrop < CITY_FLOORS) {
        drawFloor((uint8_t)rcDrop, softCyan(220));   // head — brighter
    }
    if (rcDrop - 1 >= 0 && rcDrop - 1 < CITY_FLOORS) {
        drawFloor((uint8_t)(rcDrop - 1), softCyan(140)); // tail — softer
    }
}


// ═══════════════════════════════════════════════════════════
//  PATTERN 4 — RGB SMOOTH FADE
//  -------------------------------------------------------
//  Both buildings cycle through soft pastel hues together.
//  • A slow base hue sweeps the full colour wheel (~40 s cycle).
//  • Each strip carries a tiny hue offset so the facade
//    shimmers rather than looking like a flat colour block.
//  • Each floor also has a small hue offset for depth.
//  • Saturation is kept low (110/255) for muted pastels.
//  • Brightness pulses gently (never fully off, never harsh).
// ═══════════════════════════════════════════════════════════
void patternRGBFade(uint8_t stripIndex) {
    strip.clear();

    // Base hue: full 65536-step wheel, advances ~50 steps per frame
    uint16_t baseHue = (uint32_t)animOffset * 50UL;

    // Per-strip hue shimmer (spread ~5 % of wheel across 40 strips)
    uint16_t hue = baseHue + (uint16_t)stripIndex * 80;

    // Gentle brightness sine: 70–160  (never blinding, never dark)
    uint8_t brightness = 70 +
                         (uint8_t)((uint16_t)sin8((uint8_t)(animOffset * 2)) * 90 / 255);

    for (uint8_t f = 0; f < CITY_FLOORS; f++) {
        // Tiny floor hue offset — creates a soft vertical gradient
        uint16_t floorHue = hue + (uint16_t)f * 160;
        drawFloor(f, pastelHSV(floorHue, brightness));
    }
}


// ═══════════════════════════════════════════════════════════
//  ALL ON — soft warm white
// ═══════════════════════════════════════════════════════════
void allOn() {
    uint32_t col = warmWhite(200);
    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        strip.setPixelColor(i, col);
    }
    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) pushStrip(s);
    Serial.println(F("All ON — soft warm white"));
}


// ═══════════════════════════════════════════════════════════
//  ALL OFF
// ═══════════════════════════════════════════════════════════
void allOff() {
    strip.clear();
    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) pushStrip(s);
    Serial.println(F("All OFF"));
}
