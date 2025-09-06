#include <Arduino.h>
#include <Wire.h>

#ifndef SDA
#define SDA 21
#endif
#ifndef SCL
#define SCL 22
#endif

// --- Helpers ---
bool muxWrite(uint8_t muxAddr, uint8_t mask) {
  Wire.beginTransmission(muxAddr);
  Wire.write(mask);
  uint8_t err = Wire.endTransmission();
  return (err == 0);
}

void disableAllOn(uint8_t muxAddr) { (void)muxWrite(muxAddr, 0x00); }

void scanDownstream(uint8_t muxAddr, uint8_t channel) {
  // Enable exactly this channel
  if (!muxWrite(muxAddr, (1 << channel))) {
    Serial.printf("  CH%u: SELECT FAIL (mux 0x%02X)\n", channel, muxAddr);
    return;
  }

  delay(2);
  int found = 0;
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("    - device @0x%02X", addr);
      // If likely an MPU, try WHO_AM_I for extra confirmation
      if (addr == 0x68 || addr == 0x69) {
        Wire.beginTransmission(addr);
        Wire.write(0x75); // WHO_AM_I
        if (Wire.endTransmission(false) == 0) {
          if (Wire.requestFrom((int)addr, 1) == 1) {
            uint8_t who = Wire.read();
            Serial.printf("  (WHO_AM_I=0x%02X)", who); // MPU6050 usually 0x68
          }
        }
      }
      Serial.println();
      found++;
    }
  }

  if (!found) Serial.println("    (no devices)");
  // Turn channel back off to keep buses isolated
  disableAllOn(muxAddr);
  delay(2);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(SDA, SCL);
  Wire.setClock(100000); // slow & safe for troubleshooting; raise to 400k later

  Serial.println("\n=== Upstream scan (0x03..0x77) ===");
  int upstream = 0;
  for (uint8_t a = 0x03; a <= 0x77; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  - found 0x%02X\n", a);
      upstream++;
    }
  }
  if (!upstream) Serial.println("  (none)");

  // Discover muxes (0x70..0x77)
  uint8_t muxes[8];
  int nmux = 0;
  Serial.println("\n=== Looking for TCA9548A @ 0x70..0x77 ===");
  for (uint8_t a = 0x70; a <= 0x77; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  - mux present @0x%02X\n", a);
      muxes[nmux++] = a;
      disableAllOn(a);
    }
  }
  if (nmux == 0) {
    Serial.println("No muxes found. Check A0/A1/A2 wiring, /RESET high, /OE low, pull-ups.");
    return;
  }

  // Per-mux, per-channel scan
  for (int i = 0; i < nmux; i++) {
    uint8_t mux = muxes[i];
    Serial.printf("\n=== Scanning mux 0x%02X channels ===\n", mux);

    // Sanity: ensure mux ACKs
    Wire.beginTransmission(mux);
    if (Wire.endTransmission() != 0) {
      Serial.printf("  mux 0x%02X not responding now. Check wiring.\n", mux);
      continue;
    }

    for (uint8_t ch = 0; ch < 8; ch++) {
      Serial.printf("  CH%u:\n", ch);
      scanDownstream(mux, ch);
    }
  }

  Serial.println("\n=== Done ===");
}

void loop() {
  // Nothing; rerun by pressing reset if needed.
}
