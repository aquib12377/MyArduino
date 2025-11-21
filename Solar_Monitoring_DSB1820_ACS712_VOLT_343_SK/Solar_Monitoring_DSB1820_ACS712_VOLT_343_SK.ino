/*
  ESP32 Solar Monitor (Web + WebSocket)
  - DS18B20 temperature on GPIO 4  (pull-up 4.7k to 3.3V)
  - ACS712-5A current on GPIO 34   (ADC1)
  - Voltage divider on GPIO 35     (ADC1)
  - Web page at http://<ip>/ with live values via WebSocket (port 81)

  Board: ESP32 Dev Module
  Core:  ESP32 Arduino
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include "ACS712.h"

// ---------------- WiFi ----------------
const char* WIFI_SSID     = "MyProject";
const char* WIFI_PASSWORD = "12345678";

// ---------------- Pins ----------------
#define PIN_DS18B20   4     // DS18B20 data (needs 4.7k pull-up to 3.3V)
ACS712  ACS(34, 3.3, 4095, 185);
#define PIN_VSENSE    35    // Voltage div -> ADC1
// --- Voltage divider values (Ohms) ---
// Set these to YOUR actual resistor values (measure if possible)
static const float R1 = 30000.0f;  // top resistor (to panel +)
static const float R2 =  7500.0f;  // bottom resistor (to GND)

// Effective ADC full-scale voltage for 11dB (use 3.30..3.60 as per your board/cal)
static const float ADC_FS_VOLT_11dB = 3.30f;  // start with 3.30; you can tweak after calibration

// Optional small gain correction (two-point calibration). Keep 1.000f initially.
static float V_CAL_GAIN = 1.000f;

// ---------------- ADC setup -----------
static const float ADC_REF_VOLT = 3.3f;   // effective reference
static const int   ADC_MAX      = 4095;   // 12-bit ADC

static inline float adcToVolt(int raw) {
  return (raw * ADC_REF_VOLT) / ADC_MAX;
}

void setupADC() {
  analogReadResolution(12);
  // 11dB ~ 0..~3.6V input range mapping to 0..4095
  analogSetPinAttenuation(PIN_VSENSE, ADC_11db);
}

// ------------- Sensors calib ----------
static const float VOLT_DIVIDER_RATIO = 11.0f;   // e.g., 100k:10k board ≈ 11:1
static const float ACS_SENS_V_PER_A   = 0.185f;  // ACS712-5A ≈ 185 mV/A
static float       ACS_ZERO_OFFSET_V  = 1.65f;   // will auto-zero at boot

// ------------- Sampling ----------------
static const uint16_t NUM_SAMPLES      = 400;   // per averaged read
static const uint16_t SAMPLE_DELAY_US  = 200;   // µs between samples

int readADC_Averaged(uint8_t pin, uint16_t n) {
  uint32_t acc = 0;
  for (uint16_t k = 0; k < n; ++k) {
    acc += analogRead(pin);
    if (SAMPLE_DELAY_US) delayMicroseconds(SAMPLE_DELAY_US);
  }
  return (int)(acc / n);
}

void autoZeroACS() {
  // Ensure NO LOAD at boot for correct zeroing
  // int raw = readADC_Averaged(PIN_ACS712, NUM_SAMPLES * 2);
  // ACS_ZERO_OFFSET_V = adcToVolt(raw);
}

// ------------- Computations ------------
float readPanelVolt() {
  // Average several ADC samples for noise reduction
  int   raw   = readADC_Averaged(PIN_VSENSE, NUM_SAMPLES);
  float v_adc = (raw * ADC_FS_VOLT_11dB) / 4095.0f;          // volts at ESP32 ADC pin
  float v_in  = v_adc * ((R1 + R2) / R2) * V_CAL_GAIN;       // actual panel-side voltage
  return v_in;
}
float readCurrentA() {
  return   ACS.mA_DC(50)/1000.0;
}

// ------------- DS18B20 -----------------
OneWire oneWire(PIN_DS18B20);
DallasTemperature dallas(&oneWire);

// ------------- Web ---------------------
WebServer server(80);
WebSocketsServer ws(81);  // root path "/"

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Solar Monitor</title>
<style>
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Helvetica,Arial;margin:20px;max-width:900px}
h1{margin:0 0 8px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}
.card{border:1px solid #ddd;border-radius:12px;padding:16px;box-shadow:0 2px 8px rgba(0,0,0,.06)}
.label{color:#666;font-size:12px;text-transform:uppercase;letter-spacing:.06em}
.value{font-size:28px;margin-top:6px;font-weight:600}
.unit{font-size:.6em;color:#555;margin-left:6px}
.footer{margin-top:16px;color:#666;font-size:12px}
code{background:#f3f3f3;padding:2px 6px;border-radius:6px}
</style>
</head><body>
<h1>ESP32 Solar Monitor</h1>
<div class="grid">
  <div class="card"><div class="label">Panel Voltage</div><div class="value" id="v">--<span class="unit">V</span></div></div>
  <div class="card"><div class="label">Panel Current</div><div class="value" id="i">--<span class="unit">A</span></div></div>
  <div class="card"><div class="label">Panel Power</div><div class="value" id="p">--<span class="unit">W</span></div></div>
  <div class="card"><div class="label">Temperature</div><div class="value" id="t">--<span class="unit">°C</span></div></div>
</div>
<div class="footer">WebSocket: <code id="ws">connecting...</code></div>
<script>
const v=document.getElementById('v'),i=document.getElementById('i'),p=document.getElementById('p'),t=document.getElementById('t'),wsL=document.getElementById('ws');
const url=(location.protocol==='https:'?'wss://':'ws://')+location.hostname+':81/';
const sock=new WebSocket(url);
sock.onopen = ()=> wsL.textContent='connected';
sock.onclose= ()=> wsL.textContent='disconnected';
sock.onmessage = ev=>{
  try{
    const o=JSON.parse(ev.data);
    if('voltage'in o) v.firstChild.nodeValue=o.voltage.toFixed(2);
    if('current'in o) i.firstChild.nodeValue=o.current.toFixed(3);
    if('power'  in o) p.firstChild.nodeValue=o.power.toFixed(2);
    if('tempC'  in o) t.firstChild.nodeValue=o.tempC.toFixed(2);
  }catch(e){}
};
</script>
</body></html>
)HTML";

// Health and raw JSON endpoints (optional)
void handleRoot(){ server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handleHealth(){ server.send(200, "application/json", "{\"ok\":true}"); }
void handleNow(){
  // Read once and return JSON
  dallas.requestTemperatures();
  float tC = dallas.getTempCByIndex(0);
  if (tC == DEVICE_DISCONNECTED_C) tC = NAN;

  float V = readPanelVolt();
  float I = readCurrentA();
  float P = V * I;

  char buf[160];
  snprintf(buf, sizeof(buf),
           "{\"voltage\":%.4f,\"current\":%.5f,\"power\":%.4f,\"tempC\":%.4f}",
           V, I, P, isnan(tC) ? 0.0f : tC);
  server.send(200, "application/json", buf);
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  (void)num; (void)payload; (void)length;
  // No per-client logic needed; we broadcast from loop()
}

// ------------- Timers ------------------
const uint32_t SAMPLE_MS = 1000;
uint32_t lastSample = 0;

// ------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  delay(100);
ACS.autoMidPoint();
  setupADC();

  // DS18B20
  dallas.begin();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi: connecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.printf("\nWiFi connected: %s  RSSI %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.println("Open http://" + WiFi.localIP().toString() + "/");

  // Current zeroing (ensure no load connected for best result)
  autoZeroACS();
  Serial.printf("ACS zero-offset ~ %.3f V\n", ACS_ZERO_OFFSET_V);

  // HTTP
  server.on("/", handleRoot);
  server.on("/health", handleHealth);
  server.on("/api/now", handleNow);
  server.begin();

  // WS
  ws.begin();
  ws.onEvent(onWsEvent);
}

// ------------- Loop --------------------
void loop() {
  server.handleClient();
  ws.loop();

  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;

    // Temp
    dallas.requestTemperatures();
    float tC = dallas.getTempCByIndex(0);
    if (tC == DEVICE_DISCONNECTED_C) tC = NAN;

    // Electricals
    float V = readPanelVolt();
    float I = readCurrentA();
    float P = V * I;

    // Broadcast compact JSON
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"voltage\":%.3f,\"current\":%.4f,\"power\":%.3f,\"tempC\":%.3f}",
             V, I, P, isnan(tC)?0.0f:tC);
    ws.broadcastTXT(buf);

    // Debug
    Serial.printf("V=%.2f V  I=%.3f A  P=%.2f W  T=%.2f C\n", V, I, P, tC);
  }
}
