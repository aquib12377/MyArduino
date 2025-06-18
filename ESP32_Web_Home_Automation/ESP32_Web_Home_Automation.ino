/********************************************************************
 *  ESP32 – 4-Relay Web Controller (ACTIVE LOW)
 *  ---------------------------------------------------------------
 *  Author : ChatGPT (o3) – May 30 2025
 *  Board  : ESP32-DevKit-C, Arduino core ≥ 2.0.11
 ********************************************************************/
#include <WiFi.h>
#include <WebServer.h>

/* ────────── USER CONFIG ────────── */

// ── Wi-Fi mode ──────────────────────────────────────────────
//  1) STATION: connect to your router
const bool USE_STA = true;          // false = create AP instead
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";

//  2) ACCESS-POINT: uncomment if USE_STA == false
const char* AP_SSID  = "SmartHome";
const char* AP_PASS  = "12345678";  // ≥8 chars

// relay GPIOs (change to match wiring)
const uint8_t RELAY_PIN[4] = {25, 26, 27, 14};
// initial state (all OFF = HIGH because relays are active-low)
bool relayState[4] = {false, false, false, false};

/* ────────── Globals ────────── */
WebServer server(80);

/* ────────── HTML UI (served at “/”) ────────── */
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Home Automation · ESP32</title>
<style>
:root{--bg:#f5f7fa;--fg:#111;--accent:#2563eb;--card:#fff;--off:#d1d5db}
*{box-sizing:border-box;margin:0;padding:0;font-family:system-ui,Segoe UI,Roboto}
body{background:var(--bg);color:var(--fg);display:flex;flex-direction:column;align-items:center;padding:2rem}
h1{margin-bottom:1.5rem}
.card{background:var(--card);padding:1.5rem 2rem;border-radius:1rem;box-shadow:0 4px 20px rgba(0,0,0,.08);width:100%;max-width:420px}
.switch{display:flex;align-items:center;justify-content:space-between;margin:1rem 0;font-size:1.1rem}
input[type=checkbox]{appearance:none;width:3.25rem;height:1.8rem;background:var(--off);border-radius:1rem;position:relative;cursor:pointer;outline:none;transition:.3s}
input:checked{background:var(--accent)}
input:before{content:"";position:absolute;width:1.4rem;height:1.4rem;border-radius:50%;background:#fff;top:.2rem;left:.2rem;transition:.3s}
input:checked:before{transform:translateX(1.45rem)}
footer{margin-top:2rem;font-size:.85rem;color:#666;text-align:center}
a{color:var(--accent);text-decoration:none}
</style>
</head><body>
<h1>🏠 Home Automation</h1>
<div class="card" id="panel">
  <!-- switches inserted by JS -->
</div>
<footer>ESP32 · Active-Low Relay Board</footer>
<script>
const relayCount = 4;
const panel = document.getElementById('panel');

function makeRow(i,state){
  return `<div class="switch">
    <span>Bulb ${i+1}</span>
    <input type="checkbox" id="r${i}" ${state?'checked':''}
           onchange="toggleRelay(${i},this.checked)">
  </div>`;
}

function render(states){                // build/update UI
  panel.innerHTML = states.map((s,i)=>makeRow(i,s)).join('');
}

async function fetchStates(){
  const r = await fetch('/status');
  render(await r.json());
}

async function toggleRelay(idx,on){
  await fetch(`/set?relay=${idx}&state=${on?'1':'0'}`);
  // optional: re-query to stay in sync if multiple clients
}

fetchStates();                          // initial load
setInterval(fetchStates, 5000);         // refresh every 5 s
</script></body></html>
)rawliteral";

/* ────────── Helper functions ────────── */
void setRelay(uint8_t idx, bool on){
  relayState[idx] = on;
  digitalWrite(RELAY_PIN[idx], on ? LOW : HIGH);   // ACTIVE-LOW
}

void handleRoot(){ server.send_P(200, "text/html", MAIN_page); }

void handleStatus(){
  // return JSON array: [true,false,true,false]
  String json = "[";
  for(int i=0;i<4;i++){
    json += relayState[i] ? "true":"false";
    if(i<3) json += ',';
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSet(){
  if(!server.hasArg("relay") || !server.hasArg("state")){
    server.send(400, "text/plain", "Bad Args"); return;
  }
  int idx = server.arg("relay").toInt();
  bool on = server.arg("state") == "1";
  if(idx<0 || idx>3){ server.send(400,"text/plain","Bad Idx"); return; }
  setRelay(idx, on);
  server.send(200,"text/plain","OK");
}

/* ────────── SETUP ────────── */
void setup(){
  Serial.begin(115200);
  // initialise GPIOs
  for(auto pin: RELAY_PIN){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);             // OFF (active-low)
  }

  if(USE_STA){
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("\nConnecting to Wi-Fi…");
    while(WiFi.status()!=WL_CONNECTED){
      delay(300); Serial.print('.');
    }
    Serial.print("\nIP: "); Serial.println(WiFi.localIP());
  }else{
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("\nAP IP: "); Serial.println(WiFi.softAPIP());
  }

  // web handlers
  server.on("/",      HTTP_GET, handleRoot);
  server.on("/status",HTTP_GET, handleStatus);
  server.on("/set",   HTTP_GET, handleSet);
  server.begin();
  Serial.println("Web server ready.");
}

/* ────────── LOOP ────────── */
void loop(){ server.handleClient(); }
