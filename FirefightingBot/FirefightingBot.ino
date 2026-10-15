/* =========================================================================
 *  ESP32 Firefighting Bot
 *  ---------------------------------------------------------------
 *  Features:
 *    - 3 flame sensors (Left / Middle / Right), digital output
 *    - L298N motor driver, 4 motors paralleled (2 channels, tank-style steering)
 *    - Servo that sweeps the nozzle when extinguishing
 *    - Relay that drives a 9V water pump
 *    - Two modes:
 *        MANUAL : phone connects to ESP32 AP, press-to-hold web controls
 *        AUTO   : drive forward, turn toward side flame, sweep+pump on middle flame
 *    - Safety override: middle flame ALWAYS cuts motors and runs pump+sweep,
 *      even while in manual mode.
 *
 *  Author context: Mohammed Aquib Ansari (LEOBOTICS)
 *  Board: ESP32 Dev Module (Arduino IDE)
 *
 *  Wiring (CHANGE pins to match your actual wiring):
 *      L298N            ESP32
 *      ENA   ->  25   (PWM, left side speed)
 *      IN1   ->  26
 *      IN2   ->  27
 *      IN3   ->  14
 *      IN4   ->  12
 *      ENB   ->  13   (PWM, right side speed)
 *
 *      Flame sensors (digital DO pins only):
 *        Left   DO  -> 34  (input-only OK)
 *        Middle DO -> 35  (input-only OK)
 *        Right  DO -> 32
 *      Power flame sensors from ESP32 3V3 or 5V per your module.
 *
 *      Servo signal -> 18 (use external 5V supply for the servo, common GND)
 *      Relay IN    -> 19 (relay powers 9V pump from its own supply)
 *
 *  Power notes:
 *    - DO NOT power 4 motors through the ESP32 5V rail. Use a separate
 *      battery pack (7-12V) into L298N VIN and share GND with the ESP32.
 *    - 9V pump should run off its own supply via the relay. Share GND.
 *    - A servo under load browns out the 5V rail; give it its own BEC/UBEC
 *      or the 5V pin of L298N ONLY if current is low.
 *
 *  Connect your phone to the SSID "FireBot_AP" (password below),
 *  then open http://192.168.4.1 in a browser.
 * ========================================================================= */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ============================ USER CONFIG =================================

// --- Wi-Fi Access Point ---
static const char*    AP_SSID     = "FireBot_AP";
static const char*    AP_PASSWORD = "firebot123";   // min 8 chars

// --- Flame sensor polarity ---
// Most KY-026 / 3-pin flame modules: DO goes LOW when flame detected.
// If yours is inverted, change this to 0.
#define FLAME_ACTIVE_LOW   1

// --- Pin map (edit to match your wiring) ---
static const int PIN_ENA = 25;
static const int PIN_IN1 = 26;
static const int PIN_IN2 = 27;
static const int PIN_IN3 = 14;
static const int PIN_IN4 = 12;
static const int PIN_ENB = 13;

static const int PIN_FLAME_L = 34;   // input-only pin OK for digital read
static const int PIN_FLAME_M = 35;   // input-only pin OK for digital read
static const int PIN_FLAME_R = 32;

static const int PIN_SERVO = 18;
static const int PIN_RELAY = 19;

// --- Motor behavior ---
static const int      MOTOR_SPEED_FWD  = 200;   // 0-255 PWM
static const int      MOTOR_SPEED_TURN = 220;   // slight boost for turns
static const uint32_t PWM_FREQ_HZ      = 20000; // above audible
static const uint8_t  PWM_RES_BITS     = 8;     // 0..255 duty

// --- Servo sweep ---
static const int SERVO_MIN_DEG      = 30;
static const int SERVO_MAX_DEG      = 150;
static const int SERVO_STEP_DEG     = 5;
static const unsigned long SERVO_STEP_INTERVAL_MS = 25;

// --- Relay polarity (many modules are active-LOW) ---
#define RELAY_ACTIVE_LOW   1

// --- Flame debounce ---
// Sensor must read the same state this many consecutive loops to switch.
static const uint8_t FLAME_DEBOUNCE_COUNT = 3;

// --- Loop timing ---
static const unsigned long LOOP_INTERVAL_MS = 15;

