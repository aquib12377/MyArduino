/*
 * ============================================================
 *  HOME AUTOMATION – 4 Active-Low Relay Control via Blynk
 *  Board  : ESP8266 (NodeMCU / Wemos D1 Mini / etc.)
 *  Relays : Active-LOW (relay ON when GPIO = LOW)
 *  Blynk  : Virtual Pins V1-V4 → Relay 1-4
 * ============================================================
 *
 *  Wiring:
 *    Relay 1 IN  →  D1 (GPIO5)
 *    Relay 2 IN  →  D2 (GPIO4)
 *    Relay 3 IN  →  D3 (GPIO0)
 *    Relay 4 IN  →  D4 (GPIO2)
 *
 *  Libraries needed (install via Arduino Library Manager):
 *    - Blynk  (by Volodymyr Shymanskyy) v1.x or later
 *    - ESP8266WiFi (bundled with ESP8266 board package)
 *
 *  Blynk App Setup:
 *    Add a "Button" widget for each relay, set to SWITCH mode:
 *      Relay 1 → Virtual Pin V1
 *      Relay 2 → Virtual Pin V2
 *      Relay 3 → Virtual Pin V3
 *      Relay 4 → Virtual Pin V4
 * ============================================================
 */

// ---------- Blynk credentials ----------
#define BLYNK_TEMPLATE_ID   "TMPL30uW7_TuP"
#define BLYNK_TEMPLATE_NAME "HOME AUTOMATION "
#define BLYNK_AUTH_TOKEN    "fWMHcWW_jEmNXFrWeXi4VLbH5k7csWXk"

// Optional: show connection status in Serial Monitor
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// ---------- Wi-Fi credentials ----------
const char* WIFI_SSID     = "AAA";      // ← Replace
const char* WIFI_PASSWORD = "Acube@123";  // ← Replace

// ---------- Relay GPIO pins ----------
#define RELAY1_PIN  5   // GPIO5
#define RELAY2_PIN  4   // GPIO4
#define RELAY3_PIN  0   // GPIO0
#define RELAY4_PIN  2   // GPIO2

// ---------- Active-LOW helpers ----------
// Relay is energised (ON) when its IN pin is LOW
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ============================================================
//  Blynk Virtual Pin Handlers
//  Button widget in SWITCH mode sends 1 (ON) or 0 (OFF)
// ============================================================

BLYNK_WRITE(V1) {
  int value = param.asInt();
  digitalWrite(RELAY1_PIN, value ? RELAY_ON : RELAY_OFF);
  Serial.printf("Relay 1 → %s\n", value ? "ON" : "OFF");
}

BLYNK_WRITE(V2) {
  int value = param.asInt();
  digitalWrite(RELAY2_PIN, value ? RELAY_ON : RELAY_OFF);
  Serial.printf("Relay 2 → %s\n", value ? "ON" : "OFF");
}

BLYNK_WRITE(V3) {
  int value = param.asInt();
  digitalWrite(RELAY3_PIN, value ? RELAY_ON : RELAY_OFF);
  Serial.printf("Relay 3 → %s\n", value ? "ON" : "OFF");
}

BLYNK_WRITE(V0) {
  int value = param.asInt();
  digitalWrite(RELAY4_PIN, value ? RELAY_ON : RELAY_OFF);
  Serial.printf("Relay 4 → %s\n", value ? "ON" : "OFF");
}

// ============================================================
//  Sync relay states from Blynk server on reconnect
//  so physical relays match the app buttons after reboot
// ============================================================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V1, V2, V3, V4);
  Serial.println("Blynk connected – syncing relay states…");
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Home Automation Boot ===");

  // Initialise relay pins as OUTPUT and default to OFF
  // (HIGH = relay coil de-energised for active-LOW modules)
  pinMode(RELAY1_PIN, OUTPUT); digitalWrite(RELAY1_PIN, RELAY_OFF);
  pinMode(RELAY2_PIN, OUTPUT); digitalWrite(RELAY2_PIN, RELAY_OFF);
  pinMode(RELAY3_PIN, OUTPUT); digitalWrite(RELAY3_PIN, RELAY_OFF);
  pinMode(RELAY4_PIN, OUTPUT); digitalWrite(RELAY4_PIN, RELAY_OFF);

  Serial.println("All relays initialised to OFF");

  // Connect to Blynk (also handles Wi-Fi connection internally)
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================
//  Loop
// ============================================================
void loop() {
  Blynk.run();   // Maintain Blynk connection & process events
}
