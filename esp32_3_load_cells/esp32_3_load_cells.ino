// ============================================================
//  ESP32  ×  3 Load Cells (3KG each)  ×  HX711  ×  OLED  ×  Web UI
//
//  NEW in this version:
//    • Web dashboard at http://<ESP32-IP>/  (auto-refreshes every 5 s)
//    • REST API:
//        GET  /api/status              → JSON with all readings
//        POST /api/tare               → tare all scales
//        POST /api/calibrate?channel=N → recalibrate ch N using 100 g weight
//    • Calibration factor persisted to NVS (survives reboots)
//
//  Required Libraries (install via Arduino Library Manager):
//    1. HX711              by bogde  (Bogdan Necula)
//    2. ESP_Mail_Client    by Mobizt
//    3. Adafruit SSD1306   by Adafruit
//    4. Adafruit GFX       by Adafruit  (dependency of SSD1306)
//    ── built-in to ESP32 Arduino core (no install needed) ──
//    5. WebServer          (ESP32 Arduino core)
//    6. Preferences        (ESP32 Arduino core)
// ============================================================

// ─── Library Includes ───────────────────────────────────────
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>          // NEW – built-in ESP32 core
#include <Preferences.h>        // NEW – NVS key-value store
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <ESP_Mail_Client.h>

// ─── OLED Configuration ─────────────────────────────────────
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_I2C_ADDR   0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── WiFi Credentials ───────────────────────────────────────
#define WIFI_SSID       "MyProject"
#define WIFI_PASSWORD   "12345678"

// ─── Email Configuration ────────────────────────────────────
#define SMTP_HOST        "smtp.gmail.com"
#define SMTP_PORT        465
#define SENDER_EMAIL     "ansarimohammedaquib@gmail.com"
#define SENDER_PASSWORD  "rsaa hbun qjwp sgpj"
#define RECIPIENT_EMAIL  "harshyaroman491@gmail.com"

// ─── HX711 Pin Definitions ───────────────────────────────────
#define HX711_1_DOUT    27
#define HX711_1_SCK     14
#define HX711_2_DOUT    25
#define HX711_2_SCK     26
#define HX711_3_DOUT    32
#define HX711_3_SCK     33

// ─── Tare Button ─────────────────────────────────────────────
#define TARE_BUTTON_PIN 15

// ─── Application Tuning ──────────────────────────────────────
// Default calibration factors – overridden from NVS if a saved
// value exists from a previous calibration via the web UI.
#define DEFAULT_CAL_1   -7050.0f
#define DEFAULT_CAL_2   -7050.0f
#define DEFAULT_CAL_3   -7050.0f

#define KNOWN_CAL_WEIGHT_G  100.0f    // fixed weight used for calibration
#define READ_INTERVAL_MS    500
#define ALERT_THRESHOLD_G   100.0f
#define EMAIL_COOLDOWN_MS   (5UL * 60 * 1000)
#define DEBOUNCE_MS         50

// ─── Global Objects ──────────────────────────────────────────
HX711 scale1, scale2, scale3;

unsigned long lastEmailTime[3]  = {0, 0, 0};
bool          alertActive[3]    = {false, false, false};

SMTPSession     smtp;
Session_Config  smtpConfig;

// Live readings shared between loop() and web handlers
float  dispWeight[3]        = {0, 0, 0};
bool   dispOk[3]            = {false, false, false};

// Calibration factors – loaded from NVS, overwritten by web UI
float  calibrationFactor[3] = {DEFAULT_CAL_1, DEFAULT_CAL_2, DEFAULT_CAL_3};

// NEW – web server on port 80
WebServer  server(80);
Preferences prefs;

