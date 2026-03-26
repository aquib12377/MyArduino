/*
 * ============================================================
 *  BUILDING LED REMOTE — Arduino Nano
 * ============================================================
 *  Ported from ESP32-S3 version
 *  4 Buttons (toggle) + 5 Relays + I2C to Arduino Mega
 * ============================================================
 *  BUTTON LOGIC (all toggles — 1st click ON, 2nd click OFF):
 *
 *  BTN 1 — ALL ON
 *    ON:  All relays ON + Building LEDs ON
 *    OFF: Everything OFF
 *
 *  BTN 2 — MALL
 *    ON:  Relay1,2(Mall) ON, Relay3(Surround) ON, Building LEDs ON
 *          -> Podium OFF, Pattern OFF
 *    OFF: Mall OFF, Surround OFF, Building LEDs OFF
 *
 *  BTN 3 — PODIUM
 *    ON:  Relay4,5(Podium) ON
 *          -> Mall OFF, Surround OFF, Building LEDs OFF, Pattern OFF
 *    OFF: Podium OFF
 *
 *  BTN 4 — PATTERN
 *    ON:  LED patterns start
 *          -> Mall OFF, Podium OFF
 *    OFF: Pattern OFF, LEDs OFF
 *
 * ============================================================
 *  RELAY MAP (5 relays, active LOW):
 *    Relay 1 — Mall       -> D2
 *    Relay 2 — Mall       -> D3
 *    Relay 3 — Surround   -> D4
 *    Relay 4 — Podium     -> D5
 *    Relay 5 — Podium     -> D6
 *
 *  BUTTONS (active LOW with internal pull-up):
 *    BTN 1 (D13) — All ON/OFF
 *    BTN 2 (D12) — Mall
 *    BTN 3 (A0)  — Podium
 *    BTN 4 (A1)  — Pattern
 *
 *  I2C:  SDA = A4, SCL = A5  (hardware fixed on Nano)
 *  Status LED: D11
 *
 *  NOTE: Status LED moved to D11 to avoid conflict with
 *        BTN_ALL on D13 (both were D13 in previous version!)
 * ============================================================
 */

#include <Wire.h>

// ── I2C (A4=SDA, A5=SCL — fixed on Nano, no config needed) ─
#define MEGA_I2C_ADDR   0x08

// ── Buttons (active LOW with internal pull-up) ──────────────
#define BTN_ALL         13
#define BTN_MALL        12
#define BTN_PODIUM      A0
#define BTN_PATTERN     A1

// ── Relays (active LOW) ─────────────────────────────────────
#define RELAY_MALL_1     2   // Relay 1 — Mall
#define RELAY_MALL_2     3   // Relay 2 — Mall
#define RELAY_SURROUND   4   // Relay 3 — Surround
#define RELAY_PODIUM_1   5   // Relay 4 — Podium
#define RELAY_PODIUM_2   6   // Relay 5 — Podium

const uint8_t ALL_RELAY_PINS[]    = { RELAY_MALL_1, RELAY_MALL_2, RELAY_SURROUND, RELAY_PODIUM_1, RELAY_PODIUM_2 };
const uint8_t MALL_RELAY_PINS[]   = { RELAY_MALL_1, RELAY_MALL_2 };
const uint8_t PODIUM_RELAY_PINS[] = { RELAY_PODIUM_1, RELAY_PODIUM_2 };
#define NUM_ALL_RELAYS     5
#define NUM_MALL_RELAYS    2
#define NUM_PODIUM_RELAYS  2

// ── I2C Commands to Mega ────────────────────────────────────
#define CMD_PATTERNS    0x01
#define CMD_ALL_ON      0x02
#define CMD_ALL_OFF     0x03

// ── Button debounce ─────────────────────────────────────────
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

// ── Zone states (toggle tracking) ───────────────────────────
bool allOn       = false;
bool mallOn      = false;
bool podiumOn    = false;
bool patternOn   = false;

// ── Status LED (D11 — NOT D13, that's BTN_ALL!) ────────────
#define STATUS_LED    11


// ═════════════════════════════════════════════════════════════
//  RELAY HELPERS
// ═════════════════════════════════════════════════════════════
void relayOn(uint8_t pin)  { digitalWrite(pin, LOW);  }
void relayOff(uint8_t pin) { digitalWrite(pin, HIGH); }

