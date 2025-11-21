/*
  Solar Cleaning Bot – ESP32 WebServer + WebSocket (no Async)
  - Two DC motors (tank steer), no PWM used (enable pins strapped HIGH on driver).
  - Two ultrasonic sensors for cliff/edge detection.
  - Three active-LOW relays: Microfibre cloth, Blower, Ionizer.
  - Web UI over HTTP + WS for control & telemetry.
  - AUTO mode: forward; on edge -> stop, back, turn away, continue.

  Libraries:
    - Built-in: WiFi.h, WebServer.h, WebSocketsServer.h, ESPmDNS.h
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>

// -------------------- USER CONFIG --------------------
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// Motor pins (no PWM; strap driver EN pins HIGH)
const uint8_t M1_IN1 = 13;   // Left motor
const uint8_t M1_IN2 = 12;
const uint8_t M2_IN1 = 14;   // Right motor
const uint8_t M2_IN2 = 27;

// Ultrasonic (downward-facing for edge/cliff detection)
const uint8_t US_L_TRIG = 4;
const uint8_t US_L_ECHO = 16;
const uint8_t US_R_TRIG = 17;
const uint8_t US_R_ECHO = 5;

// Relays (ACTIVE LOW)
const uint8_t RELAY_CLOTH  = 26;
const uint8_t RELAY_BLOWER = 25;
const uint8_t RELAY_ION    = 33;

// Edge detection
const float   EDGE_CLIFF_CM      = 15.0;    // if reading > this (or 0 = no echo) => cliff
const uint32_t US_READ_INTERVAL  = 100;     // ms
const uint32_t TELEMETRY_INTERVAL= 500;     // ms

// Auto avoid timings (tune for your bot)
const uint32_t BACK_MS  = 450;
const uint32_t TURN_MS  = 500;

// -------------------- GLOBALS ------------------------
WebServer server(80);
WebSocketsServer ws(81);

enum BotMode { MANUAL=0, AUTO=1 };
enum Action  { ACT_STOP=0, ACT_FWD, ACT_BACK, ACT_LEFT, ACT_RIGHT };

BotMode mode           = MANUAL;
Action  currentAction  = ACT_STOP;

bool clothOn=false, blowerOn=false, ionOn=false;

float dLeft=0, dRight=0;
bool edgeL=false, edgeR=false;

uint32_t lastUS=0, lastTX=0;

// Auto avoid state
bool autoBusy=false;
uint8_t autoStage=0;          // 0 = idle/run forward; 1=back; 2=turn; 3=resume
uint32_t stageUntil=0;

// -------------- Utilities ---------------------------
void relaySet(uint8_t pin, bool on) {
  // Active LOW relays
  digitalWrite(pin, on ? LOW : HIGH);
}

void motorsStop() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, LOW);
  currentAction = ACT_STOP;
}

void motorsForward() {
  digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW);
  currentAction = ACT_FWD;
}

void motorsBackward() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, HIGH);
  digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, HIGH);
  currentAction = ACT_BACK;
}

void motorsLeft() {
  // Left wheel back, right wheel forward
  digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, HIGH);
  digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW);
  currentAction = ACT_LEFT;
}

void motorsRight() {
  // Left wheel forward, right wheel back
  digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, LOW);  digitalWrite(M2_IN2, HIGH);
  currentAction = ACT_RIGHT;
}

float measureCM(uint8_t tpin, uint8_t epin) {
  digitalWrite(tpin, LOW); delayMicroseconds(2);
  digitalWrite(tpin, HIGH); delayMicroseconds(10);
  digitalWrite(tpin, LOW);
  unsigned long dur = pulseIn(epin, HIGH, 20000UL); // ~20ms timeout (≈3.4m)
  if (dur == 0) return 0.0f;                        // no echo
  return dur / 58.0f;                               // µs to cm
}

bool isCliff(float cm) {
  return (cm == 0.0f) || (cm > EDGE_CLIFF_CM);
}

const char* actName(Action a){
  switch(a){
    case ACT_FWD: return "FWD"; case ACT_BACK: return "BACK";
    case ACT_LEFT: return "LEFT"; case ACT_RIGHT: return "RIGHT";
    default: return "STOP";
  }
}

// -------------- AUTO logic --------------------------
void autoTick() {
  if (mode != AUTO) return;

  // If currently executing an avoidance sequence
  if (autoBusy) {
    if (millis() >= stageUntil) {
      // next stage
      if (autoStage == 1) { // finished BACK -> TURN
        // Turn away from detected side; if both edged, turn right
        if (edgeL && !edgeR) { motorsRight(); stageUntil = millis() + TURN_MS; autoStage = 2; }
        else if (edgeR && !edgeL) { motorsLeft(); stageUntil = millis() + TURN_MS; autoStage = 2; }
        else { motorsRight(); stageUntil = millis() + TURN_MS; autoStage = 2; }
      } else if (autoStage == 2) {
        // finished TURN -> resume forward
        motorsForward(); autoBusy=false; autoStage=0;
      }
    }
    return;
  }

  // Normal AUTO forward cruise
  if (currentAction != ACT_FWD) motorsForward();

  // Edge detected -> start avoidance
  if (edgeL || edgeR) {
    motorsStop(); delay(10);
    motorsBackward();
    autoBusy = true;
    autoStage = 1;
    stageUntil = millis() + BACK_MS;
  }
}

// -------------- Web UI (HTTP) -----------------------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solar Cleaning Bot</title>
<style>
  :root { font-family: system-ui, Arial, sans-serif; color-scheme: light dark; }
  body { margin: 0; padding: 16px; }
  .grid { display:grid; gap:12px; grid-template-columns: repeat(3, minmax(100px,1fr)); max-width:680px; margin:auto; }
  .card { border:1px solid #8883; border-radius:14px; padding:12px; }
  h2 { margin:6px 0 12px; }
  button, .toggle { padding:12px; border-radius:12px; border:1px solid #8884; font-size:16px; cursor:pointer; }
  button:active { transform:scale(0.98); }
  .drive { display:grid; grid-template-columns:repeat(3, 1fr); gap:8px; }
  .drive button { height:64px; }
  .wide { grid-column: 1 / -1; }
  .row { display:flex; gap:8px; align-items:center; justify-content:space-between; }
  .tag { font: 12px/1.2 monospace; padding:4px 8px; border-radius:8px; background:#8882; }
  .ok{background:#16a34a22;border:1px solid #16a34a66}
  .warn{background:#dc262622;border:1px solid #dc262666}
</style>
</head><body>
  <h2>Solar Cleaning Bot</h2>
  <div class="grid">
    <div class="card wide">
      <div class="row"><strong>Status</strong><span id="act" class="tag">STOP</span></div>
      <div class="row"><span>Mode</span>
        <label class="row"><input id="auto" type="checkbox"> <span>Auto</span></label>
      </div>
    </div>

    <div class="card">
      <strong>Ultrasonic L</strong>
      <div id="d1" class="tag ok">0.0 cm</div>
      <div id="e1" class="tag">—</div>
    </div>
    <div class="card">
      <strong>Ultrasonic R</strong>
      <div id="d2" class="tag ok">0.0 cm</div>
      <div id="e2" class="tag">—</div>
    </div>
    <div class="card">
      <strong>Relays</strong>
      <div class="row"><label><input id="cloth" type="checkbox"> Microfibre</label><span id="rc" class="tag">OFF</span></div>
      <div class="row"><label><input id="blower" type="checkbox"> Blower</label><span id="rb" class="tag">OFF</span></div>
      <div class="row"><label><input id="ion" type="checkbox"> Ionizer</label><span id="ri" class="tag">OFF</span></div>
    </div>

    <div class="card wide">
      <strong>Drive</strong>
      <div class="drive">
        <div></div><button id="fwd">▲</button><div></div>
        <button id="left">◀</button><button id="stop">■</button><button id="right">▶</button>
        <div></div><button id="back">▼</button><div></div>
      </div>
    </div>
  </div>

<script>
  const ws = new WebSocket(`ws://${location.hostname}:81/`);
  const send = (s)=>{ if(ws.readyState===1) ws.send(s); };
  let uiUpdating = false;
  const $ = (id)=>document.getElementById(id);
  const act=$("act"), d1=$("d1"), d2=$("d2"), e1=$("e1"), e2=$("e2");
  const rc=$("rc"), rb=$("rb"), ri=$("ri");

  $("fwd").onclick = ()=>send("CMD:FWD");
  $("back").onclick= ()=>send("CMD:BACK");
  $("left").onclick= ()=>send("CMD:LEFT");
  $("right").onclick=()=>send("CMD:RIGHT");
  $("stop").onclick =()=>send("CMD:STOP");

  $("auto").onchange = (e)=>send("MODE:"+(e.target.checked?"AUTO":"MANUAL"));
$("cloth").onclick  = (e)=> send("RELAY:CLOTH:" + (e.target.checked ? 1 : 0));
  $("blower").onclick = (e)=> send("RELAY:BLOWER:" + (e.target.checked ? 1 : 0));
  $("ion").onclick    = (e)=> send("RELAY:ION:"   + (e.target.checked ? 1 : 0));

  ws.onmessage = (ev)=>{
    try{
      const j = JSON.parse(ev.data);
      d1.textContent = (j.d1||0).toFixed(1)+" cm";
      d2.textContent = (j.d2||0).toFixed(1)+" cm";
      d1.className = "tag "+(j.edgeL?"warn":"ok");
      d2.className = "tag "+(j.edgeR?"warn":"ok");
      e1.textContent = j.edgeL ? "EDGE!" : "ok";
      e2.textContent = j.edgeR ? "EDGE!" : "ok";
      act.textContent = j.act || "STOP";
      $("auto").checked = (j.mode==="auto");
      $("cloth").checked  = !!j.rel?.cloth;
      $("blower").checked = !!j.rel?.blower;
      $("ion").checked    = !!j.rel?.ion;
    }catch(_){}
  };
</script>
</body></html>
)HTML";

// -------------- HTTP handlers -----------------------
void handleRoot(){
  server.send_P(200, "text/html", INDEX_HTML);
}

// -------------- WebSocket handlers ------------------
void handleWS(uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
  switch(type){
    case WStype_CONNECTED: {
      IPAddress ip = ws.remoteIP(num);
      Serial.printf("[WS] Client %u connected from %s\n", num, ip.toString().c_str());
      broadcastStatus();
      break;
    }
    case WStype_TEXT: {
      String msg = String((char*)payload, len);
      msg.trim();
      Serial.println("Command: "+msg);
      // Commands
      if (msg == "CMD:FWD")       { mode=MANUAL; autoBusy=false; motorsForward(); }
      else if (msg == "CMD:BACK") { mode=MANUAL; autoBusy=false; motorsBackward(); }
      else if (msg == "CMD:LEFT") { mode=MANUAL; autoBusy=false; motorsLeft(); }
      else if (msg == "CMD:RIGHT"){ mode=MANUAL; autoBusy=false; motorsRight(); }
      else if (msg == "CMD:STOP") { mode=MANUAL; autoBusy=false; motorsStop(); }

      else if (msg == "MODE:AUTO"){ mode=AUTO;  autoBusy=false; /* will go FWD in autoTick */ }
      else if (msg == "MODE:MANUAL"){ mode=MANUAL; autoBusy=false; motorsStop(); }

      else if (msg.startsWith("RELAY:")){
        // RELAY:<NAME>:<0|1>
        // e.g. RELAY:CLOTH:1
        int a = msg.indexOf(':', 6);
        int b = msg.lastIndexOf(':');
        if (a>0 && b>a){
          String name = msg.substring(6, a);
          int val = msg.substring(b+1).toInt();
          bool on = val!=0;
          if (name=="CLOTH"){ clothOn=on; relaySet(RELAY_CLOTH, clothOn); }
          else if (name=="BLOWER"){ blowerOn=on; relaySet(RELAY_BLOWER, blowerOn); }
          else if (name=="ION"){ ionOn=on; relaySet(RELAY_ION, ionOn); }
        }
      }
        broadcastStatus();   // push fresh state right away


      break;
    }
    default: break;
  }
}