// ─── Embedded Web Dashboard (stored in flash) ────────────────
// Full HTML/CSS/JS – ~5 KB.  Uses fetch() to poll /api/status
// every 5 seconds and sends POST requests for tare / calibrate.
// ─────────────────────────────────────────────────────────────
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Weight Monitor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=DM+Sans:wght@400;500;700&display=swap" rel="stylesheet">
<style>
:root{
  --bg:#0b0e14;--surface:#13181f;--border:#1e2530;--border-hi:#2e3c4f;
  --text:#d4dce8;--muted:#5a6a7e;--accent:#00c9ff;--green:#00e57a;
  --red:#ff4d5e;--yellow:#ffb340;--mono:'Space Mono',monospace;
  --sans:'DM Sans',sans-serif;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:var(--sans);min-height:100vh;padding:24px 20px 40px}
/* header */
.header{text-align:center;margin-bottom:28px;position:relative}
.header h1{font-family:var(--mono);font-size:1.25rem;letter-spacing:.12em;text-transform:uppercase;color:var(--accent)}
.header p{font-size:.78rem;color:var(--muted);margin-top:4px;letter-spacing:.04em}
.pulse-dot{width:8px;height:8px;border-radius:50%;background:var(--green);display:inline-block;margin-right:6px;animation:blink 2s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.25}}
/* info bar */
.infobar{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:20px}
.info-pill{background:var(--surface);border:1px solid var(--border);border-radius:6px;padding:5px 12px;font-size:.72rem;font-family:var(--mono);color:var(--muted)}
.info-pill b{color:var(--text)}
/* countdown strip */
.countdown-bar{height:3px;background:var(--border);border-radius:2px;margin-bottom:24px;overflow:hidden}
.countdown-fill{height:100%;background:var(--accent);transition:width 1s linear;border-radius:2px}
/* channel grid */
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px;margin-bottom:24px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:14px;padding:20px 18px;transition:border-color .3s}
.card.ok-card{border-color:var(--border)}
.card.alert-card{border-color:var(--red);box-shadow:0 0 20px rgba(255,77,94,.12)}
.card.alert-card{animation:pulse-border 1.2s ease-in-out infinite}
@keyframes pulse-border{0%,100%{box-shadow:0 0 18px rgba(255,77,94,.12)}50%{box-shadow:0 0 32px rgba(255,77,94,.28)}}
.ch-label{font-family:var(--mono);font-size:.65rem;letter-spacing:.15em;text-transform:uppercase;color:var(--muted);margin-bottom:10px}
.weight-val{font-family:var(--mono);font-size:2.4rem;font-weight:700;line-height:1;margin:6px 0 10px}
.weight-val.ok{color:var(--green)}
.weight-val.lo{color:var(--red)}
.weight-val.err{color:var(--muted);font-size:1.6rem}
.status-badge{display:inline-flex;align-items:center;gap:4px;font-size:.68rem;font-weight:700;letter-spacing:.05em;text-transform:uppercase;padding:3px 9px;border-radius:99px}
.status-badge.ok{background:rgba(0,229,122,.1);color:var(--green);border:1px solid rgba(0,229,122,.25)}
.status-badge.alert{background:rgba(255,77,94,.1);color:var(--red);border:1px solid rgba(255,77,94,.3)}
.cf-line{font-family:var(--mono);font-size:.63rem;color:var(--muted);margin-top:10px}
.cal-btn{margin-top:14px;width:100%;background:transparent;border:1px solid var(--border-hi);color:var(--text);border-radius:8px;padding:8px;font-family:var(--sans);font-size:.78rem;font-weight:600;cursor:pointer;transition:background .2s,border-color .2s}
.cal-btn:hover{background:var(--border-hi);border-color:var(--accent)}
/* action panel */
.panel{background:var(--surface);border:1px solid var(--border);border-radius:14px;padding:18px;margin-bottom:20px}
.panel h2{font-family:var(--mono);font-size:.7rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin-bottom:14px}
.btn{padding:9px 20px;border:none;border-radius:8px;font-family:var(--sans);font-size:.82rem;font-weight:700;cursor:pointer;transition:filter .2s}
.btn:hover{filter:brightness(1.15)}
.btn-tare{background:var(--yellow);color:#111}
.steps{margin-top:14px;background:rgba(0,201,255,.04);border:1px solid rgba(0,201,255,.12);border-radius:8px;padding:12px 14px;font-size:.72rem;color:var(--muted);line-height:1.8}
.steps b{color:var(--text)}
/* toast */
.toast{position:fixed;bottom:24px;right:24px;padding:10px 18px;border-radius:10px;font-size:.8rem;font-weight:600;opacity:0;transform:translateY(6px);transition:opacity .3s,transform .3s;pointer-events:none;max-width:300px;z-index:9999}
.toast.show{opacity:1;transform:translateY(0)}
.toast.success{background:#0d2d1a;border:1px solid var(--green);color:var(--green)}
.toast.error{background:#2d0d10;border:1px solid var(--red);color:var(--red)}
.footer{text-align:center;font-size:.68rem;font-family:var(--mono);color:var(--muted);margin-top:16px}
</style>
</head>
<body>
<div class="header">
  <h1><span class="pulse-dot"></span>Weight Monitor</h1>
  <p>ESP32 · 3-Channel Load Cell System</p>
</div>

<div class="infobar">
  <div class="info-pill">WiFi: <b id="wifi">–</b></div>
  <div class="info-pill">IP: <b id="ip">–</b></div>
  <div class="info-pill">RSSI: <b id="rssi">–</b> dBm</div>
  <div class="info-pill">Uptime: <b id="uptime">–</b></div>
  <div class="info-pill">Updated: <b id="lastUpdate">–</b></div>
</div>

<div class="countdown-bar"><div class="countdown-fill" id="cbar" style="width:100%"></div></div>

<div class="grid" id="grid">
  <div class="card ok-card" id="card0"></div>
  <div class="card ok-card" id="card1"></div>
  <div class="card ok-card" id="card2"></div>
</div>

<div class="panel">
  <h2>Controls</h2>
  <button class="btn btn-tare" onclick="doTare()">⟳ Tare All Scales</button>
  <div class="steps">
    <b>Calibration workflow:</b><br>
    ① Remove all weight from the channel → click <b>Tare All Scales</b><br>
    ② Place exactly <b>100 g</b> on the target channel<br>
    ③ Click <b>⚙ Calibrate (100 g)</b> for that channel — new factor is saved to flash
  </div>
</div>

<div class="toast" id="toast"></div>
<div class="footer">ESP32 Weight Monitor · auto-refresh every 5 s</div>

<script>
const INTERVAL = 5;
let cdLeft = INTERVAL;
let cdTimer;

function fmt(secs){
  const h=Math.floor(secs/3600), m=Math.floor((secs%3600)/60), s=secs%60;
  return [h,m,s].map(v=>String(v).padStart(2,'0')).join(':');
}

function toast(msg, type='success'){
  const el=document.getElementById('toast');
  el.textContent=msg; el.className='toast show '+type;
  clearTimeout(el._t); el._t=setTimeout(()=>el.className='toast',3000);
}

function buildCard(ch, idx){
  const alert=ch.alert;
  const card=document.getElementById('card'+idx);
  card.className='card '+(alert?'alert-card':'ok-card');
  card.innerHTML=`
    <div class="ch-label">Channel ${ch.id}</div>
    <div class="weight-val ${ch.ok?(alert?'lo':'ok'):'err'}">${ch.ok?ch.weight.toFixed(1)+' g':'ERR'}</div>
    <span class="status-badge ${alert?'alert':'ok'}">${alert?'⚠ Low Weight':'● Normal'}</span>
    <div class="cf-line">Cal factor: ${ch.calibFactor.toFixed(2)}</div>
    <button class="cal-btn" onclick="doCalibrate(${idx})">⚙ Calibrate (100 g)</button>
  `;
}

function fetchStatus(){
  fetch('/api/status')
    .then(r=>r.json())
    .then(d=>{
      document.getElementById('wifi').textContent = d.wifi.connected ? '✓ Connected' : '✗ Down';
      document.getElementById('ip').textContent   = d.wifi.ip;
      document.getElementById('rssi').textContent = d.wifi.rssi;
      document.getElementById('uptime').textContent = fmt(d.uptime);
      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
      d.channels.forEach((ch,i)=>buildCard(ch,i));
    })
    .catch(()=>toast('Status fetch failed','error'));
}

function doTare(){
  toast('Taring all scales…');
  fetch('/api/tare',{method:'POST'})
    .then(r=>r.json())
    .then(()=>{ toast('✓ All scales tared!'); fetchStatus(); })
    .catch(()=>toast('Tare failed','error'));
}

function doCalibrate(idx){
  if(!confirm(`Channel ${idx+1}: Confirm 100 g weight is on scale and it has been tared. Proceed?`)) return;
  fetch('/api/calibrate?channel='+idx,{method:'POST'})
    .then(r=>r.json())
    .then(d=>{
      if(d.success) { toast(`✓ Ch${idx+1} calibrated — factor: ${d.newFactor.toFixed(2)}`); fetchStatus(); }
      else toast('Error: '+(d.error||'unknown'),'error');
    })
    .catch(()=>toast('Calibration request failed','error'));
}

function startCountdown(){
  clearInterval(cdTimer); cdLeft=INTERVAL;
  const bar=document.getElementById('cbar');
  bar.style.transition='none'; bar.style.width='100%';
  cdTimer=setInterval(()=>{
    cdLeft--;
    const pct=Math.round((cdLeft/INTERVAL)*100);
    bar.style.transition='width 1s linear';
    bar.style.width=pct+'%';
    if(cdLeft<=0){ fetchStatus(); cdLeft=INTERVAL; bar.style.transition='none'; bar.style.width='100%'; }
  },1000);
}

fetchStatus();
startCountdown();
</script>
</body>
</html>
)rawliteral";


// ─── Helper: update OLED display ────────────────────────────
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Weight Monitor");
  display.setCursor(90, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "No WiFi");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  for (int ch = 0; ch < 3; ch++) {
    int yPos = 14 + (ch * 16);
    display.setTextSize(1);
    display.setCursor(0, yPos);
    display.printf("CH%d:", ch + 1);
    display.setCursor(28, yPos);
    if (dispOk[ch]) {
      char buf[12]; dtostrf(dispWeight[ch], 7, 1, buf);
      display.print(buf); display.print(" g");
    } else { display.print("  ERR"); }
    if (dispOk[ch] && dispWeight[ch] < ALERT_THRESHOLD_G) {
      int tx = 114, ty = yPos;
      display.fillTriangle(tx, ty+7, tx+4, ty, tx+8, ty+7, SSD1306_WHITE);
      display.setCursor(tx+2, ty+2);
      display.setTextColor(SSD1306_BLACK); display.print("!");
      display.setTextColor(SSD1306_WHITE);
    }
  }

  display.drawLine(0, 56, 127, 56, SSD1306_WHITE);
  display.setCursor(0, 58);
  int alertCount = 0;
  for (int i = 0; i < 3; i++) if (alertActive[i]) alertCount++;
  if (alertCount > 0) display.printf("ALERTS: %d", alertCount);
  else display.print("All OK");

  unsigned long secs = millis() / 1000;
  char uptimeBuf[10];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "%02lu:%02lu", secs/3600, (secs/60)%60);
  display.setCursor(90, 58); display.print(uptimeBuf);
  display.display();
}

