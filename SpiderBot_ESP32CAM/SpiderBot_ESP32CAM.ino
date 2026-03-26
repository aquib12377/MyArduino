/*
 * ESP32-CAM Spider Bot Controller
 * ================================
 * - Live MJPEG stream on /stream
 * - Web UI with D-pad + action controls on /
 * - 8 Servos via PCA9685 on I2C (SDA=14, SCL=15)
 * - 4 legs × 2 joints (hip + knee)
 *
 * Board: AI Thinker ESP32-CAM
 * Library deps: ESP32Servo, Adafruit_PWMServoDriver (via PCA9685)
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// ========================== CONFIG ==========================

// Wi-Fi credentials
const char* ssid     = "AAA";
const char* password = "Acube123";

// I2C pins for PCA9685
#define I2C_SDA 14
#define I2C_SCL 15

// PCA9685 settings
#define PCA9685_ADDR 0x40
#define PCA9685_FREQ 50        // 50 Hz for servos
#define SERVO_MIN    120       // ~0.5 ms   → 0°
#define SERVO_MAX    480       // ~2.4 ms   → 180°
#define SERVO_MID    300       // ~1.5 ms   → 90° (neutral)

// AI-Thinker ESP32-CAM pin map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ========================== GLOBALS =========================

WebServer server(80);

/*
 * Leg layout (top view):
 *
 *   Leg0 (FL)  ----  Leg1 (FR)
 *       \              /
 *        [  ESP32-CAM  ]
 *       /              \
 *   Leg2 (BL)  ----  Leg3 (BR)
 *
 * Each leg: Channel N = Hip, Channel N+1 = Knee
 *   Leg 0 → Ch 0 (hip), Ch 1 (knee)
 *   Leg 1 → Ch 2 (hip), Ch 3 (knee)
 *   Leg 2 → Ch 4 (hip), Ch 5 (knee)
 *   Leg 3 → Ch 6 (hip), Ch 7 (knee)
 */

// Current angle for each of the 8 servo channels (degrees 0-180)
int servoAngles[8] = {90, 90, 90, 90, 90, 90, 90, 90};

// Gait step tracker
int gaitStep = 0;

// ===================== PCA9685 DRIVER =======================
// Minimal register-level driver — no external library needed

void pca9685_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void pca9685_init() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // Reset
  pca9685_write(0x00, 0x80);
  delay(10);

  // Sleep, set prescaler for 50 Hz, wake
  pca9685_write(0x00, 0x10);                    // sleep
  uint8_t prescale = (uint8_t)(25000000.0 / (4096.0 * PCA9685_FREQ) - 1 + 0.5);
  pca9685_write(0xFE, prescale);                 // prescaler
  pca9685_write(0x00, 0x00);                     // wake
  delay(5);
  pca9685_write(0x00, 0x20);                     // auto-increment

  Serial.printf("[PCA9685] Init OK, prescale=%d\n", prescale);
}

void pca9685_setPWM(uint8_t channel, uint16_t on, uint16_t off) {
  uint8_t reg = 0x06 + 4 * channel;
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(reg);
  Wire.write(on & 0xFF);
  Wire.write(on >> 8);
  Wire.write(off & 0xFF);
  Wire.write(off >> 8);
  Wire.endTransmission();
}

void setServo(uint8_t channel, int angleDeg) {
  if (channel > 15) return;
  angleDeg = constrain(angleDeg, 0, 180);
  uint16_t pulse = map(angleDeg, 0, 180, SERVO_MIN, SERVO_MAX);
  pca9685_setPWM(channel, 0, pulse);
  servoAngles[channel] = angleDeg;
}

void setAllServos(int angles[8]) {
  for (int i = 0; i < 8; i++) {
    setServo(i, angles[i]);
  }
}

// ===================== GAIT FUNCTIONS =======================

/*
 * Simple alternating tripod-style gait for a 4-leg spider.
 * Diagonal pairs move together: (Leg0+Leg3) and (Leg1+Leg2).
 *
 * Hip:  < 90 = forward swing, > 90 = backward push
 * Knee: < 90 = lift,          > 90 = lower/plant
 */

// {hip0, knee0, hip1, knee1, hip2, knee2, hip3, knee3}

void standNeutral() {
  int a[8] = {90, 90, 90, 90, 90, 90, 90, 90};
  setAllServos(a);
  gaitStep = 0;
}

