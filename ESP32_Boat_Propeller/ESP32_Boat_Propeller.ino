/*******************************************************
 * ESP32 Lake Bot - Web UI + WebSocket Control
 * Uses:
 *  - WebServer (built-in ESP32 core)
 *  - WebSocketsServer (Links2004 / Markus Sattler)  <-- install
 *  - ArduinoJson (optional but used here)           <-- install
 *
 * Web UI: http://<ip>/
 * WebSocket: ws://<ip>:81/
 *
 * Arduino-ESP32 Core 3.x LEDC:
 *  ledcAttachChannel(pin, freq, res, channel)
 *  ledcWrite(pin, duty)
 *******************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ===================== WiFi =====================
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// ===================== L298N Pins =====================
// Motor A (Roller)
static const int IN1 = 14;
static const int IN2 = 27;
static const int ENA = 32;   // PWM

// Motor B (Fan / Propeller)
static const int IN3 = 26;
static const int IN4 = 25;
static const int ENB = 33;   // PWM

// ===================== PWM (Core 3.x) =====================
static const uint32_t PWM_FREQ = 20000;
static const uint8_t  PWM_RES  = 8;     // 0..255
static const int8_t   CH_ROLLER = 0;
static const int8_t   CH_FAN    = 1;

// ===================== State =====================
struct MotorState {
  bool on = false;
  int  dir = 1;     // +1 fwd, -1 rev
  int  spd = 0;     // 0..255
};

MotorState roller;
MotorState fan;

volatile uint32_t lastCmdMs = 0;
const uint32_t COMMAND_TIMEOUT_MS = 5000;

// ===================== Servers =====================
WebServer server(80);
WebSocketsServer ws(81); // WebSocket runs on port 81

// ===================== UI (HTML) =====================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Lake Bot Control</title>
  <style>
    :root{
      color-scheme: dark;
      --bg0:#020617; --bg1:#0b1224;
      --card: rgba(15,23,42,.72);
      --stroke: rgba(148,163,184,.18);
      --stroke2: rgba(148,163,184,.28);
      --txt:#e5e7eb; --muted:#94a3b8;
      --ok:#22c55e; --warn:#f59e0b; --bad:#ef4444;
      --cyan:#22d3ee; --violet:#a78bfa;
      --shadow: 0 14px 40px rgba(0,0,0,.45);
      --r: 18px;
    }
    *{box-sizing:border-box}
    body{
      margin:0; font-family: ui-sans-serif,system-ui,Segoe UI,Roboto,Helvetica,Arial;
      background: radial-gradient(1200px 800px at 20% 0%, rgba(34,211,238,.14), transparent 60%),
                  radial-gradient(900px 700px at 90% 10%, rgba(167,139,250,.16), transparent 55%),
                  linear-gradient(180deg, var(--bg1), var(--bg0));
      color:var(--txt); min-height:100vh; display:flex; justify-content:center;
    }
    .wrap{width:min(1100px, 94vw); padding:22px 0 36px}
    .top{
      display:flex; gap:14px; align-items:center; justify-content:space-between;
      padding:16px; border:1px solid var(--stroke); border-radius:var(--r);
      background: linear-gradient(180deg, rgba(15,23,42,.66), rgba(2,6,23,.55));
      box-shadow: var(--shadow);
    }
    h1{margin:0; font-size:20px}
    .sub{color:var(--muted); font-size:13px; margin-top:4px}
    .pill{
      display:flex; align-items:center; gap:10px; padding:10px 12px;
      border:1px solid var(--stroke2); border-radius:999px; background: rgba(2,6,23,.45);
      font-size:13px; color:var(--muted);
    }
    .dot{width:10px;height:10px;border-radius:999px;background:var(--warn); box-shadow:0 0 0 6px rgba(245,158,11,.12)}
    .grid{margin-top:16px; display:grid; grid-template-columns: 1.2fr 1fr; gap:16px}
    @media (max-width: 900px){ .grid{grid-template-columns:1fr} }
    .card{
      border:1px solid var(--stroke); border-radius:var(--r);
      background: var(--card);
      box-shadow: var(--shadow);
      overflow:hidden;
    }
    .card .hd{
      padding:14px; display:flex; justify-content:space-between; align-items:flex-start;
      border-bottom:1px solid var(--stroke);
      background: linear-gradient(180deg, rgba(2,6,23,.12), rgba(2,6,23,.02));
    }
    .ttl{font-weight:800}
    .meta{font-size:12px; color:var(--muted); margin-top:4px}
    .badge{
      font-size:12px; padding:6px 10px; border-radius:999px; border:1px solid var(--stroke2);
      color:var(--muted); background: rgba(2,6,23,.35);
      height: fit-content;
    }
    .bd{padding:14px}
    .row{display:flex; gap:10px; flex-wrap:wrap; align-items:center}
    .btn{
      border:1px solid var(--stroke2); background: rgba(2,6,23,.32); color:var(--txt);
      padding:10px 12px; border-radius:14px; cursor:pointer; font-weight:700;
      transition: transform .06s ease;
      user-select:none;
    }
    .btn:active{transform: translateY(1px)}
    .btn.ok{border-color: rgba(34,197,94,.45)}
    .btn.bad{border-color: rgba(239,68,68,.55)}
    .btn.violet{border-color: rgba(167,139,250,.55)}
    .btn.cyan{border-color: rgba(34,211,238,.55)}
    .btn.ghost{opacity:.9}
    .btn.full{width:100%}
    .split{display:grid; grid-template-columns: 1fr 1fr; gap:10px}
    .kv{
      display:grid; grid-template-columns: 1fr auto; gap:10px; padding:10px 12px;
      border:1px solid var(--stroke); border-radius:14px; background: rgba(2,6,23,.24);
      margin-top:10px;
    }
    .k{color:var(--muted); font-size:12px}
    .v{font-weight:800; font-size:12px}
    input[type="range"]{width:100%}
    .small{font-size:12px; color:var(--muted)}
  </style>
</head>
<body>
  <div class="wrap">
    <div class="top">
      <div>
        <h1>Lake Bot Control</h1>
        <div class="sub">WebSocket live control • Roller + Propeller</div>
      </div>
      <div class="pill">
        <div class="dot" id="dot"></div>
        <div>
          <div id="connText">Connecting…</div>
          <div class="small" id="netText">—</div>
        </div>
      </div>
    </div>

    <div class="grid">
      <div class="card">
        <div class="hd">
          <div>
            <div class="ttl">Motors</div>
            <div class="meta">Speed 0–255 • Live status</div>
          </div>
          <div class="badge" id="uptime">—</div>
        </div>
        <div class="bd">
          <div class="kv">
            <div>
              <div class="ttl">Roller</div>
              <div class="meta" id="rollerMeta">OFF • FWD • SPD 0</div>
            </div>
            <div class="badge" id="rollerBadge">OFF</div>
          </div>

          <div style="margin-top:10px" class="row">
            <button class="btn ok" onclick="setOn('roller', true)">ON</button>
            <button class="btn bad" onclick="setOn('roller', false)">OFF</button>
            <button class="btn violet" onclick="setDir('roller', 1)">FWD</button>
            <button class="btn violet" onclick="setDir('roller', -1)">REV</button>
          </div>
          <div style="margin-top:10px">
            <div class="row" style="justify-content:space-between">
              <div class="k">Speed</div>
              <div class="v" id="rollerSpdLabel">0</div>
            </div>
            <input type="range" min="0" max="255" value="0" id="rollerSpd" oninput="setSpd('roller', this.value)">
          </div>

          <div class="kv" style="margin-top:14px">
            <div>
              <div class="ttl">Fan / Propeller</div>
              <div class="meta" id="fanMeta">OFF • FWD • SPD 0</div>
            </div>
            <div class="badge" id="fanBadge">OFF</div>
          </div>

          <div style="margin-top:10px" class="row">
            <button class="btn ok" onclick="setOn('fan', true)">ON</button>
            <button class="btn bad" onclick="setOn('fan', false)">OFF</button>
            <button class="btn cyan" onclick="setDir('fan', 1)">FWD</button>
            <button class="btn cyan" onclick="setDir('fan', -1)">REV</button>
          </div>
          <div style="margin-top:10px">
            <div class="row" style="justify-content:space-between">
              <div class="k">Speed</div>
              <div class="v" id="fanSpdLabel">0</div>
            </div>
            <input type="range" min="0" max="255" value="0" id="fanSpd" oninput="setSpd('fan', this.value)">
          </div>

          <div style="margin-top:14px" class="split">
            <button class="btn bad full" onclick="stopAll()">EMERGENCY STOP</button>
            <button class="btn ghost full" onclick="syncState()">SYNC</button>
          </div>

          <div class="kv" style="margin-top:14px">
            <div class="k">Wi-Fi RSSI</div><div class="v" id="rssi">—</div>
            <div class="k">IP</div><div class="v" id="ip">—</div>
            <div class="k">WS Clients</div><div class="v" id="clients">—</div>
            <div class="k">Last Cmd</div><div class="v" id="lastCmd">—</div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="hd">
          <div>
            <div class="ttl">Safety</div>
            <div class="meta">Auto-stop if no commands</div>
          </div>
          <div class="badge">SAFE</div>
        </div>
        <div class="bd">
          <div class="small">
            If your phone disconnects or no command is received for a few seconds, both motors stop automatically.
          </div>
          <div class="small" style="margin-top:10px">
            Note: WebSocket uses port <b>81</b>.
          </div>
        </div>
      </div>
    </div>
  </div>

<script>
  let ws;

  function wsUrl(){
    return `ws://${location.hostname}:81/`;
  }

  function setConn(ok){
    const dot = document.getElementById('dot');
    const connText = document.getElementById('connText');
    if(ok){
      dot.style.background = "var(--ok)";
      dot.style.boxShadow = "0 0 0 6px rgba(34,197,94,.12)";
      connText.textContent = "Connected";
    } else {
      dot.style.background = "var(--warn)";
      dot.style.boxShadow = "0 0 0 6px rgba(245,158,11,.12)";
      connText.textContent = "Connecting…";
    }
  }

  function send(obj){
    if(ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
  }
  function setOn(target, on){ send({type:"cmd", target, on: !!on}); }
  function setDir(target, dir){ send({type:"cmd", target, dir: Number(dir)}); }
  function setSpd(target, spd){ send({type:"cmd", target, spd: Number(spd)}); }
  function stopAll(){ send({type:"cmd", target:"all", stop:true}); }
  function syncState(){ send({type:"get"}); }

  function render(state){
    document.getElementById('netText').textContent = `RSSI ${state.rssi} dBm • Clients ${state.clients}`;
    document.getElementById('uptime').textContent = `Uptime ${state.uptime}s`;

    document.getElementById('rollerMeta').textContent =
      `${state.roller.on ? "ON":"OFF"} • ${state.roller.dir===1?"FWD":"REV"} • SPD ${state.roller.spd}`;
    document.getElementById('fanMeta').textContent =
      `${state.fan.on ? "ON":"OFF"} • ${state.fan.dir===1?"FWD":"REV"} • SPD ${state.fan.spd}`;

    document.getElementById('rollerBadge').textContent = state.roller.on ? "ON":"OFF";
    document.getElementById('fanBadge').textContent = state.fan.on ? "ON":"OFF";

    document.getElementById('rollerSpdLabel').textContent = state.roller.spd;
    document.getElementById('fanSpdLabel').textContent = state.fan.spd;

    const r = document.getElementById('rollerSpd');
    const f = document.getElementById('fanSpd');
    if(document.activeElement !== r) r.value = state.roller.spd;
    if(document.activeElement !== f) f.value = state.fan.spd;

    document.getElementById('rssi').textContent = `${state.rssi} dBm`;
    document.getElementById('ip').textContent = state.ip;
    document.getElementById('clients').textContent = state.clients;
    document.getElementById('lastCmd').textContent = `${state.lastCmdAge} ms ago`;
  }

  function connect(){
    setConn(false);
    ws = new WebSocket(wsUrl());

    ws.onopen = () => { setConn(true); syncState(); };
    ws.onmessage = (ev) => {
      try{
        const msg = JSON.parse(ev.data);
        if(msg.type === "state") render(msg);
      }catch(e){}
    };
    ws.onclose = () => { setConn(false); setTimeout(connect, 800); };
    ws.onerror = () => { try{ ws.close(); }catch(e){} };
  }
  connect();
</script>
</body>
</html>
)rawliteral";

// ===================== Helpers =====================
static inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

void setMotorDirPins(int inA, int inB, int dir) {
  if (dir >= 0) { digitalWrite(inA, HIGH); digitalWrite(inB, LOW); }
  else          { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
}

void motorStopPins(int inA, int inB) {
  digitalWrite(inA, LOW);
  digitalWrite(inB, LOW);
}

void applyOutputs() {
  if (roller.on && roller.spd > 0) {
    setMotorDirPins(IN1, IN2, roller.dir);
    ledcWrite(ENA, roller.spd);
  } else {
    ledcWrite(ENA, 0);
    motorStopPins(IN1, IN2);
  }

  if (fan.on && fan.spd > 0) {
    setMotorDirPins(IN3, IN4, fan.dir);
    ledcWrite(ENB, fan.spd);
  } else {
    ledcWrite(ENB, 0);
    motorStopPins(IN3, IN4);
  }
}

void stopAllMotors() {
  roller.on = false; roller.spd = 0;
  fan.on    = false; fan.spd    = 0;
  applyOutputs();
}

void sendStateToAll() {
  StaticJsonDocument<384> doc;
  doc["type"] = "state";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = (uint32_t)(millis() / 1000);
  doc["clients"] = ws.connectedClients();
  doc["lastCmdAge"] = (uint32_t)(millis() - lastCmdMs);

  JsonObject r = doc.createNestedObject("roller");
  r["on"] = roller.on; r["dir"] = roller.dir; r["spd"] = roller.spd;

  JsonObject f = doc.createNestedObject("fan");
  f["on"] = fan.on; f["dir"] = fan.dir; f["spd"] = fan.spd;

  String out;
  serializeJson(doc, out);
  ws.broadcastTXT(out);
}

void handleCmdJson(const String& msg) {
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

  const char* type = doc["type"] | "";
  if (strcmp(type, "get") == 0) return;
  if (strcmp(type, "cmd") != 0) return;

  lastCmdMs = millis();

  String target = doc["target"] | "";
  bool stop = doc["stop"] | false;

  if (stop || target == "all") {
    stopAllMotors();
    return;
  }

  MotorState* m = nullptr;
  if (target == "roller") m = &roller;
  else if (target == "fan") m = &fan;
  else return;

  if (doc.containsKey("on"))  m->on  = (bool)doc["on"];
  if (doc.containsKey("dir")) m->dir = ((int)doc["dir"] >= 0) ? 1 : -1;
  if (doc.containsKey("spd")) {
    m->spd = clamp255((int)doc["spd"]);
    if (m->spd > 0) m->on = true;
  }

  applyOutputs();
}

// WebSocket event
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      sendStateToAll();
      break;

    case WStype_TEXT: {
      String msg;
      msg.reserve(length + 1);
      for (size_t i = 0; i < length; i++) msg += (char)payload[i];

      handleCmdJson(msg);
      sendStateToAll();
      break;
    }

    case WStype_DISCONNECTED:
      // no immediate stop; safety timeout handles it
      sendStateToAll();
      break;

    default:
      break;
  }
}

uint32_t lastBroadcastMs = 0;

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // LEDC Core 3.x
  ledcAttachChannel(ENA, PWM_FREQ, PWM_RES, CH_ROLLER);
  ledcAttachChannel(ENB, PWM_FREQ, PWM_RES, CH_FAN);

  stopAllMotors();
  lastCmdMs = millis();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // HTTP routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not Found");
  });

  server.begin();
  Serial.println("HTTP server: 80");

  // WebSocket
  ws.begin();
  ws.onEvent(onWsEvent);
  Serial.println("WebSocket server: 81");
}

void loop() {
  server.handleClient();
  ws.loop();

  uint32_t now = millis();

  // periodic status push
  if (now - lastBroadcastMs >= 500) {
    lastBroadcastMs = now;
    sendStateToAll();
  }

  // safety stop
  if ((roller.on || fan.on) && (now - lastCmdMs > COMMAND_TIMEOUT_MS)) {
    stopAllMotors();
    sendStateToAll();
  }
}
