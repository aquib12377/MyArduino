/*****************************************************************************
 *  ESP32 Perfume Dispenser  v2.1  (debug prints + weight echo)
 *  ------------------------------------------------------------
 *  © 2025-06-02  ChatGPT-o3
 *****************************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Ticker.h>

/* ------------- Wi-Fi (STA) -------------------------- */
const char* STA_SSID = "MyProject";
const char* STA_PSK  = "12345678";

/* ------------- GPIO & tuning ------------------------ */
constexpr uint8_t RX2_PIN = 16,  TX2_PIN = 17;
constexpr uint8_t PUMP_R  = 25,  VALVE_R = 26;
constexpr uint8_t STOP_BTN = 27;                 // active-LOW
constexpr bool    ACTIVE_LOW   = true;

constexpr uint32_t SCALE_BAUD  = 1200;           // 7-O-1
constexpr float  OVERSHOOT_G   = 0.10f;
constexpr float  FINAL_TOL_G   = 0.03f;
constexpr uint32_t VALVE_EXTRA = 150;            // ms

/* ------------- state machine ------------------------ */
enum DispState {IDLE,RUN_PUMP,HOLD_VALVE,DONE,EMERGENCY};
volatile DispState state = IDLE;
volatile float targetG = 0.0f, currentG = NAN;
volatile uint32_t valveCutMs = 0;

/* ------------- objects ------------------------------ */
HardwareSerial scale(2);
WebServer server(80);
Ticker pollTicker;           // 1 Hz poll trigger
const size_t   MAX_LINE     = 64;     // longest expected reply
char   lineBuf[MAX_LINE];
uint32_t lastPollMs  = 0;

inline void relay(uint8_t pin,bool on){
  digitalWrite(pin,(on ^ ACTIVE_LOW)?HIGH:LOW);
  Serial.printf("[HW] %-5s %s\n",
                pin==PUMP_R?"PUMP":"VALVE",
                on?"ON":"OFF");
}
inline void pumpOn(){ relay(PUMP_R,true);  }
inline void pumpOff(){relay(PUMP_R,false); }
inline void valveOn(){relay(VALVE_R,true); }
inline void valveOff(){relay(VALVE_R,false);}

void enterSafeState(){
  pumpOff(); valveOff();
  state = EMERGENCY;
  Serial.println("[STATE] -> EMERGENCY");
}

IRAM_ATTR void onStopBtn(){ enterSafeState(); }

void sendPoll()
{
  scale.write(0x1B);      // ESC
  scale.write('P');
  scale.write('\r');      // CR
  scale.write('\n');      // LF
}
void pollISR(){ if(state!=EMERGENCY) sendPoll(); }

/* ——— parse one reply line ————————————————
   Accepts:
        "N     +   806.23 g"
        "S   -  12.0 kg"
        "000123 + 100.0 g"
   Returns true on success. recNo is 0 if none was sent       */
bool parseLine(const char *line, float &grams, uint32_t &recNo)
{
  grams = NAN;
  recNo = 0;

  const char *p = line;

  while (*p && !(isdigit(*p) || *p == '+' || *p == '-'))
    ++p;
  const char *tokenStart = p;
  while (isdigit(*p)) ++p;              // run of digits

  bool possibleRec = (*p == ' ');       // pure digits followed by space?
  if (possibleRec) {
    const char *plusLater = strchr(p, '+');
    if (plusLater) {
      recNo = strtoul(tokenStart, nullptr, 10);
      p = plusLater;                    // jump to the plus that starts weight
    } else {
      p = tokenStart;
    }
  } else {
    p = tokenStart;
  }

  if (*p == '+' || *p == '-') ++p;      // skip sign
  while (isspace(*p)) ++p;              // skip any spaces

  char *endPtr;
  grams = strtof(p, &endPtr);
  return (endPtr != p) && !isnan(grams);
}


const char HTML[] PROGMEM = R"(
<!DOCTYPE html><html><meta name=viewport content='width=device-width'>
<title>Dispenser</title><style>
body{font-family:sans-serif;text-align:center;padding-top:2rem}
#s{font-weight:bold}.run{color:green}.stop{color:red}
button{font-size:1.2rem;padding:.6rem 1.2rem;margin:.4rem}
</style><body><h2>Perfume Dispenser</h2>
<p>Weight: <span id=w>--</span> g</p>
<form onsubmit='go(event)'>
<input id=g type=number step=0.01 min=0 placeholder='grams'>
<button>Dispense</button></form>
<button id=estop style='background:#c00;color:#fff'>EMERGENCY STOP</button>
<p id=s class=stop>IDLE</p>
<script>
const q=s=>document.querySelector(s);
function poll(){
 fetch('/weight').then(r=>r.json()).then(j=>{
   q('#w').textContent=j.g.toFixed(2);
   q('#s').textContent=j.state;
   q('#s').className=j.state=='PUMP'?'run':(j.state=='EMERGENCY'?'stop':'');
   setTimeout(poll,200);
 });}
