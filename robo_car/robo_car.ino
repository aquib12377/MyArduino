/*
 * ============================================================
 *  2WD Web-Controlled Robot Car
 *  Hardware : ESP32S (30-pin), L298N, SSD1306 OLED 0.96"
 *  Control  : WiFi Access Point + WebServer
 *  Features : Animated OLED eyes, speed control, auto-stop
 * ============================================================
 *  WIRING
 *  IN1 → GPIO25   IN2 → GPIO26   ENA → GPIO14  (Left motor)
 *  IN3 → GPIO27   IN4 → GPIO33   ENB → GPIO32  (Right motor)
 *  OLED SDA → GPIO21   SCL → GPIO22
 * ============================================================
 *  LIBRARIES (install via Library Manager)
 *    Adafruit SSD1306   Adafruit GFX
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_system.h"

// ─── Version ─────────────────────────────────────────────────────────────────
constexpr const char* FW_VERSION = "1.0.0";
constexpr const char* APP_NAME   = "2WD-WebBot";

// ─── WiFi AP Config ──────────────────────────────────────────────────────────
constexpr const char* AP_SSID     = "ROBO_CAR";
constexpr const char* AP_PASSWORD = "robo1234";   // min 8 chars required for WPA2

// ─── Motor Pins (L298N) ──────────────────────────────────────────────────────
//  Motor A = Left wheel
constexpr uint8_t PIN_IN1 = 14;
constexpr uint8_t PIN_IN2 = 27;
constexpr uint8_t PIN_ENA = 14;

//  Motor B = Right wheel
constexpr uint8_t PIN_IN3 = 26;
constexpr uint8_t PIN_IN4 = 25;
constexpr uint8_t PIN_ENB = 32;

//  LEDC PWM config
constexpr uint32_t PWM_FREQ  = 1000;   // 1 kHz — safe for most TT motors
constexpr uint8_t  PWM_RES   = 8;      // 8-bit → 0–255

// ─── OLED Config ─────────────────────────────────────────────────────────────
constexpr uint8_t SCREEN_W   = 128;
constexpr uint8_t SCREEN_H   = 64;
constexpr int8_t  OLED_RESET = -1;     // shared reset with ESP32
constexpr uint8_t OLED_ADDR  = 0x3C;  // try 0x3D if 0x3C not found

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);
bool oledOK = false;

// ─── Web Server ──────────────────────────────────────────────────────────────
WebServer server(80);

// ─── Robot State ─────────────────────────────────────────────────────────────
enum class Dir { STOP, FORWARD, BACKWARD, LEFT, RIGHT };
Dir     currentDir  = Dir::STOP;
uint8_t motorSpeed  = 200;             // default ~78% duty (0–255)

unsigned long lastCmdMs = 0;
constexpr unsigned long AUTO_STOP_MS = 3000; // 3 s no-command → stop motors

// ─── Logging Helpers ─────────────────────────────────────────────────────────
#define LOGF(tag, ...) Serial.printf("[" tag "] " __VA_ARGS__)
#define LOG(tag, msg)  Serial.printf("[%s] %s\n", tag, msg)

// ─── Motor Functions ─────────────────────────────────────────────────────────
void motorSetup() {
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);

  // ESP32 Arduino Core v3.x API: attach pin directly (no channel needed)
  ledcAttach(PIN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_ENB, PWM_FREQ, PWM_RES);

  // Ensure motors are off at boot
  digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
  ledcWrite(PIN_ENA, 0);
  ledcWrite(PIN_ENB, 0);

  LOG("INIT", "Motors configured and stopped");
}

/*
 *  Drive both channels independently.
 *  a_fwd / b_fwd : true = forward rotation
 *  a_en  / b_en  : true = channel enabled (powered)
 *  spd           : 0–255 PWM duty
 */
inline void motorDrive(bool a_fwd, bool a_en,
                       bool b_fwd, bool b_en, uint8_t spd) {
  digitalWrite(PIN_IN1, (a_en && a_fwd)  ? HIGH : LOW);
  digitalWrite(PIN_IN2, (a_en && !a_fwd) ? HIGH : LOW);
  digitalWrite(PIN_IN3, (b_en && b_fwd)  ? HIGH : LOW);
  digitalWrite(PIN_IN4, (b_en && !b_fwd) ? HIGH : LOW);
  ledcWrite(PIN_ENA, a_en ? spd : 0);
  ledcWrite(PIN_ENB, b_en ? spd : 0);
}

