/*******************************************************
 * ESP32 Smart Home Demo
 * Components:
 *  - ESP32
 *  - SG90 Servo (door lock)
 *  - Active LOW relay (alarm / exhaust)
 *  - MQ6 Gas sensor (analog)
 *  - 4x3 Keypad
 *  - 16x2 I2C LCD
 *  - Web UI (WebSocket, real-time dashboard)
 *
 * Features:
 *  - Shows gas value on web.
 *  - If gas crosses threshold => servo unlock (slow), relay ON.
 *  - House lock state (Locked / Open) on web.
 *  - Auto-generate 4-digit numeric password every 30s.
 *  - Enter password on keypad => if correct, show Authorized + Open.
 *  - Password is now shown on the web (not LCD).
 *******************************************************/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Keypad.h>

// ------------------------- USER CONFIG -------------------------
// WiFi credentials
const char* ssid     = "AAA";
const char* password = "Acube123";

// MQ6 threshold (tune according to your analog readings)
const int GAS_THRESHOLD = 2000;   // raw ADC value (0-4095)

// Password & timing
const unsigned long PASSWORD_INTERVAL_MS      = 30000; // 30 seconds
const unsigned long GAS_READ_INTERVAL_MS      = 1000;  // 1 second
const unsigned long WS_BROADCAST_INTERVAL_MS  = 500;   // 0.5 second
const unsigned long AUTH_STATUS_SHOW_MS       = 3000;  // 3 seconds

// Servo angles
const int SERVO_LOCK_ANGLE   = 0;
const int SERVO_UNLOCK_ANGLE = 90;

// ------------------------- PIN DEFINITIONS -------------------------
// Adjust these pins to your wiring
const int SERVO_PIN = 18;
const int RELAY_PIN = 5;     // Active LOW relay
const int MQ6_PIN   = 34;    // MQ-6 analog output to ADC (GPIO34 is input only)

// I2C LCD (SDA=21, SCL=22 by default on ESP32)
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to 0x3F if needed

// Keypad 4x3: Rows x Cols
const byte ROWS = 4;
const byte COLS = 3;

/*
   Example keypad layout:

   [1] [2] [3]
   [4] [5] [6]
   [7] [8] [9]
   [*] [0] [#]

   We will use '#' to submit the code, '*' to clear input.
*/

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// Example pins (change as needed, avoid using 21/22 (I2C), etc.)
byte rowPins[ROWS] = {13, 12, 14, 27}; // R1,R2,R3,R4
byte colPins[COLS] = {26, 25, 33};     // C1,C2,C3

Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// ------------------------- GLOBAL OBJECTS -------------------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo doorServo;

// ------------------------- STATE VARIABLES -------------------------
int  lastGasValue       = 0;
bool gasAlarm           = false;

bool isLocked           = true;        // true = Locked, false = Open
int  currentServoAngle  = SERVO_LOCK_ANGLE;

String currentPassword  = "0000";      // 4-digit numeric password
String enteredPassword  = "";

unsigned long lastPasswordChangeMs = 0;
unsigned long lastGasReadMs        = 0;
unsigned long lastBroadcastMs      = 0;

bool          authorizedFlag = false;
unsigned long lastAuthTimeMs = 0;