// ============================ END USER CONFIG =============================

// ---- Derived macros ----
#if FLAME_ACTIVE_LOW
  #define FLAME_DETECTED(raw)   ((raw) == LOW)
#else
  #define FLAME_DETECTED(raw)   ((raw) == HIGH)
#endif

#if RELAY_ACTIVE_LOW
  #define RELAY_ON_LEVEL   LOW
  #define RELAY_OFF_LEVEL  HIGH
#else
  #define RELAY_ON_LEVEL   HIGH
  #define RELAY_OFF_LEVEL  LOW
#endif

// ================= Globals =================
WebServer server(80);
Servo     nozzleServo;

enum BotMode : uint8_t { MODE_MANUAL = 0, MODE_AUTO = 1 };
volatile BotMode g_mode = MODE_MANUAL;

// Manual command latest value (set by HTTP handlers).
enum ManualCmd : uint8_t {
  CMD_STOP = 0, CMD_FWD, CMD_BACK, CMD_LEFT, CMD_RIGHT
};
volatile ManualCmd g_manualCmd = CMD_STOP;
volatile bool      g_manualPump = false;  // dedicated pump button

// Debounced flame states
bool g_flameL = false;
bool g_flameM = false;
bool g_flameR = false;

// Raw debounce counters
uint8_t g_cntL = 0, g_cntM = 0, g_cntR = 0;
bool    g_rawPrevL = false, g_rawPrevM = false, g_rawPrevR = false;

// Servo sweep state
int           g_servoPos       = 90;
int           g_servoDir       = +1;
unsigned long g_servoLastStep  = 0;
bool          g_servoSweeping  = false;

unsigned long g_lastLoopTick = 0;

// ================= Logging =================
static void logBoot(const char* msg)   { Serial.print("[BOOT] ");   Serial.println(msg); }
static void logInit(const char* msg)   { Serial.print("[INIT] ");   Serial.println(msg); }
static void logInfo(const char* msg)   { Serial.print("[INFO] ");   Serial.println(msg); }
static void logWarn(const char* msg)   { Serial.print("[WARN] ");   Serial.println(msg); }
static void logState(const char* msg)  { Serial.print("[STATE] ");  Serial.println(msg); }

// ================= Motor control =================
static void motorsStop() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
  ledcWrite(PIN_ENA, 0);
  ledcWrite(PIN_ENB, 0);
}

static void motorsForward(int speed) {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  ledcWrite(PIN_ENA, speed);
  ledcWrite(PIN_ENB, speed);
}

static void motorsBackward(int speed) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
  ledcWrite(PIN_ENA, speed);
  ledcWrite(PIN_ENB, speed);
}

// Tank-style turn: left side reverses, right side forwards (pivot left).
static void motorsTurnLeft(int speed) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  ledcWrite(PIN_ENA, speed);
  ledcWrite(PIN_ENB, speed);
}

static void motorsTurnRight(int speed) {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
  ledcWrite(PIN_ENA, speed);
  ledcWrite(PIN_ENB, speed);
}

// ================= Pump & Servo =================
static void pumpOn()  { digitalWrite(PIN_RELAY, RELAY_ON_LEVEL); }
static void pumpOff() { digitalWrite(PIN_RELAY, RELAY_OFF_LEVEL); }

static void servoStartSweep() {
  if (!g_servoSweeping) {
    g_servoSweeping = true;
    g_servoLastStep = millis();
    logState("Servo sweep STARTED");
  }
}

static void servoStopSweep() {
  if (g_servoSweeping) {
    g_servoSweeping = false;
    g_servoPos = 90;
    nozzleServo.write(g_servoPos);
    logState("Servo sweep STOPPED (centered)");
  }
}

static void servoSweepStep() {
  if (!g_servoSweeping) return;
  const unsigned long now = millis();
  if (now - g_servoLastStep < SERVO_STEP_INTERVAL_MS) return;
  g_servoLastStep = now;

  g_servoPos += g_servoDir * SERVO_STEP_DEG;
  if (g_servoPos >= SERVO_MAX_DEG) { g_servoPos = SERVO_MAX_DEG; g_servoDir = -1; }
  if (g_servoPos <= SERVO_MIN_DEG) { g_servoPos = SERVO_MIN_DEG; g_servoDir = +1; }
  nozzleServo.write(g_servoPos);
}