// Forward declaration so drawEyes can be called inside setDirection
void drawEyes(Dir dir);

void setDirection(Dir dir) {
  currentDir = dir;
  lastCmdMs  = millis();

  switch (dir) {
    case Dir::FORWARD:
      motorDrive(true,  true,  true,  true,  motorSpeed);
      LOGF("STATE", "FORWARD  spd=%d\n", motorSpeed);
      break;

    case Dir::BACKWARD:
      motorDrive(false, true,  false, true,  motorSpeed);
      LOGF("STATE", "BACKWARD spd=%d\n", motorSpeed);
      break;

    case Dir::LEFT:
      // Left wheel reverse, right wheel forward → pivot left
      motorDrive(false, true,  true,  true,  motorSpeed);
      LOGF("STATE", "LEFT     spd=%d\n", motorSpeed);
      break;

    case Dir::RIGHT:
      // Left wheel forward, right wheel reverse → pivot right
      motorDrive(true,  true,  false, true,  motorSpeed);
      LOGF("STATE", "RIGHT    spd=%d\n", motorSpeed);
      break;

    case Dir::STOP:
    default:
      motorDrive(false, false, false, false, 0);
      LOG("STATE", "STOP");
      break;
  }

  drawEyes(dir);
}

/*
 *  Auto-stop: if the web client drops / user closes the tab,
 *  motors stop after AUTO_STOP_MS. Called every loop().
 */
void autoStopCheck() {
  if (currentDir != Dir::STOP &&
      (millis() - lastCmdMs) > AUTO_STOP_MS) {
    LOG("WARN", "Auto-stop: no command for 3 s");
    setDirection(Dir::STOP);
  }
}

// ─── OLED Eyes ───────────────────────────────────────────────────────────────
constexpr int16_t L_EYE_X = 35, R_EYE_X = 93, EYE_CY = 30;
constexpr int16_t EYE_R   = 15;   // eye outer radius
constexpr int16_t PUP_R   = 5;    // pupil radius

void drawEyes(Dir dir) {
  if (!oledOK) return;
  display.clearDisplay();

  // Pupil offset based on direction
  int16_t dx = 0, dy = 0;
  switch (dir) {
    case Dir::FORWARD:  dy = -6; break;
    case Dir::BACKWARD: dy = +6; break;
    case Dir::LEFT:     dx = -7; break;
    case Dir::RIGHT:    dx = +7; break;
    default: break;
  }

  // Draw white eyeballs
  display.fillCircle(L_EYE_X, EYE_CY, EYE_R, WHITE);
  display.fillCircle(R_EYE_X, EYE_CY, EYE_R, WHITE);

  // Draw black pupils (clamped inside eyeball)
  display.fillCircle(L_EYE_X + dx, EYE_CY + dy, PUP_R, BLACK);
  display.fillCircle(R_EYE_X + dx, EYE_CY + dy, PUP_R, BLACK);

  // Sleepy/idle half-lid for STOP state
  if (dir == Dir::STOP) {
    display.fillRect(L_EYE_X - EYE_R, EYE_CY - EYE_R, EYE_R * 2 + 1, EYE_R / 2, BLACK);
    display.fillRect(R_EYE_X - EYE_R, EYE_CY - EYE_R, EYE_R * 2 + 1, EYE_R / 2, BLACK);
  }

  // Small label at bottom
  const char* label = "READY";
  if      (dir == Dir::FORWARD)  label = "FORWARD";
  else if (dir == Dir::BACKWARD) label = "BACK";
  else if (dir == Dir::LEFT)     label = "LEFT";
  else if (dir == Dir::RIGHT)    label = "RIGHT";

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(44, 55);
  display.print(label);

  display.display();
}