// Mall = 2 relays
void mallRelaysOn() {
    for (uint8_t i = 0; i < NUM_MALL_RELAYS; i++) relayOn(MALL_RELAY_PINS[i]);
}
void mallRelaysOff() {
    for (uint8_t i = 0; i < NUM_MALL_RELAYS; i++) relayOff(MALL_RELAY_PINS[i]);
}

// Surround = 1 relay
void surroundRelayOn()  { relayOn(RELAY_SURROUND);  }
void surroundRelayOff() { relayOff(RELAY_SURROUND); }

// Mall + Surround together (BTN2 controls both)
void mallSurroundOn()  { mallRelaysOn();  surroundRelayOn();  }
void mallSurroundOff() { mallRelaysOff(); surroundRelayOff(); }

// Podium = 2 relays
void podiumRelaysOn() {
    for (uint8_t i = 0; i < NUM_PODIUM_RELAYS; i++) relayOn(PODIUM_RELAY_PINS[i]);
}
void podiumRelaysOff() {
    for (uint8_t i = 0; i < NUM_PODIUM_RELAYS; i++) relayOff(PODIUM_RELAY_PINS[i]);
}

// All 5 relays
void allRelaysOn() {
    for (uint8_t i = 0; i < NUM_ALL_RELAYS; i++) relayOn(ALL_RELAY_PINS[i]);
}
void allRelaysOff() {
    for (uint8_t i = 0; i < NUM_ALL_RELAYS; i++) relayOff(ALL_RELAY_PINS[i]);
}


// ═════════════════════════════════════════════════════════════
//  EVERYTHING OFF (master kill)
// ═════════════════════════════════════════════════════════════
void everythingOff()
{
    allRelaysOff();
    sendCommand(CMD_ALL_OFF);
    allOn     = false;
    mallOn    = false;
    podiumOn  = false;
    patternOn = false;
    Serial.println(F("  -> Everything OFF"));
}


// ═════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════
void setup()
{
    // ── FIRST: Force all relays OFF before anything else ─────
    for (uint8_t i = 0; i < NUM_ALL_RELAYS; i++) {
        digitalWrite(ALL_RELAY_PINS[i], HIGH);
        pinMode(ALL_RELAY_PINS[i], OUTPUT);
        digitalWrite(ALL_RELAY_PINS[i], HIGH);
    }

    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== Building LED Remote (Nano) ==="));

    // ── I2C master (A4=SDA, A5=SCL — hardware default) ─────
    Wire.begin();
    Serial.println(F("I2C master started (A4/A5)"));

    // ── Buttons ─────────────────────────────────────────────
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }

    // ── Status LED ──────────────────────────────────────────
    pinMode(STATUS_LED, OUTPUT);
    blinkStatus(3, 150);

    Serial.println(F("Ready."));
    Serial.println(F("  BTN1 (D13) -> ALL ON/OFF"));
    Serial.println(F("  BTN2 (D12) -> MALL"));
    Serial.println(F("  BTN3 (A0)  -> PODIUM"));
    Serial.println(F("  BTN4 (A1)  -> PATTERN"));
    Serial.println(F("Relays: Mall=D2,D3  Surround=D4  Podium=D5,D6"));
    Serial.println(F("Status LED: D11"));
    Serial.println(F("I2C: SDA=A4, SCL=A5"));
}


// ═════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═════════════════════════════════════════════════════════════
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


