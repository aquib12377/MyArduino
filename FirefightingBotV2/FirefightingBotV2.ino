/* =========================================================================
 *  ESP32 Firefighting Bot v2  (2-wheel + HC-SR04)
 *  ---------------------------------------------------------------
 *  Hardware:
 *    - ESP32 Dev Module
 *    - L298N motor driver (2 motors, differential drive)
 *    - 3x flame sensors (KY-026 digital, configurable polarity)
 *    - HC-SR04 ultrasonic obstacle sensor
 *    - Servo (nozzle sweep)
 *    - Active-LOW relay module -> 9V water pump
 *
 *  Modes:
 *    MANUAL : phone-driven via web (press-and-hold).
 *             Auto-stop if obstacle < 20cm while pressing forward.
 *             Flame detection STILL overrides (stops + pumps + sweeps).
 *    AUTO   : bot idle until a flame is seen.
 *             Side flame  -> turn that way until flame reaches middle.
 *             Middle flame -> stop + pump + sweep.
 *             Middle flame + close obstacle -> reverse to safe distance
 *                                              then pump + sweep.
 *             (When no flame at all, no wandering. Stays put.)
 *             Obstacle while driving -> stop, reverse, scan with servo,
 *                                        pick the clearer side to turn.
 *
 *  Web UI:
 *    Connect phone to "FireBot2_AP" (password below), open http://192.168.4.1
 *    Press-and-hold directional buttons, press-and-hold pump button.
 *
 *  Author context: Mohammed Aquib Ansari (LEOBOTICS)
 *  Target: ESP32 Arduino core 3.x  (uses new ledcAttach API)
 *
 *  Wiring (change pins to match your actual wiring):
 *      L298N            ESP32
 *      ENA   ->  25   (PWM, left motor speed)
 *      IN1   ->  26
 *      IN2   ->  27
 *      IN3   ->  14
 *      IN4   ->  12
 *      ENB   ->  13   (PWM, right motor speed)
 *
 *      Flame sensors (digital DO):
 *        Left   -> 34
 *        Middle -> 35
 *        Right  -> 32
 *
 *      HC-SR04:
 *        TRIG   -> 5
 *        ECHO   -> 4         (through voltage divider if module is 5V)
 *
 *      Servo  signal -> 18   (external 5V supply for servo recommended)
 *      Relay  IN     -> 19   (active-LOW)
 *
 *  Power:
 *    - Separate motor battery into L298N VIN (7-12V). Common GND with ESP32.
 *    - Pump on its own supply through relay. Common GND.
 *    - Prefer separate 5V BEC for servo under load.
 * ========================================================================= */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ============================ USER CONFIG =================================

// --- Wi-Fi Access Point ---
static const char*    AP_SSID     = "FireBot2_AP";
static const char*    AP_PASSWORD = "firebot123";   // 8+ chars

// --- Flame polarity (change if your module is inverted) ---
#define FLAME_ACTIVE_LOW   1

// --- Pin map ---
static const int PIN_ENA = 25;
static const int PIN_IN1 = 14;
static const int PIN_IN2 = 12;
static const int PIN_IN3 = 26;
static const int PIN_IN4 = 27;
static const int PIN_ENB = 13;

static const int PIN_FLAME_L = 34;   // input-only OK
static const int PIN_FLAME_M = 35;   // input-only OK
static const int PIN_FLAME_R = 32;

static const int PIN_TRIG = 5;
static const int PIN_ECHO = 18;

static const int PIN_SERVO = 4;
static const int PIN_RELAY = 33;

// --- Motor behavior ---
static const int      MOTOR_SPEED_FWD   = 200;   // 0-255
static const int      MOTOR_SPEED_TURN  = 220;
static const int      MOTOR_SPEED_BACK  = 180;
static const uint32_t PWM_FREQ_HZ       = 20000;
static const uint8_t  PWM_RES_BITS      = 8;

// --- Servo sweep ---
static const int SERVO_CENTER_DEG      = 90;
static const int SERVO_MIN_DEG         = 30;
static const int SERVO_MAX_DEG         = 150;
static const int SERVO_STEP_DEG        = 5;
static const unsigned long SERVO_STEP_INTERVAL_MS = 25;

// --- Relay polarity ---
#define RELAY_ACTIVE_LOW   1

// --- Flame debounce ---
static const uint8_t FLAME_DEBOUNCE_COUNT = 3;

