// ESP32 Pump Controller – time‑based dosing + manual reverse button
// Author: ChatGPT (o3) – April 13 2025 (rev‑E: UI “Reverse Now” button)
// -----------------------------------------------------------------------------
// LIBRARIES: WiFi, WebServer, LiquidCrystal_I2C, ArduinoJson (v7+)
// -----------------------------------------------------------------------------
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

// ===================== USER CONFIG ==========================================
const char *SSID     = "MyProject";
const char *PASSWORD = "12345678";
const uint16_t HTTP_PORT = 80;

// GPIOs
constexpr gpio_num_t PIN_RELAY_FWD = GPIO_NUM_27;
constexpr gpio_num_t PIN_RELAY_REV = GPIO_NUM_26;

constexpr uint32_t DEBUG_MS = 1000;   // serial debug cadence

LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(HTTP_PORT);

struct PumpState {
  bool active      = false;   // a forward cycle is running
  bool priming     = false;
  bool dispensing  = false;
  bool reverse     = false;   // currently reversing (manual or cycle)
  uint32_t primeEndMs  = 0;
  uint32_t fwdEndMs    = 0;
  uint32_t revEndMs    = 0;
  float     targetMl   = 0;
} pump;

// user‑tunable timings (ms)
uint32_t primeMs   = 2000;   // priming delay
uint32_t reverseMs = 10000;  // reverse duration
uint32_t calib10Ms = 1500;   // ms to pump 10 mL forward

// ----------------------- RELAY CONTROL --------------------------------------
void setRelays(bool fwd, bool rev){
  digitalWrite(PIN_RELAY_FWD, fwd?LOW:HIGH);
  digitalWrite(PIN_RELAY_REV, rev?LOW:HIGH);
  Serial.printf("[RELAYS] FWD=%s REV=%s\n", fwd?"ON":"OFF", rev?"ON":"OFF");
}
inline void stopPump(){ setRelays(false,false);} 
inline void startForward(){ Serial.println("[PUMP] >>> FORWARD"); setRelays(true,false);} 
inline void startReverse(){ Serial.println("[PUMP] <<< REVERSE"); setRelays(false,true);} 

// ----------------------- LCD -------------------------------------------------
void lcdMsg(const char* l1,const char* l2){ lcd.setCursor(0,0); lcd.print(l1); lcd.setCursor(0,1); lcd.print(l2); }

// ----------------------- WEB UI ---------------------------------------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Smart Pump</title><style>:root{--p:#007aff;--bg:#f7f9fc;font-family:system-ui,sans-serif}*{box-sizing:border-box;margin:0;padding:0}body{background:var(--bg);display:flex;flex-direction:column;min-height:100vh;color:#111}header{background:var(--p);color:#fff;padding:1rem;text-align:center;font-size:1.25rem;box-shadow:0 2px 6px rgba(0,0,0,.15)}main{flex:1;display:flex;flex-direction:column;align-items:center;padding:1rem;gap:1.2rem}.controls{width:100%;max-width:460px;display:flex;flex-wrap:wrap;gap:.5rem}.controls input{flex:1 1 130px;padding:.8rem;border:1px solid #ccc;border-radius:.8rem;font-size:1rem}.controls button{flex:1 1 100%;padding:.8rem;border:none;border-radius:.8rem;background:var(--p);color:#fff;font-size:1rem;cursor:pointer}.dual-btn{display:flex;gap:.5rem;width:100%}.dual-btn button{flex:1 1 50%}.status{text-align:center;font-size:.95rem;margin-top:1rem}@media(min-width:600px){main{padding:2rem}}</style></head><body><header>Smart Pump (time‑based)</header><main><section class="controls"><input id="amt" type="number" min="1" placeholder="Target mL"><input id="prime" type="number" min="0" value="2" placeholder="Prime s"><input id="cal" type="number" min="0.1" step="0.1" value="1.5" placeholder="sec/10 mL" title="Calibration"><input id="rev" type="number" min="1" value="10" placeholder="Reverse s"><div class="dual-btn"><button id="go">Start Cycle</button><button id="revNow">Reverse Now</button></div></section><div class="status" id="status">Idle</div></main><script>window.addEventListener('DOMContentLoaded',()=>{const $=id=>document.getElementById(id),status=$('status');$('go').onclick=async()=>{const ml=parseFloat($('amt').value),prime=parseFloat($('prime').value)||0,cal=parseFloat($('cal').value)||1.5,rev=parseFloat($('rev').value)||10;if(!ml)return alert('Enter volume');await fetch('/pour',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ml,prime,cal,rev})});};$('revNow').onclick=async()=>{const rev=parseFloat($('rev').value)||10;await fetch('/reverse',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rev})});};setInterval(async()=>{const r=await fetch('/status');const j=await r.json();status.textContent=j.msg;},500);});</script></body></html>
)rawliteral";

