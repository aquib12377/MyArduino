/*****************************************************************
  TRAIN-BOT – drives 2-wheel robot, hosts 2-rail HTML UI,
  auto-pushes bitmap on connect (CSS width bug fixed)
*****************************************************************/
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <driver/ledc.h>  // <── add just after other #includes

/* ---------- hotspot credentials ---------------------------- */
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";

/* ---------- hardware pins ---------------------------------- */
#define IN1 13
#define IN2 12
#define IN3 14
#define IN4 27
#define BUZZ 26
constexpr uint32_t SEGMENT_TIME_MS = 2500;

/* ---------- L293D enable-pins (PWM) ------------------------ */
#define ENA 5   // choose any two free PWM-capable pins
#define ENB 18  // (GPIO 5 & 18 are common & safe on ESP32)

/* ---------- PWM setup parameters --------------------------- */
/* ---------- L293D enable-pins & PWM ------------------------ */
#define ENA 5  // any two PWM-capable GPIOs
#define ENB 18

#define PWM_FREQ 10000  // 10 kHz
#define PWM_RES LEDC_TIMER_8_BIT
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_TIMER LEDC_TIMER_0
#define PWM_CH_A LEDC_CHANNEL_0
#define PWM_CH_B LEDC_CHANNEL_1

uint8_t motorSpeed = 210;  // 0-255 (because 8-bit timer)

constexpr uint8_t NUM_SENSORS = 5;   // TC-0 … TC-4
constexpr uint8_t LAST_FWD_IDX = 4;  // TC-4  (forward terminal)
constexpr uint8_t LAST_REV_IDX = 0;  // TC-0  (reverse terminal)

/* true when sensor i reports occupied */



/* ---------- globals ---------------------------------------- */
WebServer server(80);
WebSocketsServer ws(81);

bool isForward = true;

volatile uint8_t sensorBitmap = 0;
volatile bool trainRunning = false;
uint8_t trainPos = 0;
uint32_t segDeadline = 0;
constexpr uint8_t NUM_BLOCKS = 5;  // TC0 … TC4
constexpr int8_t FORWARD_DIR = +1;
constexpr int8_t REVERSE_DIR = -1;
/* ---------- WebSocket event – push current bitmap ---------- */
void onWsEvent(uint8_t id, WStype_t type, uint8_t*, size_t) {
  if (type == WStype_CONNECTED) {
    char msg[3];
    sprintf(msg, "%02X", sensorBitmap);
    ws.sendTXT(id, msg);
  }
}
inline bool tcHigh(uint8_t i) {
  return sensorBitmap & (1 << i);
}
/* ---------- ESP-NOW receive callback ----------------------- */
inline void pushBitmap(uint8_t bits) {
  sensorBitmap = bits;
  char msg[3];
  sprintf(msg, "%02X", bits);
  Serial.println(msg);
  ws.broadcastTXT(msg, 2);
}

#if ESP_IDF_VERSION_MAJOR >= 5
void onReceive(const esp_now_recv_info_t*, const uint8_t* d, int l) {
  if (l == 5) pushBitmap(d[0]);
}
#else
void onReceive(const uint8_t*, const uint8_t* d, int l) {
  if (l == 5) pushBitmap(d[0]);
}
#endif

/* ---------- motor helpers ---------------------------------- */
inline void setDutyAll(uint32_t duty) {
  ledc_set_duty(LEDC_MODE, PWM_CH_A, duty);
  ledc_update_duty(LEDC_MODE, PWM_CH_A);
  ledc_set_duty(LEDC_MODE, PWM_CH_B, duty);
  ledc_update_duty(LEDC_MODE, PWM_CH_B);
}

void motorsForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  setDutyAll(motorSpeed);
}

void motorsReverse() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  setDutyAll(motorSpeed);
}

