// ESP32 – 10‑Pump Controller (simple 10‑second pulse)
// Author: ChatGPT (o3) – April 15 2025
// rev‑J: **Add “Start All” & “Reverse All” buttons** while keeping all existing logic
// -----------------------------------------------------------------------------
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ───────────────────────── USER CONFIG ──────────────────────────────────────
const char *SSID = "MyProject";
const char *PASSWORD = "12345678";
const uint16_t HTTP_PORT = 80;

// GPIO maps (edit to match wiring)
const int FWD_PIN[10] = { 13, 12, 14, 27, 26, 25, 33, 32, 23, 1 };
const int REV_PIN[10] = { 15, 2, 4, 16, 17, 5, 18, 19, 21, 22 };

// All relays are active‑LOW
constexpr bool ACTIVE_LOW = true;

constexpr uint32_t PULSE_MS = 10'000;  // 10‑second ON window
constexpr uint32_t DEBUG_MS = 2000;    // serial cadence

// ───────────────────────── RUNTIME STRUCT -----------------------------------
struct PumpRun {
  enum State { IDLE,
               FWD,
               REV } st = IDLE;
  uint32_t tEnd = 0;
} run[10];
WebServer server(HTTP_PORT);

// ───────────────────────── RELAY HELPERS ------------------------------------
inline void relayWrite(int pin, bool on) {
  digitalWrite(pin, ACTIVE_LOW ? !on : on);
}
void setRelays(uint8_t i, bool fwd, bool rev) {
  relayWrite(FWD_PIN[i], fwd);
  relayWrite(REV_PIN[i], rev);
}
void allOff() {
  for (int i = 0; i < 10; i++) {
    setRelays(i, false, false);
    run[i].st = PumpRun::IDLE;
  }
}

// ───────────────────────── REST ENDPOINTS -----------------------------------
void apiStart() {
  StaticJsonDocument<32> d;
  if (!server.hasArg("plain") || deserializeJson(d, server.arg("plain"))) {
    server.send(400, "text/plain", "bad");
    return;
  }
  uint8_t p = d["pump"].as<uint8_t>();
  if (p >= 10) {
    server.send(400, "text/plain", "pump");
    return;
  }
  setRelays(p, true, false);
  run[p].st = PumpRun::FWD;
  run[p].tEnd = millis() + PULSE_MS;
  server.send(200, "application/json", "{}");
}
void apiReverse() {
  StaticJsonDocument<32> d;
  if (!server.hasArg("plain") || deserializeJson(d, server.arg("plain"))) {
    server.send(400, "text/plain", "bad");
    return;
  }
  uint8_t p = d["pump"].as<uint8_t>();
  if (p >= 10) {
    server.send(400, "text/plain", "pump");
    return;
  }
  setRelays(p, false, true);
  run[p].st = PumpRun::REV;
  run[p].tEnd = millis() + PULSE_MS;
  server.send(200, "application/json", "{}");
}

// New: trigger every pump
void apiStartAll() {
  for (uint8_t i = 0; i < 10; i++) {
    setRelays(i, true, false);
    run[i].st = PumpRun::FWD;
    run[i].tEnd = millis() + PULSE_MS;
  }
  server.send(200, "application/json", "{}");
}
void apiReverseAll() {
  for (uint8_t i = 0; i < 10; i++) {
    setRelays(i, false, true);
    run[i].st = PumpRun::REV;
    run[i].tEnd = millis() + PULSE_MS;
  }
  server.send(200, "application/json", "{}");
}

void apiStatus() {
  StaticJsonDocument<128> doc;
  JsonArray a = doc.createNestedArray("s");
  for (int i = 0; i < 10; i++) a.add(run[i].st);
  String j;
  serializeJson(doc, j);
  server.send(200, "application/json", j);
}

// ─────────────────────────  Sleek HTML DASHBOARD  ──────────────────────────
const char HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1"><title>Pumps</title><style>
body{font-family:sans-serif;background:#111;color:#eee;display:flex;flex-direction:column;align-items:center;padding:2rem}
#t td,#t th{padding:.5rem .8rem;text-align:center;border:1px solid #444}
.btn{padding:.3rem .7rem;border:none;border-radius:4px;cursor:pointer}
.start{background:#238636;color:#fff}
.rev{background:#f85149;color:#fff}
.all{background:#58a6ff;color:#fff}
.idle{color:#888} .on{color:#58a6ff}
</style></head><body>
<div style="margin-bottom:1rem">
  <button class="btn all" id="allStart">Start ALL</button>
  <button class="btn all" id="allRev">Reverse ALL</button>
</div>
<table id=t><tr><th>Pump</th><th>Start</th><th>Reverse</th><th>Status</th></tr></table>
<script>
const t=document.getElementById('t');
for(let i=0;i<10;i++){
  const r=t.insertRow();
  r.innerHTML=`<td>${i}</td><td><button class='btn start'>Start</button></td><td><button class='btn rev'>Rev</button></td><td id=s${i} class='idle'>Idle</td>`;
  r.cells[1].firstChild.onclick=_=>fetch('/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pump:i})});
  r.cells[2].firstChild.onclick=_=>fetch('/reverse',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pump:i})});
}
// global buttons
const post=u=>fetch(u,{method:'POST'});
document.getElementById('allStart').onclick=_=>post('/startAll');
document.getElementById('allRev').onclick  =_=>post('/reverseAll');

setInterval(()=>fetch('/status').then(r=>r.json()).then(j=>j.s.forEach((st,i)=>{const el=document.getElementById('s'+i);el.textContent=st?'ON':'Idle';el.className=st?'on':'idle';})),500);
</script></body></html>
)HTML";

void setupWeb() {
  server.on("/", HTTP_GET, [] {
    server.send_P(200, "text/html", HTML);
  });
  server.on("/start", HTTP_POST, apiStart);
  server.on("/reverse", HTTP_POST, apiReverse);
  server.on("/startAll", HTTP_POST, apiStartAll);      // new
  server.on("/reverseAll", HTTP_POST, apiReverseAll);  // new
  server.on("/status", HTTP_GET, apiStatus);
  server.begin();
}

// ───────────────────────── SETUP / LOOP ------------------------------------
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 10; i++) {
    pinMode(FWD_PIN[i], OUTPUT);
    pinMode(REV_PIN[i], OUTPUT);
  }
  allOff();
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(WiFi.localIP());
  setupWeb();
}

void loop() {
  server.handleClient();
  uint32_t now = millis();
  for (uint8_t i = 0; i < 10; i++) {
    if (run[i].st != PumpRun::IDLE && now >= run[i].tEnd) {
      setRelays(i, false, false);
      run[i].st = PumpRun::IDLE;
    }
  }
  static uint32_t dbg = 0;
  if (now - dbg >= DEBUG_MS) {
    dbg = now;
    Serial.print("[DBG]");
    for (int i = 0; i < 10; i++) Serial.printf(" %d", run[i].st);
    Serial.println();
  }
}