// ----------------------- WEB ROUTES ----------------------------------------
void setupWeb(){
  server.on("/",HTTP_GET,[]{ server.send_P(200,"text/html",INDEX_HTML); });

  // start forward cycle
  server.on("/pour",HTTP_POST,[](){
    if(!server.hasArg("plain")){ server.send(400,"text/plain","Bad Request"); return; }
    StaticJsonDocument<200> doc; if(deserializeJson(doc,server.arg("plain"))){ server.send(400,"text/plain","Bad JSON"); return; }

    pump.targetMl = doc["ml"].as<float>();
    primeMs   = doc.containsKey("prime") ? (uint32_t)(doc["prime"].as<float>()*1000UL) : primeMs;
    calib10Ms = doc.containsKey("cal")   ? (uint32_t)(doc["cal"].as<float>()*1000UL)   : calib10Ms;
    reverseMs = doc.containsKey("rev")   ? (uint32_t)(doc["rev"].as<float>()*1000UL)   : reverseMs;

    uint32_t fwdMs = (uint32_t)((pump.targetMl/10.0f)*calib10Ms);
    uint32_t now=millis();
    pump.active=true; pump.priming=primeMs>0; pump.dispensing=false; pump.reverse=false;
    pump.primeEndMs=now+primeMs; pump.fwdEndMs=pump.primeEndMs+fwdMs; pump.revEndMs=pump.fwdEndMs+reverseMs;
    Serial.printf("[CYCLE] %.1f mL prime=%lu fwd=%lu rev=%lu (cal %.1f)\n", pump.targetMl, primeMs,fwdMs,reverseMs,(float)calib10Ms);
    startForward();
    server.send(200,"application/json","{}");
  });

  // manual reverse request
  server.on("/reverse",HTTP_POST,[](){
    if(!server.hasArg("plain")){ server.send(400,"text/plain","Bad Request"); return; }
    StaticJsonDocument<64> doc; deserializeJson(doc,server.arg("plain"));
    reverseMs = doc.containsKey("rev") ? (uint32_t)(doc["rev"].as<float>()*1000UL) : reverseMs;
    uint32_t now=millis();
    stopPump();
    startReverse();
    pump.active=false;       // not a forward cycle
    pump.priming=false; pump.dispensing=false; pump.reverse=true;
    pump.revEndMs=now+reverseMs;
    Serial.printf("[MANUAL] Reverse for %lu ms\n", reverseMs);
    server.send(200,"application/json","{}");
  });

  // status
  server.on("/status",HTTP_GET,[](){
    StaticJsonDocument<120> doc; String msg="Idle";
    if(pump.reverse) msg="Reverse flushing…"; else if(pump.dispensing) msg="Dispensing…"; else if(pump.priming) msg="Priming…"; else if(pump.active) msg="Waiting…";
    doc["msg"]=msg; String json; serializeJson(doc,json); server.send(200,"application/json",json);
  });

  server.begin(); Serial.println("[WEB] Server started");
}

// ----------------------- SETUP ---------------------------------------------
void setup(){ Serial.begin(115200); pinMode(PIN_RELAY_FWD,OUTPUT); pinMode(PIN_RELAY_REV,OUTPUT); stopPump(); lcd.begin(); lcd.backlight(); lcd.clear(); lcd.print("Connecting WiFi"); WiFi.begin(SSID,PASSWORD); while(WiFi.status()!=WL_CONNECTED){ delay(500); Serial.print('.'); } lcd.clear(); lcd.print(WiFi.localIP()); setupWeb(); Serial.println("\n[INIT] Ready"); }

// ----------------------- LOOP ----------------------------------------------
void loop(){ server.handleClient(); uint32_t now=millis();
  // forward cycle state machine
  if(pump.active){
    if(pump.priming && now>=pump.primeEndMs){ pump.priming=false; pump.dispensing=true; Serial.println("[PRIME] Done → dispensing"); }
    if(pump.dispensing && now>=pump.fwdEndMs){ pump.dispensing=false; startReverse(); pump.reverse=true; Serial.println("[FWD] Target time reached → reverse"); pump.revEndMs=now+reverseMs; }
  }
  // reverse completion (both manual and cycle)
  if(pump.reverse && now>=pump.revEndMs){ stopPump(); pump.reverse=false; pump.active=false; Serial.println("[REVERSE] Complete"); }

  static uint32_t dbg=0; if(now-dbg>=DEBUG_MS){ dbg=now; Serial.printf("[DBG] state=%s time=%lu\n", pump.reverse?"REV":pump.dispensing?"FWD":pump.priming?"PRIME":"IDLE", now); }
  delay(20);
}