// ------------------------- HTML PAGE -------------------------
// Decent UI with cards and badges, now showing PIN on web
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>ESP32 Smart Home</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    color-scheme: dark;
  }

  * {
    box-sizing: border-box;
  }

  body {
    margin: 0;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    background:
      radial-gradient(circle at top left, #1e293b 0, transparent 55%),
      radial-gradient(circle at bottom right, #020617 0, #020617 35%, #000 100%);
    color: #e5e7eb;
  }

  .shell {
    max-width: 960px;
    width: 100%;
    padding: 20px;
  }

  .card {
    background: rgba(15, 23, 42, 0.9);
    border-radius: 20px;
    padding: 18px 18px 16px;
    border: 1px solid rgba(148, 163, 184, 0.25);
    box-shadow:
      0 28px 60px rgba(15, 23, 42, 0.9),
      0 0 0 1px rgba(15, 23, 42, 0.7);
    backdrop-filter: blur(16px);
    transition: border-color 0.2s ease, box-shadow 0.2s ease;
  }

  .card.gas-danger {
    border-color: #b91c1c;
    box-shadow:
      0 28px 60px rgba(15, 23, 42, 0.9),
      0 0 0 1px rgba(248, 113, 113, 0.4),
      0 0 40px rgba(248, 113, 113, 0.35);
  }

  .card-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    margin-bottom: 12px;
  }

  .title-block {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }

  .title {
    font-size: 1.3rem;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    font-weight: 600;
  }

  .subtitle {
    font-size: 0.8rem;
    color: #9ca3af;
  }

  .chip-row {
    display: flex;
    flex-direction: column;
    gap: 6px;
    align-items: flex-end;
  }

  .chip {
    font-size: 0.75rem;
    padding: 4px 10px;
    border-radius: 999px;
    border: 1px solid #1e293b;
    background: rgba(15, 23, 42, 0.9);
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }

  .dot {
    width: 8px;
    height: 8px;
    border-radius: 999px;
    display: inline-block;
  }
  .dot-ok {
    background: #4ade80;
    box-shadow: 0 0 0 4px rgba(34, 197, 94, 0.35);
  }
  .dot-error {
    background: #f97373;
    box-shadow: 0 0 0 4px rgba(248, 113, 113, 0.35);
  }

  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1.3fr) minmax(0, 1fr);
    gap: 16px;
  }

  @media (max-width: 720px) {
    .layout {
      grid-template-columns: minmax(0, 1fr);
    }
    .chip-row {
      align-items: flex-start;
    }
  }

  /* Gas panel */
  .panel {
    border-radius: 16px;
    padding: 14px 14px 12px;
    background: radial-gradient(circle at top left, rgba(56, 189, 248, 0.12), transparent 60%),
                #020617;
    border: 1px solid rgba(31, 41, 55, 0.95);
  }

  .panel-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 10px;
    gap: 10px;
  }

  .panel-title {
    font-size: 0.85rem;
    text-transform: uppercase;
    letter-spacing: 0.12em;
    color: #9ca3af;
  }

  .panel-meta {
    font-size: 0.7rem;
    color: #6b7280;
  }

  .gas-main {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .gas-value-row {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
  }

  .gas-value {
    font-size: 1.8rem;
    font-weight: 600;
  }

  .badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 3px 8px;
    border-radius: 999px;
    font-size: 0.7rem;
    font-weight: 500;
    border: 1px solid transparent;
  }
  .badge-green {
    background: rgba(22, 163, 74, 0.2);
    color: #4ade80;
    border-color: rgba(74, 222, 128, 0.35);
  }
  .badge-red {
    background: rgba(220, 38, 38, 0.18);
    color: #fecaca;
    border-color: rgba(248, 113, 113, 0.5);
  }
  .badge-amber {
    background: rgba(217, 119, 6, 0.18);
    color: #fbbf24;
    border-color: rgba(251, 191, 36, 0.5);
  }

  .gas-bar-shell {
    margin-top: 4px;
    width: 100%;
    height: 8px;
    border-radius: 999px;
    background: rgba(15, 23, 42, 0.9);
    border: 1px solid rgba(55, 65, 81, 0.9);
    overflow: hidden;
  }

  .gas-bar-fill {
    height: 100%;
    width: 0%;
    border-radius: 999px;
    background: linear-gradient(90deg, #22c55e, #eab308, #ef4444);
    transition: width 0.25s ease-out;
  }

  .gas-footer {
    display: flex;
    justify-content: space-between;
    margin-top: 6px;
    font-size: 0.7rem;
    color: #9ca3af;
  }

  /* Right column cards */
  .stack {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }

  .mini-card {
    border-radius: 14px;
    padding: 10px 12px;
    background: #020617;
    border: 1px solid rgba(31, 41, 55, 0.9);
  }

  .mini-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 4px;
  }

  .mini-label {
    font-size: 0.75rem;
    text-transform: uppercase;
    letter-spacing: 0.14em;
    color: #9ca3af;
  }

  .mini-value {
    font-size: 1.05rem;
    font-weight: 600;
  }

  .mini-sub {
    margin-top: 4px;
    font-size: 0.7rem;
    color: #6b7280;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  .pill {
    font-size: 0.7rem;
    padding: 2px 8px;
    border-radius: 999px;
    background: rgba(15, 23, 42, 0.9);
    border: 1px solid rgba(55, 65, 81, 0.9);
    color: #9ca3af;
  }

  /* Footer */
  .footer {
    margin-top: 14px;
    font-size: 0.72rem;
    color: #6b7280;
    display: flex;
    justify-content: space-between;
    flex-wrap: wrap;
    gap: 6px;
  }

  .footer span {
    opacity: 0.9;
  }