// ================= Flame reading with debounce =================
static bool debounceRead(int pin, uint8_t& counter, bool& prev) {
  const int raw = digitalRead(pin);
  const bool detected = FLAME_DETECTED(raw);
  if (detected == prev) {
    if (counter < FLAME_DEBOUNCE_COUNT) counter++;
  } else {
    counter = 0;
    prev = detected;
  }
  return (counter >= FLAME_DEBOUNCE_COUNT) ? prev : prev;
}

static void updateFlameStates() {
  const bool newL = debounceRead(PIN_FLAME_L, g_cntL, g_rawPrevL);
  const bool newM = debounceRead(PIN_FLAME_M, g_cntM, g_rawPrevM);
  const bool newR = debounceRead(PIN_FLAME_R, g_cntR, g_rawPrevR);

  if (newL != g_flameL) { g_flameL = newL; Serial.printf("[INFO] Flame LEFT   %s\n", newL ? "DETECTED" : "clear"); }
  if (newM != g_flameM) { g_flameM = newM; Serial.printf("[INFO] Flame MIDDLE %s\n", newM ? "DETECTED" : "clear"); }
  if (newR != g_flameR) { g_flameR = newR; Serial.printf("[INFO] Flame RIGHT  %s\n", newR ? "DETECTED" : "clear"); }
}

// ================= Mode behaviors =================
// Priority override used in BOTH modes: if middle flame, stop driving and
// run pump + sweep until the middle flame clears.
static bool middleFlameOverride() {
  if (!g_flameM) return false;
  motorsStop();
  pumpOn();
  servoStartSweep();
  return true;
}

static void runAutoMode() {
  if (middleFlameOverride()) return;

  // Middle is clear -> no sweep, no pump from auto logic.
  pumpOff();
  servoStopSweep();

  if (g_flameL && !g_flameR) {
    motorsTurnLeft(MOTOR_SPEED_TURN);
  } else if (g_flameR && !g_flameL) {
    motorsTurnRight(MOTOR_SPEED_TURN);
  } else {
    // No side flames OR both sides (ambiguous): drive forward to search.
    motorsForward(MOTOR_SPEED_FWD);
  }
}

static void runManualMode() {
  // Safety override: middle flame wins even during manual control.
  if (middleFlameOverride()) return;

  // Not overriding -> honor current manual command.
  switch (g_manualCmd) {
    case CMD_FWD:   motorsForward(MOTOR_SPEED_FWD);   break;
    case CMD_BACK:  motorsBackward(MOTOR_SPEED_FWD);  break;
    case CMD_LEFT:  motorsTurnLeft(MOTOR_SPEED_TURN); break;
    case CMD_RIGHT: motorsTurnRight(MOTOR_SPEED_TURN);break;
    case CMD_STOP:
    default:        motorsStop();                     break;
  }

  // Manual pump button controls pump + sweep independently of flame.
  if (g_manualPump) {
    pumpOn();
    servoStartSweep();
  } else {
    pumpOff();
    servoStopSweep();
  }
}