// ─── Helper: show a centered message on OLED ────────────────
void oledShowMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w1, h1;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
  display.setCursor((SCREEN_WIDTH - w1) / 2, 20); display.print(line1);
  if (line2 && strlen(line2) > 0) {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &w1, &h1);
    display.setCursor((SCREEN_WIDTH - w1) / 2, 38); display.print(line2);
  }
  display.display();
}

// ─── Helper: connect to WiFi ─────────────────────────────────
void connectWiFi() {
  Serial.println("\n[WiFi] Connecting to: " + String(WIFI_SSID));
  oledShowMessage("Connecting WiFi...", WIFI_SSID);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (++attempts > 40) {
      Serial.println("\n[WiFi] ERROR: Could not connect. Rebooting...");
      oledShowMessage("WiFi FAILED", "Rebooting..."); delay(2000); ESP.restart();
    }
  }
  Serial.println("\n[WiFi] Connected!  IP: " + WiFi.localIP().toString());
  oledShowMessage("WiFi Connected!", WiFi.localIP().toString().c_str());
  delay(1500);
}

// ─── Helper: configure SMTP ──────────────────────────────────
void configureEmail() {
  smtpConfig.server.host_name  = SMTP_HOST;
  smtpConfig.server.port       = SMTP_PORT;
  smtpConfig.login.email       = SENDER_EMAIL;
  smtpConfig.login.password    = SENDER_PASSWORD;
  smtpConfig.login.user_domain = "";
}