// --- Ultrasonic ---
static const unsigned long ULTRA_READ_INTERVAL_MS = 60;   // how often to ping
static const uint32_t      ULTRA_PULSE_TIMEOUT_US = 20000; // ~3.4m max
static const float         SOUND_CM_PER_US        = 0.0343f / 2.0f; // go+return

// --- Obstacle thresholds ---
static const float OBSTACLE_STOP_CM      = 20.0f;   // stop if closer than this
static const float OBSTACLE_SAFE_CM      = 25.0f;   // want at least this much for pump+sweep
static const float OBSTACLE_REVERSE_CM   = 30.0f;   // after reverse, want this clear

// --- Auto-mode maneuver timings ---
static const unsigned long REVERSE_DURATION_MS = 600;
static const unsigned long TURN_DURATION_MS    = 400;
static const unsigned long APPROACH_DURATION_MS = 50;   // NEW: 0.2s forward push on center flame

// --- Loop timing ---
static const unsigned long LOOP_INTERVAL_MS = 15;

// ============================ END USER CONFIG =============================

// ---- Derived ----
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

enum ManualCmd : uint8_t { CMD_STOP = 0, CMD_FWD, CMD_BACK, CMD_LEFT, CMD_RIGHT };
volatile ManualCmd g_manualCmd  = CMD_STOP;
volatile bool      g_manualPump = false;

// Debounced flame states
bool g_flameL = false, g_flameM = false, g_flameR = false;
uint8_t g_cntL = 0,   g_cntM = 0,   g_cntR = 0;
bool    g_prevL = false, g_prevM = false, g_prevR = false;

// Ultrasonic
float g_lastDistanceCm = 999.0f;
unsigned long g_lastUltraRead = 0;

// Servo
int           g_servoPos       = SERVO_CENTER_DEG;
int           g_servoDir       = +1;
unsigned long g_servoLastStep  = 0;
bool          g_servoSweeping  = false;

// Auto-mode obstacle state machine
enum AutoSub : uint8_t {
  AS_IDLE = 0,        // no flame, bot stopped
  AS_SEEK_LEFT,
  AS_SEEK_RIGHT,
    AS_APPROACH,        // NEW: brief forward nudge when center flame detected
  AS_EXTINGUISH,      // stopped, pump+sweep
  AS_BACKOFF,         // reversing to safe distance before extinguish
  AS_AVOID_REVERSE,
  AS_AVOID_TURN
};
AutoSub        g_autoSub        = AS_IDLE;
unsigned long  g_autoSubStart   = 0;
bool           g_autoTurnLeft   = true;   // chosen after scan

unsigned long g_lastLoopTick = 0;

// ================= Logging =================
static void logBoot (const char* m){ Serial.print("[BOOT] ");  Serial.println(m); }
static void logInit (const char* m){ Serial.print("[INIT] ");  Serial.println(m); }
static void logInfo (const char* m){ Serial.print("[INFO] ");  Serial.println(m); }
static void logWarn (const char* m){ Serial.print("[WARN] ");  Serial.println(m); }
static void logState(const char* m){ Serial.print("[STATE] "); Serial.println(m); }

// ================= Motor control =================
static void motorsStop() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
//  ledcWrite(PIN_ENA, 0);
//  ledcWrite(PIN_ENB, 0);
}

static void motorsForward(int speed) {
  digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
//  ledcWrite(PIN_ENA, speed);   ledcWrite(PIN_ENB, speed);
}

static void motorsBackward(int speed) {
  digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
//  ledcWrite(PIN_ENA, speed);   ledcWrite(PIN_ENB, speed);
}

// Pivot left: left wheel reverse, right wheel forward
static void motorsTurnLeft(int speed) {
  digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
//  ledcWrite(PIN_ENA, speed);   ledcWrite(PIN_ENB, speed);
}

static void motorsTurnRight(int speed) {
  digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
//  ledcWrite(PIN_ENA, speed);   ledcWrite(PIN_ENB, speed);
}

// ================= Pump & Servo =================
static void pumpOn()  { digitalWrite(PIN_RELAY, RELAY_ON_LEVEL);  }
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
    g_servoPos = SERVO_CENTER_DEG;
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

// ================= Ultrasonic =================
static float ultraMeasureCmBlocking(uint32_t timeoutUs) {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  const unsigned long dur = pulseIn(PIN_ECHO, HIGH, timeoutUs);
  if (dur == 0) return 999.0f; // timeout -> treat as clear
  return (float)dur * SOUND_CM_PER_US;
}