// ================= Web UI =================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>FireBot Control</title>
<style>
  :root { --bg:#0e1116; --panel:#1a1f29; --accent:#ff5a3c; --ok:#35d06a; --txt:#f1f3f6; }
  * { box-sizing:border-box; -webkit-user-select:none; user-select:none; -webkit-tap-highlight-color:transparent; }
  html,body { margin:0; padding:0; background:var(--bg); color:var(--txt); font-family:system-ui,Segoe UI,Roboto,sans-serif; height:100%; }
  header { padding:14px 16px; background:var(--panel); display:flex; justify-content:space-between; align-items:center; }
  header h1 { margin:0; font-size:18px; letter-spacing:.5px; }
  .mode { display:flex; gap:8px; }
  .mode button { background:#2a303d; border:0; color:var(--txt); padding:8px 12px; border-radius:8px; font-size:13px; }
  .mode button.active { background:var(--accent); }
  .status { padding:10px 16px; font-size:13px; background:#141820; border-bottom:1px solid #222; }
  .pad { display:grid; grid-template-columns:1fr 1fr 1fr; grid-template-rows:1fr 1fr 1fr; gap:10px; padding:16px; height:60vh; }
  .pad button { font-size:24px; font-weight:600; background:var(--panel); border:0; color:var(--txt); border-radius:14px; box-shadow:inset 0 0 0 1px #2a303d; }
  .pad button:active { background:var(--accent); }
  .pad .fwd { grid-column:2; grid-row:1; }
  .pad .left{ grid-column:1; grid-row:2; }
  .pad .stop{ grid-column:2; grid-row:2; background:#2a303d; }
  .pad .right{grid-column:3; grid-row:2; }
  .pad .back{ grid-column:2; grid-row:3; }
  .pump { margin:0 16px 16px; padding:16px; border-radius:14px; background:var(--panel); display:flex; justify-content:space-between; align-items:center; }
  .pump button { padding:12px 20px; font-size:16px; border:0; border-radius:10px; background:var(--ok); color:#08210f; font-weight:700; }
  .pump button:active { background:var(--accent); color:#fff; }
  .dot { width:10px; height:10px; border-radius:50%; display:inline-block; margin-right:6px; }
  .d-ok { background:var(--ok); }
  .d-fire { background:var(--accent); }
</style>
</head>
<body>
<header>
  <h1>🔥 FireBot</h1>
  <div class="mode">
    <button id="mManual" class="active" onclick="setMode('manual')">Manual</button>
    <button id="mAuto" onclick="setMode('auto')">Auto</button>
  </div>
</header>
<div class="status" id="status">connecting…</div>

<div class="pad" id="pad">
  <button class="fwd"   data-cmd="fwd">▲</button>
  <button class="left"  data-cmd="left">◀</button>
  <button class="stop"  data-cmd="stop">■</button>
  <button class="right" data-cmd="right">▶</button>
  <button class="back"  data-cmd="back">▼</button>
</div>

<div class="pump">
  <div>Pump + Sweep</div>
  <button id="pumpBtn">HOLD TO SPRAY</button>
</div>

<script>
  const send = (p) => fetch(p, {cache:'no-store'}).catch(()=>{});
  let currentCmd = 'stop';

  function press(cmd){
    if (cmd === currentCmd) return;
    currentCmd = cmd;
    send('/cmd?v=' + cmd);
  }
  function release(){
    if (currentCmd === 'stop') return;
    currentCmd = 'stop';
    send('/cmd?v=stop');
  }

  document.querySelectorAll('#pad button').forEach(btn => {
    const cmd = btn.dataset.cmd;
    const down = (e)=>{ e.preventDefault(); press(cmd); };
    const up   = (e)=>{ e.preventDefault(); if (cmd !== 'stop') release(); };
    btn.addEventListener('pointerdown', down);
    btn.addEventListener('pointerup', up);
    btn.addEventListener('pointerleave', up);
    btn.addEventListener('pointercancel', up);
  });

  const pump = document.getElementById('pumpBtn');
  const pDown = (e)=>{ e.preventDefault(); send('/pump?v=1'); };
  const pUp   = (e)=>{ e.preventDefault(); send('/pump?v=0'); };
  pump.addEventListener('pointerdown', pDown);
  pump.addEventListener('pointerup', pUp);
  pump.addEventListener('pointerleave', pUp);
  pump.addEventListener('pointercancel', pUp);

  function setMode(m){
    document.getElementById('mManual').classList.toggle('active', m==='manual');
    document.getElementById('mAuto').classList.toggle('active',   m==='auto');
    send('/mode?v=' + m);
  }

  async function poll(){
    try {
      const r = await fetch('/status', {cache:'no-store'});
      const j = await r.json();
      const dot = (b) => '<span class="dot ' + (b?'d-fire':'d-ok') + '"></span>';
      document.getElementById('status').innerHTML =
        'mode: <b>' + j.mode + '</b> &nbsp; ' +
        dot(j.l) + 'L ' + dot(j.m) + 'M ' + dot(j.r) + 'R' +
        ' &nbsp; pump: <b>' + (j.pump?'ON':'off') + '</b>';
    } catch(e){}
  }
  setInterval(poll, 600); poll();
</script>
</body>
</html>
)HTML";

// ---- Handlers ----
static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleCmd() {
  if (!server.hasArg("v")) { server.send(400, "text/plain", "missing v"); return; }
  const String v = server.arg("v");
  if      (v == "fwd")   g_manualCmd = CMD_FWD;
  else if (v == "back")  g_manualCmd = CMD_BACK;
  else if (v == "left")  g_manualCmd = CMD_LEFT;
  else if (v == "right") g_manualCmd = CMD_RIGHT;
  else                   g_manualCmd = CMD_STOP;
  server.send(200, "text/plain", "ok");
}

static void handlePump() {
  g_manualPump = (server.hasArg("v") && server.arg("v") == "1");
  server.send(200, "text/plain", "ok");
}

static void handleMode() {
  if (!server.hasArg("v")) { server.send(400, "text/plain", "missing v"); return; }
  const String v = server.arg("v");
  if (v == "auto") {
    g_mode = MODE_AUTO;
    logState("Mode -> AUTO");
  } else {
    g_mode = MODE_MANUAL;
    g_manualCmd  = CMD_STOP;
    g_manualPump = false;
    logState("Mode -> MANUAL");
  }
  server.send(200, "text/plain", "ok");
}

static void handleStatus() {
  char buf[128];
  snprintf(buf, sizeof(buf),
           "{\"mode\":\"%s\",\"l\":%d,\"m\":%d,\"r\":%d,\"pump\":%d}",
           (g_mode == MODE_AUTO) ? "auto" : "manual",
           g_flameL ? 1 : 0,
           g_flameM ? 1 : 0,
           g_flameR ? 1 : 0,
           (digitalRead(PIN_RELAY) == RELAY_ON_LEVEL) ? 1 : 0);
  server.send(200, "application/json", buf);
}

static void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

// ================= Setup / Loop =================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  logBoot("FireBot v1.0  (ESP32 @ 115200)");
  logBoot("Modes: MANUAL (web) / AUTO (sensor)");

  // Motor pins
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  // Core 3.x LEDC: single call per pin (freq + resolution), channels are auto-managed.
  if (!ledcAttach(PIN_ENA, PWM_FREQ_HZ, PWM_RES_BITS)) {
    logWarn("ledcAttach failed on PIN_ENA");
  }
  if (!ledcAttach(PIN_ENB, PWM_FREQ_HZ, PWM_RES_BITS)) {
    logWarn("ledcAttach failed on PIN_ENB");
  }
  motorsStop();
  logInit("Motors OK (L298N, 4 paralleled, tank steering)");

  // Flame inputs
  pinMode(PIN_FLAME_L, INPUT);
  pinMode(PIN_FLAME_M, INPUT);
  pinMode(PIN_FLAME_R, INPUT);
  logInit("Flame sensors OK (digital, debounce=3)");

  // Relay
  pinMode(PIN_RELAY, OUTPUT);
  pumpOff();
  logInit("Relay OK (pump off)");

  // Servo
  ESP32PWM::allocateTimer(2);     // avoid timer clash with LEDC
  nozzleServo.setPeriodHertz(50);
  nozzleServo.attach(PIN_SERVO, 500, 2400);
  nozzleServo.write(90);
  g_servoPos = 90;
  logInit("Servo OK (centered at 90 deg)");

  // Wi-Fi AP
  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (!apOk) {
    logWarn("softAP start failed");
  } else {
    IPAddress ip = WiFi.softAPIP();
    Serial.print("[INIT] AP \""); Serial.print(AP_SSID);
    Serial.print("\" at http://"); Serial.println(ip);
  }

  // Routes
  server.on("/",       handleRoot);
  server.on("/cmd",    handleCmd);
  server.on("/pump",   handlePump);
  server.on("/mode",   handleMode);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  logInit("HTTP server listening on :80");

  logInfo("Ready. Connect phone to FireBot_AP, open http://192.168.4.1");
}

void loop() {
  server.handleClient();

  const unsigned long now = millis();
  if (now - g_lastLoopTick >= LOOP_INTERVAL_MS) {
    g_lastLoopTick = now;

    updateFlameStates();

    if (g_mode == MODE_AUTO) runAutoMode();
    else                     runManualMode();
  }

  // Servo sweep runs on its own non-blocking schedule.
  servoSweepStep();
}