// ═════════════════════════════════════════════════════════════
//  BUTTON HANDLER — Mode Logic
// ═════════════════════════════════════════════════════════════
void handleButton(uint8_t index)
{
    switch (index) {

        // ─────────────────────────────────────────────────────
        //  BTN 1 — ALL ON / OFF (toggle)
        // ─────────────────────────────────────────────────────
        case 0:
            if (!allOn) {
                Serial.println(F("> ALL ON"));
                allRelaysOn();
                sendCommand(CMD_ALL_ON);
                allOn     = true;
                mallOn    = true;
                podiumOn  = true;
                patternOn = false;
                blinkStatus(2, 80);
            } else {
                Serial.println(F("> ALL OFF"));
                everythingOff();
                blinkStatus(1, 300);
            }
            break;

        // ─────────────────────────────────────────────────────
        //  BTN 2 — MALL (toggle)
        //  ON:  Mall(2) + Surround(1) ON, Building LEDs ON
        //       Podium OFF, Pattern OFF
        //  OFF: Mall + Surround OFF, Building LEDs OFF
        // ─────────────────────────────────────────────────────
        case 1:
            if (!mallOn) {
                Serial.println(F("> MALL ON"));
                if (podiumOn) {
                    podiumRelaysOff();
                    podiumOn = false;
                    Serial.println(F("  Podium -> OFF"));
                }
                if (patternOn) {
                    patternOn = false;
                    Serial.println(F("  Pattern -> OFF"));
                }
                allOn = false;

                mallSurroundOn();
                sendCommand(CMD_ALL_ON);
                mallOn = true;
                blinkStatus(1, 100);
            } else {
                Serial.println(F("> MALL OFF"));
                mallSurroundOff();
                sendCommand(CMD_ALL_OFF);
                mallOn = false;
                allOn  = false;
                blinkStatus(1, 300);
            }
            break;

        // ─────────────────────────────────────────────────────
        //  BTN 3 — PODIUM (toggle)
        //  ON:  Podium(2) ON
        //       Mall OFF, Surround OFF, LEDs OFF, Pattern OFF
        //  OFF: Podium OFF
        // ─────────────────────────────────────────────────────
        case 2:
            if (!podiumOn) {
                Serial.println(F("> PODIUM ON"));
                if (mallOn) {
                    mallSurroundOff();
                    mallOn = false;
                    Serial.println(F("  Mall+Surround -> OFF"));
                }
                if (patternOn) {
                    patternOn = false;
                    Serial.println(F("  Pattern -> OFF"));
                }
                allOn = false;

                sendCommand(CMD_ALL_OFF);
                podiumRelaysOn();
                podiumOn = true;
                blinkStatus(1, 100);
            } else {
                Serial.println(F("> PODIUM OFF"));
                podiumRelaysOff();
                podiumOn = false;
                allOn    = false;
                blinkStatus(1, 300);
            }
            break;

        // ─────────────────────────────────────────────────────
        //  BTN 4 — PATTERN (toggle)
        //  ON:  LED patterns start
        //       Mall OFF, Podium OFF
        //  OFF: Pattern OFF, LEDs OFF
        // ─────────────────────────────────────────────────────
        case 3:
            if (!patternOn) {
                Serial.println(F("> PATTERN ON"));
                if (mallOn) {
                    mallSurroundOff();
                    mallOn = false;
                    Serial.println(F("  Mall+Surround -> OFF"));
                }
                if (podiumOn) {
                    podiumRelaysOff();
                    podiumOn = false;
                    Serial.println(F("  Podium -> OFF"));
                }
                allOn = false;

                sendCommand(CMD_PATTERNS);
                patternOn = true;
                blinkStatus(3, 60);
            } else {
                Serial.println(F("> PATTERN OFF"));
                sendCommand(CMD_ALL_OFF);
                patternOn = false;
                allOn     = false;
                blinkStatus(1, 300);
            }
            break;
    }

    // ── Print current state ─────────────────────────────────
    Serial.print(F("  State: ALL="));
    Serial.print(allOn);
    Serial.print(F(" MALL="));
    Serial.print(mallOn);
    Serial.print(F(" PODIUM="));
    Serial.print(podiumOn);
    Serial.print(F(" PATTERN="));
    Serial.println(patternOn);
}


// ═════════════════════════════════════════════════════════════
//  SEND I2C COMMAND TO ARDUINO MEGA
// ═════════════════════════════════════════════════════════════
void sendCommand(uint8_t cmd)
{
    Wire.beginTransmission(MEGA_I2C_ADDR);
    Wire.write(cmd);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
        Serial.print(F("  I2C -> 0x"));
        Serial.println(cmd, HEX);
    } else {
        Serial.print(F("  ! I2C ERR: "));
        Serial.println(error);
        delay(10);
        Wire.beginTransmission(MEGA_I2C_ADDR);
        Wire.write(cmd);
        error = Wire.endTransmission();
        Serial.println(error == 0 ? F("  Retry OK") : F("  Retry FAIL"));
    }
}


// ═════════════════════════════════════════════════════════════
//  STATUS LED FEEDBACK
// ═════════════════════════════════════════════════════════════
void blinkStatus(uint8_t times, uint16_t intervalMs)
{
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(intervalMs);
        digitalWrite(STATUS_LED, LOW);
        delay(intervalMs);
    }
}