static void ultraUpdate() {
  const unsigned long now = millis();
  if (now - g_lastUltraRead < ULTRA_READ_INTERVAL_MS) return;
  g_lastUltraRead = now;
  g_lastDistanceCm = ultraMeasureCmBlocking(ULTRA_PULSE_TIMEOUT_US);
}

// Blocking single read at a specific servo angle (used only during avoidance scan).
static float ultraReadAtAngle(int angle) {
  nozzleServo.write(angle);
  delay(250); // let servo settle
  return ultraMeasureCmBlocking(ULTRA_PULSE_TIMEOUT_US);
}

// ================= Flame reading (debounced) =================
static bool debounceRead(int pin, uint8_t& counter, bool& prev) {
  const int raw = digitalRead(pin);
  const bool detected = FLAME_DETECTED(raw);
  if (detected == prev) {
    if (counter < FLAME_DEBOUNCE_COUNT) counter++;
  } else {
    counter = 0;
    prev = detected;
  }
  return prev;
}

static void updateFlameStates() {
  const bool nL = debounceRead(PIN_FLAME_L, g_cntL, g_prevL);
  const bool nM = debounceRead(PIN_FLAME_M, g_cntM, g_prevM);
  const bool nR = debounceRead(PIN_FLAME_R, g_cntR, g_prevR);
  if (nL != g_flameL) { g_flameL = nL; Serial.printf("[INFO] Flame LEFT   %s\n", nL?"DETECTED":"clear"); }
  if (nM != g_flameM) { g_flameM = nM; Serial.printf("[INFO] Flame MIDDLE %s\n", nM?"DETECTED":"clear"); }
  if (nR != g_flameR) { g_flameR = nR; Serial.printf("[INFO] Flame RIGHT  %s\n", nR?"DETECTED":"clear"); }
}

// ================= Auto-mode helpers =================
static void enterSub(AutoSub s, const char* name) {
  g_autoSub      = s;
  g_autoSubStart = millis();
  Serial.print("[STATE] AUTO -> ");
  Serial.println(name);
}

// Picks clearer side by moving servo left/right and pinging.
// Returns true = turn left is better, false = right is better.
static bool scanClearerSide() {
  const bool wasSweeping = g_servoSweeping;
  g_servoSweeping = false;

  const float leftCm  = ultraReadAtAngle(SERVO_MAX_DEG);
  const float rightCm = ultraReadAtAngle(SERVO_MIN_DEG);
  nozzleServo.write(SERVO_CENTER_DEG);
  g_servoPos = SERVO_CENTER_DEG;

  Serial.printf("[INFO] Scan  left=%.1fcm  right=%.1fcm\n", leftCm, rightCm);

  if (wasSweeping) {
    g_servoSweeping = true;
    g_servoLastStep = millis();
  }
  return leftCm >= rightCm;
}

// ================= Auto mode logic =================
static void runAutoMode() {
  const bool flameSide = (g_flameL || g_flameR) && !g_flameM;

  // Handle in-progress avoidance maneuvers first.
  if (g_autoSub == AS_AVOID_REVERSE) {
    if (millis() - g_autoSubStart >= REVERSE_DURATION_MS) {
      g_autoTurnLeft = scanClearerSide();
      enterSub(AS_AVOID_TURN, g_autoTurnLeft ? "AVOID_TURN (left)" : "AVOID_TURN (right)");
    } else {
      motorsBackward(MOTOR_SPEED_BACK);
      pumpOff(); servoStopSweep();
      return;
    }
  }

  if (g_autoSub == AS_AVOID_TURN) {
    if (millis() - g_autoSubStart >= TURN_DURATION_MS) {
      motorsStop();
      enterSub(AS_IDLE, "IDLE (post-avoid)");
    } else {
      if (g_autoTurnLeft) motorsTurnLeft(MOTOR_SPEED_TURN);
      else                motorsTurnRight(MOTOR_SPEED_TURN);
      return;
    }
  }

  if (g_flameM) {
    if (g_autoSub != AS_EXTINGUISH && g_autoSub != AS_APPROACH) {
      enterSub(AS_APPROACH, "APPROACH (center flame)");
    }

    if (g_autoSub == AS_APPROACH) {
      if (millis() - g_autoSubStart >= APPROACH_DURATION_MS) {
        motorsStop();
        enterSub(AS_EXTINGUISH, "EXTINGUISH");
        pumpOn();
        servoStartSweep();
      } else {
        motorsForward(MOTOR_SPEED_FWD);
        pumpOff();
        servoStopSweep();   // don't spray while still moving
      }
      return;
    }

    // AS_EXTINGUISH
    motorsStop();
    pumpOn();
    servoStartSweep();
    return;
  }

  // --- Side flame: seek it until it reaches middle ---
  if (flameSide) {
    pumpOff();
    servoStopSweep();

    if (g_lastDistanceCm < OBSTACLE_STOP_CM) {
      motorsStop();
      enterSub(AS_AVOID_REVERSE, "AVOID_REVERSE (while seeking)");
      return;
    }
    if (g_flameL && !g_flameR) {
      if (g_autoSub != AS_SEEK_LEFT) enterSub(AS_SEEK_LEFT, "SEEK_LEFT");
      motorsTurnLeft(MOTOR_SPEED_TURN);
    } else if (g_flameR && !g_flameL) {
      if (g_autoSub != AS_SEEK_RIGHT) enterSub(AS_SEEK_RIGHT, "SEEK_RIGHT");
      motorsTurnRight(MOTOR_SPEED_TURN);
    } else {
      motorsTurnLeft(MOTOR_SPEED_TURN);
    }
    return;
  }

  // --- No flame at all: bot idles per spec ---
  if (g_autoSub != AS_IDLE) enterSub(AS_IDLE, "IDLE (no flame)");
  motorsStop();
  pumpOff();
  servoStopSweep();
}

