/*
 * ============================================================
 *  BUILDING LED REMOTE — ESP32-S3
 * ============================================================
 *  4 Buttons (toggle) + 5 Relays + I2C to Arduino Mega
 * ============================================================
 *  BUTTON LOGIC (all toggles — 1st click ON, 2nd click OFF):
 *
 *  BTN 1 — ALL ON
 *    ON:  All relays ON + Building LEDs ON
 *    OFF: Everything OFF
 *
 *  BTN 2 — MALL
 *    ON:  Relay1(Mall) ON, Relay2(Surround) ON, Building LEDs ON
 *          → Podium OFF, Pattern OFF
 *    OFF: Mall OFF, Surround OFF, Building LEDs OFF
 *
 *  BTN 3 — PODIUM
 *    ON:  Relay3,4,5(Podium) ON
 *          → Mall OFF, Surround OFF, Building LEDs OFF, Pattern OFF
 *    OFF: Podium OFF
 *
 *  BTN 4 — PATTERN
 *    ON:  LED patterns start
 *          → Mall OFF, Podium OFF
 *    OFF: Pattern OFF, LEDs OFF
 *
 * ============================================================
 *  RELAY MAP (5 relays, active LOW):
 *    Relay 1 — Mall       → GPIO 10
 *    Relay 2 — Surround   → GPIO 9
 *    Relay 3 — Podium     → GPIO 8
 *    Relay 4 — Podium     → GPIO 7
 *    Relay 5 — Podium     → GPIO 6
 *
 *  BUTTONS:
 *    BTN 1 (GPIO 47) — All ON/OFF
 *    BTN 2 (GPIO 48) — Mall
 *    BTN 3 (GPIO 1)  — Podium
 *    BTN 4 (GPIO 2)  — Pattern
 *
 *  I2C:  SDA=GPIO 11, SCL=GPIO 12
 *  Status LED: GPIO 21
 * ============================================================
 */

#include <Wire.h>

// ── I2C ──────────────────────────────────────────────────
#define I2C_SDA         11
#define I2C_SCL         12
#define MEGA_I2C_ADDR   0x08

// ── Buttons (active LOW with internal pull-up) ───────────
#define BTN_ALL         47
#define BTN_MALL        48
#define BTN_PODIUM       1
#define BTN_PATTERN      2

// ── Relays (active LOW) ──────────────────────────────────
#define RELAY_MALL      10   // Relay 1
#define RELAY_SURROUND   9   // Relay 2
#define RELAY_PODIUM_1   8   // Relay 3
#define RELAY_PODIUM_2   7   // Relay 4
#define RELAY_PODIUM_3   6   // Relay 5

const uint8_t ALL_RELAY_PINS[]    = { RELAY_MALL, RELAY_SURROUND, RELAY_PODIUM_1, RELAY_PODIUM_2, RELAY_PODIUM_3 };
const uint8_t PODIUM_RELAY_PINS[] = { RELAY_PODIUM_1, RELAY_PODIUM_2, RELAY_PODIUM_3 };
#define NUM_ALL_RELAYS     5
#define NUM_PODIUM_RELAYS  3

// ── I2C Commands to Mega ─────────────────────────────────
#define CMD_PATTERNS    0x01
#define CMD_ALL_ON      0x02
#define CMD_ALL_OFF     0x03

// ── Button debounce ──────────────────────────────────────
#define DEBOUNCE_MS     300

struct Button {
    uint8_t   pin;
    uint32_t  lastPress;
    bool      lastState;
};

Button buttons[] = {
    { BTN_ALL,     0, HIGH },
    { BTN_MALL,    0, HIGH },
    { BTN_PODIUM,  0, HIGH },
    { BTN_PATTERN, 0, HIGH }
};
#define NUM_BUTTONS 4

// ── Zone states (toggle tracking) ────────────────────────
bool allOn       = false;
bool mallOn      = false;
bool podiumOn    = false;
bool patternOn   = false;

// ── Status LED ───────────────────────────────────────────
#define STATUS_LED    21


// ═════════════════════════════════════════════════════════
//  RELAY HELPERS
// ═════════════════════════════════════════════════════════
void relayOn(uint8_t pin)  { digitalWrite(pin, LOW);  }
void relayOff(uint8_t pin) { digitalWrite(pin, HIGH); }

