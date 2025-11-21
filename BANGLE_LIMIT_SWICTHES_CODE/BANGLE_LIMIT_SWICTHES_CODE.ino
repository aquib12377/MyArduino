/*
  UNO-SLAVE: 14 limit switches → I2C 16-bit bitmap (addr 0x42)
  Added Serial printing for live status debugging.
  Wiring: each switch → pin and GND, using INPUT_PULLUP (active LOW).
  Sends little-endian {lo, hi} on I2C request.
*/

#include <Wire.h>

// -------- Pin map --------
const uint8_t LS_PINS[14] = {
  2, 3, 4, 5,     // 0..3
  6, 7, 8, 9,     // 4..7
  10, 11, 12, 13, // 8..11
  A0, A1          // 12..13
};

// -------- Bit layout --------
enum LsIndex : uint8_t {
  H1_OFF = 0, H1_READY,
  H2_OFF,     H2_READY,
  L1_OFF,     L1_READY,
  T1_OFF,     T1_READY,
  E1_OFF,     E1_READY,
  X1_OFF,     X1_READY,
  X2_OFF,     X2_READY
};

volatile uint16_t ls_bitmap = 0;

// Simple debounce
const uint8_t  DEBOUNCE_MS = 8;
uint16_t stable_state = 0;
uint16_t last_sample  = 0;
unsigned long last_change_ms = 0;
unsigned long last_print_ms = 0;

// ---- Optional names for Serial output ----
const char* LS_NAMES[14] = {
  "H1_OFF","H1_READY","H2_OFF","H2_READY",
  "L1_OFF","L1_READY","T1_OFF","T1_READY",
  "E1_OFF","E1_READY","X1_OFF","X1_READY",
  "X2_OFF","X2_READY"
};

void sampleSwitches() {
  uint16_t raw = 0;
  for (uint8_t i = 0; i < 14; i++) {
    // active LOW → set bit =1 when pressed
    if (digitalRead(LS_PINS[i]) == LOW)
      raw |= (1u << i);
  }

  if (raw != last_sample) {
    last_sample = raw;
    last_change_ms = millis();
  } else {
    if (millis() - last_change_ms >= DEBOUNCE_MS) {
      if (raw != stable_state) {
        uint16_t changed = raw ^ stable_state;
        for (uint8_t i = 0; i < 14; i++) {
          if (changed & (1u << i)) {
            bool active = (raw >> i) & 1;
            Serial.print(F("[CHG] "));
            Serial.print(LS_NAMES[i]);
            Serial.print(F(" -> "));
            Serial.println(active ? F("ACTIVE") : F("idle"));
          }
        }
        stable_state = raw;
        ls_bitmap = stable_state; // publish
      }
    }
  }

  // Periodic summary every 1s
  if (millis() - last_print_ms >= 1000) {
    last_print_ms = millis();
    Serial.print(F("[BITS] 0x"));
    Serial.println(ls_bitmap, HEX);
  }
}

void onI2CRequest() {
  // Send little-endian 16-bit
  Wire.write((uint8_t)(ls_bitmap & 0xFF));
  Wire.write((uint8_t)((ls_bitmap >> 8) & 0xFF));
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("UNO-SLAVE Limit Switch Monitor (I2C addr 0x42)"));
  Serial.println(F("Active = switch closed to GND (LOW)."));

  for (uint8_t i = 0; i < 14; i++) {
    pinMode(LS_PINS[i], INPUT_PULLUP);
  }

  Wire.begin(0x42);
  Wire.setClock(100000);
  Wire.onRequest(onI2CRequest);
}

void loop() {
  sampleSwitches();
  delayMicroseconds(1000); // ~1 kHz polling
}