void oledSplash(const char* line1, const char* line2 = nullptr,
                const char* line3 = nullptr) {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,  4); display.print(line1);
  if (line2) { display.setCursor(0, 20); display.print(line2); }
  if (line3) { display.setCursor(0, 36); display.print(line3); }
  display.display();
}

// ─── Web Page (stored in flash) ──────────────────────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>ROBO CAR</title>
<style>
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  body{background:#0d0d0d;color:#fff;font-family:'Segoe UI',sans-serif;
       text-align:center;margin:0;padding:12px;min-height:100vh}
  h2{margin:8px 0 4px;font-size:1.4em;color:#00dfff;letter-spacing:2px}
  .subtitle{font-size:.75em;color:#555;margin-bottom:16px}
  .dpad{display:grid;grid-template-columns:repeat(3,76px);
        grid-template-rows:repeat(3,76px);gap:6px;
        justify-content:center;margin:0 auto 20px}
  .btn{width:76px;height:76px;font-size:26px;border:2px solid #222;
       border-radius:14px;background:#1a1a1a;color:#ccc;cursor:pointer;
       display:flex;align-items:center;justify-content:center;
       user-select:none;transition:background .08s,border-color .08s}
  .btn:active,.btn.held{background:#00dfff!important;border-color:#00dfff;color:#000}
  #bstop{background:#2a0a0a;border-color:#a00;color:#f55;font-size:.85em;font-weight:700}
  #bstop:active{background:#f55!important;color:#000;border-color:#f55}
  .speed-row{margin:8px 0}
  .speed-row label{font-size:.8em;color:#777;display:block;margin-bottom:4px}
  input[type=range]{width:220px;accent-color:#00dfff;height:6px}
  #spdval{color:#00dfff;font-weight:700}
  #status{margin-top:14px;font-size:.78em;color:#555;min-height:1.4em}
  .dot{display:inline-block;width:8px;height:8px;border-radius:50%;
       background:#555;margin-right:6px;transition:background .3s}
  .dot.on{background:#0f0}
</style>
</head>
<body>
<h2>🤖 ROBO CAR</h2>
<div class="subtitle">Hold button to move — release to stop</div>

<div class="dpad">
  <div></div>
  <div class="btn" id="bF"
    ontouchstart="hold('F')" ontouchend="release()"
    onmousedown="hold('F')" onmouseup="release()" onmouseleave="release()">▲</div>
  <div></div>
  <div class="btn" id="bL"
    ontouchstart="hold('L')" ontouchend="release()"
    onmousedown="hold('L')" onmouseup="release()" onmouseleave="release()">◀</div>
  <div class="btn" id="bstop"
    ontouchstart="cmd('S')" onmousedown="cmd('S')">STOP</div>
  <div class="btn" id="bR"
    ontouchstart="hold('R')" ontouchend="release()"
    onmousedown="hold('R')" onmouseup="release()" onmouseleave="release()">▶</div>
  <div></div>
  <div class="btn" id="bB"
    ontouchstart="hold('B')" ontouchend="release()"
    onmousedown="hold('B')" onmouseup="release()" onmouseleave="release()">▼</div>
  <div></div>
</div>

<div class="speed-row">
  <label>SPEED: <span id="spdval">200</span></label>
  <input type="range" min="80" max="255" value="200"
         oninput="updateSpd(this.value)" onchange="sendSpd(this.value)">
</div>

<div id="status"><span class="dot" id="dot"></span>Connecting...</div>

<script>
const btns = {F:'bF',B:'bB',L:'bL',R:'bR'};
let activeBtn = null;

function clearBtns(){
  Object.values(btns).forEach(id=>{
    const el=document.getElementById(id);
    if(el) el.classList.remove('held');
  });
}

function hold(c){
  clearBtns();
  const id=btns[c];
  if(id) document.getElementById(id).classList.add('held');
  activeBtn=c;
  cmd(c);
}

function release(){
  clearBtns();
  activeBtn=null;
  cmd('S');
}

function cmd(c){
  fetch('/cmd?v='+c)
    .then(r=>{
      setStatus(c==='S'?'Stopped':'Moving: '+c, true);
    })
    .catch(()=>setStatus('⚠ Connection lost',false));
}

function updateSpd(v){
  document.getElementById('spdval').textContent=v;
}
function sendSpd(v){
  fetch('/speed?v='+v).catch(()=>{});
}

function setStatus(msg,ok){
  document.getElementById('status').innerHTML=
    '<span class="dot on" id="dot"></span>'+msg;
  if(!ok) document.getElementById('dot').classList.remove('on');
}

// Keyboard support (desktop)
const keyMap={ArrowUp:'F',ArrowDown:'B',ArrowLeft:'L',ArrowRight:'R',' ':'S','s':'S'};
const keyDown={};
document.addEventListener('keydown',e=>{
  const c=keyMap[e.key];
  if(!c) return;
  e.preventDefault();
  if(keyDown[e.key]) return;
  keyDown[e.key]=true;
  if(c==='S') cmd('S');
  else hold(c);
});
document.addEventListener('keyup',e=>{
  const c=keyMap[e.key];
  if(!c) return;
  keyDown[e.key]=false;
  if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)) release();
});

setStatus('Ready',true);
</script>
</body>
</html>
)rawhtml";

// ─── HTTP Handlers ───────────────────────────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleCmd() {
  if (!server.hasArg("v")) { server.send(400, "text/plain", "ERR: missing arg"); return; }
  String v = server.arg("v");
  if      (v == "F") setDirection(Dir::FORWARD);
  else if (v == "B") setDirection(Dir::BACKWARD);
  else if (v == "L") setDirection(Dir::LEFT);
  else if (v == "R") setDirection(Dir::RIGHT);
  else               setDirection(Dir::STOP);
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  if (!server.hasArg("v")) { server.send(400, "text/plain", "ERR: missing arg"); return; }
  int val = server.arg("v").toInt();
  motorSpeed = (uint8_t)constrain(val, 0, 255);
  LOGF("INFO", "Speed updated → %d\n", motorSpeed);
  if (currentDir != Dir::STOP) setDirection(currentDir);  // re-apply live
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);

  // Log restart reason
  LOGF("BOOT", "=== %s v%s ===\n", APP_NAME, FW_VERSION);
  LOGF("BOOT", "Reset reason code: %d\n", (int)esp_reset_reason());

  // ── Motors
  motorSetup();

  // ── OLED
  Wire.begin(21, 22);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oledOK = true;
    LOG("INIT", "OLED OK (0x3C)");
    char verLine[20];
    snprintf(verLine, sizeof(verLine), "  v%s", FW_VERSION);
    oledSplash("  ROBO CAR", verLine, "  Starting WiFi...");
  } else {
    LOG("WARN", "OLED not detected on 0x3C — try 0x3D or check wiring");
    LOG("WARN", "Continuing without display");
  }

  // ── WiFi Access Point
  WiFi.mode(WIFI_AP);
  bool apOK = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apOK) {
    IPAddress ip = WiFi.softAPIP();
    LOGF("INIT", "AP started | SSID: %s | IP: %s\n", AP_SSID, ip.toString().c_str());

    if (oledOK) {
      char ipStr[20];
      ip.toString().toCharArray(ipStr, sizeof(ipStr));
      char line1[32], line2[32], line3[32];
      snprintf(line1, sizeof(line1), "SSID: %s", AP_SSID);
      snprintf(line2, sizeof(line2), "PWD:  %s", AP_PASSWORD);
      snprintf(line3, sizeof(line3), "IP:   %s", ipStr);
      oledSplash(line1, line2, line3);
      delay(4000);  // let user read connection info
    }
  } else {
    LOG("ERROR", "WiFi softAP() failed! Check for channel conflicts.");
    if (oledOK) oledSplash("WiFi AP FAILED", "Reboot device");
    // Continue — server will just have no clients
  }

  // ── HTTP Routes
  server.on("/",       handleRoot);
  server.on("/cmd",    HTTP_GET, handleCmd);
  server.on("/speed",  HTTP_GET, handleSpeed);
  server.onNotFound(   handleNotFound);
  server.begin();
  LOG("INIT", "HTTP server started on port 80");

  // ── Initial eye state
  drawEyes(Dir::STOP);
  LOG("BOOT", "System ready — open browser to 192.168.4.1");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  autoStopCheck();
}
