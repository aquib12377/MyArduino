/*******************************************************
 * ESP32 – 4-Channel Relay Home Automation (ACTIVE-LOW)
 * - Web page served on http://<ESP-IP>/
 * - WebSocket on ws://<ESP-IP>:81/
 * - 4 relays controlled from a dashboard-style UI
 *
 * Requires:
 *   - WiFi library (built-in)
 *   - arduinoWebSockets library by Links2004
 *
 * Relay board: ACTIVE-LOW
 *   RELAY_ON  = LOW
 *   RELAY_OFF = HIGH
 *******************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// -------------------- USER CONFIG --------------------
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// Adjust these pins to your wiring
const uint8_t RELAY1_PIN = 14;
const uint8_t RELAY2_PIN = 27;
const uint8_t RELAY3_PIN = 26;
const uint8_t RELAY4_PIN = 25;

// ACTIVE-LOW relay logic
const uint8_t RELAY_ON  = LOW;
const uint8_t RELAY_OFF = HIGH;

// -------------------- GLOBALS ------------------------
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Store relay states in RAM (true = ON, false = OFF)
bool relayState[4] = { false, false, false, false };

// -------------------- HTML PAGE ----------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Home Automation Panel</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    :root {
      --bg: #0b1220;
      --card-bg: #111827;
      --accent: #38bdf8;
      --accent-soft: rgba(56,189,248,0.2);
      --danger: #fb7185;
      --text: #e5e7eb;
      --muted: #6b7280;
      --border: #1f2937;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }

    body {
      background: radial-gradient(circle at top left, #1f2937, #020617);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 16px;
    }

    .frame {
      width: min(960px, 100%);
      background: linear-gradient(145deg, #020617, #020617);
      border-radius: 20px;
      border: 1px solid rgba(148,163,184,0.3);
      box-shadow: 0 18px 40px rgba(15,23,42,0.9);
      overflow: hidden;
    }

    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 16px 20px;
      border-bottom: 1px solid var(--border);
      background: linear-gradient(90deg, rgba(56,189,248,0.08), transparent);
    }

    .brand {
      display: flex;
      align-items: center;
      gap: 12px;
    }

    .brand-badge {
      width: 32px;
      height: 32px;
      border-radius: 10px;
      background: radial-gradient(circle at 30% 30%, #38bdf8, #0ea5e9);
      display: flex;
      align-items: center;
      justify-content: center;
      color: #0b1120;
      font-weight: 800;
      font-size: 18px;
      box-shadow: 0 0 20px rgba(56,189,248,0.7);
    }

    .brand-text {
      display: flex;
      flex-direction: column;
    }

    .brand-title {
      font-size: 18px;
      font-weight: 600;
      letter-spacing: 0.03em;
    }

    .brand-sub {
      font-size: 11px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.22em;
    }

    .status {
      display: flex;
      align-items: center;
      gap: 10px;
      font-size: 12px;
      color: var(--muted);
    }

    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 999px;
      background: #22c55e;
      box-shadow: 0 0 8px #22c55e;
    }

    .status-label {
      text-transform: uppercase;
      letter-spacing: 0.14em;
      font-size: 10px;
    }

    .chip {
      padding: 4px 10px;
      border-radius: 999px;
      background: rgba(15,23,42,0.8);
      border: 1px solid #1e293b;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      color: var(--muted);
    }

    .chip-dot {
      width: 6px;
      height: 6px;
      border-radius: 999px;
      background: var(--accent);
    }

    .content {
      padding: 18px 20px 20px;
      display: grid;
      grid-template-columns: 2fr 1fr;
      gap: 16px;
    }

    @media (max-width: 720px) {
      .content {
        grid-template-columns: 1fr;
      }
    }

    .devices-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
      gap: 12px;
    }

    .device-card {
      background: radial-gradient(circle at top left, rgba(56,189,248,0.08), rgba(15,23,42,0.95));
      border-radius: 14px;
      border: 1px solid rgba(148,163,184,0.25);
      padding: 14px 12px 12px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      gap: 12px;
      position: relative;
      overflow: hidden;
    }

    .device-card::before {
      content: "";
      position: absolute;
      inset: -60%;
      background: radial-gradient(circle at top, rgba(56,189,248,0.18), transparent 70%);
      opacity: 0;
      transition: opacity 0.25s ease-out;
      pointer-events: none;
    }

    .device-card.on::before {
      opacity: 1;
    }

    .device-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }

    .device-name {
      font-size: 14px;
      font-weight: 500;
    }

    .device-zone {
      font-size: 11px;
      color: var(--muted);
    }

    .device-status {
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.15em;
      padding: 3px 8px;
      border-radius: 999px;
      background: rgba(15,23,42,0.9);
      border: 1px solid rgba(148,163,184,0.4);
    }

    .device-card.on .device-status {
      border-color: rgba(56,189,248,0.9);
      color: #bae6fd;
      background: rgba(15,23,42,0.4);
    }

    .device-footer {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }

    .device-meta {
      font-size: 11px;
      color: var(--muted);
      line-height: 1.4;
    }

    /* Toggle switch */
    .switch {
      position: relative;
      display: inline-block;
      width: 52px;
      height: 26px;
    }

    .switch input {
      display: none;
    }

    .slider {
      position: absolute;
      cursor: pointer;
      inset: 0;
      background-color: #020617;
      border-radius: 999px;
      border: 1px solid var(--border);
      transition: .3s;
      box-shadow: inset 0 0 0 1px rgba(15,23,42,0.9);
    }

    .slider::before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 4px;
      top: 3px;
      background: radial-gradient(circle at 30% 20%, #64748b, #0f172a);
      border-radius: 50%;
      transition: .25s;
      box-shadow: 0 6px 12px rgba(15,23,42,0.9);
    }

    input:checked + .slider {
      background: linear-gradient(90deg, rgba(56,189,248,0.3), rgba(56,189,248,0.05));
      border-color: var(--accent);
      box-shadow: 0 0 0 1px rgba(34,211,238,0.4);
    }

    input:checked + .slider::before {
      transform: translateX(22px);
      background: radial-gradient(circle at 30% 20%, #f9fafb, #38bdf8);
      box-shadow: 0 6px 16px rgba(56,189,248,0.8);
    }

    .sidebar {
      background: radial-gradient(circle at top, rgba(56,189,248,0.08), rgba(15,23,42,0.98));
      border-radius: 14px;
      border: 1px solid rgba(51,65,85,0.8);
      padding: 12px 12px 10px;
      display: flex;
      flex-direction: column;
      gap: 10px;
      font-size: 12px;
    }

    .sidebar-title {
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.16em;
      color: var(--muted);
      display: flex;
      align-items: center;
      justify-content: space-between;
    }

    .log {
      max-height: 140px;
      overflow-y: auto;
      padding-right: 4px;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
      font-size: 11px;
      line-height: 1.4;
    }

    .log-line {
      margin-bottom: 2px;
      color: #9ca3af;
    }

    .log-line span {
      color: var(--accent);
    }

    .badge-soft {
      padding: 2px 7px;
      border-radius: 999px;
      font-size: 10px;
      background: var(--accent-soft);
      color: #e0f2fe;
    }

  </style>
</head>
<body>
<div class="frame">
  <div class="header">
    <div class="brand">
      <div class="brand-badge">AE</div>
      <div class="brand-text">
        <div class="brand-title">Alpha Electronz</div>
        <div class="brand-sub">HOME AUTOMATION NODE</div>
      </div>
    </div>
    <div class="status">
      <div class="status-dot" id="wsDot"></div>
      <div>
        <div class="status-label" id="wsStatusLabel">CONNECTING...</div>
        <div class="chip">
          <div class="chip-dot"></div>
          <span id="ipLabel">IP: -</span>
        </div>
      </div>
    </div>
  </div>

  <div class="content">
    <div>
      <div class="devices-grid">
        <div class="device-card" id="card1">
          <div class="device-header">
            <div>
              <div class="device-name">Relay 1</div>
              <div class="device-zone">Living Room Light</div>
            </div>
            <div class="device-status" id="status1">OFF</div>
          </div>
          <div class="device-footer">
            <div class="device-meta">
              Channel 1<br>
              Mode: Manual
            </div>
            <label class="switch">
              <input type="checkbox" id="relay1" onchange="toggleRelay(1)">
              <span class="slider"></span>
            </label>
          </div>
        </div>

        <div class="device-card" id="card2">
          <div class="device-header">
            <div>
              <div class="device-name">Relay 2</div>
              <div class="device-zone">Bedroom Light</div>
            </div>
            <div class="device-status" id="status2">OFF</div>
          </div>
          <div class="device-footer">
            <div class="device-meta">
              Channel 2<br>
              Mode: Manual
            </div>
            <label class="switch">
              <input type="checkbox" id="relay2" onchange="toggleRelay(2)">
              <span class="slider"></span>
            </label>
          </div>
        </div>

        <div class="device-card" id="card3">
          <div class="device-header">
            <div>
              <div class="device-name">Relay 3</div>
              <div class="device-zone">Fan / Appliance</div>
            </div>
            <div class="device-status" id="status3">OFF</div>
          </div>
          <div class="device-footer">
            <div class="device-meta">
              Channel 3<br>
              Mode: Manual
            </div>
            <label class="switch">
              <input type="checkbox" id="relay3" onchange="toggleRelay(3)">
              <span class="slider"></span>
            </label>
          </div>
        </div>

        <div class="device-card" id="card4">
          <div class="device-header">
            <div>
              <div class="device-name">Relay 4</div>
              <div class="device-zone">Extra / Socket</div>
            </div>
            <div class="device-status" id="status4">OFF</div>
          </div>
          <div class="device-footer">
            <div class="device-meta">
              Channel 4<br>
              Mode: Manual
            </div>
            <label class="switch">
              <input type="checkbox" id="relay4" onchange="toggleRelay(4)">
              <span class="slider"></span>
            </label>
          </div>
        </div>
      </div>
    </div>

    <div class="sidebar">
      <div class="sidebar-title">
        SYSTEM LOG
        <span class="badge-soft" id="nodeLabel">NODE ONLINE</span>
      </div>
      <div class="log" id="logBox"></div>
    </div>
  </div>
</div>

<script>
  let socket;
  let reconnectTimer;

  function log(msg) {
    const box = document.getElementById('logBox');
    const line = document.createElement('div');
    line.className = 'log-line';
    line.innerHTML = '<span>•</span> ' + msg;
    box.prepend(line);
    const children = box.children;
    if (children.length > 80) {
      box.removeChild(box.lastChild);
    }
  }

  function setWsStatus(connected) {
    const dot = document.getElementById('wsDot');
    const label = document.getElementById('wsStatusLabel');
    if (connected) {
      dot.style.background = '#22c55e';
      dot.style.boxShadow = '0 0 8px #22c55e';
      label.textContent = 'ONLINE';
      log('WebSocket connected');
    } else {
      dot.style.background = '#f97373';
      dot.style.boxShadow = '0 0 8px #f97373';
      label.textContent = 'OFFLINE';
      log('WebSocket disconnected');
    }
  }

  function updateRelayUI(index, isOn) {
    const card = document.getElementById('card' + index);
    const status = document.getElementById('status' + index);
    const checkbox = document.getElementById('relay' + index);

    checkbox.checked = isOn;
    if (isOn) {
      card.classList.add('on');
      status.textContent = 'ON';
    } else {
      card.classList.remove('on');
      status.textContent = 'OFF';
    }
  }

  function toggleRelay(index) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      log('Cannot toggle R' + index + ' – WebSocket not connected');
      return;
    }
    const checkbox = document.getElementById('relay' + index);
    const value = checkbox.checked ? 1 : 0;
    const msg = 'SET:' + index + ':' + value;
    socket.send(msg);
    log('TX: ' + msg);
  }

  function handleMessage(msg) {
    log('RX: ' + msg);

    // Format from ESP32: "STATE:1010" (R1..R4)
    if (msg.startsWith('STATE:')) {
      const bits = msg.substring(6); // after "STATE:"
      for (let i = 0; i < 4; i++) {
        const c = bits.charAt(i);
        if (c === '1' || c === '0') {
          updateRelayUI(i + 1, c === '1');
        }
      }
    }

    // Optional: simple text logs like "INFO:..."
  }

  function initWebSocket() {
    const wsUrl = 'ws://' + window.location.hostname + ':81/';
    log('Connecting to ' + wsUrl + ' ...');
    socket = new WebSocket(wsUrl);

    socket.onopen = function() {
      setWsStatus(true);
      socket.send('GET');    // ask for current state
    };

    socket.onclose = function() {
      setWsStatus(false);
      clearTimeout(reconnectTimer);
      reconnectTimer = setTimeout(initWebSocket, 2000);
    };

    socket.onerror = function(e) {
      log('WebSocket error');
    };

    socket.onmessage = function(event) {
      handleMessage(event.data);
    };
  }

  window.addEventListener('load', function() {
    document.getElementById('ipLabel').textContent = 'IP: ' + window.location.hostname;
    initWebSocket();
  });
