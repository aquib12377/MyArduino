#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <math.h>

// =========================
// WiFi credentials
// =========================
const char* ssid = "MyProject";
const char* password = "12345678";

// =========================
// Pin definitions
// =========================
#define PIR_PIN        27
#define RELAY_PIN      26
#define DHT_PIN        4
#define ACS712_PIN     34

// =========================
// DHT setup
// =========================
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// =========================
// Relay logic
// Active LOW relay:
// LOW  = ON
// HIGH = OFF
// =========================
const bool RELAY_ON  = LOW;
const bool RELAY_OFF = HIGH;

// =========================
// Motion hold time
// Relay stays ON for this many ms
// after last motion detection
// =========================
const unsigned long motionHoldMs = 20000;  // 20 seconds

// =========================
// ACS712 config
// ACS712 30A sensitivity = 66mV/A = 0.066 V/A
// =========================
const float ACS712_SENSITIVITY = 0.066;

// ESP32 ADC reference handling
const float ADC_REF_VOLTAGE = 3.3;
const int ADC_RESOLUTION = 4095;

// If you use a divider between ACS712 output and ESP32 ADC,
// set ratio accordingly.
const float VOLTAGE_DIVIDER_RATIO = 1.0;

// =========================
// Globals
// =========================
WebServer server(80);

bool humanDetected = false;
bool relayStatus = false;
bool webForceOff = false;   // NEW: web override lock

float temperatureC = 0.0;
float humidity = 0.0;
float currentA = 0.0;

unsigned long lastMotionTime = 0;
unsigned long lastDHTReadTime = 0;
const unsigned long dhtInterval = 2000;

float zeroCurrentVoltage = 0.0;   // calibrated at startup

// =========================
// HTML page
// =========================
String getHTMLPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Sensor Dashboard</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f4f7fb;
      margin: 0;
      padding: 20px;
      color: #222;
    }
    .container {
      max-width: 800px;
      margin: auto;
    }
    .title {
      text-align: center;
      margin-bottom: 20px;
    }
    .card {
      background: white;
      border-radius: 14px;
      padding: 18px;
      margin-bottom: 14px;
      box-shadow: 0 4px 14px rgba(0,0,0,0.08);
    }
    .label {
      font-size: 14px;
      color: #666;
    }
    .value {
      font-size: 28px;
      font-weight: bold;
      margin-top: 6px;
    }
    .status-on { color: green; }
    .status-off { color: red; }
    .status-warn { color: #d97706; }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 14px;
    }
    @media (max-width: 600px) {
      .grid { grid-template-columns: 1fr; }
    }
    .ip {
      text-align: center;
      color: #555;
      margin-top: 12px;
      font-size: 14px;
    }
    .button-row {
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
      margin-top: 10px;
    }
    button {
      border: none;
      border-radius: 10px;
      padding: 12px 18px;
      font-size: 16px;
      cursor: pointer;
      font-weight: bold;
    }
    .btn-off {
      background: #dc2626;
      color: white;
    }
    .btn-auto {
      background: #16a34a;
      color: white;
    }
    .small {
      font-size: 14px;
      color: #555;
      margin-top: 8px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2 class="title">ESP32 Sensor Dashboard</h2>

    <div class="grid">
      <div class="card">
        <div class="label">Relay Status</div>
        <div class="value" id="relay">--</div>
      </div>

      <div class="card">
        <div class="label">Human Detected</div>
        <div class="value" id="human">--</div>
      </div>

      <div class="card">
        <div class="label">Temperature</div>
        <div class="value" id="temp">--</div>
      </div>

      <div class="card">
        <div class="label">Humidity</div>
        <div class="value" id="hum">--</div>
      </div>

      <div class="card">
        <div class="label">Current</div>
        <div class="value" id="current">--</div>
      </div>

      <div class="card">
        <div class="label">Control Mode</div>
        <div class="value" id="mode">--</div>

        <div class="button-row">
          <button class="btn-off" onclick="relayOff()">Turn Relay OFF</button>
          <button class="btn-auto" onclick="resumeAuto()">Resume Auto</button>
        </div>
        <div class="small">Turn OFF overrides PIR until Auto Resume is pressed.</div>
      </div>
    </div>

    <div class="ip">
      Refreshing every 2 seconds
    </div>
  </div>

  <script>
    async function fetchData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();

        document.getElementById('relay').innerHTML =
          data.relay ? '<span class="status-on">ON</span>' : '<span class="status-off">OFF</span>';

        document.getElementById('human').innerHTML =
          data.human ? '<span class="status-on">YES</span>' : '<span class="status-off">NO</span>';

        document.getElementById('temp').innerText = data.temperature + ' °C';
        document.getElementById('hum').innerText = data.humidity + ' %';
        document.getElementById('current').innerText = data.current + ' A';

        document.getElementById('mode').innerHTML =
          data.webForceOff
            ? '<span class="status-warn">WEB OVERRIDE OFF</span>'
            : '<span class="status-on">AUTO PIR MODE</span>';

      } catch (e) {
        console.log('Fetch error:', e);
      }
    }

    async function relayOff() {
      try {
        await fetch('/relayControl?state=off');
        fetchData();
      } catch (e) {
        console.log('Relay OFF error:', e);
      }
    }

    async function resumeAuto() {
      try {
        await fetch('/relayControl?state=auto');
        fetchData();
      } catch (e) {
        console.log('Resume AUTO error:', e);
      }
    }

    fetchData();
    setInterval(fetchData, 2000);
  </script>
