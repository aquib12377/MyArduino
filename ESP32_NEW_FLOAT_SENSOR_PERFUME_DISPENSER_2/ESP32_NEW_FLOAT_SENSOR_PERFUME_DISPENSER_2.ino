/*****************************************************************
  ESP32 DISPENSER – WebServer + WebSockets   (direct-start edition)
*****************************************************************/
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

/* ---------- user settings ----------------------------------- */
constexpr char  WIFI_SSID[] = "MyProject";
constexpr char  WIFI_PASS[] = "12345678";

constexpr uint8_t  FLOW_PIN        = 13;
constexpr uint8_t  PUMP_RELAY_PIN  = 25;      // LOW = ON
constexpr uint8_t  VALVE_RELAY_PIN = 26;      // LOW = ON

constexpr uint16_t PULSES_PER_LITRE = 1750;
constexpr uint32_t WS_PUSH_MS       = 100;    // browser status rate
constexpr uint32_t SERIAL_PUSH_MS   = 500;    // CLI status rate
/* ------------------------------------------------------------- */

/* ---------- globals / state machine -------------------------- */
enum Phase : uint8_t {IDLE, DISPENSE};
volatile Phase     phase        = IDLE;

volatile uint32_t  pulseCount   = 0;           // updated in ISR
uint32_t           targetPulses = 0;

uint32_t nextWsMs  = 0;
uint32_t nextSerMs = 0;

/* ---------- Wi-Fi / HTTP / WS objects ----------------------- */
WebServer        server(80);
WebSocketsServer ws(81);                        // ws://<ip>:81/

/* ---------- ISR --------------------------------------------- */
void IRAM_ATTR onPulse() { pulseCount++; }

/* ---------- GPIO helpers ------------------------------------ */
inline void pumpON () { digitalWrite(PUMP_RELAY_PIN , LOW);  }
inline void pumpOFF() { digitalWrite(PUMP_RELAY_PIN , HIGH); }
inline void valveON (){ digitalWrite(VALVE_RELAY_PIN, LOW);  }
inline void valveOFF(){ digitalWrite(VALVE_RELAY_PIN, HIGH); }

/* ---------- JSON status push -------------------------------- */
void sendStatus()
{
  static char buf[128];
  StaticJsonDocument<128> doc;
  doc["phase"]  = static_cast<uint8_t>(phase);
  doc["pulses"] = pulseCount;
  doc["target"] = targetPulses;
  doc["litres"] = pulseCount / (float)PULSES_PER_LITRE;
  size_t len = serializeJson(doc, buf);
  ws.broadcastTXT(buf, len);
}

/* ---------- dispense control -------------------------------- */
void startDispense(uint32_t pulses)
{
  if (phase != IDLE || pulses == 0) return;

  targetPulses = pulses;

  valveON();
  pumpON();

  noInterrupts(); pulseCount = 0; interrupts();   // start from zero
  phase = DISPENSE;

  Serial.printf("▶ Start: %lu pulses  (≈ %.3f L)\n",
                pulses, pulses / (float)PULSES_PER_LITRE);
  sendStatus();
}

void stopDispense()
{
  pumpOFF();
  valveOFF();
  phase = IDLE;

  Serial.printf("✔ Done : %lu pulses  (%.3f L)\n",
                pulseCount, pulseCount / (float)PULSES_PER_LITRE);
  sendStatus();
}

/* ---------- WebSocket callback ------------------------------ */
void wsEvent(uint8_t, WStype_t t, uint8_t *p, size_t l)
{
  if (t != WStype_TEXT) return;
  StaticJsonDocument<64> doc;
  if (!deserializeJson(doc, p, l) && doc.containsKey("pulses"))
      startDispense(doc["pulses"].as<uint32_t>());
}

/* ---------- serve minimal page ------------------------------ */
const char INDEX_HTML[] PROGMEM = R"html(
<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Dispenser</title>
<style>body{font-family:Arial;background:#111;color:#eee;text-align:center;padding-top:40px}
input,button{font-size:1rem;padding:6px}button{margin-left:8px}</style></head><body>
<h2>ESP32 Pulse Dispenser</h2>
<input id='p' type='number' placeholder='pulses'><button onclick='go()'>START</button>
<pre id='s'>Connecting…</pre>
<script>
let ws,ph=["IDLE","DISPENSE"];
const $=id=>document.getElementById(id);
function go(){let n=parseInt($('p').value||0);
  if(ws&&ws.readyState===1&&n>0) ws.send(JSON.stringify({pulses:n}));}
(function connect(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen =_=>$('s').textContent='Connected';
  ws.onmessage=e=>{let j=JSON.parse(e.data);
    $('s').textContent=`${ph[j.phase]}\n${j.pulses}/${j.target} pulses\n${j.litres.toFixed(3)} L`;};
  ws.onclose =_=>{$('s').textContent='Disconnected';setTimeout(connect,2000);}
})();
</script></body></html>)html";

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

/* ---------- setup ------------------------------------------- */
void setup()
{
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onPulse, FALLING);

  pinMode(PUMP_RELAY_PIN , OUTPUT);
  pinMode(VALVE_RELAY_PIN, OUTPUT);
  pumpOFF(); valveOFF();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print('.'); }
  Serial.printf("\nIP  : %s\n", WiFi.localIP().toString().c_str());

  server.on("/", handleRoot);
  server.begin();
  ws.begin(); ws.onEvent(wsEvent);

  Serial.printf("Open: http://%s\n", WiFi.localIP().toString().c_str());
}

/* ---------- main loop --------------------------------------- */
void loop()
{
  server.handleClient();
  ws.loop();

  uint32_t now = millis();

  /* ------------ dispense finished? ---------------- */
  if (phase == DISPENSE && pulseCount >= targetPulses) stopDispense();

  /* ------------ timed reports --------------------- */
  if (now - nextWsMs >= WS_PUSH_MS) { nextWsMs = now; sendStatus(); }
  if (phase == DISPENSE && now - nextSerMs >= SERIAL_PUSH_MS)
  {
    nextSerMs = now;
    Serial.printf("… %lu / %lu pulses  (%.3f L)\n",
                  pulseCount, targetPulses,
                  pulseCount / (float)PULSES_PER_LITRE);
  }
}