void walkForward() {
  // 4-phase gait
  int gait[4][8] = {
    // Phase 0: Lift diagonal A (Leg0+Leg3), swing forward
    { 60, 60,  90, 90,  90, 90,  60, 60},
    // Phase 1: Plant A, push back
    { 60, 90, 90, 90,  90, 90,  60, 90},
    // Phase 2: Lift diagonal B (Leg1+Leg2), swing forward
    { 90, 90,  60, 60,  60, 60,  90, 90},
    // Phase 3: Plant B, push back
    { 90, 90,  60, 90,  60, 90,  90, 90}
  };
  setAllServos(gait[gaitStep % 4]);
  gaitStep++;
}

void walkBackward() {
  int gait[4][8] = {
    {120, 60,  90, 90,  90, 90, 120, 60},
    {120, 90,  90, 90,  90, 90, 120, 90},
    { 90, 90, 120, 60, 120, 60,  90, 90},
    { 90, 90, 120, 90, 120, 90,  90, 90}
  };
  setAllServos(gait[gaitStep % 4]);
  gaitStep++;
}

void turnLeft() {
  // All hips rotate same direction
  int gait[2][8] = {
    { 60, 60,  60, 60, 120, 60, 120, 60},   // lift + rotate
    { 60, 90,  60, 90, 120, 90, 120, 90}    // plant
  };
  setAllServos(gait[gaitStep % 2]);
  gaitStep++;
}

void turnRight() {
  int gait[2][8] = {
    {120, 60, 120, 60,  60, 60,  60, 60},
    {120, 90, 120, 90,  60, 90,  60, 90}
  };
  setAllServos(gait[gaitStep % 2]);
  gaitStep++;
}

void danceWave() {
  int seq[4][8] = {
    { 90, 45,  90, 90,  90, 90,  90, 90},
    { 90, 90,  90, 45,  90, 90,  90, 90},
    { 90, 90,  90, 90,  90, 45,  90, 90},
    { 90, 90,  90, 90,  90, 90,  90, 45}
  };
  setAllServos(seq[gaitStep % 4]);
  gaitStep++;
}

void sitDown() {
  int a[8] = {90, 140, 90, 140, 90, 140, 90, 140};
  setAllServos(a);
}

void standTall() {
  int a[8] = {90, 40, 90, 40, 90, 40, 90, 40};
  setAllServos(a);
}

// ===================== CAMERA INIT ==========================

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;    // 640×480
  config.jpeg_quality = 12;               // 10-63, lower = better
  config.fb_count     = 2;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;

  if (!psramFound()) {
    config.frame_size  = FRAMESIZE_QVGA;
    config.fb_count    = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("[CAM] No PSRAM — using QVGA");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init FAILED: 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 12);
  }

  Serial.println("[CAM] Init OK");
  return true;
}

// ===================== MJPEG STREAM =========================

void handleStream() {
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                    "Access-Control-Allow-Origin: *\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[STREAM] Frame grab failed");
      continue;
    }

    String header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                    + String(fb->len) + "\r\n\r\n";
    client.print(header);
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    if (!client.connected()) break;
  }
}

// ==================== CONTROL ENDPOINT ======================