</body>
</html>
)rawliteral";
  return page;
}

// =========================
// Web handlers
// =========================
void handleRoot() {
  server.send(200, "text/html", getHTMLPage());
}

void handleData() {
  String json = "{";
  json += "\"relay\":" + String(relayStatus ? "true" : "false") + ",";
  json += "\"human\":" + String(humanDetected ? "true" : "false") + ",";
  json += "\"webForceOff\":" + String(webForceOff ? "true" : "false") + ",";
  json += "\"temperature\":\"" + String(temperatureC, 1) + "\",";
  json += "\"humidity\":\"" + String(humidity, 1) + "\",";
  json += "\"current\":\"" + String(currentA, 2) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRelayControl() {
  if (server.hasArg("state")) {
    String state = server.arg("state");

    if (state == "off") {
      webForceOff = true;
      digitalWrite(RELAY_PIN, RELAY_OFF);
      relayStatus = false;
    }
    else if (state == "auto") {
      webForceOff = false;
    }

    server.send(200, "text/plain", "OK");
    return;
  }

  server.send(400, "text/plain", "Missing state parameter");
}

// =========================
// Read DHT22 every few sec
// =========================
void updateDHT() {
  unsigned long now = millis();
  if (now - lastDHTReadTime >= dhtInterval) {
    lastDHTReadTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) temperatureC = t;
    if (!isnan(h)) humidity = h;
  }
}

// =========================
// Calibrate ACS712 zero point
// Run at startup with no load if possible
// =========================
float calibrateACS712() {
  const int samples = 1000;
  long total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(ACS712_PIN);
    delay(2);
  }

  float avgADC = total / (float)samples;
  float adcVoltage = (avgADC / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
  float sensorVoltage = adcVoltage * VOLTAGE_DIVIDER_RATIO;

  return sensorVoltage;
}

// =========================
// Read RMS current
// Good enough for small dashboard use
// =========================
float readCurrentRMS() {
  const int samples = 500;
  double sumSquares = 0.0;

  for (int i = 0; i < samples; i++) {
    int raw = analogRead(ACS712_PIN);
    float adcVoltage = (raw / (float)ADC_RESOLUTION) * ADC_REF_VOLTAGE;
    float sensorVoltage = adcVoltage * VOLTAGE_DIVIDER_RATIO;

    float delta = sensorVoltage - zeroCurrentVoltage;
    sumSquares += (delta * delta);

    delayMicroseconds(300);
  }

  float vrms = sqrt(sumSquares / samples);
  float amps = vrms / ACS712_SENSITIVITY;

  if (amps < 0.05) amps = 0.0;
  amps = amps / 10.0;

  return amps;
}

// =========================
// PIR + Relay logic
// =========================
void updatePIRAndRelay() {
  int pirState = digitalRead(PIR_PIN);

  if (pirState == HIGH) {
    humanDetected = true;
    lastMotionTime = millis();
  } else {
    if (millis() - lastMotionTime > motionHoldMs) {
      humanDetected = false;
    }
  }

  // If web override OFF is active,
  // keep relay OFF no matter what PIR says
  if (webForceOff) {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    relayStatus = false;
    return;
  }

  if (humanDetected) {
    digitalWrite(RELAY_PIN, RELAY_ON);
    relayStatus = true;
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    relayStatus = false;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Keep relay OFF at startup
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Calibrating ACS712 zero point...");
  Serial.println("Make sure no load/current is flowing right now.");
  delay(2000);
  zeroCurrentVoltage = calibrateACS712();
  Serial.print("Zero current voltage: ");
  Serial.println(zeroCurrentVoltage, 3);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/relayControl", handleRelayControl);
  server.begin();

  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  updatePIRAndRelay();
  updateDHT();
  currentA = readCurrentRMS();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();

    Serial.println("------ STATUS ------");
    Serial.print("Human Detected: ");
    Serial.println(humanDetected ? "YES" : "NO");

    Serial.print("Relay Status: ");
    Serial.println(relayStatus ? "ON" : "OFF");

    Serial.print("Web Force OFF: ");
    Serial.println(webForceOff ? "YES" : "NO");

    Serial.print("Temperature: ");
    Serial.print(temperatureC, 1);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");

    Serial.print("Current: ");
    Serial.print(currentA, 2);
    Serial.println(" A");
  }
}