// ─── Helper: send email alert ────────────────────────────────
void sendLowWeightEmail(int channelIndex, float weightGrams) {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[Email] No WiFi – skipping."); return; }
  SMTP_Message message;
  message.sender.name  = "ESP32 Weight Monitor";
  message.sender.email = SENDER_EMAIL;
  message.subject = "⚠️ Low Weight Alert – Channel " + String(channelIndex + 1);
  message.addRecipient("Admin", RECIPIENT_EMAIL);
  String body = "Channel " + String(channelIndex + 1) + " dropped below threshold.\n"
                "Weight: " + String(weightGrams, 1) + " g  |  Threshold: " + String(ALERT_THRESHOLD_G, 0) + " g\n"
                "-- ESP32 Weight Monitor";
  message.text.content = body;
  message.text.charSet  = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
  if (!smtp.connect(&smtpConfig)) { Serial.println("[Email] SMTP error: " + smtp.errorReason()); return; }
  if (!MailClient.sendMail(&smtp, &message)) Serial.println("[Email] Send error: " + smtp.errorReason());
  else Serial.println("[Email] ✓ Sent.");
  smtp.closeSession();
}

// ─── Helper: tare all scales ─────────────────────────────────
void tareAllScales() {
  Serial.println("[Tare] Re-zeroing all scales...");
  oledShowMessage("TARING...", "Remove all weight!");
  scale1.tare(10); scale2.tare(10); scale3.tare(10);
  alertActive[0] = alertActive[1] = alertActive[2] = false;
  Serial.printf("[Tare] Offsets: %ld  %ld  %ld\n",
    scale1.get_offset(), scale2.get_offset(), scale3.get_offset());
  oledShowMessage("Tare Complete", "All scales zeroed"); delay(1500);
}

