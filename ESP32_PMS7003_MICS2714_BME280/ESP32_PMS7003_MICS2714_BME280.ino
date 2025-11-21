/*
  ESP32 Air Quality Dashboard (Web + WebSocket)
  - Sensors:
      * Plantower PMSx003 on UART (RX=16, TX=17)
      * MICS-2714 v1.0 NO2 analog output on GPIO34 (ADC1)
      * BME280 (I2C) for pressure
  - HTTP :80 serves the UI
  - WS   :81 pushes JSON every ~1s:
      {"pm01":12,"pm25":34,"pm10":56,"no2_mv":742,"pres":1013,"ts":1731045600}

  Libraries:
    - PMserial
    - arduinoWebSockets (Links2004)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <PMserial.h>
#include "esp32-hal-adc.h"   // for analogReadMilliVolts, attenuation
#include <Wire.h>

// -------------------- USER CONFIG --------------------
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// PMSx003 wired to UART on GPIO16 (RX) and GPIO17 (TX)
SerialPM pms(PMSx003, 16, 17);

// MICS-2714 v1.0 analog OUT -> GPIO34 (ADC1 only; ADC2 conflicts with WiFi)
#define NO2_PIN 34
// averaging for noise reduction
const int NO2_SAMPLES = 32;

// -------------------- BME280 DEFINES -----------------
#define BME280_ADDR   0x76
#define REG_ID        0xD0
#define REG_RESET     0xE0
#define REG_CTRL_HUM  0xF2
#define REG_STATUS    0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_PRESS_MSB 0xF7
#define REG_TEMP_MSB  0xFA
#define REG_HUM_MSB   0xFD

// Calibration storage
uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3; int16_t dig_P4; int16_t dig_P5; int16_t dig_P6; int16_t dig_P7; int16_t dig_P8; int16_t dig_P9;
uint8_t dig_H1;  int16_t dig_H2; uint8_t dig_H3; int16_t dig_H4; int16_t dig_H5; int8_t dig_H6;
int32_t t_fine;

// -------------------- HTTP + WS ----------------------
WebServer server(80);
WebSocketsServer ws(81);

unsigned long lastPush = 0;
const unsigned long PUSH_EVERY_MS = 1000;

// Simple, self-contained HTML/JS/CSS (no external CDNs)
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Air Quality</title>
<style>
  :root{
    --bg:#0b1220; --card:#111a2b; --muted:#8ea0bf; --ok:#22c55e; --warn:#f59e0b; --bad:#ef4444;
    --purple:#a855f7; --yellow:#fbbf24; --orange:#fb923c; --red:#ef4444; --maroon:#991b1b;
    --text:#e6edf6; --chip:#1b2842;
  }
  *{box-sizing:border-box} body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 system-ui,Segoe UI,Roboto}
  .wrap{max-width:1100px;margin:24px auto;padding:0 16px}
  header{display:flex;align-items:center;gap:12px;margin-bottom:16px;flex-wrap:wrap}
  .dot{width:10px;height:10px;border-radius:50%}
  .card-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}
  .card{background:var(--card);border-radius:16px;padding:16px;box-shadow:0 6px 24px rgba(0,0,0,.25)}
  .metric{display:flex;justify-content:space-between;align-items:end}
  .metric h2{margin:0;font-size:16px;color:var(--muted)}
  .metric .val{font-size:28px;font-weight:700}
  .chip{display:inline-flex;align-items:center;gap:8px;background:var(--chip);border-radius:999px;padding:6px 10px;color:#c9d7f2}
  .row{display:flex;gap:12px;flex-wrap:wrap;margin-top:12px}
  .grow{flex:1 1 340px}
  canvas{width:100%;height:240px;background:#0e1728;border-radius:12px}
  footer{margin-top:16px;color:var(--muted)}
  @media (max-width:760px){.card-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div class="chip"><span class="dot" id="wsdot" style="background:#999"></span><span id="wslabel">Connecting…</span></div>
    <div class="chip">Updated: <span id="ts">—</span></div>
    <div class="chip">Status: <strong id="aqiLabel">—</strong></div>
  </header>

  <div class="card-grid">
    <div class="card">
      <div class="metric"><h2>PM1.0</h2><div class="val" id="pm01">—</div></div>
      <div class="row"><div class="chip">μg/m³</div></div>
    </div>
    <div class="card">
      <div class="metric"><h2>PM2.5</h2><div class="val" id="pm25">—</div></div>
      <div class="row"><div class="chip" id="pm25Chip">—</div></div>
    </div>
    <div class="card">
      <div class="metric"><h2>PM10</h2><div class="val" id="pm10">—</div></div>
      <div class="row"><div class="chip">μg/m³</div></div>
    </div>
    <div class="card">
      <div class="metric"><h2>Pressure</h2><div class="val" id="pres">—</div></div>
      <div class="row"><div class="chip">hPa</div></div>
    </div>
  </div>

  <div class="row">
    <div class="card grow">
      <h2 style="margin:0 0 8px;color:var(--muted);font-size:16px">PM2.5 Trend (last 180s)</h2>
      <canvas id="chart"></canvas>
    </div>
    <div class="card" style="min-width:240px">
      <div class="metric"><h2>NO₂ (MICS-2714)</h2><div class="val" id="no2mv">—</div></div>
      <div class="row"><div class="chip">mV (raw)</div></div>
      <p style="margin:10px 0 0;color:var(--muted);font-size:12px">Calibrate to convert mV → ppb.</p>
    </div>
  </div>

  <footer>ESP32 Air Quality Dashboard • WebSocket feed • Local network</footer>
</div>

<script>
  // WebSocket
  let ws, dataPM25=[];
  const MAX_POINTS = 180; // ~3 minutes at 1s sampling
  const el = id => document.getElementById(id);

  function aqiCatPM25(v){
    // US EPA breakpoints
    if (v<=12.0) return ["Good","#22c55e"];
    if (v<=35.4) return ["Moderate","#f59e0b"];
    if (v<=55.4) return ["Unhealthy for SG","#fb923c"];
    if (v<=150.4) return ["Unhealthy","#ef4444"];
    if (v<=250.4) return ["Very Unhealthy","#a855f7"];
    return ["Hazardous","#991b1b"];
  }

  function connectWS(){
    const url = `ws://${location.hostname}:81/`;
    ws = new WebSocket(url);
    ws.onopen = () => { el("wsdot").style.background="#22c55e"; el("wslabel").textContent="Live"; };
    ws.onclose= () => { el("wsdot").style.background="#999"; el("wslabel").textContent="Disconnected"; setTimeout(connectWS, 1000); };
    ws.onmessage = (ev) => {
      try{
        const d = JSON.parse(ev.data);  // {pm01,pm25,pm10,no2_mv,pres,ts}
        el("pm01").textContent  = d.pm01.toFixed(0);
        el("pm25").textContent  = d.pm25.toFixed(0);
        el("pm10").textContent  = d.pm10.toFixed(0);
        el("no2mv").textContent = d.no2_mv.toFixed(0);
        el("pres").textContent  = d.pres.toFixed(0);

        const cat = aqiCatPM25(d.pm25);
        el("pm25Chip").textContent = cat[0];
        el("pm25Chip").style.background = "#1b2842";
        el("aqiLabel").textContent = cat[0];
        el("aqiLabel").style.color = cat[1];

        const dt = new Date(d.ts*1000);
        el("ts").textContent = dt.toLocaleTimeString();

        dataPM25.push(d.pm25);
        if (dataPM25.length>MAX_POINTS) dataPM25.shift();
        drawChart();
      }catch(e){}
    };
  }

  // tiny canvas line chart
  const canvas = document.getElementById("chart");
  const ctx = canvas.getContext("2d");

  function sizeCanvas(){
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width  = Math.floor(rect.width * dpr);
    canvas.height = Math.floor(240 * dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);
    drawChart();
  }
  window.addEventListener("resize", sizeCanvas);

  function drawChart(){
    const w = canvas.clientWidth, h = 240;
    ctx.clearRect(0,0,w,h);

    // axes
    ctx.strokeStyle="#2a3652"; ctx.lineWidth=1;
    for (let i=0;i<=4;i++){
      const y = 10 + i*(h-20)/4;
      ctx.beginPath(); ctx.moveTo(40,y); ctx.lineTo(w-10,y); ctx.stroke();
    }

    // y-scale based on data or fallback
    const vals = dataPM25.length? dataPM25 : [0];
    const maxV = Math.max(60, Math.ceil(Math.max(...vals)/10)*10);
    const minV = 0;

    // labels
    ctx.fillStyle="#8ea0bf"; ctx.font="12px system-ui";
    ctx.fillText(maxV.toFixed(0), 4, 14);
    ctx.fillText(((maxV+minV)/2).toFixed(0), 4, h/2);
    ctx.fillText(minV.toFixed(0), 4, h-6);
    ctx.fillText("PM2.5 (µg/m³)", 40, 14);

    if (dataPM25.length>1){
      const left=40, right=w-10, top=10, bottom=h-10;
      const dx = (right-left)/(Math.max(1,dataPM25.length-1));
      ctx.beginPath();
      ctx.strokeStyle="#69b1ff"; ctx.lineWidth=2;
      dataPM25.forEach((v,i)=>{
        const x = left + i*dx;
        const y = bottom - (v-minV)/(maxV-minV)*(bottom-top);
        if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
      });
      ctx.stroke();
    }
  }

  sizeCanvas();
  connectWS();
</script>
</body>
</html>
)HTML";

//---------------------- BME280 LOW-LEVEL -------------
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
uint8_t read8(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}
uint16_t read16(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, (uint8_t)2);
  uint16_t msb = Wire.read();
  uint16_t lsb = Wire.read();
  return (msb << 8) | lsb;
}
uint16_t read16_LE(uint8_t reg) {
  uint16_t v = read16(reg);
  return (v >> 8) | (v << 8);
}
int16_t readS16_LE(uint8_t reg) {
  return (int16_t)read16_LE(reg);
}
void readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, len);
  uint8_t i = 0;
  while (Wire.available() && i < len) buf[i++] = Wire.read();
}

// ---------- Calibration / raw reads ----------
bool readCalibration() {
  uint8_t id = read8(REG_ID);
  if (id != 0x60) {
    Serial.print("BME280 ID: 0x"); Serial.println(id, HEX);
  }
  dig_T1 = read16_LE(0x88);
  dig_T2 = readS16_LE(0x8A);
  dig_T3 = readS16_LE(0x8C);

  dig_P1 = read16_LE(0x8E);
  dig_P2 = readS16_LE(0x90);
  dig_P3 = readS16_LE(0x92);
  dig_P4 = readS16_LE(0x94);
  dig_P5 = readS16_LE(0x96);
  dig_P6 = readS16_LE(0x98);
  dig_P7 = readS16_LE(0x9A);
  dig_P8 = readS16_LE(0x9C);
  dig_P9 = readS16_LE(0x9E);

  dig_H1 = read8(0xA1);
  dig_H2 = (int16_t)read16_LE(0xE1);
  dig_H3 = read8(0xE3);
  int8_t e4 = (int8_t)read8(0xE4);
  int8_t e5 = (int8_t)read8(0xE5);
  int8_t e6 = (int8_t)read8(0xE6);
  dig_H4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
  dig_H5 = (int16_t)((e6 << 4) | ((e5 & 0xF0) >> 4));
  dig_H6 = (int8_t)read8(0xE7);

  return true;
}

// Read raw pressure + temperature from data registers
bool readRaw(int32_t &adc_P, int32_t &adc_T) {
  uint8_t buf[8];
  readBytes(REG_PRESS_MSB, buf, 8);
  // P_MSB, P_LSB, P_XLSB
  adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | ((int32_t)buf[2] >> 4);
  // T_MSB, T_LSB, T_XLSB
  adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | ((int32_t)buf[5] >> 4);
  return true;
}

// ---------- Compensation (Bosch, float version) ----------
float compensateTemperature(int32_t adc_T) {
  float var1 = (((float)adc_T)/16384.0f - ((float)dig_T1)/1024.0f) * ((float)dig_T2);
  float var2 = ((((float)adc_T)/131072.0f - ((float)dig_T1)/8192.0f) *
                (((float)adc_T)/131072.0f - ((float)dig_T1)/8192.0f)) * ((float)dig_T3);
  t_fine = (int32_t)(var1 + var2);
  float T = (var1 + var2) / 5120.0f;
  return T; // °C
}

float compensatePressure(int32_t adc_P) {
  int64_t var1, var2, p;
  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + ((int64_t)dig_P4 << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1) * ((int64_t)dig_P1)) >> 33;
  if (var1 == 0) return 0;
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);
  float pressure_pa = (float)p / 256.0f;
  return pressure_pa / 100.0f;  // hPa
}

// -------------------- NO2 READ --------------------
uint16_t readNO2mV() {
  analogSetPinAttenuation(NO2_PIN, ADC_11db); // ~0-3.3V range
  uint32_t acc = 0;
  for (int i=0;i<NO2_SAMPLES;i++){
    acc += analogReadMilliVolts(NO2_PIN);
    delayMicroseconds(250);
  }
  return (uint16_t)(acc / NO2_SAMPLES);
}

// -------------------- WEBSOCKET --------------------
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  switch(type){
    case WStype_CONNECTED:{
      IPAddress ip = ws.remoteIP(num);
      Serial.printf("[WS] Client %u connected: %s\n", num, ip.toString().c_str());
    }break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected\n", num);
      break;
    case WStype_TEXT:
      // (Optional) handle incoming commands
      break;
    default: break;
  }
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  // PMS sensor
  Serial.println("Starting PMS sensor…");
  pms.init();  // config serial port at 9600 on the given pins

  Wire.begin();

  // ADC setup (NO2)
  pinMode(NO2_PIN, INPUT);
  analogSetPinAttenuation(NO2_PIN, ADC_11db);

  // WiFi
  Serial.printf("WiFi: connecting to %s…\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 20000){
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status()==WL_CONNECTED){
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed—check credentials/power.");
  }

  // HTTP server: serve the dashboard
  server.on("/", HTTP_GET, [](){
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.begin();
  Serial.println("HTTP server on :80");

  // BME280 init
  Serial.print("BME280 at 0x"); Serial.println(BME280_ADDR, HEX);
  writeRegister(REG_RESET, 0xB6); 
  delay(300);
  if (!readCalibration()) {
    Serial.println("Calibration read failed");
    // continue anyway
  }
  writeRegister(REG_CTRL_HUM,  0x01); // osrs_h x1
  writeRegister(REG_CTRL_MEAS, 0x27); // osrs_t x1, osrs_p x1, normal mode
  writeRegister(REG_CONFIG,    0xA0); // t_sb 1000ms, filter off

  // WebSocket server on :81
  ws.begin();
  ws.onEvent(onWsEvent);
  Serial.println("WebSocket server on :81");
}

// -------------------- LOOP --------------------
void loop() {
  // Service servers
  server.handleClient();
  ws.loop();

  // Read sensors and broadcast once per second
  if (millis() - lastPush >= PUSH_EVERY_MS) {
    lastPush = millis();

    // --- BME280: get raw, then compensated temp + pressure ---
    int32_t adc_P, adc_T;
    if (!readRaw(adc_P, adc_T)) {
      Serial.println("Raw BME280 read failed");
      return;
    }
    float tempC        = compensateTemperature(adc_T);   // updates t_fine
    float pressure_hPa = compensatePressure(adc_P);      // uses t_fine
    uint16_t pres      = (uint16_t)(pressure_hPa + 0.5f); // integer hPa

    // PMS
    pms.read();  // update pm01, pm25, pm10
    uint16_t pm01 = pms.pm01;
    uint16_t pm25 = pms.pm25;
    uint16_t pm10 = pms.pm10;

    // MICS-2714 (raw mV)
    uint16_t no2_mv = readNO2mV();

    // Debug
    Serial.printf("PM1.0=%u  PM2.5=%u  PM10=%u ug/m3  NO2=%u mV  P=%u hPa  T=%.2f C\n",
                  pm01, pm25, pm10, no2_mv, pres, tempC);

    // JSON (no ArduinoJson)
    // {"pm01":12,"pm25":34,"pm10":56,"no2_mv":742,"pres":1013,"ts":1731045600}
    char msg[160];
    unsigned long ts = millis()/1000;
    snprintf(msg, sizeof(msg),
             "{\"pm01\":%u,\"pm25\":%u,\"pm10\":%u,\"no2_mv\":%u,\"pres\":%u,\"ts\":%lu}",
             pm01, pm25, pm10, no2_mv, pres, ts);

    ws.broadcastTXT(msg);
  }
}