</script>
</body>
</html>
)rawliteral";

// -------------------- RELAY HELPERS ------------------
void applyRelayOutputs() {
  // relayState[i] true => ON => ACTIVE-LOW => digitalWrite LOW
  digitalWrite(RELAY1_PIN, relayState[0] ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY2_PIN, relayState[1] ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY3_PIN, relayState[2] ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY4_PIN, relayState[3] ? RELAY_ON : RELAY_OFF);
}

String buildStateString() {
  // Compose: "STATE:1010" where 1=ON, 0=OFF
  String s = "STATE:";
  for (int i = 0; i < 4; i++) {
    s += (relayState[i] ? '1' : '0');
  }
  return s;
}

// -------------------- WEBSOCKET EVENT -----------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[WS] Client %u connected from %s\n", num, ip.toString().c_str());
      // Send current state on connect
      String stateMsg = buildStateString();
      webSocket.sendTXT(num, stateMsg);
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected\n", num);
      break;

    case WStype_TEXT: {
      payload[length] = 0;  // null-terminate
      String msg = (char*)payload;
      Serial.printf("[WS] RX '%s'\n", msg.c_str());

      if (msg == "GET") {
        String stateMsg = buildStateString();
        webSocket.sendTXT(num, stateMsg);
      } else if (msg.startsWith("SET:")) {
        // Format: SET:<index>:<value>
        // Example: SET:1:1  => Relay1 ON
        int firstColon = msg.indexOf(':');
        int secondColon = msg.indexOf(':', firstColon + 1);
        if (secondColon > 0) {
          int idx = msg.substring(firstColon + 1, secondColon).toInt(); // 1..4
          int val = msg.substring(secondColon + 1).toInt();             // 0 or 1

          if (idx >= 1 && idx <= 4 && (val == 0 || val == 1)) {
            relayState[idx - 1] = (val == 1);
            applyRelayOutputs();
            String stateMsg = buildStateString();
            webSocket.broadcastTXT(stateMsg); // notify all clients
            Serial.printf("[WS] Relay %d -> %s\n", idx, relayState[idx-1] ? "ON" : "OFF");
          }
        }
      }
      break;
    }

    default:
      break;
  }
}

// -------------------- HTTP HANDLERS -------------------
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// -------------------- SETUP & LOOP --------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== ESP32 Home Automation – 4 Relay ===");

  // Relay pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  // Initialize all relays OFF
  relayState[0] = relayState[1] = relayState[2] = relayState[3] = false;
  applyRelayOutputs();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());

  // HTTP server
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started on port 80");

  // WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}