// ─── Helper: apply calibration factors to scale objects ──────
void applyCalibrationFactors() {
  scale1.set_scale(calibrationFactor[0]);
  scale2.set_scale(calibrationFactor[1]);
  scale3.set_scale(calibrationFactor[2]);
  Serial.printf("[Cal] Factors applied: %.2f | %.2f | %.2f\n",
    calibrationFactor[0], calibrationFactor[1], calibrationFactor[2]);
}

// ─── Helper: load calibration factors from NVS ───────────────
void loadCalibrationFromNVS() {
  prefs.begin("wmon", true);  // true = read-only
  calibrationFactor[0] = prefs.getFloat("cf0", DEFAULT_CAL_1);
  calibrationFactor[1] = prefs.getFloat("cf1", DEFAULT_CAL_2);
  calibrationFactor[2] = prefs.getFloat("cf2", DEFAULT_CAL_3);
  prefs.end();
  Serial.printf("[NVS] Loaded factors: %.2f | %.2f | %.2f\n",
    calibrationFactor[0], calibrationFactor[1], calibrationFactor[2]);
}

// ─── Helper: save one calibration factor to NVS ──────────────
void saveCalibrationToNVS(int ch, float factor) {
  char key[4]; snprintf(key, sizeof(key), "cf%d", ch);
  prefs.begin("wmon", false);
  prefs.putFloat(key, factor);
  prefs.end();
  Serial.printf("[NVS] Saved cf%d = %.2f\n", ch, factor);
}