void handleControl() {
  if (!server.hasArg("cmd")) {
    server.send(400, "text/plain", "Missing cmd");
    return;
  }

  String cmd = server.arg("cmd");
  Serial.printf("[CMD] %s\n", cmd.c_str());

  if      (cmd == "forward")   walkForward();
  else if (cmd == "backward")  walkBackward();
  else if (cmd == "left")      turnLeft();
  else if (cmd == "right")     turnRight();
  else if (cmd == "stop")      standNeutral();
  else if (cmd == "dance")     danceWave();
  else if (cmd == "sit")       sitDown();
  else if (cmd == "tall")      standTall();
  else if (cmd == "servo") {
    // Fine-tune: /control?cmd=servo&ch=0&angle=90
    if (server.hasArg("ch") && server.hasArg("angle")) {
      int ch  = server.arg("ch").toInt();
      int ang = server.arg("angle").toInt();
      setServo(ch, ang);
      server.send(200, "application/json",
                  "{\"ok\":true,\"ch\":" + String(ch) + ",\"angle\":" + String(ang) + "}");
      return;
    }
  }
  else {
    server.send(400, "text/plain", "Unknown cmd");
    return;
  }

  // Return current servo state as JSON
  String json = "{\"ok\":true,\"cmd\":\"" + cmd + "\",\"servos\":[";
  for (int i = 0; i < 8; i++) {
    json += String(servoAngles[i]);
    if (i < 7) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ==================== STATUS ENDPOINT =======================

void handleStatus() {
  String json = "{\"servos\":[";
  for (int i = 0; i < 8; i++) {
    json += String(servoAngles[i]);
    if (i < 7) json += ",";
  }
  json += "],\"heap\":" + String(ESP.getFreeHeap());
  json += ",\"psram\":" + String(ESP.getFreePsram());
  json += "}";
  server.send(200, "application/json", json);
}

// ===================== WEB UI PAGE ==========================

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Spider Bot Control</title>
<style>
  :root {
    --bg: #0f1117; --card: #1a1d27; --accent: #00e5ff;
    --accent2: #7c4dff; --text: #e0e0e0; --dim: #666;
    --danger: #ff5252; --success: #69f0ae;
  }
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    font-family: 'Segoe UI', system-ui, sans-serif;
    background: var(--bg); color: var(--text);
    min-height: 100vh; padding: 10px;
  }
  h1 { text-align:center; font-size:1.3em; padding:10px 0;
       background: linear-gradient(90deg, var(--accent), var(--accent2));
       -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
  .grid { display:grid; grid-template-columns:1fr; gap:10px; max-width:800px; margin:0 auto; }
  @media(min-width:600px) { .grid { grid-template-columns:1fr 1fr; } }

  .card { background:var(--card); border-radius:12px; padding:14px; }
  .card h2 { font-size:0.85em; color:var(--dim); text-transform:uppercase;
             letter-spacing:1px; margin-bottom:10px; }

  /* Stream */
  .stream-box { position:relative; border-radius:8px; overflow:hidden;
                background:#000; aspect-ratio:4/3; }
  .stream-box img { width:100%; height:100%; object-fit:contain; }
  .stream-box .offline { position:absolute; inset:0; display:flex;
    align-items:center; justify-content:center; color:var(--dim); font-size:0.9em; }

  /* D-Pad */
  .dpad { display:grid; grid-template-columns:repeat(3,60px);
          grid-template-rows:repeat(3,60px); gap:4px; justify-content:center; }
  .dpad button {
    border:none; border-radius:8px; font-size:1.4em; cursor:pointer;
    background:var(--bg); color:var(--text); transition:all .15s;
  }
  .dpad button:hover { background:var(--accent); color:#000; }
  .dpad button:active { transform:scale(0.92); }
  .dpad .center { background:var(--danger); color:#fff; font-size:0.7em; font-weight:700; }
  .dpad .center:hover { background:#ff1744; }
  .dpad .empty { visibility:hidden; }

  /* Action buttons */
  .actions { display:flex; flex-wrap:wrap; gap:8px; justify-content:center; }
  .actions button {
    padding:10px 18px; border:1px solid #333; border-radius:8px;
    background:var(--bg); color:var(--text); cursor:pointer;
    font-size:0.85em; transition:all .15s;
  }
  .actions button:hover { border-color:var(--accent); color:var(--accent); }

  /* Servo sliders */
  .servo-grid { display:grid; grid-template-columns:1fr 1fr; gap:8px; }
  .servo-item { display:flex; flex-direction:column; gap:2px; }
  .servo-item label { font-size:0.75em; color:var(--dim); }
  .servo-item input[type=range] { width:100%; accent-color:var(--accent); }
  .servo-item .val { font-size:0.8em; text-align:center; color:var(--accent); }

  /* Status bar */
  .status { text-align:center; font-size:0.75em; color:var(--dim); padding:6px; }
  .status .dot { display:inline-block; width:8px; height:8px; border-radius:50%;
                 background:var(--danger); margin-right:4px; vertical-align:middle; }
  .status .dot.ok { background:var(--success); }

  /* Log */
  #log { font-family:monospace; font-size:0.7em; color:var(--dim);
         max-height:80px; overflow-y:auto; margin-top:8px; padding:6px;
         background:var(--bg); border-radius:6px; }
</style>
</head>
<body>

<h1>&#128375; Spider Bot Control</h1>

<div class="grid">

  <!-- Live Stream -->
  <div class="card">
    <h2>Live Stream</h2>
    <div class="stream-box">
      <img id="stream" src="/stream" alt="Stream">
      <div class="offline" id="offline" style="display:none">Camera Offline</div>
    </div>
  </div>

  <!-- Controls -->
  <div class="card">
    <h2>Movement</h2>
    <div class="dpad">
      <div class="empty"></div>
      <button onclick="cmd('forward')" title="Forward">&#9650;</button>
      <div class="empty"></div>
      <button onclick="cmd('left')" title="Left">&#9664;</button>
      <button class="center" onclick="cmd('stop')">STOP</button>
      <button onclick="cmd('right')" title="Right">&#9654;</button>
      <div class="empty"></div>
      <button onclick="cmd('backward')" title="Backward">&#9660;</button>
      <div class="empty"></div>
    </div>

    <h2 style="margin-top:14px">Actions</h2>
    <div class="actions">
      <button onclick="cmd('dance')">&#128131; Dance</button>
      <button onclick="cmd('sit')">&#128059; Sit</button>
      <button onclick="cmd('tall')">&#9995; Stand Tall</button>
      <button onclick="cmd('stop')">&#127968; Neutral</button>
    </div>
  </div>

  <!-- Servo Tuning -->
  <div class="card" style="grid-column:1/-1">
    <h2>Servo Fine-Tuning</h2>
    <div class="servo-grid" id="servoGrid"></div>
    <div id="log"></div>
  </div>

</div>

<div class="status">
  <span class="dot" id="dot"></span>
  <span id="statusText">Connecting...</span>
</div>

<script>
  const legNames = ['FL Hip','FL Knee','FR Hip','FR Knee','BL Hip','BL Knee','BR Hip','BR Knee'];

  // Build servo sliders
  const grid = document.getElementById('servoGrid');
  legNames.forEach((name, i) => {
    const div = document.createElement('div');
    div.className = 'servo-item';
    div.innerHTML = `<label>Ch${i}: ${name}</label>
      <input type="range" min="0" max="180" value="90" id="s${i}"
             oninput="servoChange(${i}, this.value)">
      <div class="val" id="v${i}">90&deg;</div>`;
    grid.appendChild(div);
  });

  function log(msg) {
    const el = document.getElementById('log');
    el.textContent = new Date().toLocaleTimeString() + ' ' + msg + '\n' + el.textContent;
  }

  async function cmd(c) {
    try {
      const r = await fetch('/control?cmd=' + c);
      const j = await r.json();
      if (j.servos) updateSliders(j.servos);
      log('CMD: ' + c + ' → OK');
      setOnline(true);
    } catch(e) {
      log('CMD FAIL: ' + e.message);
      setOnline(false);
    }
  }

  let servoTimer = null;
  function servoChange(ch, val) {
    document.getElementById('v'+ch).textContent = val + '°';
    clearTimeout(servoTimer);
    servoTimer = setTimeout(() => {
      fetch('/control?cmd=servo&ch='+ch+'&angle='+val)
        .then(r=>r.json()).then(j => log('Servo ch'+ch+'='+val+'°'))
        .catch(e => log('Servo FAIL: '+e.message));
    }, 50);
  }

  function updateSliders(angles) {
    angles.forEach((a, i) => {
      document.getElementById('s'+i).value = a;
      document.getElementById('v'+i).textContent = a + '°';
    });
  }

  function setOnline(ok) {
    document.getElementById('dot').className = 'dot' + (ok ? ' ok' : '');
    document.getElementById('statusText').textContent = ok ? 'Connected' : 'Offline';
  }

  // Periodic status poll
  setInterval(async () => {
    try {
      const r = await fetch('/status');
      const j = await r.json();
      updateSliders(j.servos);
      setOnline(true);
    } catch(e) { setOnline(false); }
  }, 3000);

  // Stream error handler
  document.getElementById('stream').onerror = () => {
    document.getElementById('offline').style.display = 'flex';
    setTimeout(() => {
      document.getElementById('stream').src = '/stream?' + Date.now();
      document.getElementById('offline').style.display = 'none';
    }, 2000);
  };

  // Keyboard controls
  document.addEventListener('keydown', e => {
    const map = {ArrowUp:'forward', ArrowDown:'backward',
                 ArrowLeft:'left', ArrowRight:'right',
                 ' ':'stop', Escape:'stop'};
    if (map[e.key]) { e.preventDefault(); cmd(map[e.key]); }
  });
</script>

</body>
</html>
)rawliteral";

// =================== HTTP HANDLERS ==========================

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

// ======================== SETUP =============================

void setup() {
  Serial.begin(115200);
  Serial.println("\n====== Spider Bot ESP32-CAM ======");

  // Init PCA9685
  pca9685_init();
  standNeutral();
  Serial.println("[SERVO] All neutral (90°)");

  // Init Camera
  if (!initCamera()) {
    Serial.println("[FATAL] Camera init failed — halting");
    while (true) delay(1000);
  }

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    // Fallback AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SpiderBot", "12345678");
    Serial.printf("[WiFi] AP Mode — IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  // HTTP routes
  server.on("/",        HTTP_GET, handleRoot);
  server.on("/stream",  HTTP_GET, handleStream);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/status",  HTTP_GET, handleStatus);

  server.begin();
  Serial.println("[HTTP] Server started");
  Serial.println("================================");
  Serial.printf("  Open http://%s/\n",
    WiFi.getMode() == WIFI_STA ? WiFi.localIP().toString().c_str()
                                : WiFi.softAPIP().toString().c_str());
  Serial.println("================================");
}

// ======================== LOOP ==============================

void loop() {
  server.handleClient();
}