function go(e){
 e.preventDefault();
 fetch('/start?g='+q('#g').value)
   .then(r=>r.json())
   .then(j=>{ alert(j.msg+'  Target:'+j.target+'  Now:'+j.weight); });}
q('#estop').onclick=_=>{
 fetch('/stop').then(r=>r.json()).then(j=>alert(j.msg+'  Weight:'+j.weight));};
poll();
</script></body></html>)";

void root(){ server.send(200,"text/html",HTML); }

void weight(){
  String j="{\"g\":"+String(currentG,2)+",\"state\":\"";
  switch(state){
    case RUN_PUMP:j+="PUMP";break;
    case HOLD_VALVE:j+="VALVE";break;
    case DONE:j+="DONE";break;
    case EMERGENCY:j+="EMERGENCY";break;
    default:j+="IDLE";
  } j+="\"}";
  server.send(200,"application/json",j);
}

void start(){
  Serial.println("[HTTP] /start");
  if(state==EMERGENCY){ server.send(423,"text/plain","blocked"); return; }
  float g = server.arg("g").toFloat();
  if(g<=0){ server.send(400,"text/plain","bad g"); return; }

  targetG = g;
  state = RUN_PUMP;
  Serial.printf("[STATE] -> RUN_PUMP  target=%.2f\n", targetG);
  pumpOn(); valveOn();

  String json = "{\"msg\":\"Dispensing\",\"target\":"+String(targetG,2)+
                ",\"weight\":"+String(currentG,2)+"}";
  server.send(200,"application/json",json);
}

void stop(){
  Serial.println("[HTTP] /stop");
  enterSafeState();
  String json = "{\"msg\":\"STOPPED\",\"weight\":"+String(currentG,2)+"}";
  server.send(200,"application/json",json);
}

void setup(){
  Serial.begin(115200);

  pinMode(PUMP_R,OUTPUT); pinMode(VALVE_R,OUTPUT);
  pinMode(STOP_BTN,INPUT_PULLUP);
  attachInterrupt(STOP_BTN,onStopBtn,FALLING);
  pumpOff(); valveOff();

  scale.begin(SCALE_BAUD, SERIAL_7O1, RX2_PIN, TX2_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PSK);
  Serial.printf("[NET] Joining %s …\n", STA_SSID);

  for(uint32_t t=millis(); WiFi.status()!=WL_CONNECTED && millis()-t<15000;){
      delay(200); Serial.print('.');
  }
  if(WiFi.status()==WL_CONNECTED)
      Serial.printf("\n[NET] Connected. IP %s\n", WiFi.localIP().toString().c_str());
  else Serial.println("\n[NET] FAILED");

  server.on("/",root);
  server.on("/weight",weight);
  server.on("/start",start);
  server.on("/stop",stop);
  server.begin();
  Serial.println("[WEB] Listening on port 80");

  pollTicker.attach_ms(1000,pollISR);
}

static char buf[64]; size_t idx=0;

void loop(){
  uint32_t now = millis();
  if (now - lastPollMs >= 150) {
    sendPoll();
    lastPollMs = now;
  }

  while (scale.available()) {
    char c = scale.read();

    if (idx >= MAX_LINE - 1) idx = 0;     // protect buffer

    if (c == '\r' || c == '\n') {         // <CR>/<LF> terminator
      if (idx == 0) continue;             // ignore empty CR/LF
      lineBuf[idx] = '\0';                // C-string
      idx = 0;

      Serial.print(F("[RAW] ")); Serial.println(lineBuf);

      float weight;
      uint32_t recNo;
      if (parseLine(lineBuf, weight, recNo)) {
        Serial.print(F("[DATA] "));
        if (recNo) { Serial.print(F("rec=")); Serial.print(recNo); Serial.print(' '); }
        Serial.print(weight, 3); Serial.println(F(" g"));
      } else {
        Serial.println(F("[WARN] parse failed"));
      }
    }
    else {
      lineBuf[idx++] = c;                 // keep filling buffer
    }
  }

  switch(state){
    case RUN_PUMP:
      if(!isnan(weight)&&weight>=targetG-OVERSHOOT_G){
        pumpOff(); valveCutMs=millis()+VALVE_EXTRA; state=HOLD_VALVE;
        Serial.println("[STATE] -> HOLD_VALVE");
      } break;
    case HOLD_VALVE:
      if(millis()>=valveCutMs){
        valveOff(); state=DONE; Serial.println("[STATE] -> DONE");
      } break;
    default: break;
  }
  server.handleClient();
}