// ════════════════════════════════════════════════════════════
//  WEB SERVER HANDLERS
// ════════════════════════════════════════════════════════════

// GET /  – serve the dashboard HTML
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", INDEX_HTML);
}

// ─────────────────────────────────────────────────────────────
// GET /api/status
// Returns a JSON object with live weight readings, alert flags,
// calibration factors, WiFi stats, and uptime.
// ─────────────────────────────────────────────────────────────
void handleApiStatus() {
  // Build compact JSON by hand to avoid heap cost of ArduinoJson
  String json = "{\"channels\":[";
  HX711* scales[3] = {&scale1, &scale2, &scale3};
  for (int i = 0; i < 3; i++) {
    if (i) json += ",";
    json += "{\"id\":"; json += (i + 1);
    json += ",\"weight\":";
    if (dispOk[i]) { json += dispWeight[i]; }
    else           { json += "null"; }
    json += ",\"ok\":";    json += dispOk[i]    ? "true" : "false";
    json += ",\"alert\":"; json += alertActive[i] ? "true" : "false";
    json += ",\"calibFactor\":"; json += calibrationFactor[i];
    json += "}";
  }
  json += "],\"wifi\":{";
  json += "\"connected\":"; json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  json += ",\"ip\":\""; json += WiFi.localIP().toString(); json += "\"";
  json += ",\"rssi\":";  json += WiFi.RSSI();
  json += "},\"uptime\":"; json += (millis() / 1000);
  json += ",\"threshold\":"; json += ALERT_THRESHOLD_G;
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────────────────────
// POST /api/tare
// Tares all three scales and clears alert flags.
// ─────────────────────────────────────────────────────────────
void handleApiTare() {
  Serial.println("[Web] POST /api/tare received");
  tareAllScales();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"success\":true}");
}

