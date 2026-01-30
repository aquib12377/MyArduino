/*******************************************************
 * ESP32 Smart Home Demo (Pins unchanged)
 * Keypad correct PIN -> ONLY web + LCD state change
 * Gas alert -> servo + relay
 *
 * FIXED:
 * 1) Auto-lock UI now updates reliably after 5 seconds
 * 2) Gas alarm logic was inverted -> UI was not updating correctly
 * 3) Relay default ON -> fixed pinMode/digitalWrite order + force OFF early
 *******************************************************/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Keypad.h>

// ------------------------- USER CONFIG -------------------------
const char* ssid     = "AAA";
const char* password = "Acube123";

const unsigned long PASSWORD_INTERVAL_MS      = 30000;
const unsigned long GAS_READ_INTERVAL_MS      = 250;
const unsigned long WS_BROADCAST_INTERVAL_MS  = 500;

const unsigned long AUTH_STATUS_SHOW_MS       = 3000;
const unsigned long AUTO_LOCK_MS              = 5000;

const int SERVO_LOCK_ANGLE   = 0;
const int SERVO_UNLOCK_ANGLE = 90;

// ------------------------- PIN DEFINITIONS (UNCHANGED) -------------------------
const int SERVO_PIN = 19;
const int RELAY_PIN = 15;     // Active LOW relay (BOOT STRAP PIN!)
const int MQ6_PIN   = 18;     // MQ6 DIGITAL D0 on GPIO18

const bool MQ6_ALARM_ACTIVE_HIGH = true;  // D0 goes HIGH on alarm? set true/false accordingly

// ------------------------- LCD -------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------------- KEYPAD (UNCHANGED) -------------------------
const byte ROWS = 4;
const byte COLS = 3;

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33};

Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// ------------------------- WEB -------------------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo doorServo;

// ------------------------- STATE -------------------------
int  lastGasValue       = 0;   // 0..4095 (we map digital to 0/4095)
bool gasAlarm           = false;

bool isLocked           = true;   // UI/logic state
int  currentServoAngle  = SERVO_LOCK_ANGLE; // physical servo position tracking

String currentPassword  = "0000";
String enteredPassword  = "";

unsigned long lastPasswordChangeMs = 0;
unsigned long lastGasReadMs        = 0;
unsigned long lastBroadcastMs      = 0;

bool          authorizedFlag = false;
unsigned long lastAuthTimeMs = 0;

bool          autoLockArmed  = false;
unsigned long autoLockDueMs  = 0;