</style>
</head>
<body>
<div class="shell">
  <div id="card" class="card">
    <!-- Header -->
    <div class="card-header">
      <div class="title-block">
        <div class="title">Smart Home Console</div>
        <div class="subtitle">ESP32 · MQ6 · Servo Lock · Keypad Access</div>
      </div>
      <div class="chip-row">
        <div id="connectionChip" class="chip">
          <span class="dot dot-error" id="ws-dot"></span>
          <span id="ws-status">Connecting…</span>
        </div>
        <div class="chip">
          <span style="font-size:0.7rem;opacity:.8;">WebSocket Channel</span>
        </div>
      </div>
    </div>

    <!-- Main layout -->
    <div class="layout">
      <!-- Gas / Environment -->
      <section class="panel">
        <div class="panel-header">
          <div>
            <div class="panel-title">Gas Safety</div>
            <div class="panel-meta">Live MQ6 readings from ESP32</div>
          </div>
          <span id="gasStatus" class="badge badge-green">Normal</span>
        </div>

        <div class="gas-main">
          <div class="gas-value-row">
            <div class="gas-value" id="gasValue">—</div>
            <div style="text-align:right;font-size:0.7rem;color:#9ca3af;">
              <div>ADC 0 – 4095</div>
              <div>Threshold in firmware</div>
            </div>
          </div>

          <div class="gas-bar-shell">
            <div id="gasBar" class="gas-bar-fill"></div>
          </div>

          <div class="gas-footer">
            <span>Higher value → higher gas concentration</span>
            <span>Alarm controls servo + relay</span>
          </div>
        </div>
      </section>

      <!-- House / Access -->
      <div class="stack">
        <!-- House lock -->
        <div class="mini-card">
          <div class="mini-header">
            <div class="mini-label">House State</div>
            <span id="lockBadge" class="badge badge-amber">Unknown</span>
          </div>
          <div class="mini-value" id="lockStatus">—</div>
          <div class="mini-sub">
            <span>Physical lock via SG90 servo.</span>
            <span class="pill">Updated in real-time</span>
          </div>
        </div>

        <!-- Authorization + PIN -->
        <div class="mini-card">
          <div class="mini-header">
            <div class="mini-label">Access Authorization</div>
            <span id="authBadge" class="badge badge-amber">Waiting</span>
          </div>
          <div class="mini-value" id="authText">—</div>
          <div class="mini-sub">
            <span>Enter rotating PIN on keypad.</span>
            <span class="pill">PIN changes every 30s</span>
          </div>
          <div class="mini-sub">
            <span>Current PIN:</span>
            <span id="pwdValue" class="pill" style="font-family:monospace;">----</span>
          </div>
        </div>

        <!-- Info / Hint -->
        <div class="mini-card">
          <div class="mini-header">
            <div class="mini-label">System Notes</div>
          </div>
          <div style="font-size:0.72rem;color:#9ca3af;">
            · PIN regenerates every 30 seconds (shown here &amp; required on keypad).<br>
            · Gas alarm may auto-unlock the door and activate relay.<br>
            · This page is a live monitor; control logic runs on the ESP32.
          </div>
        </div>
      </div>
    </div>

    <!-- Footer -->
    <div class="footer">
      <span>Firmware handles thresholds &amp; safety actions.</span>
      <span>All telemetry streamed via WebSocket from ESP32.</span>
    </div>
  </div>
</div>