// ─────────────────────────────────────────────────────────────
// POST /api/calibrate?channel=N  (N = 0, 1, or 2)
//
// Calibration algorithm with a known 100 g weight:
//   1. Scale must already be tared (zero offset set).
//   2. Place exactly KNOWN_CAL_WEIGHT_G grams on the scale.
//   3. Read the raw ADC value (tare offset already subtracted)
//      using get_value().
//   4. new_factor = raw_value / KNOWN_CAL_WEIGHT_G
//   5. Apply new_factor and persist to NVS.
//
// Why get_value() instead of get_units()?
//   get_units() = get_value() / current_scale_factor
//   get_value() = raw_ADC_average − tare_offset   (unit-free)
//   So the correct factor is simply: raw / known_grams.
// ─────────────────────────────────────────────────────────────
void handleApiCalibrate() {
  if (!server.hasArg("channel")) {
    server.send(400, "application/json", "{\"error\":\"Missing ?channel=N param (0-2)\"}");
    return;
  }

  int ch = server.arg("channel").toInt();
  if (ch < 0 || ch > 2) {
    server.send(400, "application/json", "{\"error\":\"channel must be 0, 1, or 2\"}");
    return;
  }

  Serial.printf("[Cal] Calibrating channel %d with %.0f g weight...\n", ch, KNOWN_CAL_WEIGHT_G);
  oledShowMessage("Calibrating...", ("CH" + String(ch + 1)).c_str());

  // Read raw ADC average (10 samples, tare offset subtracted)
  float rawVal = 0;
  HX711* scalePtr = nullptr;
  switch (ch) {
    case 0: scalePtr = &scale1; break;
    case 1: scalePtr = &scale2; break;
    case 2: scalePtr = &scale3; break;
  }

  if (!scalePtr->is_ready()) {
    server.send(503, "application/json", "{\"error\":\"HX711 not ready\"}");
    return;
  }

  rawVal = scalePtr->get_value(10);  // raw ADC − tare offset, averaged

  Serial.printf("[Cal] Raw ADC reading: %.2f\n", rawVal);

  if (abs(rawVal) < 100) {
    // Very small raw value usually means no weight was placed
    server.send(422, "application/json",
      "{\"error\":\"Raw reading too small – is 100 g actually on the scale?\",\"rawVal\":" + String(rawVal, 2) + "}");
    return;
  }

  float newFactor = rawVal / KNOWN_CAL_WEIGHT_G;
  calibrationFactor[ch] = newFactor;
  scalePtr->set_scale(newFactor);
  saveCalibrationToNVS(ch, newFactor);

  Serial.printf("[Cal] ✓ Channel %d new factor: %.2f  (raw=%.2f / %.0fg)\n",
    ch + 1, newFactor, rawVal, KNOWN_CAL_WEIGHT_G);
  oledShowMessage(("CH" + String(ch+1) + " Cal OK").c_str(), ("F=" + String(newFactor, 0)).c_str());
  delay(1500);

  String json = "{\"success\":true,\"channel\":"; json += ch;
  json += ",\"newFactor\":"; json += newFactor;
  json += ",\"rawValue\":"; json += rawVal;
  json += ",\"knownWeight\":"; json += KNOWN_CAL_WEIGHT_G;
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─── Register routes & start server ─────────────────────────
void setupWebServer() {
  server.on("/",               HTTP_GET,  handleRoot);
  server.on("/api/status",     HTTP_GET,  handleApiStatus);
  server.on("/api/tare",       HTTP_POST, handleApiTare);
  server.on("/api/calibrate",  HTTP_POST, handleApiCalibrate);

  // Simple 404
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("[Web] HTTP server started on port 80");
  Serial.println("[Web] Dashboard: http://" + WiFi.localIP().toString() + "/");
}

// ────────────────────────────────────────────────────────────
//  setup()
// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("  ESP32 – 3× Load Cell + OLED + Web UI");
  Serial.println("========================================\n");

  // ── OLED ──────────────────────────────────────────────────
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("[Init] ERROR: SSD1306 not found – check SDA=21 SCL=22");
  } else {
    display.clearDisplay(); display.display();
    oledShowMessage("Booting...", "Weight Monitor v2.0");
    delay(1500);
  }

  // ── Tare button ───────────────────────────────────────────
  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);

  // ── Load saved calibration factors from NVS ───────────────
  loadCalibrationFromNVS();

  // ── HX711 init ────────────────────────────────────────────
  oledShowMessage("Init HX711...", "Scales 1-3");
  Serial.println("[Init] Initialising HX711 modules...");

  scale1.begin(HX711_1_DOUT, HX711_1_SCK);
  if (scale1.is_ready()) { scale1.tare(10); Serial.println("[Init] HX711 #1 ready."); }
  else Serial.printf("[Init] ERROR: HX711 #1 not found! GPIO %d/%d\n", HX711_1_DOUT, HX711_1_SCK);

  scale2.begin(HX711_2_DOUT, HX711_2_SCK);
  if (scale2.is_ready()) { scale2.tare(10); Serial.println("[Init] HX711 #2 ready."); }
  else Serial.printf("[Init] ERROR: HX711 #2 not found! GPIO %d/%d\n", HX711_2_DOUT, HX711_2_SCK);

  scale3.begin(HX711_3_DOUT, HX711_3_SCK);
  if (scale3.is_ready()) { scale3.tare(10); Serial.println("[Init] HX711 #3 ready."); }
  else Serial.printf("[Init] ERROR: HX711 #3 not found! GPIO %d/%d\n", HX711_3_DOUT, HX711_3_SCK);

  // Apply loaded factors AFTER init (tare sets the zero, set_scale sets the factor)
  applyCalibrationFactors();

  // ── WiFi ──────────────────────────────────────────────────
  connectWiFi();

  // ── Email ─────────────────────────────────────────────────
  configureEmail();

  // ── Web server ────────────────────────────────────────────
  setupWebServer();

  Serial.println("\n[Init] ✓ Setup complete.\n");
  Serial.println("Ch1 (g)\t\tCh2 (g)\t\tCh3 (g)");
  Serial.println("----------------------------------------------");
}