void mallRelaysOn()  { relayOn(RELAY_MALL);  relayOn(RELAY_SURROUND); }
void mallRelaysOff() { relayOff(RELAY_MALL); relayOff(RELAY_SURROUND); }

void podiumRelaysOn() {
    for (uint8_t i = 0; i < NUM_PODIUM_RELAYS; i++) relayOn(PODIUM_RELAY_PINS[i]);
}
void podiumRelaysOff() {
    for (uint8_t i = 0; i < NUM_PODIUM_RELAYS; i++) relayOff(PODIUM_RELAY_PINS[i]);
}

void allRelaysOn() {
    for (uint8_t i = 0; i < NUM_ALL_RELAYS; i++) relayOn(ALL_RELAY_PINS[i]);
}
void allRelaysOff() {
    for (uint8_t i = 0; i < NUM_ALL_RELAYS; i++) relayOff(ALL_RELAY_PINS[i]);
}


// ═════════════════════════════════════════════════════════
//  EVERYTHING OFF (master kill)
// ═════════════════════════════════════════════════════════
void everythingOff()
{
    allRelaysOff();
    sendCommand(CMD_ALL_OFF);
    allOn    = false;
    mallOn   = false;
    podiumOn = false;
    patternOn = false;
    Serial.println(F("  → Everything OFF"));
}


// ═════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════
void setup()
{
    // ── FIRST: Force all relays OFF before anything else ──
    for (uint8_t i = 0; i < NUM_ALL_RELAYS; i++) {
        digitalWrite(ALL_RELAY_PINS[i], HIGH);
        pinMode(ALL_RELAY_PINS[i], OUTPUT);
        digitalWrite(ALL_RELAY_PINS[i], HIGH);
    }

    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== Building LED Remote (ESP32-S3) ==="));

    // ── I2C master ───────────────────────────────────────
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    Serial.println(F("I2C master started"));

    // ── Buttons ──────────────────────────────────────────
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }

    // ── Status LED ───────────────────────────────────────
    pinMode(STATUS_LED, OUTPUT);
    blinkStatus(3, 150);

    Serial.println(F("Ready."));
    Serial.println(F("  BTN1 (GPIO47) → ALL ON/OFF"));
    Serial.println(F("  BTN2 (GPIO48) → MALL"));
    Serial.println(F("  BTN3 (GPIO1)  → PODIUM"));
    Serial.println(F("  BTN4 (GPIO2)  → PATTERN"));
    Serial.println(F("Relays: Mall=10, Surround=9, Podium=8,7,6"));
}


// ═════════════════════════════════════════════════════════
//  MAIN LOOP
// ═════════════════════════════════════════════════════════
void loop()
{
    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        bool reading = digitalRead(buttons[i].pin);

        if (reading == LOW && buttons[i].lastState == HIGH
            && (now - buttons[i].lastPress) > DEBOUNCE_MS)
        {
            buttons[i].lastPress = now;
            handleButton(i);
        }
        buttons[i].lastState = reading;
    }

    delay(10);
}


