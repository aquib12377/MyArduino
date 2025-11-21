/*******************************************************
 * ESP32 WROOM-32 – AC Power Meter (ZMPT101B + ACS712)
 * - True RMS measurement over a window
 * - Web page + WebSocket (no Async)
 * - 16x2 I2C LCD
 *
 * Notes:
 * - Use ADC1 pins only for analog read (WiFi uses ADC2).
 * - ZMPT & ACS have mid-supply offset ~Vcc/2; we estimate
 *   offset in a pre-pass, then compute RMS in main pass.
 * - For accurate volts, set V_CAL after looking at serial.
 * - For accurate amps, set ACS_SENS_mV_PER_A to your module.
 *******************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "esp32-hal-adc.h"   // <-- add this near the top

// ADC settings
// 11dB ~ up to ~3.3V input range; 12-bit -> 0..4095
const adc_attenuation_t ADC_ATTEN = ADC_11db; 
const int ADC_BITS = 12;
// ----------------------- USER CONFIG -----------------------
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// Analog pins (ADC1 only!): 32,33,34,35,36,39
const int V_SENSE_PIN = 34;   // ZMPT OUT -> GPIO34
const int I_SENSE_PIN = 35;   // ACS OUT  -> GPIO35



// I2C LCD (16x2). Common addresses: 0x27 or 0x3F
const uint8_t LCD_ADDR = 0x27;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;

// I2C pins (ESP32 lets you choose; 21/22 are default)
const int I2C_SDA = 21;
const int I2C_SCL = 22;

// Measurement window (ms)
const uint32_t PREPASS_MS   = 120;  // estimate DC offset
const uint32_t MEASURE_MS   = 480;  // compute RMS (total ~600ms)
const uint32_t PUBLISH_MS   = 500;  // push WebSocket update

// --- Calibration ---
// After first run, tune V_CAL so voltage matches multimeter.
// Example: if serial shows v_rms_sensor_mV ≈ 320 mV when mains is 230 V,
// set V_CAL = 230.0 / 320.0 = 0.71875.
float V_CAL = 0.5f;  // <<<< TUNE THIS for your ZMPT module

// ACS712 sensitivity (mV per Amp)
// 5A  -> 185.0, 20A -> 100.0, 30A -> 66.0 (typical)
float ACS_SENS_mV_PER_A = 100.0f;  // <<<< choose your ACS module

// If your install is noisy, clamp small currents to zero:
const float I_NOISE_FLOOR_A = 0.05f;

// Optional: set an assumed power factor for "real power" estimate
// (Without simultaneous voltage/current phase calibration, true PF is unknown)
float POWER_FACTOR = 0.95f;

// mDNS name (visit http://esp-acpower.local if your OS supports it)
const char* MDNS_NAME = "esp-acpower";

// -------------------- GLOBALS --------------------
WebServer server(80);
WebSocketsServer ws(81);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

volatile float gVrms = 0.0f;
volatile float gIrms = 0.0f;
volatile float gS_apparent = 0.0f; // V * A
volatile float gP_real = 0.0f;     // V * A * PF

// Cached raw sensor RMS (for calibration help)
volatile float gV_sensor_rms_mV = 0.0f;
volatile float gI_sensor_rms_mV = 0.0f;

uint32_t lastPublish = 0;

// Simple, embedded page
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>ESP32 AC Meter</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:0;background:#0b0f14;color:#e7eef7}
    header{padding:14px 18px;background:#131a22;border-bottom:1px solid #203040}
    .wrap{max-width:900px;margin:24px auto;padding:0 16px}
    .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px}
    .card{background:#111823;border:1px solid #22324a;border-radius:12px;padding:16px}
    .title{font-size:13px;opacity:.8;margin-bottom:8px}
    .val{font-size:34px;font-weight:700;letter-spacing:.3px}
    .unit{font-size:16px;opacity:.8;margin-left:4px}
    footer{opacity:.6;text-align:center;padding:18px}
    code{background:#0f141a;border:1px solid #1f2c40;padding:2px 6px;border-radius:6px}
  </style>
</head>
<body>
  <header><b>ESP32 AC Power Meter</b> · WebSocket Live</header>
  <div class="wrap">
    <div class="cards">
      <div class="card"><div class="title">Voltage (RMS)</div><div class="val" id="v">--<span class="unit">V</span></div></div>
      <div class="card"><div class="title">Current (RMS)</div><div class="val" id="i">--<span class="unit">A</span></div></div>
      <div class="card"><div class="title">Apparent Power</div><div class="val" id="s">--<span class="unit">VA</span></div></div>
      <div class="card"><div class="title">Real Power (est.)</div><div class="val" id="p">--<span class="unit">W</span></div></div>
    </div>
    <p style="margin-top:18px;opacity:.75">
      Tip: If voltage seems off, calibrate <code>V_CAL</code>. If current seems off, set <code>ACS_SENS_mV_PER_A</code> for your ACS712 module (5A/20A/30A).
    </p>
    <div class="card" style="margin-top:14px">
      <div class="title">Debug (raw sensor RMS)</div>
      <div>V<sub>sensor</sub>: <span id="v_raw">--</span> mV&nbsp;&nbsp;·&nbsp;&nbsp;I<sub>sensor</sub>: <span id="i_raw">--</span> mV</div>
    </div>
  </div>
  <footer>ws://&lt;device-ip&gt;:81 · JSON: <code>/json</code></footer>
<script>
(function(){
  const el = (id)=>document.getElementById(id);
  let ws, wsOk = false, pollTimer = null;

  function render(d){
    if(d.v!==undefined) el("v").innerHTML = d.v.toFixed(1) + '<span class="unit">V</span>';
    if(d.i!==undefined) el("i").innerHTML = d.i.toFixed(3) + '<span class="unit">A</span>';
    if(d.s!==undefined) el("s").innerHTML = d.s.toFixed(1) + '<span class="unit">VA</span>';
    if(d.p!==undefined) el("p").innerHTML = d.p.toFixed(1) + '<span class="unit">W</span>';
    if(d.v_raw!==undefined) el("v_raw").textContent = d.v_raw.toFixed(1);
    if(d.i_raw!==undefined) el("i_raw").textContent = d.i_raw.toFixed(1);
  }

  async function poll(){
    try{
      const r = await fetch('/json', {cache:'no-store'});
      const d = await r.json();
      render(d);
    }catch(_){}
  }

  function startPolling(){
    if(pollTimer) return;
    pollTimer = setInterval(poll, 1000);
    poll(); // immediate
  }

  function connectWS(){
    try{
      ws = new WebSocket("ws://" + location.hostname + ":81/");
      ws.onopen = ()=>{ wsOk = true; };
      ws.onmessage = (e)=>{ try{ render(JSON.parse(e.data)); }catch(_){} };
      ws.onclose = ()=>{ wsOk = false; setTimeout(connectWS, 1000); };
      ws.onerror = ()=>{ wsOk = false; startPolling(); };
      // If no open in 1.5s, assume blocked and start polling
      setTimeout(()=>{ if(!wsOk) startPolling(); }, 1500);
    }catch(e){
      startPolling();
    }
  }
  connectWS();
})();
</script>

</body>
</html>
)HTML";

// -------------------- MEASUREMENT --------------------

static inline int readMilliVolts(int pin) {
  // Use calibrated mV reading (slower but simple & consistent)
  // Ensure attenuation set for the pin
  return analogReadMilliVolts(pin);
}

void estimateOffsets(float &v_off_mV, float &i_off_mV) {
  uint32_t t0 = millis();
  uint64_t vSum = 0;
  uint64_t iSum = 0;
  uint32_t n = 0;

  while (millis() - t0 < PREPASS_MS) {
    vSum += (uint32_t)readMilliVolts(V_SENSE_PIN);
    iSum += (uint32_t)readMilliVolts(I_SENSE_PIN);
    n++;
    // yield to servers
    server.handleClient();
    ws.loop();
  }
  if (n == 0) n = 1;
  v_off_mV = (float)vSum / (float)n;
  i_off_mV = (float)iSum / (float)n;
}

void measureRMS(float &v_rms_V, float &i_rms_A, float &v_sensor_rms_mV, float &i_sensor_rms_mV) {
  float v_off_mV = 0, i_off_mV = 0;
  estimateOffsets(v_off_mV, i_off_mV);

  uint32_t t0 = millis();
  double vsqSum = 0.0, isqSum = 0.0;
  uint32_t n = 0;

  while (millis() - t0 < MEASURE_MS) {
    int vmv = readMilliVolts(V_SENSE_PIN);
    int imv = readMilliVolts(I_SENSE_PIN);

    float v_ac = (float)vmv - v_off_mV; // center around offset
    float i_ac = (float)imv - i_off_mV;

    vsqSum += (double)v_ac * (double)v_ac;
    isqSum += (double)i_ac * (double)i_ac;
    n++;

    // keep network responsive
    server.handleClient();
    ws.loop();
  }

  if (n == 0) n = 1;

  // Sensor RMS in millivolts
  v_sensor_rms_mV = sqrt(vsqSum / (double)n);
  i_sensor_rms_mV = sqrt(isqSum / (double)n);

  // Convert to mains units:
  // Voltage: scale sensor mV to line Vrms with V_CAL
  float v_V = v_sensor_rms_mV * V_CAL;

  // Current: ACS712 gives mV per Amp directly
  float i_A = i_sensor_rms_mV / ACS_SENS_mV_PER_A;

  // Noise clamp on current
  if (i_A < I_NOISE_FLOOR_A) i_A = 0.0f;

  v_rms_V = v_V;
  i_rms_A = i_A;
}

void updateLCD(float v, float i) {
  lcd.setCursor(0, 0);
  // "V:xxx.x  I:x.xxx"
  char line1[17];
  snprintf(line1, sizeof(line1), "V:%6.1f  I:%1.3f", v, i);
  for (int k = 0; k < LCD_COLS; ++k) {
    char c = (k < (int)strlen(line1)) ? line1[k] : ' ';
    lcd.write(c);
  }

  lcd.setCursor(0, 1);
  // "S:xxxxx.x P:xxxxx"
  float s = v * i;
  float p = s * POWER_FACTOR;
  char line2[17];
  snprintf(line2, sizeof(line2), "S:%6.1f P:%6.0f", s, p);
  for (int k = 0; k < LCD_COLS; ++k) {
    char c = (k < (int)strlen(line2)) ? line2[k] : ' ';
    lcd.write(c);
  }
}

void broadcastNow() {
  // send latest as JSON
  String msg = "{";
  msg += "\"v\":" + String(gVrms, 3) + ",";
  msg += "\"i\":" + String(gIrms, 4) + ",";
  msg += "\"s\":" + String(gS_apparent, 3) + ",";
  msg += "\"p\":" + String(gP_real, 3) + ",";
  msg += "\"v_raw\":" + String(gV_sensor_rms_mV, 1) + ",";
  msg += "\"i_raw\":" + String(gI_sensor_rms_mV, 1);
  msg += "}";
  ws.broadcastTXT(msg);
}

// -------------------- SERVER HANDLERS --------------------

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleJson() {
  String j = "{";
  j += "\"v\":" + String(gVrms, 3) + ",";
  j += "\"i\":" + String(gIrms, 4) + ",";
  j += "\"s\":" + String(gS_apparent, 3) + ",";
  j += "\"p\":" + String(gP_real, 3) + ",";
  j += "\"v_raw\":" + String(gV_sensor_rms_mV, 1) + ",";
  j += "\"i_raw\":" + String(gI_sensor_rms_mV, 1);
  j += "}";
  server.send(200, "application/json", j);
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

 switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = ws.remoteIP(num);
      Serial.printf("[WS] Client #%u connected from %s\n", num, ip.toString().c_str());
      broadcastNow(); // snapshot
    } break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      break;
    case WStype_TEXT:
      Serial.printf("[WS] <- #%u: %.*s\n", num, (int)length, (const char*)payload);
      break;
    case WStype_PING:
      Serial.println("[WS] ping");
      break;
    case WStype_PONG:
      Serial.println("[WS] pong");
      break;
    default:
      break;
  }
}


// -------------------- SETUP / LOOP --------------------

void setup() {
  Serial.begin(115200);
  delay(200);

  // ADC config
  analogReadResolution(ADC_BITS);
  analogSetPinAttenuation(V_SENSE_PIN, ADC_ATTEN);
  analogSetPinAttenuation(I_SENSE_PIN, ADC_ATTEN);

  // I2C + LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.begin();       // initialize LCD
  lcd.backlight();  // turn on backlight
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 AC Meter");
  lcd.setCursor(0, 1);
  lcd.print("WiFi starting...");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("IP:");
    lcd.print(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED (continuing AP-less)");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("WiFi Failed");
  }

  // (Optional) mDNS


  // HTTP
  server.on("/", handleRoot);
  server.on("/json", handleJson);
  server.begin();

  // WebSocket
  ws.begin();
  ws.onEvent(onWsEvent);
ws.enableHeartbeat(15000, 3000, 2); // ping every 15s, expect pong in 3s, 2 misses -> drop
  // First screen
  updateLCD(0.0f, 0.0f);
}

void loop() {
  // 1) Take one measurement window
  float v_rms_V = 0, i_rms_A = 0, v_raw_mV = 0, i_raw_mV = 0;
  measureRMS(v_rms_V, i_rms_A, v_raw_mV, i_raw_mV);

  // 2) Update globals
  gVrms = v_rms_V;
  gIrms = i_rms_A;
  gS_apparent = gVrms * gIrms;
  gP_real = gS_apparent * POWER_FACTOR;

  gV_sensor_rms_mV = v_raw_mV;
  gI_sensor_rms_mV = i_raw_mV;

  // 3) Serial log (useful for calibration)
  Serial.printf("[RMS] V_sensor=%.1f mV  I_sensor=%.1f mV  ->  V=%.1f V  I=%.3f A  S=%.1f VA  P~%.1f W\n",
                v_raw_mV, i_raw_mV, gVrms, gIrms, gS_apparent, gP_real);

  // 4) LCD
  updateLCD(gVrms, gIrms);
broadcastNow();

  // 5) Service web + WS, and broadcast every ~PUBLISH_MS
  server.handleClient();
  ws.loop();

  if (millis() - lastPublish >= PUBLISH_MS) {
    broadcastNow();
    lastPublish = millis();
  }

  // (small yield)
  delay(5);
}