// ------------------------- HTML PAGE -------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Home Security Control</title>
  <style>
    :root{
      color-scheme: dark;
      --bg0:#020617;
      --bg1:#0b1224;
      --card: rgba(15, 23, 42, 0.72);
      --card2: rgba(2, 6, 23, 0.65);
      --stroke: rgba(148, 163, 184, 0.18);
      --stroke2: rgba(148, 163, 184, 0.26);
      --txt:#e5e7eb;
      --muted:#94a3b8;
      --muted2:#64748b;
      --ok:#22c55e;
      --warn:#f59e0b;
      --bad:#ef4444;
      --shadow: 0 24px 60px rgba(0,0,0,.55);
      --r16: 18px;
      --r20: 22px;
      --mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
      --sans: system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif;
    }
    *{ box-sizing: border-box; }
    html,body{ height:100%; }
    body{
      margin:0;
      font-family: var(--sans);
      color: var(--txt);
      background:
        radial-gradient(900px 520px at 15% 15%, rgba(34,211,238,.18), transparent 55%),
        radial-gradient(900px 520px at 85% 10%, rgba(167,139,250,.16), transparent 60%),
        radial-gradient(980px 560px at 78% 92%, rgba(34,197,94,.14), transparent 60%),
        linear-gradient(180deg, var(--bg1), var(--bg0));
      display:flex;
      align-items:center;
      justify-content:center;
      padding:18px;
    }
    .shell{ width:min(1100px, 100%); display:grid; gap:14px; }
    .topbar{
      display:flex; align-items:center; justify-content:space-between; gap:12px;
      padding:14px 16px; border-radius: var(--r20);
      background: linear-gradient(180deg, rgba(15,23,42,.68), rgba(2,6,23,.52));
      border: 1px solid var(--stroke); box-shadow: var(--shadow);
      backdrop-filter: blur(14px);
    }
    .brand{ display:flex; align-items:center; gap:12px; min-width: 240px; }
    .logo{
      width:38px;height:38px; border-radius: 12px;
      background:
        radial-gradient(circle at 30% 30%, rgba(34,211,238,.7), transparent 55%),
        radial-gradient(circle at 70% 70%, rgba(167,139,250,.7), transparent 55%),
        rgba(15,23,42,.9);
      border: 1px solid rgba(148,163,184,.22);
      box-shadow: 0 10px 22px rgba(0,0,0,.35);
      position:relative; overflow:hidden;
    }
    .logo:after{
      content:""; position:absolute; inset:-30%;
      background: linear-gradient(90deg, transparent, rgba(255,255,255,.18), transparent);
      transform: rotate(25deg); animation: shine 7s linear infinite;
    }
    @keyframes shine{ 0%{ transform: translateX(-60%) rotate(25deg); } 100%{ transform: translateX(60%) rotate(25deg); } }
    .brand h1{ margin:0; font-size: 1.05rem; letter-spacing: .08em; text-transform: uppercase; font-weight: 750; line-height: 1.1; }
    .brand p{ margin:2px 0 0; font-size: .82rem; color: var(--muted); }

    .statusRow{ display:flex; align-items:center; gap:10px; flex-wrap: wrap; justify-content:flex-end; }
    .chip{
      display:inline-flex; align-items:center; gap:8px; padding:8px 10px; border-radius: 999px;
      background: rgba(2,6,23,.55); border: 1px solid rgba(148,163,184,.20);
      font-size:.82rem; color: var(--muted); user-select:none;
    }
    .dot{ width:10px; height:10px; border-radius:999px; background: var(--bad); box-shadow: 0 0 0 4px rgba(239,68,68,.18); }
    .dot.ok{ background: var(--ok); box-shadow: 0 0 0 4px rgba(34,197,94,.18); }

    .layout{ display:grid; grid-template-columns: 1.3fr 1fr; gap:14px; }
    @media (max-width: 860px){ .layout{ grid-template-columns: 1fr; } .brand{ min-width: unset; } }

    .card{
      border-radius: var(--r20);
      background: linear-gradient(180deg, rgba(15,23,42,.70), rgba(2,6,23,.52));
      border: 1px solid var(--stroke);
      box-shadow: var(--shadow);
      backdrop-filter: blur(14px);
      overflow:hidden;
    }
    .cardHeader{
      display:flex; justify-content:space-between; align-items:flex-start; gap:12px;
      padding:14px 16px 10px; border-bottom: 1px solid rgba(148,163,184,.12);
    }
    .cardHeader .title{ font-weight: 720; letter-spacing: .08em; text-transform: uppercase; font-size: .85rem; color: #cbd5e1; }
    .cardHeader .sub{ margin-top:4px; font-size: .82rem; color: var(--muted2); }

    .badge{
      display:inline-flex; align-items:center; justify-content:center; gap:7px;
      padding:7px 10px; border-radius: 999px; font-size: .78rem; font-weight: 650;
      border: 1px solid transparent; white-space: nowrap; user-select:none;
    }
    .badge .bDot{ width:8px;height:8px;border-radius:999px; }
    .b-ok{ background: rgba(34,197,94,.16); color: #86efac; border-color: rgba(34,197,94,.30); }
    .b-ok .bDot{ background: var(--ok); }
    .b-warn{ background: rgba(245,158,11,.14); color: #fcd34d; border-color: rgba(245,158,11,.28); }
    .b-warn .bDot{ background: var(--warn); }
    .b-bad{ background: rgba(239,68,68,.14); color: #fecaca; border-color: rgba(239,68,68,.28); }
    .b-bad .bDot{ background: var(--bad); }

    .grid2{ display:grid; grid-template-columns: 1fr 1fr; gap:12px; padding:14px 16px 16px; }
    @media (max-width: 520px){ .grid2{ grid-template-columns: 1fr; } }

    .metric{ border-radius: var(--r16); padding:12px 12px 10px; background: rgba(2,6,23,.55); border: 1px solid rgba(148,163,184,.14); }
    .metricTop{ display:flex; justify-content:space-between; align-items:center; gap:10px; margin-bottom: 10px; }
    .metricLabel{ font-size:.78rem; color: var(--muted); letter-spacing: .12em; text-transform: uppercase; }
    .barShell{
      width:100%; height:10px; border-radius:999px; background: rgba(15,23,42,.75);
      border:1px solid rgba(148,163,184,.14); overflow:hidden; margin: 10px 0 8px;
    }
    .barFill{
      height:100%; width:0%; border-radius:999px;
      background: linear-gradient(90deg, rgba(34,197,94,.9), rgba(245,158,11,.9), rgba(239,68,68,.9));
      transition: width .25s ease;
    }
    .pill{
      font-family: var(--mono); font-size: .9rem; padding:8px 10px; border-radius: 999px;
      background: rgba(15,23,42,.75); border: 1px solid rgba(148,163,184,.18);
      color: #cbd5e1; min-width: 92px; text-align:center;
    }
    .bigState{ display:flex; flex-direction:column; gap:10px; padding:14px 16px 16px; }
    .stateRow{
      display:flex; gap:10px; align-items:center; justify-content:space-between;
      padding:12px 12px; border-radius: var(--r16);
      background: rgba(2,6,23,.55); border: 1px solid rgba(148,163,184,.14);
    }
    .stateLeft{ display:flex; flex-direction:column; gap:4px; }
    .stateTitle{ font-size:.78rem; color: var(--muted); letter-spacing:.12em; text-transform: uppercase; }
    .stateMain{ font-size: 1.12rem; font-weight: 760; }
    .foot{
      padding:12px 16px 14px; border-top: 1px solid rgba(148,163,184,.10);
      color: var(--muted2); font-size: .78rem;
      display:flex; justify-content:space-between; gap:10px; flex-wrap: wrap;
    }
    .kbd{
      font-family: var(--mono); padding:2px 6px; border-radius: 8px;
      border: 1px solid rgba(148,163,184,.18); background: rgba(2,6,23,.55); color: #cbd5e1;
    }
  </style>
</head>
<body>
  <div class="shell">
    <div class="topbar">
      <div class="brand">
        <div class="logo" aria-hidden="true"></div>
        <div>
          <h1>Home Security Control</h1>
          <p>Live dashboard · ESP32 + MQ6 + Keypad + Servo</p>
        </div>
      </div>

      <div class="statusRow">
        <div class="chip">
          <span class="dot" id="wsDot"></span>
          <span id="wsText">Connecting…</span>
        </div>
        <div class="chip">
          <span style="opacity:.9;">Channel</span>
          <span class="kbd">/ws</span>
        </div>
      </div>
    </div>

    <div class="layout">
      <section class="card" id="gasCard">
        <div class="cardHeader">
          <div>
            <div class="title">Air Safety</div>
            <div class="sub">MQ6 status & live level indicator</div>
          </div>
          <div class="badge b-ok" id="gasBadge">
            <span class="bDot"></span>
            <span id="gasBadgeText">Normal</span>
          </div>
        </div>

        <div class="grid2">
          <div class="metric">
            <div class="metricTop">
              <div class="metricLabel">Gas Level</div>
              <div class="pill" id="gasValue">—</div>
            </div>
            <div class="barShell" aria-hidden="true">
              <div class="barFill" id="gasBar"></div>
            </div>
            <div class="metricHint">
              <span>ADC 0 – 4095</span>
              <span>Firmware decides alarm</span>
            </div>
          </div>

          <div class="metric">
            <div class="metricTop">
              <div class="metricLabel">Alarm</div>
              <div class="pill" id="gasStatus">—</div>
            </div>
            <div class="metricHint">
              <span>When alarm triggers</span>
              <span>servo + relay act</span>
            </div>
          </div>
        </div>

        <div class="foot">
          <span>Tip: If values don’t change, check WebSocket and sensor wiring.</span>
          <span>Updates stream ~500ms</span>
        </div>
      </section>

      <aside class="card">
        <div class="cardHeader">
          <div>
            <div class="title">Security & Access</div>
            <div class="sub">Lock state, authorization, and rotating PIN</div>
          </div>
          <div class="badge b-warn" id="lockBadge">
            <span class="bDot"></span>
            <span id="lockBadgeText">Standby</span>
          </div>
        </div>

        <div class="bigState">
          <div class="stateRow">
            <div class="stateLeft">
              <div class="stateTitle">Door Lock</div>
              <div class="stateMain" id="lockStatus">—</div>
            </div>
            <div class="pill" id="lockPill">—</div>
          </div>

          <div class="stateRow">
            <div class="stateLeft">
              <div class="stateTitle">Authorization</div>
              <div class="stateMain" id="authText">—</div>
            </div>
            <div class="pill" id="authPill">—</div>
          </div>

          <div class="stateRow">
            <div class="stateLeft">
              <div class="stateTitle">Current PIN</div>
              <div class="stateMain" style="opacity:.9;">Use keypad & press <span class="kbd">#</span></div>
            </div>
            <div class="pill" id="pwdValue">----</div>
          </div>
        </div>

        <div class="foot">
          <span><span class="kbd">*</span> clears · <span class="kbd">#</span> submits</span>
          <span>PIN rotates every ~30s</span>
        </div>
      </aside>
    </div>
  </div>

  <script>
    let ws;

    function clamp(n, a, b){ return Math.min(b, Math.max(a, n)); }
    function pctFromGas(g){ return clamp((Number(g)||0) / 4095 * 100, 0, 100); }

    function connectWS(){
      ws = new WebSocket(`ws://${location.host}/ws`);

      const wsDot  = document.getElementById('wsDot');
      const wsText = document.getElementById('wsText');

      ws.onopen = () => {
        wsDot.className = 'dot ok';
        wsText.textContent = 'Live';
      };

      ws.onclose = () => {
        wsDot.className = 'dot';
        wsText.textContent = 'Reconnecting…';
        setTimeout(connectWS, 2000);
      };

      ws.onerror = () => {
        wsDot.className = 'dot';
        wsText.textContent = 'Error';
      };

      ws.onmessage = (e) => {
        let d;
        try { d = JSON.parse(e.data); } catch { return; }

        const gas = d.gas ?? '—';
        const gasAlarm = !!d.gasAlarm;
        const locked = !!d.locked;
        const authorized = !!d.authorized;

        document.getElementById('gasValue').textContent  = gas;
        document.getElementById('gasStatus').textContent = gasAlarm ? 'DANGER' : 'NORMAL';
        document.getElementById('gasBar').style.width = pctFromGas(gas) + '%';

        const gasBadge = document.getElementById('gasBadge');
        const gasBadgeText = document.getElementById('gasBadgeText');
        if (gasAlarm){
          gasBadge.className = 'badge b-bad';
          gasBadgeText.textContent = 'Danger';
        }else{
          gasBadge.className = 'badge b-ok';
          gasBadgeText.textContent = 'Normal';
        }

        document.getElementById('lockStatus').textContent = locked ? 'Locked' : 'Open';
        document.getElementById('lockPill').textContent   = locked ? 'SECURED' : 'UNLOCKED';

        document.getElementById('authText').textContent = authorized ? 'Authorized' : 'Waiting';
        document.getElementById('authPill').textContent = authorized ? 'GRANTED' : 'PENDING';

        const lockBadge = document.getElementById('lockBadge');
        const lockBadgeText = document.getElementById('lockBadgeText');

        if (gasAlarm){
          lockBadge.className = 'badge b-bad';
          lockBadgeText.textContent = 'Gas Alert';
        } else if (!locked){
          lockBadge.className = 'badge b-warn';
          lockBadgeText.textContent = 'Door Open';
        } else {
          lockBadge.className = 'badge b-ok';
          lockBadgeText.textContent = 'Secured';
        }

        document.getElementById('pwdValue').textContent = d.pwd || '----';
      };
    }

    connectWS();
  </script>
</body>
</html>
)rawliteral";

// ------------------------- HELPERS -------------------------
void rotateServoSmooth(int targetAngle) {
  if (targetAngle == currentServoAngle) return;
  int step = (targetAngle > currentServoAngle) ? 1 : -1;
  for (int pos = currentServoAngle; pos != targetAngle; pos += step) {
    doorServo.write(pos);
    delay(15);
  }
  doorServo.write(targetAngle);
  currentServoAngle = targetAngle;
}

void generateNewPassword() {
  int code = random(1000, 9999);
  currentPassword = String(code);
  Serial.print("[PWD] New: ");
  Serial.println(currentPassword);
}

void updateLCDTopLine() {
  lcd.setCursor(0, 0);
  lcd.print(isLocked ? "LOCKED " : "OPEN   ");
  lcd.setCursor(7, 0);
  lcd.print(gasAlarm ? "GAS!" : "SAFE ");
  lcd.setCursor(12,0);
  lcd.print("    ");
}

void showKeyOnLCD(char key) {
  lcd.setCursor(0, 1);
  lcd.print("Key:");
  lcd.print(key);
  lcd.print(" In:");
  lcd.print(enteredPassword);
  int used = 6 + enteredPassword.length();
  for (int i = used; i < 16; i++) lcd.print(' ');
}

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
  ws.textAll(buildJsonState());
}

// ------------------------- GAS UPDATE -------------------------
void updateGas() {
  int raw = digitalRead(MQ6_PIN);

  // ✅ FIX #2: alarm logic was inverted in your code
  // If MQ6_ALARM_ACTIVE_HIGH == true => alarm when raw == HIGH
  // else => alarm when raw == LOW
  bool alarmNow = (MQ6_ALARM_ACTIVE_HIGH) ? (raw == HIGH) : (raw == LOW);

  bool prevAlarm = gasAlarm;
  gasAlarm = alarmNow;

  // Digital D0, so we map to 0/4095 for UI bar
  lastGasValue = raw ? 4095 : 0;

  if (gasAlarm && !prevAlarm) {
    Serial.println("[GAS] ALARM -> Servo Unlock + Relay ON");
    digitalWrite(RELAY_PIN, LOW);        // ON (active low)

    // UI state open
    isLocked = false;

    rotateServoSmooth(SERVO_UNLOCK_ANGLE);

    // cancel keypad auto-lock during gas
    autoLockArmed = false;
    authorizedFlag = false;

    updateLCDTopLine();
    broadcastState();
  }
  else if (!gasAlarm && prevAlarm) {
    Serial.println("[GAS] SAFE -> Relay OFF");
    digitalWrite(RELAY_PIN, HIGH);       // OFF

    // NOTE: you didn't request re-lock servo here, so leaving as-is.
    // If you want: rotateServoSmooth(SERVO_LOCK_ANGLE); isLocked=true;

    updateLCDTopLine();
    broadcastState();
  }
}

// ------------------------- KEYPAD (PIN ONLY CHANGES STATE) -------------------------
void handleKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  Serial.print("[KEY] ");
  Serial.println(key);

  if (key >= '0' && key <= '9') {
    if (enteredPassword.length() < 4) enteredPassword += key;
  } else if (key == '*') {
    enteredPassword = "";
  } else if (key == '#') {
    Serial.print("[KEY] Submit: ");
    Serial.println(enteredPassword);

    if (enteredPassword == currentPassword) {
      Serial.println("[AUTH] OK (STATE ONLY, NO SERVO/RELAY)");

      authorizedFlag = true;
      lastAuthTimeMs = millis();

      // ✅ ONLY update UI state
      isLocked = false;

      // ✅ FIX #1: always arm auto-lock when unlocked via PIN (unless gas alarm)
      if (!gasAlarm) {
        autoLockArmed = true;
        autoLockDueMs = millis() + AUTO_LOCK_MS;
      } else {
        autoLockArmed = false;
      }

      updateLCDTopLine();
      broadcastState();

    } else {
      Serial.println("[AUTH] WRONG");
      authorizedFlag = false;
      lastAuthTimeMs = millis();
      broadcastState();
    }

    enteredPassword = "";
  }

  showKeyOnLCD(key);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected\n", client->id());
    client->text(buildJsonState());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Starting...");

  randomSeed(esp_random());

  // ✅ FIX #3: relay default ON
  // Set OUTPUT first, then write HIGH (OFF).
  // GPIO15 is a boot-strap pin; hardware pull-ups/another GPIO is best,
  // but this is the safest software order.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // OFF (active low) ASAP

  // LCD
  Wire.begin(21, 22);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("ESP32 SmartHome");
  lcd.setCursor(0, 1); lcd.print("Booting...");

  // MQ6
  pinMode(MQ6_PIN, INPUT);

  // Servo
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  doorServo.write(SERVO_LOCK_ANGLE);
  currentServoAngle = SERVO_LOCK_ANGLE;

  // WiFi
  Serial.print("[WIFI] Connecting ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
  Serial.println("[HTTP] Server started");

  lastPasswordChangeMs = millis() - PASSWORD_INTERVAL_MS;
  lastGasReadMs = millis();
  lastBroadcastMs = millis();

  generateNewPassword();
  updateGas();

  lcd.clear();
  updateLCDTopLine();
  lcd.setCursor(0, 1); lcd.print("Ready");

  broadcastState();
  Serial.println("[BOOT] Ready");
}

void loop() {
  unsigned long now = millis();

  // password rotate
  if (now - lastPasswordChangeMs >= PASSWORD_INTERVAL_MS) {
    lastPasswordChangeMs = now;
    generateNewPassword();
    broadcastState();
  }

  // gas read
  if (now - lastGasReadMs >= GAS_READ_INTERVAL_MS) {
    lastGasReadMs = now;
    updateGas();
  }

  // keypad
  handleKeypad();

  // auth badge timeout (UI only)
  if (authorizedFlag && (now - lastAuthTimeMs > AUTH_STATUS_SHOW_MS)) {
    authorizedFlag = false;
    broadcastState();
  }

  // ✅ FIX #1: robust auto-lock check (handles millis overflow correctly)
  if (autoLockArmed && (int32_t)(now - autoLockDueMs) >= 0) {
    autoLockArmed = false;

    if (!gasAlarm) {
      Serial.println("[LOCK] Auto lock (STATE ONLY, NO SERVO)");
      isLocked = true;
      updateLCDTopLine();
      broadcastState();
    }
  }

  // periodic broadcast
  if (now - lastBroadcastMs >= WS_BROADCAST_INTERVAL_MS) {
    lastBroadcastMs = now;
    broadcastState();
  }

  ws.cleanupClients();
}
