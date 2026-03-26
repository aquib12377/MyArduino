#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <Preferences.h>

// ── PINS ──────────────────────────────────────────────────────────
const int LIMIT_PIN  = 4;    // Limit switch
const int LED_PIN    = 2;    // Built-in LED
const int RESET_PIN  = 0;    // Hold 3s to reset WiFi (BOOT button)
// ─────────────────────────────────────────────────────────────────

const int SERVER_PORT = 3000;

Preferences prefs;
char serverIP[40] = "192.168.0.240";

bool lastSwitchState = HIGH;
bool lastResetState  = HIGH;
unsigned long resetHoldStart = 0;
unsigned long lastTriggerTime = 0;
const int DEBOUNCE_MS = 300;   // ignore bounces within 300ms

// ── LED helpers ───────────────────────────────────────────────────
void ledBlink(int times, int ms = 150) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}
void ledSolid(bool on) { digitalWrite(LED_PIN, on ? HIGH : LOW); }

// ── Send click to Node.js server ──────────────────────────────────
void sendClick(const char* reason) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi lost — reconnecting...");
    ledBlink(5, 80);
    WiFi.reconnect();
    return;
  }

  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(SERVER_PORT) + "/click";

  Serial.print("→ [");
  Serial.print(reason);
  Serial.print("] Sending click to: ");
  Serial.println(url);

  http.begin(url);
  http.setTimeout(3000);
  int code = http.GET();

  if (code == 200) {
    Serial.println("✅ Click sent!");
    ledBlink(2, 100);
  } else if (code < 0) {
    Serial.println("❌ Server unreachable — is node server.js running?");
    ledBlink(5, 80);
  } else {
    Serial.print("⚠️  HTTP code: "); Serial.println(code);
    ledBlink(3, 200);
  }

  http.end();
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LIMIT_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);
  pinMode(LED_PIN,   OUTPUT);

  ledSolid(true);

  prefs.begin("config", false);
  String saved = prefs.getString("serverIP", "");
  if (saved.length() > 0) {
    saved.toCharArray(serverIP, sizeof(serverIP));
    Serial.print("📦 Loaded server IP: ");
    Serial.println(serverIP);
  }

  WiFiManager wm;

  WiFiManagerParameter serverParam("server_ip", "Server IP (Mac/PC)", serverIP, 40);
  wm.addParameter(&serverParam);
  wm.setConfigPortalTimeout(180);

  wm.setAPCallback([](WiFiManager* wm) {
    Serial.println("\n📶 Config portal open!");
    Serial.println("   Connect to WiFi: ESP32-Bridge-Setup");
    Serial.println("   Then open:       http://192.168.4.1");
    ledSolid(false);
  });

  Serial.println("\nConnecting via WiFiManager...");

  if (!wm.autoConnect("ESP32-Bridge-Setup")) {
    Serial.println("❌ Portal timed out — restarting...");
    ledBlink(10, 80);
    ESP.restart();
  }

  String newIP = String(serverParam.getValue());
  if (newIP.length() > 0 && newIP != String(serverIP)) {
    newIP.toCharArray(serverIP, sizeof(serverIP));
    prefs.putString("serverIP", newIP);
    Serial.print("💾 Saved new server IP: ");
    Serial.println(serverIP);
  }

  prefs.end();

  // Read initial state of switch so we don't fire on boot
  lastSwitchState = digitalRead(LIMIT_PIN);

  Serial.println("\n✅ WiFi connected!");
  Serial.print("   ESP32 IP:  "); Serial.println(WiFi.localIP());
  Serial.print("   Server IP: "); Serial.println(serverIP);
  Serial.println("\nReady — lift or press telephone to trigger click.");
  Serial.println("Hold BOOT 3s to reset WiFi.\n");

  ledBlink(3, 200);
  ledSolid(false);
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {

  // ── Limit switch — trigger on BOTH edges ──────────────────────
  bool switchState = digitalRead(LIMIT_PIN);
  unsigned long now = millis();

  if (switchState != lastSwitchState && (now - lastTriggerTime) > DEBOUNCE_MS) {

    lastTriggerTime = now;

    if (switchState == HIGH) {
      // Phone LIFTED — switch released
      Serial.println("📞 Phone lifted → START call");
      sendClick("LIFTED");
    } else {
      // Phone PRESSED DOWN — switch triggered
      Serial.println("📵 Phone down  → END call");
      sendClick("PRESSED");
    }
  }

  lastSwitchState = switchState;

  // ── BOOT button — hold 3s to reset ────────────────────────────
  bool resetState = digitalRead(RESET_PIN);
  if (resetState == LOW) {
    if (lastResetState == HIGH) {
      resetHoldStart = millis();
      Serial.println("⏳ Hold 3s to reset WiFi...");
    }
    if (millis() - resetHoldStart >= 3000) {
      Serial.println("🗑️  Resetting...");
      ledBlink(5, 100);
      prefs.begin("config", false);
      prefs.clear();
      prefs.end();
      WiFiManager wm;
      wm.resetSettings();
      delay(500);
      ESP.restart();
    }
  }
  lastResetState = resetState;

  delay(10);
}