// ================= Manual mode logic =================
static void runManualMode() {
  // Flame override wins (stop + pump + sweep regardless of user input).
  if (g_flameM || g_flameL || g_flameR) {
    motorsStop();
    pumpOn();
    servoStartSweep();
    return;
  }

  const bool tooClose = (g_lastDistanceCm < OBSTACLE_STOP_CM);

  switch (g_manualCmd) {
    case CMD_FWD:
      if (tooClose) motorsStop();
      else          motorsForward(MOTOR_SPEED_FWD);
      break;
    case CMD_BACK:  motorsBackward(MOTOR_SPEED_BACK); break;
    case CMD_LEFT:  motorsTurnLeft(MOTOR_SPEED_TURN); break;
    case CMD_RIGHT: motorsTurnRight(MOTOR_SPEED_TURN);break;
    case CMD_STOP:
    default:        motorsStop();                     break;
  }

  if (g_manualPump) { pumpOn();  servoStartSweep(); }
  else              { pumpOff(); servoStopSweep();  }
}

// ================= Web UI =================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>FireBot v2</title>
<style>
  :root { --bg:#0e1116; --panel:#1a1f29; --accent:#ff5a3c; --ok:#35d06a; --warn:#f4c542; --txt:#f1f3f6; }
  * { box-sizing:border-box; -webkit-user-select:none; user-select:none; -webkit-tap-highlight-color:transparent; }
  html,body { margin:0; padding:0; background:var(--bg); color:var(--txt); font-family:system-ui,Segoe UI,Roboto,sans-serif; height:100%; }
  header { padding:14px 16px; background:var(--panel); display:flex; justify-content:space-between; align-items:center; }
  header h1 { margin:0; font-size:18px; letter-spacing:.5px; }
  .mode { display:flex; gap:8px; }
  .mode button { background:#2a303d; border:0; color:var(--txt); padding:8px 12px; border-radius:8px; font-size:13px; }
  .mode button.active { background:var(--accent); }
  .status { padding:10px 16px; font-size:13px; background:#141820; border-bottom:1px solid #222; line-height:1.5; }
  .pad { display:grid; grid-template-columns:1fr 1fr 1fr; grid-template-rows:1fr 1fr 1fr; gap:10px; padding:16px; height:55vh; }
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
  .dot { width:10px; height:10px; border-radius:50%; display:inline-block; margin-right:6px; vertical-align:middle; }
  .d-ok { background:var(--ok); }
  .d-fire { background:var(--accent); }
  .d-warn { background:var(--warn); }
</style>
</head>
<body>
<header>
  <h1>🔥 FireBot v2</h1>
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
      const dot = (b, cls) => '<span class="dot ' + (b?(cls||'d-fire'):'d-ok') + '"></span>';
      const obsCls = (j.dist < 20) ? 'd-fire' : (j.dist < 40 ? 'd-warn' : 'd-ok');
      document.getElementById('status').innerHTML =
        'mode: <b>' + j.mode + '</b>  |  sub: <b>' + j.sub + '</b><br>' +
        dot(j.l) + 'L ' + dot(j.m) + 'M ' + dot(j.r) + 'R' +
        '   <span class="dot ' + obsCls + '"></span>' + j.dist.toFixed(0) + ' cm' +
        '   pump: <b>' + (j.pump?'ON':'off') + '</b>';
    } catch(e){}
  }
  setInterval(poll, 500); poll();
</script>
</body>
</html>
)HTML";

