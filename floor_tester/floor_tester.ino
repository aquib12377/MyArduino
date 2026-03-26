/*
 * ============================================================
 *  FLOOR TESTER — Arduino Mega 2560
 *  62 Strips × 32 LEDs — Serial Monitor Control
 * ============================================================
 *  Usage: Open Serial Monitor (115200 baud)
 *         Type a floor number (1–62) → that floor turns WHITE
 *         All other floors turn OFF
 *         Type 0 → all OFF
 *         Type 99 → all ON
 * ============================================================
 */

#include <Adafruit_NeoPixel.h>

#define TOTAL_FLOORS     62
#define LEDS_PER_FLOOR   32
#define BRIGHTNESS       255

// ── Single shared NeoPixel buffer (reused via setPin) ─────
Adafruit_NeoPixel strip(LEDS_PER_FLOOR, 2, NEO_GRB + NEO_KHZ800);

// ── Pin map: index 0 = Floor 1, index 61 = Floor 62 ──────
const uint8_t FLOOR_PINS[TOTAL_FLOORS] = {
    13, 12, 11, 10,  9,  8,  7,  6,  5,  4,       // Floor  1–10
     3,  2, 14, 15, 16, 17, 18, 19,                // Floor 11–18
    22, 23, 25, 27, 29, 31, 33, 35, 37, 39,        // Floor 19–28
    41, 43, 45, 47, 49, 51, 53, 52,                // Floor 29–36
    A15, A14, A13, A12, A11, A10, A9, A8,           // Floor 37–44
    A7, A6, A5, A4, A3, A2, A1, A0,                // Floor 45–52
    50, 48, 46, 44, 42, 40, 38, 36, 34, 32         // Floor 53–62
};

// ── Current active floor (0 = none) ──────────────────────
uint8_t activeFloor = 0;

// ── Push buffer to a specific strip pin ──────────────────
void pushStrip(uint8_t pin) {
    strip.setPin(pin);
    pinMode(pin, OUTPUT);
    strip.show();
}

// ── Turn all floors OFF ──────────────────────────────────
void allOff() {
    strip.clear();
    for (uint8_t i = 0; i < TOTAL_FLOORS; i++) {
        pushStrip(FLOOR_PINS[i]);
    }
}

// ── Turn all floors WHITE ────────────────────────────────
void allOn() {
    for (uint8_t p = 0; p < LEDS_PER_FLOOR; p++) {
        strip.setPixelColor(p, strip.Color(255, 255, 255));
    }
    for (uint8_t i = 0; i < TOTAL_FLOORS; i++) {
        pushStrip(FLOOR_PINS[i]);
    }
}

// ── Light one floor white, rest off ──────────────────────
void setFloor(uint8_t floor) {
    // Turn off all strips
    strip.clear();
    for (uint8_t i = 0; i < TOTAL_FLOORS; i++) {
        pushStrip(FLOOR_PINS[i]);
    }

    // Turn on the target floor
    if (floor >= 1 && floor <= TOTAL_FLOORS) {
        for (uint8_t p = 0; p < LEDS_PER_FLOOR; p++) {
            strip.setPixelColor(p, strip.Color(255, 255, 255));
        }
        pushStrip(FLOOR_PINS[floor - 1]);

        Serial.print(F("Floor "));
        Serial.print(floor);
        Serial.print(F(" ON → Pin "));
        Serial.println(FLOOR_PINS[floor - 1]);
    }
}

// ══════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n══════════════════════════════════════"));
    Serial.println(F("  FLOOR TESTER — 62 Strips × 32 LEDs"));
    Serial.println(F("══════════════════════════════════════"));
    Serial.println(F("Commands:"));
    Serial.println(F("  1–62  → Light that floor WHITE"));
    Serial.println(F("  0     → All OFF"));
    Serial.println(F("  99    → All ON"));
    Serial.println(F("══════════════════════════════════════\n"));

    strip.begin();
    strip.setBrightness(BRIGHTNESS);

    // Set all pins as OUTPUT
    for (uint8_t i = 0; i < TOTAL_FLOORS; i++) {
        pinMode(FLOOR_PINS[i], OUTPUT);
    }

    allOff();
    Serial.println(F("Ready. Enter floor number...\n"));
}

// ══════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════
void loop() {
    if (Serial.available()) {
        int val = Serial.parseInt();

        // Flush leftover newline/chars
        while (Serial.available()) Serial.read();

        if (val == 0) {
            allOff();
            activeFloor = 0;
            Serial.println(F("All OFF"));
        } else if (val == 99) {
            allOn();
            activeFloor = 99;
            Serial.println(F("All ON"));
        } else if (val >= 1 && val <= TOTAL_FLOORS) {
            activeFloor = val;
            setFloor(val);
        } else {
            Serial.print(F("Invalid. Enter 1–"));
            Serial.print(TOTAL_FLOORS);
            Serial.println(F(", 0=OFF, 99=ALL"));
        }
    }
}