// ═════════════════════════════════════════════════════════
//  BUTTON HANDLER — Mode Logic
// ═════════════════════════════════════════════════════════
void handleButton(uint8_t index)
{
    switch (index) {

        // ─────────────────────────────────────────────────
        //  BTN 1 — ALL ON / OFF (toggle)
        // ─────────────────────────────────────────────────
        case 0:
            if (!allOn) {
                Serial.println(F("▸ ALL ON"));
                // Kill any running pattern first
                allRelaysOn();
                sendCommand(CMD_ALL_ON);
                allOn    = true;
                mallOn   = true;
                podiumOn = true;
                patternOn = false;
                blinkStatus(2, 80);
            } else {
                Serial.println(F("▸ ALL OFF"));
                everythingOff();
                blinkStatus(1, 300);
            }
            break;

        // ─────────────────────────────────────────────────
        //  BTN 2 — MALL (toggle)
        //  ON:  Mall + Surround relays ON, Building LEDs ON
        //       Podium OFF, Pattern OFF
        //  OFF: Mall + Surround OFF, Building LEDs OFF
        // ─────────────────────────────────────────────────
        case 1:
            if (!mallOn) {
                Serial.println(F("▸ MALL ON"));
                // Turn off conflicting modes
                if (podiumOn) {
                    podiumRelaysOff();
                    podiumOn = false;
                    Serial.println(F("  Podium → OFF"));
                }
                if (patternOn) {
                    patternOn = false;
                    Serial.println(F("  Pattern → OFF"));
                }
                allOn = false;

                // Activate Mall
                mallRelaysOn();
                sendCommand(CMD_ALL_ON);
                mallOn = true;
                blinkStatus(1, 100);
            } else {
                Serial.println(F("▸ MALL OFF"));
                mallRelaysOff();
                sendCommand(CMD_ALL_OFF);
                mallOn = false;
                allOn  = false;
                blinkStatus(1, 300);
            }
            break;

        // ─────────────────────────────────────────────────
        //  BTN 3 — PODIUM (toggle)
        //  ON:  Podium relays (3,4,5) ON
        //       Mall OFF, Surround OFF, Building LEDs OFF,
        //       Pattern OFF
        //  OFF: Podium relays OFF
        // ─────────────────────────────────────────────────
        case 2:
            if (!podiumOn) {
                Serial.println(F("▸ PODIUM ON"));
                // Turn off conflicting modes
                if (mallOn) {
                    mallRelaysOff();
                    mallOn = false;
                    Serial.println(F("  Mall → OFF"));
                }
                if (patternOn) {
                    patternOn = false;
                    Serial.println(F("  Pattern → OFF"));
                }
                allOn = false;

                // Activate Podium — LEDs OFF
                sendCommand(CMD_ALL_OFF);
                podiumRelaysOn();
                podiumOn = true;
                blinkStatus(1, 100);
            } else {
                Serial.println(F("▸ PODIUM OFF"));
                podiumRelaysOff();
                podiumOn = false;
                allOn    = false;
                blinkStatus(1, 300);
            }
            break;

        // ─────────────────────────────────────────────────
        //  BTN 4 — PATTERN (toggle)
        //  ON:  LED patterns start
        //       Mall OFF, Podium OFF
        //  OFF: Pattern OFF, LEDs OFF
        // ─────────────────────────────────────────────────
        case 3:
            if (!patternOn) {
                Serial.println(F("▸ PATTERN ON"));
                // Turn off conflicting modes
                if (mallOn) {
                    mallRelaysOff();
                    mallOn = false;
                    Serial.println(F("  Mall → OFF"));
                }
                if (podiumOn) {
                    podiumRelaysOff();
                    podiumOn = false;
                    Serial.println(F("  Podium → OFF"));
                }
                allOn = false;

                // Start patterns
                sendCommand(CMD_PATTERNS);
                patternOn = true;
                blinkStatus(3, 60);
            } else {
                Serial.println(F("▸ PATTERN OFF"));
                sendCommand(CMD_ALL_OFF);
                patternOn = false;
                allOn     = false;
                blinkStatus(1, 300);
            }
            break;
    }

    // ── Print current state ──────────────────────────────
    Serial.print(F("  State: ALL="));
    Serial.print(allOn);
    Serial.print(F(" MALL="));
    Serial.print(mallOn);
    Serial.print(F(" PODIUM="));
    Serial.print(podiumOn);
    Serial.print(F(" PATTERN="));
    Serial.println(patternOn);
}


// ═════════════════════════════════════════════════════════
//  SEND I2C COMMAND TO ARDUINO MEGA
// ═════════════════════════════════════════════════════════
void sendCommand(uint8_t cmd)
{
    Wire.beginTransmission(MEGA_I2C_ADDR);
    Wire.write(cmd);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
        Serial.print(F("  I2C → 0x"));
        Serial.println(cmd, HEX);
    } else {
        Serial.print(F("  ⚠ I2C ERR: "));
        Serial.println(error);
        delay(10);
        Wire.beginTransmission(MEGA_I2C_ADDR);
        Wire.write(cmd);
        error = Wire.endTransmission();
        Serial.println(error == 0 ? F("  Retry OK") : F("  Retry FAIL"));
    }
}


// ═════════════════════════════════════════════════════════
//  STATUS LED FEEDBACK
// ═════════════════════════════════════════════════════════
void blinkStatus(uint8_t times, uint16_t intervalMs)
{
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(intervalMs);
        digitalWrite(STATUS_LED, LOW);
        delay(intervalMs);
    }
}