// ---- Web handlers ----
static void handleRoot()   { server.send_P(200, "text/html", INDEX_HTML); }

static void handleCmd() {
  if (!server.hasArg("v")) { server.send(400, "text/plain", "missing v"); return; }
  const String v = server.arg("v");
  Serial.println("Command: "+String(v));
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
  if (server.arg("v") == "auto") {
    g_mode = MODE_AUTO;
    g_autoSub = AS_IDLE;
    logState("Mode -> AUTO");
  } else {
    g_mode = MODE_MANUAL;
    g_manualCmd  = CMD_STOP;
    g_manualPump = false;
    logState("Mode -> MANUAL");
  }
  server.send(200, "text/plain", "ok");
}

static const char* autoSubName(AutoSub s) {
  switch (s) {
    case AS_IDLE:          return "idle";
    case AS_SEEK_LEFT:     return "seekL";
    case AS_SEEK_RIGHT:    return "seekR";
    case AS_APPROACH:      return "apprc";
    case AS_EXTINGUISH:    return "extng";
    case AS_AVOID_REVERSE: return "avRev";
    case AS_AVOID_TURN:    return "avTrn";
  }
  return "?";
}

static void handleStatus() {
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"mode\":\"%s\",\"sub\":\"%s\",\"l\":%d,\"m\":%d,\"r\":%d,\"dist\":%.1f,\"pump\":%d}",
           (g_mode == MODE_AUTO) ? "auto" : "manual",
           (g_mode == MODE_AUTO) ? autoSubName(g_autoSub) : "-",
           g_flameL ? 1 : 0, g_flameM ? 1 : 0, g_flameR ? 1 : 0,
           g_lastDistanceCm,
           (digitalRead(PIN_RELAY) == RELAY_ON_LEVEL) ? 1 : 0);
  server.send(200, "application/json", buf);
}

static void handleNotFound() { server.send(404, "text/plain", "not found"); }

// ================= Setup / Loop =================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  logBoot("FireBot v2  (2-wheel + HC-SR04)");

  // Motor IO
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  if (!ledcAttach(PIN_ENA, PWM_FREQ_HZ, PWM_RES_BITS)) logWarn("ledcAttach ENA failed");
  if (!ledcAttach(PIN_ENB, PWM_FREQ_HZ, PWM_RES_BITS)) logWarn("ledcAttach ENB failed");
  motorsStop();
  logInit("Motors OK (L298N, 2-motor differential)");

  // Flame
  pinMode(PIN_FLAME_L, INPUT);
  pinMode(PIN_FLAME_M, INPUT);
  pinMode(PIN_FLAME_R, INPUT);
  logInit("Flame sensors OK (digital, debounce=3)");

  // Ultrasonic
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  logInit("HC-SR04 OK");

  // Relay
  pinMode(PIN_RELAY, OUTPUT);
  pumpOff();
  logInit("Relay OK (pump off, active-LOW)");

  // Servo
  ESP32PWM::allocateTimer(2);
  nozzleServo.setPeriodHertz(50);
  nozzleServo.attach(PIN_SERVO, 500, 2400);
  nozzleServo.write(SERVO_CENTER_DEG);
  g_servoPos = SERVO_CENTER_DEG;
  logInit("Servo OK (centered)");

  // AP
  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (!apOk) logWarn("softAP failed");
  else {
    IPAddress ip = WiFi.softAPIP();
    Serial.print("[INIT] AP \""); Serial.print(AP_SSID);
    Serial.print("\" at http://"); Serial.println(ip);
  }

  server.on("/",       handleRoot);
  server.on("/cmd",    handleCmd);
  server.on("/pump",   handlePump);
  server.on("/mode",   handleMode);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  logInit("HTTP on :80");

  logInfo("Ready. Connect phone to FireBot2_AP, open http://192.168.4.1");
}

void loop() {
  server.handleClient();

  ultraUpdate();

  const unsigned long now = millis();
  if (now - g_lastLoopTick >= LOOP_INTERVAL_MS) {
    g_lastLoopTick = now;

    updateFlameStates();

    if (g_mode == MODE_AUTO) runAutoMode();
    else                     runManualMode();
  }

  servoSweepStep();
}