<script>
  let ws;

  function mapRange(value, inMin, inMax, outMin, outMax) {
    return ((value - inMin) * (outMax - outMin)) / (inMax - inMin) + outMin;
  }

  function connectWS() {
    ws = new WebSocket(`ws://${location.host}/ws`);

    const wsDot    = document.getElementById('ws-dot');
    const wsStatus = document.getElementById('ws-status');
    const card     = document.getElementById('card');
    const gasBar   = document.getElementById('gasBar');
    const pwdEl    = document.getElementById('pwdValue');

    ws.onopen = () => {
      wsDot.className = 'dot dot-ok';
      wsStatus.textContent = 'Live';
    };

    ws.onclose = () => {
      wsDot.className = 'dot dot-error';
      wsStatus.textContent = 'Reconnecting…';
      setTimeout(connectWS, 2000);
    };

    ws.onerror = () => {
      wsDot.className = 'dot dot-error';
      wsStatus.textContent = 'Error';
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        // Gas
        const gasValueEl = document.getElementById('gasValue');
        const gasStatus  = document.getElementById('gasStatus');

        gasValueEl.textContent = data.gas;

        // Normalized bar width (0–4095 → 0–100%)
        const raw = Number(data.gas) || 0;
        const pct = Math.min(100, Math.max(0, mapRange(raw, 0, 4095, 0, 100)));
        gasBar.style.width = pct + '%';

        if (data.gasAlarm) {
          gasStatus.textContent = 'Danger';
          gasStatus.className = 'badge badge-red';
          card.classList.add('gas-danger');
        } else {
          gasStatus.textContent = 'Normal';
          gasStatus.className = 'badge badge-green';
          card.classList.remove('gas-danger');
        }

        // Lock status
        const lockStatus = document.getElementById('lockStatus');
        const lockBadge  = document.getElementById('lockBadge');

        if (data.locked) {
          lockStatus.textContent = 'Locked';
          lockBadge.textContent  = 'Secured';
          lockBadge.className    = 'badge badge-green';
        } else {
          lockStatus.textContent = 'Open';
          lockBadge.textContent  = 'Unlocked';
          lockBadge.className    = 'badge badge-amber';
        }

        // Authorization
        const authText  = document.getElementById('authText');
        const authBadge = document.getElementById('authBadge');

        if (data.authorized) {
          authText.textContent   = 'Authorized';
          authBadge.textContent  = 'Access Granted';
          authBadge.className    = 'badge badge-green';
        } else {
          authText.textContent   = '—';
          authBadge.textContent  = 'Waiting';
          authBadge.className    = 'badge badge-amber';
        }

        // Password (PIN) display
        if (pwdEl) {
          pwdEl.textContent = data.pwd || '----';
        }

      } catch (e) {
        console.error('Invalid WS data', e);
      }
    };
  }

  connectWS();
</script>
</body>
</html>
)rawliteral";

// ------------------------- HELPER FUNCTIONS -------------------------

void rotateServoSmooth(int targetAngle) {
  if (targetAngle == currentServoAngle) return;
  int step = (targetAngle > currentServoAngle) ? 1 : -1;
  for (int pos = currentServoAngle; pos != targetAngle; pos += step) {
    doorServo.write(pos);
    delay(15); // smooth movement, adjust speed
  }
  doorServo.write(targetAngle);
  currentServoAngle = targetAngle;
}

void generateNewPassword() {
  int code = random(1000, 9999); // ensures 4-digit
  currentPassword = String(code);
  Serial.print("New password: ");
  Serial.println(currentPassword);
}

// LCD now only shows lock + gas, NOT the password
void updateLCD() {
  lcd.clear();
  // Line 1: Lock state
  lcd.setCursor(0, 0);
  if (isLocked) {
    lcd.print("LOCKED ");
  } else {
    lcd.print("OPEN   ");
  }
  // Line 2: Gas value
  lcd.setCursor(0, 1);
  lcd.print("Gas:");
  lcd.print(lastGasValue);
}

