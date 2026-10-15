/* =========================================================================
 *  Flame Sensor Test  (ESP32 + 3x KY-026 / digital flame modules)
 *  ---------------------------------------------------------------
 *  Purpose:
 *    Read 3 flame sensors and print their RAW digital value + a human label.
 *    Use this to:
 *      1. Confirm wiring (which physical sensor is L / M / R).
 *      2. Confirm polarity (does DO read LOW or HIGH when flame is near?).
 *      3. See detection range and how stable the signal is.
 *
 *  Wiring:
 *    Sensor VCC -> 3V3 (or 5V if your module requires it)
 *    Sensor GND -> GND
 *    Sensor DO  -> GPIO (see pin config below)
 *    (Ignore AO — this sketch only uses the digital DO pin.)
 *
 *  Pins default to the same ones as the main firefighting bot firmware.
 *  Change them if you wired differently.
 *
 *  How to use:
 *    - Upload this sketch
 *    - Open Serial Monitor at 115200 baud
 *    - Wave a lighter flame ~10-30 cm in front of each sensor one at a time
 *    - Watch which column changes and whether it goes 0 or 1 on flame
 *    - Set #define EXPECTED_ACTIVE_LOW below to match what you observed,
 *      then the "STATE" column will read FLAME / ----
 * ========================================================================= */

// ---- Pin config (match your wiring) ----
static const int PIN_FLAME_L = 34;   // input-only pin OK
static const int PIN_FLAME_M = 35;   // input-only pin OK
static const int PIN_FLAME_R = 32;

// ---- Expected polarity (best guess; adjust after you see raw output) ----
// 1 = DO goes LOW when flame detected (most KY-026 modules)
// 0 = DO goes HIGH when flame detected
#define EXPECTED_ACTIVE_LOW  1

// ---- How often to print ----
static const unsigned long PRINT_INTERVAL_MS = 200;

// ---- Internal ----
unsigned long g_lastPrint = 0;

// Change-detection (so we can print a loud banner when a flame enters/leaves)
int g_lastRawL = -1, g_lastRawM = -1, g_lastRawR = -1;

static const char* stateLabel(int raw) {
#if EXPECTED_ACTIVE_LOW
  return (raw == LOW) ? "FLAME" : "----";
#else
  return (raw == HIGH) ? "FLAME" : "----";
#endif
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("==============================================");
  Serial.println("  Flame Sensor Test  (ESP32, 3 sensors)");
  Serial.println("==============================================");
  Serial.printf("  Pins:  L=%d   M=%d   R=%d\n",
                PIN_FLAME_L, PIN_FLAME_M, PIN_FLAME_R);
#if EXPECTED_ACTIVE_LOW
  Serial.println("  Polarity assumption: ACTIVE-LOW  (LOW = flame)");
#else
  Serial.println("  Polarity assumption: ACTIVE-HIGH (HIGH = flame)");
#endif
  Serial.println("----------------------------------------------");
  Serial.println("Tip: if 'STATE' is backwards (shows FLAME when");
  Serial.println("no flame), flip EXPECTED_ACTIVE_LOW and re-upload.");
  Serial.println();

  pinMode(PIN_FLAME_L, INPUT);
  pinMode(PIN_FLAME_M, INPUT);
  pinMode(PIN_FLAME_R, INPUT);

  // Header
  Serial.println("time(ms)    L(raw)  M(raw)  R(raw)    L         M         R");
}

void loop() {
  const int rawL = digitalRead(PIN_FLAME_L);
  const int rawM = digitalRead(PIN_FLAME_M);
  const int rawR = digitalRead(PIN_FLAME_R);

  // --- Change events: print a loud marker as soon as any sensor toggles ---
  if (rawL != g_lastRawL) {
    Serial.printf(">>> LEFT   changed: raw=%d  -> %s\n", rawL, stateLabel(rawL));
    g_lastRawL = rawL;
  }
  if (rawM != g_lastRawM) {
    Serial.printf(">>> MIDDLE changed: raw=%d  -> %s\n", rawM, stateLabel(rawM));
    g_lastRawM = rawM;
  }
  if (rawR != g_lastRawR) {
    Serial.printf(">>> RIGHT  changed: raw=%d  -> %s\n", rawR, stateLabel(rawR));
    g_lastRawR = rawR;
  }

  // --- Periodic table row, so you can see stable levels too ---
  const unsigned long now = millis();
  if (now - g_lastPrint >= PRINT_INTERVAL_MS) {
    g_lastPrint = now;
    Serial.printf("%8lu      %d       %d       %d       %-8s  %-8s  %-8s\n",
                  now,
                  rawL, rawM, rawR,
                  stateLabel(rawL),
                  stateLabel(rawM),
                  stateLabel(rawR));
  }
}
