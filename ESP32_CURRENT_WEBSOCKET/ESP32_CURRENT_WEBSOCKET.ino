/*
  ESP32 + ACS712-20A (GPIO34 ADC) -> Live RMS current over WebSocket + Web UI

  Features:
  - AC Irms calculation from ADC samples (windowed, 200 ms default)
  - Web server (GET /) serves a self-contained dashboard (no CDN)
  - WebSocket (/ws, port 81) pushes ~5Hz JSON: {irms, vref_mv, samples, ts}
  - "Calibrate" button in UI learns zero-offset (no load)

  Libraries:
  - ESP32 Arduino core: WiFi.h, WebServer.h
  - arduinoWebSockets (by Links2004): WebSocketsServer.h
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ==================== USER CONFIG ====================
#define WIFI_SSID "MyProject"
#define WIFI_PASS "12345678"

// ADC pin (ADC1 recommended for WiFi coexistence). GPIO34 is input-only.
#define PIN_ADC            34

// ESP32 ADC: we'll use 12-bit (0..4095) and 11dB attenuation (~0..3.3V)
#define ADC_MAX_COUNTS     4095
#define ADC_RANGE_MV       3300   // effective millivolt range at the pin

// ACS712-20A nominal sensitivity (~100 mV/A at 5V supply)
#define ACS_SENSITIVITY_MV_PER_A  100.0f

// Sampling params: ~5kHz (200us each) for 200ms window -> ~1000 samples per window
#define SAMPLE_US          200
#define WINDOW_MS          200
#define SEND_INTERVAL_MS   200

// Serial log
#define LOG_SERIAL         1
// =====================================================

#if LOG_SERIAL
  #define LOGF(...)  Serial.printf(__VA_ARGS__)
#else
  #define LOGF(...)
#endif

WebServer server(80);
WebSocketsServer ws(81);

volatile bool doCalibrate = false;   // set from WS to perform zero-offset calibration

// Dynamic state
uint32_t lastWindowStartMs = 0;
uint32_t lastSendMs        = 0;

// Zero (midpoint) reference in millivolts seen at ADC pin (will be calibrated)
float vref_mV = ADC_RANGE_MV / 2.0f;

// ---------- RMS computation over a fixed time window ----------
float computeIrmsAndUpdateVref(uint16_t &samplesOut) {
  const uint32_t window_us = WINDOW_MS * 1000UL;
  const uint32_t t_start   = micros();

  uint32_t count = 0;
  double accSq = 0.0;    // sum of squared (v - vref)
  double meanV = 0.0;    // for vref refinement on calibration
  double meanN = 0.0;

  while ((micros() - t_start) < window_us) {
    uint16_t raw = analogRead(PIN_ADC);         // 0..4095
    float v_mV = (raw / (float)ADC_MAX_COUNTS) * ADC_RANGE_MV;

    // Running mean for potential vref update
    meanN += 1.0;
    meanV += (v_mV - meanV) / meanN;

    float dv = v_mV - vref_mV;
    accSq += (double)dv * (double)dv;
    count++;

    delayMicroseconds(SAMPLE_US);
  }

  samplesOut = count;

  if (doCalibrate) {
    // Capture midpoint with NO LOAD for best result
    vref_mV = (float)meanV;
    doCalibrate = false;
  }

  if (count == 0) return 0.0f;

  float vrms_mV = sqrt(accSq / (double)count);
  float irms_A  = vrms_mV / ACS_SENSITIVITY_MV_PER_A;
  return irms_A;
}

// ---------- Minimal, self-contained web UI ----------
const char* INDEX_HTML PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ACS712 Current Monitor</title>
<style>
  :root{--bg:#0b1220;--card:#121a2b;--fg:#e8eefc;--muted:#9bb0d1;--accent:#4da3ff;--ok:#22c55e}
  *{box-sizing:border-box}
  body{margin:0;background:linear-gradient(160deg,#0b1220,#0d1528 60%);color:var(--fg);font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Arial}
  header{padding:18px 16px;border-bottom:1px solid #1d2a44;background:rgba(0,0,0,.2);backdrop-filter:blur(6px)}
  .wrap{max-width:900px;margin:0 auto;padding:20px}
  h1{margin:0;font-size:20px;letter-spacing:.4px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:16px;margin-top:18px}
  .card{background:var(--card);border:1px solid #1d2a44;border-radius:16px;padding:16px;box-shadow:0 10px 30px rgba(0,0,0,.25)}
  .title{font-size:14px;color:var(--muted);margin:0 0 6px}
  .value{font-size:40px;font-weight:700;letter-spacing:.5px}
  .units{font-size:16px;color:var(--muted);margin-left:6px}
  .row{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
  button{background:var(--accent);border:none;color:#021126;font-weight:700;padding:10px 14px;border-radius:10px;cursor:pointer;transition:transform .08s ease,opacity .2s ease}
  button:hover{transform:translateY(-1px)}
  .pill{background:#0f233e;border:1px solid #1d2a44;color:var(--muted);padding:6px 10px;border-radius:999px;font-size:12px}
  .status{display:inline-flex;align-items:center;gap:8px}
  .dot{width:10px;height:10px;border-radius:50%;background:#ef4444;box-shadow:0 0 8px #ef4444aa}
  .dot.ok{background:var(--ok);box-shadow:0 0 8px #22c55ebb}
  .mono{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace}
  canvas{width:100%;height:180px;display:block}
  .footer{margin-top:18px;color:var(--muted);font-size:12px}
</style>
</head>
<body>
  <header>
    <div class="wrap row" style="justify-content:space-between">
      <h1>ACS712 Current Monitor</h1>
      <div class="status"><span id="wsdot" class="dot"></span><span id="wstxt">Disconnected</span></div>
    </div>
  </header>

  <main class="wrap">
    <div class="grid">
      <div class="card">
        <p class="title">RMS Current</p>
        <div><span id="irms" class="value">0.000</span><span class="units">A</span></div>
        <div class="row" style="margin-top:10px">
          <button id="cal">Calibrate Zero</button>
          <span class="pill mono" id="meta">—</span>
        </div>
      </div>
      <div class="card">
        <p class="title">Live Chart (last 60s)</p>
        <canvas id="chart"></canvas>
      </div>
      <div class="card">
        <p class="title">Sensor Reference (ADC midpoint)</p>
        <div><span id="vref" class="value">—</span><span class="units">mV</span></div>
      </div>
    </div>

    <p class="footer">Tip: before measuring AC load, click <b>Calibrate Zero</b> with no load to capture the sensor’s midpoint.</p>
  </main>

<script>
(function(){
  const irmsEl = document.getElementById('irms');
  const vrefEl = document.getElementById('vref');
  const metaEl = document.getElementById('meta');
  const calBtn = document.getElementById('cal');
  const dot = document.getElementById('wsdot');
  const wstxt = document.getElementById('wstxt');

  const canvas = document.getElementById('chart');
  const ctx = canvas.getContext('2d');
  let W, H;
  function resize(){ W = canvas.clientWidth*devicePixelRatio; H = canvas.clientHeight*devicePixelRatio; canvas.width=W; canvas.height=H; }
  resize(); window.addEventListener('resize', resize);

  const maxPoints = 300; // ~60s if 5Hz updates
  const data = [];

  function draw(){
    ctx.clearRect(0,0,W,H);
    if (data.length < 2) return;
    const pad = 10 * devicePixelRatio;
    const xmin = 0, xmax = data.length-1;
    let ymin = Math.min(...data), ymax = Math.max(...data);
    if (ymin === ymax) { ymin -= 0.001; ymax += 0.001; }
    ctx.globalAlpha = 0.6;
    ctx.lineWidth = 1*devicePixelRatio;
    ctx.strokeStyle = '#1d2a44';
    ctx.beginPath();
    ctx.moveTo(pad, pad); ctx.lineTo(pad, H-pad); ctx.lineTo(W-pad, H-pad);
    ctx.stroke();
    ctx.globalAlpha = 1;

    ctx.lineWidth = 2*devicePixelRatio;
    ctx.strokeStyle = '#4da3ff';
    ctx.beginPath();
    for (let i=0;i<data.length;i++){
      const x = pad + (W-2*pad) * (i/(xmax||1));
      const y = H-pad - (H-2*pad) * ((data[i]-ymin)/(ymax-ymin));
      (i===0)? ctx.moveTo(x,y) : ctx.lineTo(x,y);
    }
    ctx.stroke();
  }

  function pushPoint(v){
    data.push(v);
    if (data.length > maxPoints) data.shift();
    draw();
  }

  let ws;
  function connect(){
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    ws = new WebSocket(proto + '://' + location.host.replace(/:\d+$/,'') + ':81/ws');
    ws.onopen = () => { dot.classList.add('ok'); wstxt.textContent='Connected'; };
    ws.onclose = () => { dot.classList.remove('ok'); wstxt.textContent='Disconnected'; setTimeout(connect, 1000); };
    ws.onerror = () => { ws.close(); };
    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        if (msg.type === 'telemetry') {
          irmsEl.textContent = (msg.irms).toFixed(3);
          vrefEl.textContent = Math.round(msg.vref_mv);
          metaEl.textContent = `samples=${msg.samples} t=${(msg.ts/1000).toFixed(1)}s`;
          pushPoint(msg.irms);
        } else if (msg.type === 'info' && msg.text) {
          metaEl.textContent = msg.text;
        }
      } catch{}
    };
  }
  connect();

  calBtn.addEventListener('click', () => {
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({cmd:'calibrate'}));
  });
})();
</script>
</body>
</html>
)HTML";

// ---------- HTTP & WebSocket ----------
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = ws.remoteIP(num);
      LOGF("[WS] Client %u connected from %s\n", num, ip.toString().c_str());
      String hi = "{\"type\":\"info\",\"text\":\"Ready. Click Calibrate Zero with no load.\"}";
      ws.sendTXT(num, hi);
      break;
    }
    case WStype_DISCONNECTED:
      LOGF("[WS] Client %u disconnected\n", num);
      break;
    case WStype_TEXT: {
      // Build a String from raw payload safely
      String s;
      s.reserve(length + 1);
      s.concat((const char*)payload, (unsigned int)length);
      if (s.indexOf(F("calibrate")) >= 0) doCalibrate = true;
      break;
    }
    default: break;
  }
}

// ---------- WiFi ----------
void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  LOGF("Connecting to %s", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(300); LOGF(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    LOGF("\nWiFi OK: %s\n", WiFi.localIP().toString().c_str());
  } else {
    LOGF("\nWiFi failed. Starting AP...\n");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ACS712_Monitor", "12345678");
    LOGF("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }
}

void setup() {
#if LOG_SERIAL
  Serial.begin(115200);
  delay(50);
  Serial.println();
#endif

  // ESP32 ADC config
  analogReadResolution(12);                        // 0..4095
  analogSetPinAttenuation(PIN_ADC, ADC_11db);     // ≈0..3.3V range on that pin

  startWiFi();

  server.on("/", handleRoot);
  server.begin();
  LOGF("HTTP server started on :80\n");

  ws.begin();
  ws.onEvent(onWsEvent);
  LOGF("WebSocket started on :81 (/ws)\n");

  // Initial quick auto-calibration at boot (no-load recommended)
  LOGF("Boot calibration...\n");
  const int N = 300;
  double m = 0.0;
  for (int i = 0; i < N; ++i) {
    uint16_t raw = analogRead(PIN_ADC);
    float v_mV = (raw / (float)ADC_MAX_COUNTS) * ADC_RANGE_MV;
    m += (v_mV - m) / (i + 1);
    delay(2);
  }
  vref_mV = (float)m;
  LOGF("vref_mV=%.1f\n", vref_mV);

  lastWindowStartMs = millis();
  lastSendMs = millis();
}

void loop() {
  server.handleClient();
  ws.loop();

  const uint32_t now = millis();

  // Compute Irms for the last window
  static float lastIrms = 0.0f;
  static uint16_t lastSamples = 0;

  if (now - lastWindowStartMs >= WINDOW_MS) {
    lastWindowStartMs = now;
    lastIrms = computeIrmsAndUpdateVref(lastSamples);
  }

  // Push telemetry periodically
  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;

    String msg;
    msg.reserve(128);
    msg += F("{\"type\":\"telemetry\",\"irms\":");
    msg += String(lastIrms, 4);
    msg += F(",\"vref_mv\":");
    msg += String(vref_mV, 1);
    msg += F(",\"samples\":");
    msg += lastSamples;
    msg += F(",\"ts\":");
    msg += now;
    msg += "}";

    ws.broadcastTXT(msg);
  }
}
