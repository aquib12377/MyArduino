/*
 * ESP32 Dental Monitoring Project
 * - FSR for bite pressure
 * - LM35 for mouth temperature
 * - MPU6050 for orientation (pitch & roll)
 * - Live Web UI via WebSockets
 *
 * ESP32 runs as Wi-Fi Access Point:
 *   SSID:  ESP32-Dental
 *   PASS:  dental123
 *
 * Required Libraries:
 *   - ESPAsyncWebServer
 *   - AsyncTCP
 *   - MPU6050
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;
// ------------------- SENSOR PINS -------------------
const int FSR_PIN  = 34;  // FSR analog input
const int LM35_PIN = 39;  // LM35 analog input

// ------------------- MPU6050 OBJECT ----------------

// ------------- WIFI & WEBSERVER CONFIG ------------
const char* ap_ssid     = "ESP32-Dental";
const char* ap_password = "dental123";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ------------- TIMING FOR SENSOR UPDATES ----------
unsigned long lastSensorSend = 0;
const unsigned long SENSOR_INTERVAL_MS = 200; // send data every 200ms

// ------------- HTML PAGE (SERVED FROM FLASH) ------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<title>Dental Monitor - ESP32</title>
<meta name="viewport" content="width=device-width, initial-scale=1" />
<style>
  :root {
    --bg: #0f172a;
    --card: #111827;
    --accent: #38bdf8;
    --accent-soft: rgba(56, 189, 248, 0.15);
    --text: #e5e7eb;
    --muted: #9ca3af;
    --danger: #f97373;
  }
  * {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  }
  body {
    background: radial-gradient(circle at top, #1e293b 0, #020617 55%);
    color: var(--text);
    min-height: 100vh;
    padding: 16px;
    display: flex;
    align-items: stretch;
    justify-content: center;
  }
  .wrapper {
    width: 100%;
    max-width: 1080px;
    margin: auto;
  }
  .header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 18px;
    gap: 1rem;
    flex-wrap: wrap;
  }
  .title-block h1 {
    font-size: 1.6rem;
    font-weight: 700;
    letter-spacing: 0.04em;
  }
  .title-block p {
    color: var(--muted);
    font-size: 0.9rem;
    margin-top: 4px;
  }
  .status-pill {
    display: inline-flex;
    align-items: center;
    gap: 0.4rem;
    padding: 6px 12px;
    border-radius: 999px;
    background: var(--accent-soft);
    color: var(--accent);
    font-size: 0.8rem;
    border: 1px solid rgba(148, 163, 184, 0.35);
  }
  .status-dot {
    width: 8px;
    height: 8px;
    border-radius: 999px;
    background: #22c55e;
    box-shadow: 0 0 8px #22c55e;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
    gap: 16px;
  }

  .card {
    background: linear-gradient(145deg, rgba(15,23,42,0.96), rgba(15,23,42,0.9));
    border-radius: 18px;
    padding: 16px 16px 18px;
    border: 1px solid rgba(148, 163, 184, 0.35);
    box-shadow: 0 18px 30px rgba(15, 23, 42, 0.8);
    position: relative;
    overflow: hidden;
  }
  .card::before {
    content: "";
    position: absolute;
    inset: 0;
    background: radial-gradient(circle at top right, rgba(56,189,248,0.18), transparent 55%);
    opacity: 0.7;
    pointer-events: none;
  }
  .card-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 6px;
    position: relative;
    z-index: 1;
  }
  .card-title {
    font-size: 0.95rem;
    font-weight: 600;
    letter-spacing: 0.03em;
    text-transform: uppercase;
    color: #cbd5f5;
  }
  .card-subtitle {
    font-size: 0.75rem;
    color: var(--muted);
  }

  .metric-value {
    font-size: 2.1rem;
    font-weight: 700;
    margin-top: 6px;
    position: relative;
    z-index: 1;
  }
  .metric-unit {
    font-size: 0.9rem;
    color: var(--muted);
    margin-left: 4px;
  }

  .progress-track {
    margin-top: 10px;
    height: 8px;
    border-radius: 999px;
    background: #020617;
    overflow: hidden;
    border: 1px solid rgba(31, 41, 55, 0.9);
    position: relative;
    z-index: 1;
  }
  .progress-fill {
    height: 100%;
    width: 0%;
    border-radius: inherit;
    background: linear-gradient(90deg, #22c55e, #eab308, #f97316, #ef4444);
    transition: width 0.18s ease-out;
  }

  .badge-row {
    margin-top: 10px;
    display: flex;
    gap: 6px;
    flex-wrap: wrap;
    position: relative;
    z-index: 1;
  }
  .badge {
    padding: 3px 8px;
    font-size: 0.72rem;
    border-radius: 999px;
    border: 1px solid rgba(148, 163, 184, 0.5);
    color: var(--muted);
    background: rgba(15, 23, 42, 0.9);
  }

  .orientation-wrapper {
    margin-top: 12px;
    display: flex;
    gap: 10px;
    position: relative;
    z-index: 1;
    align-items: center;
    flex-wrap: wrap;
  }
  .orientation-circle {
    width: 72px;
    height: 72px;
    border-radius: 999px;
    border: 2px solid rgba(148, 163, 184, 0.7);
    position: relative;
    overflow: hidden;
  }
  .orientation-inner {
    position: absolute;
    inset: 10px;
    border-radius: inherit;
    border: 2px dashed rgba(148, 163, 184, 0.6);
  }
  .orientation-horizon {
    position: absolute;
    left: 6px;
    right: 6px;
    top: 50%;
    height: 2px;
    background: rgba(148, 163, 184, 0.8);
    transform-origin: center center;
  }
  .orientation-dot {
    position: absolute;
    width: 10px;
    height: 10px;
    border-radius: 999px;
    background: var(--accent);
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    box-shadow: 0 0 10px var(--accent);
  }
  .orientation-text {
    display: flex;
    flex-direction: column;
    gap: 4px;
    font-size: 0.85rem;
  }
  .orientation-text span strong {
    font-weight: 600;
    color: #e5e7ff;
  }

  .connection-status {
    margin-top: 10px;
    font-size: 0.8rem;
    color: var(--muted);
  }

  .footer-note {
    margin-top: 16px;
    font-size: 0.76rem;
    color: #6b7280;
    text-align: right;
  }
  .footer-note span {
    color: var(--accent);
  }
</style>
</head>
<body>
<div class="wrapper">
  <div class="header">
    <div class="title-block">
      <h1>Dental Monitoring Dashboard</h1>
      <p>Live bite pressure, intraoral temperature, and head orientation.</p>
    </div>
    <div class="status-pill">
      <div class="status-dot"></div>
      <span id="wsStatus">Connecting...</span>
    </div>
  </div>

  <div class="grid">
    <!-- PRESSURE CARD -->
    <div class="card">
      <div class="card-header">
        <div>
          <div class="card-title">Bite Pressure</div>
          <div class="card-subtitle">FSR sensor reading</div>
        </div>
      </div>
      <div class="metric-value">
        <span id="pressureVal">--</span><span class="metric-unit">%</span>
      </div>
      <div class="progress-track">
        <div id="pressureBar" class="progress-fill"></div>
      </div>
      <div class="badge-row">
        <span class="badge" id="pressureLabel">Waiting for data...</span>
      </div>
    </div>

    <!-- TEMPERATURE CARD -->
    <div class="card">
      <div class="card-header">
        <div>
          <div class="card-title">Mouth Temperature</div>
          <div class="card-subtitle">LM35 analog sensor</div>
        </div>
      </div>
      <div class="metric-value">
        <span id="tempVal">--</span><span class="metric-unit">°C</span>
      </div>
      <div class="progress-track">
        <div id="tempBar" class="progress-fill"></div>
      </div>
      <div class="badge-row">
        <span class="badge" id="tempLabel">Waiting for data...</span>
      </div>
    </div>

    <!-- ORIENTATION CARD -->
    <div class="card">
      <div class="card-header">
        <div>
          <div class="card-title">Head Orientation</div>
          <div class="card-subtitle">MPU6050 – Pitch &amp; Roll</div>
        </div>
      </div>
      <div class="orientation-wrapper">
        <div class="orientation-circle">
          <div class="orientation-inner"></div>
          <div class="orientation-horizon" id="horizonLine"></div>
          <div class="orientation-dot"></div>
        </div>
        <div class="orientation-text">
          <span>Pitch: <strong><span id="pitchVal">--</span>°</strong></span>
          <span>Roll: <strong><span id="rollVal">--</span>°</strong></span>
          <span class="connection-status" id="oriStatus">Waiting for data...</span>
        </div>
      </div>
    </div>
  </div>

  <div class="footer-note">
    <span>ESP32 Dental Monitor</span> · WebSocket live stream
  </div>
</div>

<script>
  let ws;
  function connectWS() {
    const protocol = (location.protocol === "https:") ? "wss://" : "ws://";
    const wsUrl = protocol + location.host + "/ws";
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      document.getElementById("wsStatus").textContent = "Live";
    };

    ws.onclose = () => {
      document.getElementById("wsStatus").textContent = "Reconnecting...";
      setTimeout(connectWS, 2000);
    };

    ws.onerror = () => {
      document.getElementById("wsStatus").textContent = "Error";
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        // Pressure
        if (typeof data.pressure === "number") {
          const p = Math.max(0, Math.min(100, data.pressure));
          document.getElementById("pressureVal").textContent = p.toFixed(0);
          document.getElementById("pressureBar").style.width = p + "%";

          let pLabel = "Normal bite";
          if (p < 10) pLabel = "Very light bite";
          else if (p < 30) pLabel = "Light bite";
          else if (p < 60) pLabel = "Moderate bite";
          else if (p < 80) pLabel = "Firm bite";
          else pLabel = "High pressure";

          document.getElementById("pressureLabel").textContent = pLabel;
        }

        // Temperature
        if (typeof data.tempC === "number") {
          const t = data.tempC;
          document.getElementById("tempVal").textContent = t.toFixed(1);
          const tNorm = Math.max(30, Math.min(45, t));
          const tProgress = ((tNorm - 30) / (45 - 30)) * 100;
          document.getElementById("tempBar").style.width = tProgress + "%";

          let tLabel = "Within normal range";
          if (t < 35) tLabel = "Below typical";
          else if (t > 38) tLabel = "Above typical";
          document.getElementById("tempLabel").textContent = tLabel;
        }

        // Orientation
        if (typeof data.pitch === "number" && typeof data.roll === "number") {
          const pitch = data.pitch;
          const roll  = data.roll;
          document.getElementById("pitchVal").textContent = pitch.toFixed(1);
          document.getElementById("rollVal").textContent  = roll.toFixed(1);

          const horizon = document.getElementById("horizonLine");
          horizon.style.transform = "translateY(-50%) rotate(" + roll.toFixed(1) + "deg)";
          document.getElementById("oriStatus").textContent = "Stable stream";
        }

      } catch (e) {
        console.error("WS parse error:", e);
      }
    };
  }

  window.addEventListener("load", () => {
    connectWS();
  });
</script>
</body>
</html>
)rawliteral";

// ------------- WEBSOCKET EVENT HANDLER ------------
void onWsEvent(AsyncWebSocket *server,
               AsyncWebSocketClient *client,
               AwsEventType type,
               void *arg,
               uint8_t *data,
               size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client %u connected from %s\n",
                  client->id(),
                  client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client %u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // We don't expect any commands from client right now
  }
}

// ------------- SENSOR INITIALIZATION --------------
bool initMPU() {
  //mpu.initialize();
  Serial.println("Testing MPU6050 connection...");
  if (mpu.begin()) {
    Serial.println("MPU6050 connected.");
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

    return true;
  } else {
    Serial.println("MPU6050 connection failed!");
    return false;
  }
}

// ------------- SENSOR READING HELPERS -------------
float readFSRPressurePercent() {
  int raw = analogRead(FSR_PIN); // 0 - 4095
  // Map raw value to 0-100% with simple scaling
  // You can adjust scaling based on your calibration
  float percent = (raw / 4095.0f) * 100.0f;
  return percent;
}

float readLM35TemperatureC() {
  int raw = analogRead(LM35_PIN);
  float voltage = (raw / 4095.0f) * 3.3f;   // ESP32 ADC reference approx 3.3V
  float tempC = voltage * 100.0f;          // LM35: 10mV per °C
  return tempC;
}

void readMPUOrientation(float &pitch, float &roll) {
  
sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  // Convert to 'g' units (for typical MPU6050 scale 16384 LSB/g)
  float axg = a.acceleration.x / 16384.0f;
  float ayg = a.acceleration.y / 16384.0f;
  float azg = a.acceleration.z / 16384.0f;

  // Simple tilt calculation using accelerometer only
  pitch = atan2f(-g.gyro.x, sqrtf(g.gyro.y * g.gyro.y + g.gyro.z * g.gyro.z)) * 180.0f / PI;
  roll  = atan2f(g.gyro.y, g.gyro.z) * 180.0f / PI;
}

// ------------- SETUP ------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // ADC configuration
  analogReadResolution(12);  // 0-4095
  analogSetPinAttenuation(FSR_PIN, ADC_11db);
  analogSetPinAttenuation(LM35_PIN, ADC_11db);

  // I2C and MPU
  Wire.begin(); // SDA=21, SCL=22 by default on ESP32
  initMPU();

  // Wi-Fi AP mode
  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(ap_ssid, ap_password);
  if (apStarted) {
    Serial.println("WiFi AP started.");
    Serial.print("SSID: ");
    Serial.println(ap_ssid);
    Serial.print("Password: ");
    Serial.println(ap_password);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to start WiFi AP!");
  }

  // Websocket setup
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // HTTP route for main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Start server
  server.begin();
  Serial.println("HTTP server started.");
}

// ------------- LOOP -------------------------------
void loop() {
  // AsyncWebServer & AsyncWebSocket do not require polling in loop,
  // but we can push sensor data at fixed interval here.
  unsigned long now = millis();
  if (now - lastSensorSend >= SENSOR_INTERVAL_MS) {
    lastSensorSend = now;

    float pressure = readFSRPressurePercent();
    float tempC    = readLM35TemperatureC();
    float pitch, roll;
    readMPUOrientation(pitch, roll);

    // Build JSON payload
    char jsonBuffer[200];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"pressure\":%.2f,\"tempC\":%.2f,\"pitch\":%.2f,\"roll\":%.2f}",
             pressure, tempC, pitch, roll);

    ws.textAll(jsonBuffer);
  }

  // Short delay to avoid tight looping
  delay(5);
}