void motorsStop() {
  setDutyAll(0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

/* ---------- HTML page (CSS width added) -------------------- */
const char HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Rail Track Monitor</title>
<style>
  body{font-family:sans-serif;background:#111;color:#eee;text-align:center}
  .signal-container{
    display:flex;align-items:center;justify-content:center;gap:16px;
    width:80%;margin:20px auto;
  }
  .signal-btn{
    width:32px;height:32px;border:none;border-radius:50%;cursor:pointer;
    transition:background .2s;
  }
  .red  {background:#c00;}
  .green{background:#0c0;}
  #track{flex:1}
  .rail{display:flex;margin:4px 0}
  .seg{flex:1;margin:2px;height:20px;background:#444;border-radius:4px;
       transition:background .2s}
  button.control{
    margin-top:18px;padding:12px 28px;font-size:20px;border-radius:8px;border:none;
  }
</style>
</head>
<body>
<h2>Railway Track Status</h2>

<div class="signal-container">
  <h3 style="color:white;">Signal-02</h3>
  <button id="sig2" class="signal-btn"></button>

  <div id="track">
    <div class="rail">
      <div id="t0" class="seg"><h3 style="color:white;margin-top:70px;">TC1</h3></div>
      <div id="t1" class="seg"><h3 style="color:white;margin-top:70px;">TC2</h3></div>
      <div id="t2" class="seg"><h3 style="color:white;margin-top:70px;">TC3</h3></div>
      <div id="t3" class="seg"><h3 style="color:white;margin-top:70px;">TC4</h3></div>
    </div>
    <div class="rail">
      <div id="b0" class="seg" style="display:none"></div>
      <div id="b1" class="seg" style="display:none"></div>
      <div id="b2" class="seg" style="display:none"></div>
      <div id="b3" class="seg" style="display:none"></div>
    </div>
  </div>

  <button id="sig1" class="signal-btn"></button>
  <h3 style="color:white;">Signal-01</h3>
</div>

<button class="control" onclick="fetch('/train');" style="margin-top:70px;">TRAIN</button>
<h2 style="margin-top:70px;">OCC – Operation Control Center</h2>

<script>
/* ---------- constants -------------------------------------- */
const NUM_TRACKS  = 4;   // TC1-TC4 are shown
const NUM_SENSORS = 5;   // TC0-TC4 reported by the MCU
const LAST_SENSOR = 4;   // index of the “exit” sensor

/* ---------- DOM shortcuts ---------------------------------- */
const topSegs=[t0,t1,t2,t3], botSegs=[b0,b1,b2,b3];
const btn1=document.getElementById('sig1');   // Signal-01 (right)
const btn2=document.getElementById('sig2');   // Signal-02 (left)

/* ---------- signal states ---------------------------------- */
let state1='red', state2='red';
function updateSignalButtons(){
  btn1.className='signal-btn '+state1;
  btn2.className='signal-btn '+state2;
}

/* ---------- live sensor bits + marker ---------------------- */
const occ=new Array(NUM_SENSORS).fill(false); // raw sensor flags
let markedIndex=null;                         // block that stays red

/* helper: true when sensor i is HIGH */
const sensorHigh = i => occ[i];

/* ---------- colour resolver -------------------------------- */
function colourFor(trackIdx){
  if(trackIdx===markedIndex)        return '#c00';        // sticky red
  if(sensorHigh(trackIdx))          return '#c00';        // live red
  if(state1==='red' && state2==='red') return '#ff0';     // caution
  return '#0f0';                                        // clear
}

/* ---------- repaint everything ----------------------------- */
function paint(){
  updateSignalButtons();
  for(let i=0;i<NUM_TRACKS;i++){
    const c=colourFor(i);
    [topSegs[i],botSegs[i]].forEach(seg=>seg.style.background=c);
  }
}

/* ---------- WebSocket listener ----------------------------- */
const ws=new WebSocket('ws://'+location.hostname+':81');
ws.onmessage = ({data})=>{
  const bits=parseInt(data,16);

  /* refresh raw sensor array (5 bits) */
  for(let i=0;i<NUM_SENSORS;i++){
    occ[i]=Boolean(bits&(1<<i));
  }

  /* find first active sensor, if any */
  let active=null;
  for(let i=0;i<NUM_SENSORS;i++){
    if(occ[i]){ active=i; break; }
  }

  if(active!==null){
    if(active===LAST_SENSOR){
      /* train has exited → clear the sticky red */
      markedIndex=null;
    }else if(active!==markedIndex){
      /* move sticky red to the newly-hit block (0-3) */
      markedIndex=active;
    }
  }
  paint();
};

/* ---------- button handlers -------------------------------- */
btn1.addEventListener('click', ()=>{
  state1 = (state1==='red') ? 'green' : 'red';
  /*  right button (Signal-01) green → REVERSE  */
  if(state1 === 'green') {
      state2 = 'red';
      fetch('/reverse').catch(()=>{});
      markedIndex = null;            // optional UI reset
  }
  paint();
});

btn2.addEventListener('click', ()=>{
  state2 = (state2==='red') ? 'green' : 'red';
  /*  left button (Signal-02) green → FORWARD  */
  if(state2 === 'green') {
      state1 = 'red';
      fetch('/forward').catch(()=>{});
      markedIndex = null;            // optional UI reset
  }
  paint();
});


/* ---------- first render ----------------------------------- */
paint();
</script>

</body>
</html>
)HTML";



/* ---------- HTTP handlers ---------------------------------- */
void handleRoot() {
  server.send_P(200, "text/html", HTML);
}
void handleTrain() {
  trainPos = isForward ? 0 : LAST_FWD_IDX;   // 0 or 4
  digitalWrite(BUZZ, LOW);                   // silence buzzer
  trainRunning = true;
  server.send(200, "text/plain", "OK");
}

void handleForward() {
  isForward = true;
  server.send(200, "text/plain", "OK");
}
void handleReverse() {
  isForward = false;
  server.send(200, "text/plain", "OK");
}


/* ---------- setup ------------------------------------------ */
void setup() {
  Serial.begin(115200);
  for (int p : { IN1, IN2, IN3, IN4 }) pinMode(p, OUTPUT);

  ledc_timer_config_t tcfg = {
    .speed_mode = LEDC_MODE,
    .duty_resolution = PWM_RES,
    .timer_num = LEDC_TIMER,
    .freq_hz = PWM_FREQ,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

  /* -------- LEDC channel A (ENA) -------------------------------- */
  ledc_channel_config_t chA = {
    .gpio_num = ENA,
    .speed_mode = LEDC_MODE,
    .channel = PWM_CH_A,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ESP_ERROR_CHECK(ledc_channel_config(&chA));

  /* -------- LEDC channel B (ENB) -------------------------------- */
  ledc_channel_config_t chB = chA;  // clone then edit
  chB.gpio_num = ENB;
  chB.channel = PWM_CH_B;
  ESP_ERROR_CHECK(ledc_channel_config(&chB));

  pinMode(BUZZ, OUTPUT);
  digitalWrite(BUZZ, LOW);
  motorsStop();

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print('.');
  }
  Serial.printf("\n[WiFi] IP=%s  CH=%d  MAC=%s\n",
                WiFi.localIP().toString().c_str(),
                WiFi.channel(),
                WiFi.macAddress().c_str());

  ESP_ERROR_CHECK(esp_now_init());
  esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t p{};
  memset(&p, 0xFF, 6);
  p.channel = WiFi.channel();
  p.encrypt = false;
  ESP_ERROR_CHECK(esp_now_add_peer(&p));

  server.on("/", handleRoot);
  server.on("/train", handleTrain);
  server.on("/forward", handleForward);
  server.on("/reverse", handleReverse);
  server.on("/signal", HTTP_GET, []() {
    String rail = server.arg("rail");    // "1" or "2"
    String state = server.arg("state");  // "stop" or "go"
    Serial.printf("[SIG] Rail %s -> %s\n", rail.c_str(), state.c_str());
    // TODO: hook into your logic: e.g., set digital pins, update variables, etc.
    server.send(200, "text/plain", "OK");
  });
  server.begin();
  ws.begin();
  ws.onEvent(onWsEvent);

  Serial.println("[READY] open http://" + WiFi.localIP().toString());
}

/* ---------- main loop -------------------------------------- */
void loop() {
  server.handleClient();
  ws.loop();

  static bool motorsActive = false;  // remember if we already
                                     // started the motors

  if (trainRunning)
{
  const int8_t dir      = isForward ? +1 : -1;         // +1 → TC0→4
  uint8_t      nextIdx  = trainPos + dir;              // block engine will enter
  uint8_t      lookIdx  = nextIdx + dir;               // first “gap” beyond that

  /* ----------- start motors once -------------------------------- */
  if (isForward) motorsForward();
  else           motorsReverse();

  /* ----------- advance pointer when nose hits next sensor ------- */
  if (nextIdx < NUM_SENSORS && tcHigh(nextIdx))
      trainPos = nextIdx;                              // confirmed entry

  /* ----------- obstacle detection (non-contiguous red) ---------- */
  bool obstacleAhead = false;
  for (int8_t j = lookIdx;
       j >= 0 && j < NUM_SENSORS;
       j += dir)
  {
      if (tcHigh(j)) { obstacleAhead = true; break; }
  }

  if (obstacleAhead) {
      motorsStop();  digitalWrite(BUZZ, HIGH);
      trainRunning = false;
      Serial.println("[TRAIN] STOP – obstacle");
      return;                                         // done for this loop()
  }

  /* ----------- arrival at end of section ------------------------ */
  bool arrived =
        ( isForward && trainPos >= LAST_FWD_IDX && tcHigh(LAST_FWD_IDX)) ||
        (!isForward && trainPos <= LAST_REV_IDX && tcHigh(LAST_REV_IDX));

  if (arrived) {
      motorsStop();
      trainRunning = false;
      Serial.println("[TRAIN] Arrived.");
  }
}
}