// ────────────────────────────────────────────────────────────
//  loop()
// ────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── 1. Service web server clients ────────────────────────
  server.handleClient();

  // ── 2. Check Tare Button (debounced) ─────────────────────
  static unsigned long lastTareCheck = 0;
  static bool          lastBtnState  = HIGH;
  bool currentBtnState = digitalRead(TARE_BUTTON_PIN);
  if (currentBtnState == LOW && lastBtnState == HIGH) {
    if ((now - lastTareCheck) > DEBOUNCE_MS) { lastTareCheck = now; tareAllScales(); }
  }
  lastBtnState = currentBtnState;

  // ── 3. Read Weights at fixed interval ────────────────────
  static unsigned long lastReadTime = 0;
  if ((now - lastReadTime) < READ_INTERVAL_MS) return;
  lastReadTime = now;

  float w1=0, w2=0, w3=0;
  bool  ok1=false, ok2=false, ok3=false;
  if (scale1.is_ready()) { w1 = scale1.get_units(5); ok1 = true; }
  else Serial.println("[Read] WARNING: HX711 #1 not ready");
  if (scale2.is_ready()) { w2 = scale2.get_units(5); ok2 = true; }
  else Serial.println("[Read] WARNING: HX711 #2 not ready");
  if (scale3.is_ready()) { w3 = scale3.get_units(5); ok3 = true; }
  else Serial.println("[Read] WARNING: HX711 #3 not ready");

  dispWeight[0]=w1; dispWeight[1]=w2; dispWeight[2]=w3;
  dispOk[0]=ok1;    dispOk[1]=ok2;    dispOk[2]=ok3;

  // ── 4. Serial output ─────────────────────────────────────
  if (ok1) Serial.printf("%.1f g\t\t", w1); else Serial.print("ERR\t\t");
  if (ok2) Serial.printf("%.1f g\t\t", w2); else Serial.print("ERR\t\t");
  if (ok3) Serial.printf("%.1f g\n",   w3); else Serial.print("ERR\n");

  // ── 5. Alert Logic ───────────────────────────────────────
  float weights[3] = {w1, w2, w3};
  bool  okFlags[3] = {ok1, ok2, ok3};
  for (int ch = 0; ch < 3; ch++) {
    if (!okFlags[ch]) continue;
    bool below = (weights[ch] < ALERT_THRESHOLD_G);
    if (below) {
      bool cooldown = ((now - lastEmailTime[ch]) > EMAIL_COOLDOWN_MS);
      if (!alertActive[ch] || cooldown) {
        Serial.printf("[Alert] Ch%d below threshold: %.1f g\n", ch+1, weights[ch]);
        sendLowWeightEmail(ch, weights[ch]);
        alertActive[ch] = true; lastEmailTime[ch] = now;
      }
    } else {
      if (alertActive[ch]) {
        Serial.printf("[Alert] Ch%d recovered: %.1f g\n", ch+1, weights[ch]);
        alertActive[ch] = false;
      }
    }
  }

  // ── 6. Refresh OLED ──────────────────────────────────────
  updateOLED();
}

// ============================================================
//  WIRING QUICK-REFERENCE
// ============================================================
//  SSD1306 OLED (I2C)  →  ESP32
//  VCC → 3.3V   GND → GND   SDA → GPIO 21   SCL → GPIO 22
//
//  HX711 #1  DT=GPIO 27  SCK=GPIO 14
//  HX711 #2  DT=GPIO 25  SCK=GPIO 26
//  HX711 #3  DT=GPIO 32  SCK=GPIO 33
//
//  TARE BUTTON: GPIO 15 → GND  (INPUT_PULLUP)
//
//  LOAD CELL → HX711
//  Red(E+)→E+  Black(E-)→E-  White(A-)→A-  Green(A+)→A+
// ============================================================
