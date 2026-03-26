/*
 * ===============================================================================
 * ARDUINO NANO — GAS + LDR SENSOR ALERT SYSTEM
 * ===============================================================================
 *
 * DESCRIPTION:
 *   Monitors a digital gas sensor and digital LDR. When HIGH GAS or DARKNESS
 *   is detected, the buzzer and LED turn ON and a HIGH signal is sent to the
 *   ESP32-CAM to trigger a photo capture + Supabase upload.
 *
 * PIN CONFIGURATION:
 * ──────────────────
 *   INPUT:
 *     Gas Sensor (Digital OUT) → D2
 *     LDR Sensor  (Digital OUT) → D3
 *       (Both sensors output HIGH = alert condition)
 *
 *   OUTPUT:
 *     Buzzer                   → D5
 *     LED                      → D6
 *     Signal to ESP32-CAM      → D7
 *       ⚠️  IMPORTANT: Use a voltage divider on D7 → ESP32-CAM GPIO 16
 *           Nano D7 (5V) → 10kΩ → ESP32 GPIO16 → 20kΩ → GND
 *           This steps 5V down to ~3.3V safe for ESP32
 *
 * SENSOR LOGIC:
 * ─────────────
 *   Gas Sensor  : HIGH = Gas detected  (MQ-2 / MQ-135 digital pin)
 *   LDR Sensor  : HIGH = Darkness      (LDR module with comparator)
 *
 * ALERT BEHAVIOUR:
 * ────────────────
 *   Alert ON  → Buzzer ON + LED ON + Signal pin HIGH (held for SIGNAL_PULSE_MS)
 *   Alert OFF → Buzzer OFF + LED OFF + Signal pin LOW
 *   Cooldown  → ALERT_COOLDOWN_MS between repeated alerts
 *
 * ===============================================================================
 */

// ─── Pin Definitions ─────────────────────────────────────────────────────────
#define PIN_GAS_SENSOR    3    // Digital OUT from gas sensor module
#define PIN_LDR_SENSOR    2    // Digital OUT from LDR module
#define PIN_BUZZER        4    // Buzzer
#define PIN_LED           6    // Alert LED
#define PIN_ESP32_SIGNAL  7    // Signal wire to ESP32-CAM (via voltage divider!)

// ─── Timing Settings ─────────────────────────────────────────────────────────
#define SIGNAL_PULSE_MS    2000    // How long to hold signal HIGH (2 seconds)
#define ALERT_COOLDOWN_MS  10000   // Min time between alerts (10 seconds)
#define BUZZER_BEEP_MS     300     // Buzzer beep duration during alert

// ─── Global State ─────────────────────────────────────────────────────────────
static bool          alertActive      = false;
static unsigned long alertStartMs     = 0;
static unsigned long lastAlertMs      = 0;
static unsigned long lastBeepMs       = 0;
static bool          buzzerState      = false;

// ═══════════════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);
  Serial.println("=== Nano Sensor Alert System ===");

  // Inputs
  pinMode(PIN_GAS_SENSOR,   INPUT_PULLUP);
  pinMode(PIN_LDR_SENSOR,   INPUT_PULLUP);

  // Outputs
  pinMode(PIN_BUZZER,       OUTPUT);
  pinMode(PIN_LED,          OUTPUT);
  pinMode(PIN_ESP32_SIGNAL, OUTPUT);

  // All OFF at start
  digitalWrite(PIN_BUZZER,       LOW);
  digitalWrite(PIN_LED,          LOW);
  digitalWrite(PIN_ESP32_SIGNAL, LOW);

  Serial.println("[READY] Monitoring Gas + LDR sensors...");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  TRIGGER ALERT
// ═══════════════════════════════════════════════════════════════════════════════
void triggerAlert(const char* reason) {
  unsigned long now = millis();

  // Respect cooldown
  if (now - lastAlertMs < ALERT_COOLDOWN_MS) {
    Serial.print("[COOLDOWN] Alert suppressed\n");
    return;
  }

  Serial.println("==========================================");
  Serial.print("[ALERT] Triggered by: "+ String(reason)+"\n");
  Serial.println("[ALERT] Buzzer ON | LED ON | Signal -> ESP32-CAM");
  Serial.println("==========================================");

  alertActive   = true;
  alertStartMs  = now;
  lastAlertMs   = now;

  // LED ON immediately
  digitalWrite(PIN_LED,          HIGH);
  // Signal to ESP32-CAM HIGH
  digitalWrite(PIN_ESP32_SIGNAL, HIGH);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  STOP ALERT
// ═══════════════════════════════════════════════════════════════════════════════
void stopAlert() {
  alertActive = false;
  digitalWrite(PIN_BUZZER,       LOW);
  digitalWrite(PIN_LED,          LOW);
  digitalWrite(PIN_ESP32_SIGNAL, LOW);
  buzzerState = false;
  Serial.println("[ALERT] Cleared — all outputs OFF");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // ── Read sensors ────────────────────────────────────────────────────────────
  bool gasDetected      = (digitalRead(PIN_GAS_SENSOR) == LOW);
  bool darknessDetected = (digitalRead(PIN_LDR_SENSOR) == LOW);

  // ── Check for new alert condition ───────────────────────────────────────────
  if (gasDetected && !alertActive) {
    Serial.println("[SENSOR] Gas detected!");
    triggerAlert("GAS SENSOR");
  }
  else if (darknessDetected && !alertActive) {
    Serial.println("[SENSOR] Darkness detected!");
    triggerAlert("LDR DARKNESS");
  }
  else if (gasDetected && darknessDetected && !alertActive) {
    Serial.println("[SENSOR] Gas + Darkness detected!");
    triggerAlert("GAS + DARKNESS");
  }

  // ── Handle active alert ─────────────────────────────────────────────────────
  if (alertActive) {
    // Beep buzzer ON/OFF pattern
    if (now - lastBeepMs >= BUZZER_BEEP_MS) {
      buzzerState = !buzzerState;
      digitalWrite(PIN_BUZZER, buzzerState ? HIGH : LOW);
      lastBeepMs = now;
    }

    // Drop signal pin after pulse duration
    // (ESP32-CAM only needs a brief HIGH to detect the trigger)
    if (now - alertStartMs >= SIGNAL_PULSE_MS) {
      digitalWrite(PIN_ESP32_SIGNAL, LOW);
    }

    // Keep alert active as long as sensor still triggered
    // Stop alert only when both sensors clear
    if (!gasDetected && !darknessDetected) {
      stopAlert();
    }
  }

  // ── Serial status every 5 seconds ───────────────────────────────────────────
  static unsigned long lastStatusMs = 0;
  if (now - lastStatusMs >= 5000) {
    lastStatusMs = now;
    Serial.print("[STATUS] Gas: %s | LDR: %s | Alert: %s\n");
  }

  delay(50);  // 50ms poll rate
}
