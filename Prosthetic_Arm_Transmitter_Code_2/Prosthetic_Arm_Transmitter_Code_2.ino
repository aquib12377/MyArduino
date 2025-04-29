#include <SoftwareSerial.h>
#include <Wire.h>
// ─── Choose interface for HC‑12 ────────────────────────────────────────────
#define USE_SOFTWARE_SERIAL  true     // false = use Serial1 (Mega, Leonardo…)

// Pin mapping for SoftwareSerial (UNO/Nano, etc.)
#if USE_SOFTWARE_SERIAL
const byte HC12_RX_PIN = 2;   // HC‑12 → Arduino
const byte HC12_TX_PIN = 3;   // Arduino → HC‑12
SoftwareSerial hc12(HC12_RX_PIN, HC12_TX_PIN);
#else
#define hc12 Serial1          // hardware UART
#endif
// ───────────────────────────────────────────────────────────────────────────

// --- User‑tunable threshold ----------------------------------------------
const int THRESHOLD = 200;          // minimum change required to report
// -------------------------------------------------------------------------

unsigned int prevAvg   = 0;         // last reported average
bool         firstRead = true;      // force one initial transmission

void setup() {
  Serial.begin(9600);               // USB serial for local debug

  // HC‑12 serial port
  hc12.begin(9600);                 // HC‑12 factory default baud

  // Leads‑off detection pins (if applicable)
  pinMode(10, INPUT);               // LO+
  pinMode(11, INPUT);               // LO-
}

void loop() {
  // ── Take 50 readings and average ───────────────────────
  unsigned long sum = 0;
  for (uint8_t i = 0; i < 50; ++i) {
    sum += analogRead(A0);
    delayMicroseconds(200);
  }
  unsigned int average = sum / 50;  // 0‑1023
  Serial.println(average);
  // ── Only act on significant change ─────────────────────
  if (firstRead || abs((int)average - (int)prevAvg) > THRESHOLD) {

    // 1) Local debug print
    Serial.print  ("Avg: ");
    Serial.println(average);

    // 2) Wireless transmit via HC‑12
    hc12.print  ("AVG: ");
    hc12.println(average);

    // 3) Update state
    prevAvg   = average;
    firstRead = false;
  }
}