// -------------- Telemetry ---------------------------
void broadcastStatus() {
  String json = "{";
  json += "\"d1\":" + String(dLeft,1)  + ",";
  json += "\"d2\":" + String(dRight,1) + ",";
  json += "\"edgeL\":" + String(edgeL ? "true":"false") + ",";
  json += "\"edgeR\":" + String(edgeR ? "true":"false") + ",";
  json += "\"act\":\"" + String(actName(currentAction)) + "\",";
  json += "\"mode\":\"" + String(mode==AUTO?"auto":"manual") + "\",";
  json += "\"rel\":{\"cloth\":" + String(clothOn?1:0) + ",\"blower\":" + String(blowerOn?1:0) + ",\"ion\":" + String(ionOn?1:0) + "}";
  json += "}";
  ws.broadcastTXT(json);
}

void blinkRelay(uint8_t pin, const char* name){
  Serial.printf("Self-test: %s ON\n", name);
  digitalWrite(pin, LOW);  // ACTIVE-LOW -> ON
  delay(300);
  Serial.printf("Self-test: %s OFF\n", name);
  digitalWrite(pin, HIGH); // OFF
  delay(200);
}
// -------------- Setup / Loop ------------------------
void setup() {
  Serial.begin(115200); delay(200);

  // Pins
  pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
  pinMode(M2_IN1, OUTPUT); pinMode(M2_IN2, OUTPUT);
  motorsStop();

  pinMode(US_L_TRIG, OUTPUT); pinMode(US_L_ECHO, INPUT);
  pinMode(US_R_TRIG, OUTPUT); pinMode(US_R_ECHO, INPUT);

  pinMode(RELAY_CLOTH, OUTPUT);
pinMode(RELAY_BLOWER, OUTPUT);
pinMode(RELAY_ION, OUTPUT);
relaySet(RELAY_CLOTH, false);
relaySet(RELAY_BLOWER, false);
relaySet(RELAY_ION, false);

// ✅ self-test pulses (should click in this order)
blinkRelay(RELAY_CLOTH,  "CLOTH");
blinkRelay(RELAY_BLOWER, "BLOWER");
blinkRelay(RELAY_ION,    "ION");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  uint32_t t0 = millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-t0<8000) { Serial.print("."); delay(400); }
  if (WiFi.status()!=WL_CONNECTED) {
    Serial.println("\nSTA failed, starting AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SolarBot-AP", "12345678");
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("\nIP: "); Serial.println(WiFi.localIP());
    if (MDNS.begin("solarbot")) Serial.println("mDNS: http://solarbot.local/");
  }

  // HTTP
  server.on("/", handleRoot);
  server.on("/relay", [](){
  String name = server.arg("n");  // cloth|blower|ion
  int v = server.arg("v").toInt(); // 0/1 (1=ON)
  bool on = v!=0;
  if (name=="cloth")  { clothOn=on;  relaySet(RELAY_CLOTH,  clothOn); }
  else if (name=="blower"){ blowerOn=on; relaySet(RELAY_BLOWER, blowerOn); }
  else if (name=="ion")   { ionOn=on;   relaySet(RELAY_ION,   ionOn); }
  else { server.send(400,"text/plain","bad name"); return; }
  broadcastStatus();
  server.send(200,"application/json",
    String("{\"ok\":1,\"name\":\"")+name+"\",\"on\":"+String(on?1:0)+"}");
});
  server.begin();

  // WS
  ws.begin();
  ws.onEvent(handleWS);

  Serial.println("Ready.");
}

void loop() {
  server.handleClient();
  ws.loop();

  // Ultrasonic read
  if (millis() - lastUS >= US_READ_INTERVAL) {
    lastUS = millis();
    dLeft  = measureCM(US_L_TRIG, US_L_ECHO);
    dRight = measureCM(US_R_TRIG, US_R_ECHO);
    edgeL  = isCliff(dLeft);
    edgeR  = isCliff(dRight);

    // In MANUAL, immediately stop if an edge is seen
    if (mode == MANUAL) {
      if (edgeL || edgeR) motorsStop();
    }
  }

  // AUTO behavior
  autoTick();

  // Telemetry
  if (millis() - lastTX >= TELEMETRY_INTERVAL) {
    lastTX = millis();
    broadcastStatus();
  }
}
