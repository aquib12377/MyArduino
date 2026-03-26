/*
 * ============================================================
 *  BUILDING LED CONTROLLER — Arduino Mega 2560
 *  Adafruit NeoPixel Edition
 * ============================================================
 *  Both buildings share same data pins (Y-split/daisy-chained)
 *  41 strips × 100 LEDs = 4100 LEDs per building pair
 *
 *  Memory: ONE Adafruit_NeoPixel object with setPin() swap.
 *  Only 300 bytes pixel buffer for everything.
 *
 *  I2C Slave (address 0x08) — receives commands from ESP32-S3
 * ============================================================
 *  COMMANDS:
 *    0x01 = Pattern Mode (cycles 4 patterns continuously)
 *    0x02 = All ON  (warm white)
 *    0x03 = All OFF
 * ============================================================
 *  PIN MAP (41 strips):
 *    Strip  1 → Pin 13       Strip 19 → Pin 22
 *    Strip  2 → Pin 12       Strip 20 → Pin 23
 *    Strip  3 → Pin 11       Strip 21 → Pin 25
 *    Strip  4 → Pin 10       Strip 22 → Pin 27
 *    Strip  5 → Pin  9       Strip 23 → Pin 29
 *    Strip  6 → Pin  8       Strip 24 → Pin 31
 *    Strip  7 → Pin  7       Strip 25 → Pin 33
 *    Strip  8 → Pin  6       Strip 26 → Pin 35
 *    Strip  9 → Pin  5       Strip 27 → Pin 37
 *    Strip 10 → Pin  4       Strip 28 → Pin 39
 *                             Strip 29 → Pin 41
 *
 *    Strip 37 → Pin A15(69)  Strip 53 → Pin 50
 *    Strip 38 → Pin A14(68)  Strip 54 → Pin 48
 *    Strip 39 → Pin A13(67)  Strip 55 → Pin 46
 *    Strip 40 → Pin A12(66)  Strip 56 → Pin 44
 *    Strip 41 → Pin A11(65)  Strip 57 → Pin 42
 *    Strip 42 → Pin A10(64)  Strip 58 → Pin 40
 *    Strip 43 → Pin A9 (63)  Strip 59 → Pin 38
 *    Strip 44 → Pin A8 (62)  Strip 60 → Pin 36
 *    Strip 45 → Pin A7 (61)  Strip 61 → Pin 34
 *    Strip 46 → Pin A6 (60)  Strip 62 → Pin 32
 *
 *    I2C:  SDA=20, SCL=21
 * ============================================================
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ── Geometry ──────────────────────────────────────────────
#define NUM_LEDS_PER_STRIP   100
#define TOTAL_STRIPS          41
#define BRIGHTNESS           180   // 0–255

// ── I2C ──────────────────────────────────────────────────
#define I2C_SLAVE_ADDR       0x08

// ── Commands ─────────────────────────────────────────────
#define CMD_PATTERNS         0x01
#define CMD_ALL_ON           0x02
#define CMD_ALL_OFF          0x03

// ── Single NeoPixel object — pin is swapped via setPin() ─
Adafruit_NeoPixel strip(NUM_LEDS_PER_STRIP, 4, NEO_GRB + NEO_KHZ800);

// ── Pin map — exact mapping from strip number to Mega pin ─
//  Index 0–9:   Strips 1–10   (Pins 13,12,11,10,9,8,7,6,5,4)
//  Index 10–20: Strips 19–29  (Pins 22,23,25,27,29,31,33,35,37,39,41)
//  Index 21–30: Strips 37–46  (Pins A15–A6 = 69–60)
//  Index 31–40: Strips 53–62  (Pins 50,48,46,44,42,40,38,36,34,32)

const uint8_t STRIP_PINS[TOTAL_STRIPS] = {
    // ── Strips 1–10 ─────────────────────────────────────
    13, 12, 11, 10,  9,  8,  7,  6,  5,  4,     // [0–9]

    // ── Strips 19–29 ────────────────────────────────────
    22, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, // [10–20]

    // ── Strips 37–46 (Analog pins as digital) ───────────
    69, 68, 67, 66, 65, 64, 63, 62, 61, 60,     // [21–30]  A15–A6

    // ── Strips 53–62 ────────────────────────────────────
    50, 48, 46, 44, 42, 40, 38, 36, 34, 32      // [31–40]
};

// ── State ────────────────────────────────────────────────
volatile uint8_t currentCommand  = CMD_ALL_OFF;
volatile bool    commandChanged  = false;

uint8_t  activePattern       = 0;
uint32_t patternCycleTimer   = 0;
const uint32_t PATTERN_DURATION = 30000;   // 30s per pattern
const uint8_t  NUM_PATTERNS     = 4;

uint32_t frameTimer          = 0;
const uint16_t FRAME_INTERVAL = 30;        // ~33 FPS target

uint16_t animOffset          = 0;          // master animation counter


// ═════════════════════════════════════════════════════════
//  FAST 8-BIT SINE TABLE (PROGMEM — 256 bytes flash)
//  Input 0–255 → Output 0–255 full sine wave
// ═════════════════════════════════════════════════════════
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

static inline uint8_t qadd8(uint8_t a, uint8_t b) {
    uint16_t t = (uint16_t)a + b;
    return (t > 255) ? 255 : (uint8_t)t;
}


// ═════════════════════════════════════════════════════════
//  HSV → RGB CONVERSION
//  h: 0–255 hue, s: 0–255 sat, v: 0–255 value
// ═════════════════════════════════════════════════════════
uint32_t hsvToColor(uint8_t h, uint8_t s, uint8_t v)
{
    if (s == 0) {
        return strip.Color(v, v, v);
    }

    uint8_t region    = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    uint8_t r, g, b;
    switch (region) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return strip.Color(r, g, b);
}


// ═════════════════════════════════════════════════════════
//  PUSH BUFFER TO ONE STRIP (setPin + show)
// ═════════════════════════════════════════════════════════
void pushStrip(uint8_t stripIndex)
{
    strip.setPin(STRIP_PINS[stripIndex]);
    pinMode(STRIP_PINS[stripIndex], OUTPUT);
    strip.show();
}


// ═════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════
void setup()
{
    Serial.begin(115200);
    Serial.println(F("=== Building LED Controller (NeoPixel) ==="));
    Serial.print(F("Strips: "));
    Serial.print(TOTAL_STRIPS);
    Serial.print(F("  LEDs/strip: "));
    Serial.print(NUM_LEDS_PER_STRIP);
    Serial.print(F("  Total: "));
    Serial.println((uint16_t)TOTAL_STRIPS * NUM_LEDS_PER_STRIP);

    // ── I2C slave ────────────────────────────────────────
    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onReceive(onI2CReceive);

    // ── NeoPixel init ────────────────────────────────────
    strip.begin();
    strip.setBrightness(BRIGHTNESS);

    // ── Set all strip pins as OUTPUT ─────────────────────
    for (uint8_t i = 0; i < TOTAL_STRIPS; i++) {
        pinMode(STRIP_PINS[i], OUTPUT);
    }

    // ── Start with all off ───────────────────────────────
    allOff();

    Serial.println(F("Ready. Waiting for I2C commands..."));
    Serial.print(F("Free RAM: "));
    Serial.print(freeRam());
    Serial.println(F(" bytes"));
}


// ═════════════════════════════════════════════════════════
//  FREE RAM HELPER
// ═════════════════════════════════════════════════════════
int freeRam()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0
           ? (int)&__heap_start : (int)__brkval);
}


// ═════════════════════════════════════════════════════════
//  I2C RECEIVE HANDLER
// ═════════════════════════════════════════════════════════
void onI2CReceive(int numBytes)
{
    while (Wire.available()) {
        uint8_t cmd = Wire.read();
        if (cmd >= CMD_PATTERNS && cmd <= CMD_ALL_OFF) {
            currentCommand  = cmd;
            commandChanged  = true;
        }
    }
}


// ═════════════════════════════════════════════════════════
//  MAIN LOOP
// ═════════════════════════════════════════════════════════
void loop()
{
    uint32_t now = millis();

    // ── Handle new command ───────────────────────────────
    if (commandChanged) {
        commandChanged = false;
        Serial.print(F("CMD: 0x"));
        Serial.println(currentCommand, HEX);

        if (currentCommand == CMD_ALL_OFF) {
            allOff();
            return;
        }
        if (currentCommand == CMD_ALL_ON) {
            allOn();
            return;
        }
        if (currentCommand == CMD_PATTERNS) {
            activePattern     = 0;
            patternCycleTimer = now;
            animOffset        = 0;
        }
    }

    // ── Static states — no animation needed ──────────────
    if (currentCommand == CMD_ALL_OFF || currentCommand == CMD_ALL_ON) {
        delay(50);
        return;
    }

    // ── Pattern animation frame ──────────────────────────
    if (now - frameTimer < FRAME_INTERVAL) return;
    frameTimer = now;
    animOffset++;

    // ── Cycle patterns ───────────────────────────────────
    if (now - patternCycleTimer >= PATTERN_DURATION) {
        patternCycleTimer = now;
        activePattern = (activePattern + 1) % NUM_PATTERNS;
        Serial.print(F("Pattern → "));
        Serial.println(activePattern);
    }

    // ── Render & push each strip ─────────────────────────
    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) {
        renderPattern(s);
        pushStrip(s);
    }
}


// ═════════════════════════════════════════════════════════
//  RENDER PATTERN INTO SHARED BUFFER
// ═════════════════════════════════════════════════════════
void renderPattern(uint8_t stripIndex)
{
    switch (activePattern) {
        case 0:  patternWarmWave(stripIndex);       break;
        case 1:  patternOceanBreathe(stripIndex);   break;
        case 2:  patternAuroraBorealis(stripIndex);  break;
        case 3:  patternSunsetCascade(stripIndex);   break;
    }
}


// ═════════════════════════════════════════════════════════
//  PATTERN 0 — WARM WAVE
//  Soft golds, ambers & peach flowing upward
// ═════════════════════════════════════════════════════════
void patternWarmWave(uint8_t stripIndex)
{
    uint16_t stripPhase = stripIndex * 600;

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        uint8_t wave1 = sin8(i * 4 + animOffset * 2 + stripPhase);
        uint8_t wave2 = sin8(i * 7 + animOffset * 3 + stripPhase / 2);
        uint8_t blend = (wave1 + wave2) / 2;

        uint8_t hue = 20 + (blend >> 4);
        uint8_t sat = 180 + (wave1 >> 3);
        uint8_t val = 120 + (blend >> 2);

        strip.setPixelColor(i, hsvToColor(hue, sat, val));
    }
}


// ═════════════════════════════════════════════════════════
//  PATTERN 1 — OCEAN BREATHE
//  Gentle blue/cyan/teal undulation like calm water
// ═════════════════════════════════════════════════════════
void patternOceanBreathe(uint8_t stripIndex)
{
    uint16_t stripPhase = stripIndex * 500;
    uint8_t breathe = sin8(animOffset);

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        uint8_t wave   = sin8(i * 5 + animOffset * 2 + stripPhase);
        uint8_t ripple = sin8(i * 11 - animOffset * 4 + stripPhase);

        uint8_t hue = 130 + ((wave + ripple) >> 4);
        uint8_t sat = 170 + (wave >> 3);
        uint8_t val = 80 + ((uint16_t)breathe * 100 / 255)
                         + (ripple >> 3);
        if (val > 220) val = 220;

        strip.setPixelColor(i, hsvToColor(hue, sat, val));
    }
}


// ═════════════════════════════════════════════════════════
//  PATTERN 2 — AURORA BOREALIS
//  Flowing greens, teals & purples like the northern lights
// ═════════════════════════════════════════════════════════
void patternAuroraBorealis(uint8_t stripIndex)
{
    uint16_t stripPhase = stripIndex * 700;

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        uint8_t layer1 = sin8(i * 3 + animOffset     + stripPhase);
        uint8_t layer2 = sin8(i * 5 + animOffset * 2  + stripPhase / 3);
        uint8_t layer3 = sin8(i * 2 - animOffset      + stripPhase * 2);

        uint8_t mix = (layer1 + layer2) / 2;

        uint8_t hue = 85 + ((uint16_t)mix * 95 / 255);
        if (layer3 > 210) {
            hue = 200 + ((layer3 - 210) >> 1);
        }

        uint8_t sat = 190 + (layer1 >> 3);
        uint8_t val = 60 + ((uint16_t)mix * 160 / 255);
        if (layer2 > 240) val = qadd8(val, 50);

        strip.setPixelColor(i, hsvToColor(hue, sat, val));
    }
}


// ═════════════════════════════════════════════════════════
//  PATTERN 3 — SUNSET CASCADE
//  Warm pinks, corals, lavenders cascading down
// ═════════════════════════════════════════════════════════
void patternSunsetCascade(uint8_t stripIndex)
{
    uint16_t stripPhase = stripIndex * 550;

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        uint8_t cascade = sin8(i * 6 - animOffset * 3 + stripPhase);
        uint8_t drift   = sin8(i * 3 + animOffset     + stripPhase);

        uint8_t blend = (cascade + drift) / 2;

        uint8_t hue;
        if (blend < 85) {
            hue = map(blend, 0, 84, 250, 0);
        } else if (blend < 170) {
            hue = map(blend, 85, 169, 0, 16);
        } else {
            hue = map(blend, 170, 255, 16, 210);
        }

        uint8_t sat = 160 + (cascade >> 2);
        uint8_t val = 100 + ((uint16_t)blend * 120 / 255);

        strip.setPixelColor(i, hsvToColor(hue, sat, val));
    }
}


// ═════════════════════════════════════════════════════════
//  ALL ON — warm white
// ═════════════════════════════════════════════════════════
void allOn()
{
    uint32_t warmWhite = strip.Color(255, 200, 130);

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        strip.setPixelColor(i, warmWhite);
    }

    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) {
        pushStrip(s);
    }
    Serial.println(F("All ON — warm white"));
}


// ═════════════════════════════════════════════════════════
//  ALL OFF
// ═════════════════════════════════════════════════════════
void allOff()
{
    strip.clear();

    for (uint8_t s = 0; s < TOTAL_STRIPS; s++) {
        pushStrip(s);
    }
    Serial.println(F("All OFF"));
}