void updateGas() {
  lastGasValue = analogRead(MQ6_PIN);
  Serial.print("Gas ADC: ");
  Serial.println(lastGasValue);

  // Basic threshold logic + small hysteresis
  bool prevAlarm = gasAlarm;
  if (lastGasValue >= GAS_THRESHOLD) {
    gasAlarm = true;
  } else if (lastGasValue <= GAS_THRESHOLD - 150) {
    gasAlarm = false;
  }

  if (gasAlarm && !prevAlarm) {
    // Gas just went into alarm zone
    Serial.println("Gas threshold crossed! Unlocking and activating relay.");
    // Active LOW relay ON
    digitalWrite(RELAY_PIN, LOW);
    // Unlock door as safety
    isLocked = false;
    rotateServoSmooth(SERVO_UNLOCK_ANGLE);
  } else if (!gasAlarm && prevAlarm) {
    Serial.println("Gas back to safe range. Turning relay OFF.");
    // Turn relay OFF (inactive)
    digitalWrite(RELAY_PIN, HIGH);
    // (You can choose whether to auto-lock or keep open)
  }
}

// Build JSON string for WebSocket broadcast (now includes pwd)
String buildJsonState() {
  String json = "{";
  json += "\"gas\":" + String(lastGasValue) + ",";
  json += "\"gasAlarm\":" + String(gasAlarm ? "true" : "false") + ",";
  json += "\"locked\":" + String(isLocked ? "true" : "false") + ",";
  json += "\"authorized\":" + String(authorizedFlag ? "true" : "false") + ",";
  json += "\"pwd\":\"" + currentPassword + "\"";
  json += "}";
  return json;
}

void broadcastState() {
  String msg = buildJsonState();
  ws.textAll(msg);
}

// Process keypad input
void handleKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  Serial.print("Keypad: ");
  Serial.println(key);

  if (key >= '0' && key <= '9') {
    if (enteredPassword.length() < 4) {
      enteredPassword += key;
    }
  } else if (key == '*') {
    // Clear input
    enteredPassword = "";
  } else if (key == '#') {
    // Submit
    Serial.print("Entered: ");
    Serial.println(enteredPassword);

    if (enteredPassword == currentPassword) {
      Serial.println("PASSWORD OK - AUTHORIZED");
      authorizedFlag = true;
      lastAuthTimeMs = millis();
      isLocked = false;
      rotateServoSmooth(SERVO_UNLOCK_ANGLE);
      // Make sure relay is off (if no gas alarm)
      if (!gasAlarm) {
        digitalWrite(RELAY_PIN, HIGH);
      }
    } else {
      Serial.println("PASSWORD WRONG");
      authorizedFlag = false;
      lastAuthTimeMs = millis();
    }
    enteredPassword = "";
  }
}

// WebSocket events
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected\n", client->id());
    // Send initial state (including current PIN)
    client->text(buildJsonState());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // We don't expect messages from client in this example, but you can extend.
  }
}

// ------------------------- SETUP & LOOP -------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Seed random generator for password
  randomSeed(analogRead(39));

  // LCD
  Wire.begin(); // default SDA=21, SCL=22 on ESP32
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 SmartHome");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // Servo
  doorServo.attach(SERVO_PIN);
  doorServo.write(SERVO_LOCK_ANGLE);
  currentServoAngle = SERVO_LOCK_ANGLE;

  // Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Active LOW -> HIGH = OFF

  // MQ6 analog pin
  pinMode(MQ6_PIN, INPUT);

  // WiFi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());

  // Async WebServer routes
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("HTTP server started");

  // Initialize first password immediately
  lastPasswordChangeMs = millis() - PASSWORD_INTERVAL_MS;
}

void loop() {
  unsigned long now = millis();

  // Periodic password change
  if (now - lastPasswordChangeMs >= PASSWORD_INTERVAL_MS) {
    lastPasswordChangeMs = now;
    generateNewPassword();
    // LCD only shows lock/gas, password is on web
    updateLCD();
  }

  // Read gas periodically
  if (now - lastGasReadMs >= GAS_READ_INTERVAL_MS) {
    lastGasReadMs = now;
    updateGas();
    updateLCD();
  }

  // Keypad handling (frequent)
  handleKeypad();

  // Authorization flag timeout
  if (authorizedFlag && (now - lastAuthTimeMs > AUTH_STATUS_SHOW_MS)) {
    authorizedFlag = false;
  }

  // Periodic WebSocket broadcast
  if (now - lastBroadcastMs >= WS_BROADCAST_INTERVAL_MS) {
    lastBroadcastMs = now;
    broadcastState();
  }

  ws.cleanupClients();
}
