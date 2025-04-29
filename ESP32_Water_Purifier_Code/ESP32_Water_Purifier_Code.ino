/*****************************************************
 * ESP32 Water Monitoring with 2 Pumps & Ultrasonic
 *
 * User Enters Tank Height (cm) Instead of Percentage
 * --------------------------------------------------
 * Features:
 *  - TDS & pH measurement (analog)
 *  - Ultrasonic sensor to measure filtered tank level
 *  - 2 pumps (active-low relays)
 *  - Modern web UI with auto-refresh (AJAX)
 *  - User-settable tank height (cm)
 *  - Fill percentage is calculated automatically
 *  - Responsive design for mobile
 *
 * NOTE: Adapt pin definitions, calibrations, etc.
 *****************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>

// ---------------- PIN DEFINITIONS ----------------
#define TDS_SENSOR_PIN         4   // Example analog pin for TDS
#define PH_SENSOR_PIN         34   // Example analog pin for pH

// Ultrasonic pins
#define ULTRASONIC_TRIG_PIN   12   // Example digital pin for ultrasonic TRIG
#define ULTRASONIC_ECHO_PIN   14   // Example digital pin for ultrasonic ECHO

// Two pumps, each controlled by an active-low relay
#define PUMP1_RELAY_PIN       15   // Pump #1 -> from upper container to filter
#define PUMP2_RELAY_PIN       16   // Pump #2 -> from filter tank to drinking tank

// -------------- TDS MEDIAN FILTER ---------------
#define VREF      3.3               // ESP32 ADC reference voltage
#define SCOUNT    30                // Number of samples for median filter

int   analogBuffer[SCOUNT];
int   analogBufferTemp[SCOUNT];
int   analogBufferIndex  = 0;
int   copyIndex          = 0;

float tdsValue           = 0.0;     // Global TDS reading
float temperature        = 25.0;    // Hard-coded temp for TDS compensation

// -------------- pH & FILTER TANK LEVEL --------------
float pHValue            = 7.0;     // Last read pH
float waterLevelPercent  = 0.0;     // Filter tank level in % (0..100)

// -------------- USER-SET TANK HEIGHT --------------
float tankHeightCm       = 30.0;    // Default 30 cm, user can change this via web

// -------------- 2 Pump States --------------
bool pump1State = false;  // false = OFF, true = ON
bool pump2State = false;  // false = OFF, true = ON

// ------------------- WIFI -------------------
const char* ssid     = "MyProject";
const char* password = "12345678";

// -------------- WEB SERVER --------------
WebServer server(80);

// -------------- LCD (I2C) --------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------------------------------------------------------------
//                     MEDIAN FILTER FOR TDS
// ------------------------------------------------------------------------
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) {
    bTab[i] = bArray[i];
  }
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

// ------------------------------------------------------------------------
//                 READ TDS (with median filter)
// ------------------------------------------------------------------------
float readTDS() {
  int medianAdc = getMedianNum(analogBufferTemp, SCOUNT);
  float voltage = medianAdc * (float)VREF / 4096.0;  // 12-bit ADC on ESP32

  // Temperature compensation
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensationVoltage     = voltage / compensationCoefficient;

  // Convert voltage to TDS (polynomial from your snippet)
  float tdsCalc = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage
                  - 255.86 * compensationVoltage * compensationVoltage
                  + 857.39 * compensationVoltage) * 0.5;
  return tdsCalc;
}

// ------------------------------------------------------------------------
//            READ pH (Example / Placeholder Formula)
// ------------------------------------------------------------------------
float readPH() {
  int   raw  = analogRead(PH_SENSOR_PIN);
  float volt = raw * (3.3 / 4095.0); 
  // Placeholder formula, adapt to your sensor calibration:
  float phCalc = 7.0 + ((2.5 - volt) * 3.0);
  return phCalc;
}

// ------------------------------------------------------------------------
//           READ FILTER TANK LEVEL USING ULTRASONIC
//           with user-set tankHeightCm
// ------------------------------------------------------------------------
float readFilteredTankLevel() {
  // 1) Trigger ultrasonic pulse
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // 2) Read echo time (in microseconds)
  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000UL); 
  // 30000us = 30ms = max ~5 meters, adjust as needed

  // 3) Convert time to distance (cm).
  // Speed of sound ~ 343 m/s => ~29.1 us/cm for round trip.
  // So distance in cm = (duration / 2) / 29.1
  float distanceCm = (duration * 0.0343) / 2.0; // or use 29.1 factor
  Serial.println(distanceCm);
  // 4) If distance > tankHeightCm, it means the tank is essentially empty.
  //    If distance is near 0, it means water is up near the sensor (full).
  //    We'll map 0..tankHeightCm to 100..0% for "fill level."
  float fillPercent = 0.0;
  if (distanceCm < 0) {
    // Possibly an error reading
    fillPercent = 0.0;
  } 
  else if (distanceCm >= tankHeightCm) {
    fillPercent = 0.0;  // completely empty
  } 
  else {
    float waterDepth = tankHeightCm - distanceCm; // how many cm of water
    fillPercent      = (waterDepth / tankHeightCm) * 100.0;
  }
  return fillPercent;
}

// ------------------------------------------------------------------------
//                 PUMP CONTROL (Active-Low)
// ------------------------------------------------------------------------
void pump1On() {
  digitalWrite(PUMP1_RELAY_PIN, LOW); 
  pump1State = true;
}
void pump1Off() {
  digitalWrite(PUMP1_RELAY_PIN, HIGH);
  pump1State = false;
}

void pump2On() {
  digitalWrite(PUMP2_RELAY_PIN, LOW);
  pump2State = true;
}
void pump2Off() {
  digitalWrite(PUMP2_RELAY_PIN, HIGH);
  pump2State = false;
}

// ------------------------------------------------------------------------
//                  WEB SERVER HANDLERS
// ------------------------------------------------------------------------

// Main page (HTML + CSS + JS)
void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Water Purification Dashboard</title>
  <style>
    /* RESET / BASE STYLES */
    * {
      margin: 0; 
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: #f4f6f9;
      color: #333;
    }

    /* HEADER / NAVBAR */
    header {
      background: #0d6efd; /* Bootstrap-like primary color */
      color: #fff;
      padding: 1rem;
      text-align: center;
      font-size: 1.25rem;
      font-weight: 500;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
      margin-bottom: 1.5rem;
    }

    /* MAIN CONTAINER */
    .container {
      max-width: 900px;
      margin: 0 auto;
      padding: 0 15px 30px;
    }

    /* DASHBOARD GRID */
    .dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 1rem;
      margin-bottom: 1.5rem;
    }

    /* CARD STYLES */
    .card {
      background: #fff;
      border-radius: 8px;
      box-shadow: 0 2px 3px rgba(0,0,0,0.1);
      padding: 1.25rem;
    }
    .card h3 {
      margin-bottom: 1rem;
      font-size: 1.1rem;
    }
    .card h2 {
      margin-bottom: 0.5rem;
      font-size: 1.25rem;
    }
    .card p {
      margin-bottom: 0.5rem;
    }

    /* SENSOR METRICS */
    .metric {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 0.75rem;
    }
    .metric .label {
      font-weight: 600;
      font-size: 0.95rem;
      color: #444;
    }
    .metric .value {
      font-size: 1rem;
      font-weight: 500;
      color: #111;
    }

    /* PROGRESS BAR (for tank level) */
    .progress-bar-container {
      background: #e9ecef;
      border-radius: 4px;
      width: 100%;
      height: 8px;
      overflow: hidden;
      margin-top: 0.3rem;
    }
    .progress-bar {
      background: #0d6efd;
      height: 100%;
      width: 0%; /* JS will update */
      transition: width 0.3s ease;
    }

    /* CONTROLS */
    .control-group {
      margin-bottom: 1.25rem;
    }
    .control-group label {
      display: block;
      margin-bottom: 0.3rem;
      font-weight: 500;
    }
    .control-group input[type="number"] {
      width: 120px;
      padding: 6px;
      font-size: 0.95rem;
      border: 1px solid #ccc;
      border-radius: 4px;
    }
    .btn {
      display: inline-block;
      background: #0d6efd;
      color: #fff;
      border: none;
      border-radius: 4px;
      padding: 8px 14px;
      font-size: 0.95rem;
      cursor: pointer;
      margin-right: 0.5rem;
      margin-top: 0.5rem;
    }
    .btn:hover {
      background: #0b5ed7;
    }
    /* Make rows vertical on narrow screens */
    @media (max-width: 600px) {
      .dashboard {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <header>
    Water Purification Dashboard
  </header>
  
  <div class="container">
    <!-- DASHBOARD GRID: SENSORS + PUMPS -->
    <div class="dashboard">
      <!-- CARD: WATER METRICS -->
      <div class="card">
        <h3>Sensor Readings</h3>
        
        <div class="metric">
          <span class="label">TDS:</span>
          <span class="value" id="tdsValue">--</span>
        </div>
        <div class="metric">
          <span class="label">pH:</span>
          <span class="value" id="pHValue">--</span>
        </div>
        <div class="metric">
          <span class="label">Tank Lvl:</span>
          <div style="text-align:right;">
            <span class="value" id="levelValue">--</span>
            <div class="progress-bar-container">
              <div class="progress-bar" id="levelBar"></div>
            </div>
          </div>
        </div>
        <div class="metric">
          <span class="label">Tank Ht:</span>
          <span class="value" id="tankHeight">--</span>
        </div>
      </div>
      <!-- CARD: PUMP STATES -->
      <div class="card">
        <h3>Pump Status</h3>
        
        <div class="metric">
          <span class="label">Pump1:</span>
          <span class="value" id="pump1State">OFF</span>
        </div>
        <div class="metric">
          <span class="label">Pump2:</span>
          <span class="value" id="pump2State">OFF</span>
        </div>
      </div>
    </div>

    <!-- CONTROLS CARD -->
    <div class="card">
      <h2>Controls</h2>

      <div class="control-group">
        <label for="heightInput">Set Tank Height (cm)</label>
        <input type="number" id="heightInput" placeholder="Enter cm" />
        <button class="btn" onclick="setTankHeight()">Update</button>
      </div>
      <hr/>

      <h3>Pump #1 Control</h3>
      <div class="control-group">
        <button class="btn" onclick="startPump1()">Start Pump1</button>
        <button class="btn" onclick="stopPump1()">Stop Pump1</button>
      </div>
      <hr/>

      <h3>Pump #2 Control</h3>
      <div class="control-group">
        <button class="btn" onclick="startPump2()">Start Pump2</button>
        <button class="btn" onclick="stopPump2()">Stop Pump2</button>
      </div>
    </div>
  </div>

  <!-- JAVASCRIPT -->
  <script>
    async function fetchSensorData() {
      try {
        let response = await fetch('/api/data');
        if (!response.ok) return;
        let data = await response.json();

        // Update UI with sensor data
        document.getElementById('tdsValue').textContent   = data.tds.toFixed(0) + ' ppm';
        document.getElementById('pHValue').textContent    = data.ph.toFixed(2);
        document.getElementById('levelValue').textContent = data.level.toFixed(1) + ' %';
        document.getElementById('tankHeight').textContent = data.tankHeight.toFixed(1) + ' cm';

        // Update progress bar for tank level
        const levelBar = document.getElementById('levelBar');
        levelBar.style.width = data.level.toFixed(1) + '%';

        document.getElementById('pump1State').textContent = data.pump1 ? 'ON' : 'OFF';
        document.getElementById('pump2State').textContent = data.pump2 ? 'ON' : 'OFF';
      } catch (err) {
        console.log('Error fetching data:', err);
      }
    }

    // Auto-refresh every 2 seconds
    setInterval(fetchSensorData, 2000);
    // Initial load
    fetchSensorData();

    // Set the tank height in cm (user input)
    async function setTankHeight() {
      let val = document.getElementById('heightInput').value;
      if(!val) return;
      try {
        await fetch('/api/setTankHeight', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: 'height=' + encodeURIComponent(val)
        });
        fetchSensorData();
      } catch (err) {
        console.log('Error setting tank height:', err);
      }
    }

    // Pump1 control
    async function startPump1() {
      try { 
        await fetch('/api/startPump1', { method: 'POST' }); 
        fetchSensorData(); 
      }
      catch (err) { console.log('Error starting pump1:', err); }
    }
    async function stopPump1() {
      try { 
        await fetch('/api/stopPump1', { method: 'POST' }); 
        fetchSensorData(); 
      }
      catch (err) { console.log('Error stopping pump1:', err); }
    }

    // Pump2 control
    async function startPump2() {
      try { 
        await fetch('/api/startPump2', { method: 'POST' }); 
        fetchSensorData(); 
      }
      catch (err) { console.log('Error starting pump2:', err); }
    }
    async function stopPump2() {
      try { 
        await fetch('/api/stopPump2', { method: 'POST' }); 
        fetchSensorData(); 
      }
      catch (err) { console.log('Error stopping pump2:', err); }
    }
  </script>
</body>
</html>
  )=====";

  server.send(200, "text/html", html);
}

// Return JSON data for auto-refresh
void handleApiData() {
  // Build JSON with sensor data + user-set tank height + pump states
  String json = "{";
  json += "\"tds\":"        + String(tdsValue, 2)             + ",";
  json += "\"ph\":"         + String(pHValue, 2)              + ",";
  json += "\"level\":"      + String(waterLevelPercent, 2)    + ",";
  json += "\"tankHeight\":" + String(tankHeightCm, 1)         + ",";
  json += "\"pump1\":"      + String(pump1State ? "true":"false") + ",";
  json += "\"pump2\":"      + String(pump2State ? "true":"false");
  json += "}";
  server.send(200, "application/json", json);
}

// POST to set the tank height in cm
void handleSetTankHeight() {
  if (server.hasArg("height")) {
    float newHt = server.arg("height").toFloat();
    if (newHt < 0) newHt = 0;  // can clamp if needed
    tankHeightCm = newHt;
  }
  server.send(200, "text/plain", "OK");
}

// Pump1 endpoints
void handleStartPump1() {
  pump1On();
  server.send(200, "text/plain", "Pump1 Started");
}
void handleStopPump1() {
  pump1Off();
  server.send(200, "text/plain", "Pump1 Stopped");
}

// Pump2 endpoints
void handleStartPump2() {
  pump2On();
  server.send(200, "text/plain", "Pump2 Started");
}
void handleStopPump2() {
  pump2Off();
  server.send(200, "text/plain", "Pump2 Stopped");
}

// ------------------------------------------------------------------------
//                         WIFI & SERVER SETUP
// ------------------------------------------------------------------------
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address:");
  Serial.println(WiFi.localIP());
}

void setupServer() {
  server.on("/",               HTTP_GET,  handleRoot);
  server.on("/api/data",       HTTP_GET,  handleApiData);

  server.on("/api/setTankHeight", HTTP_POST, handleSetTankHeight);
  
  server.on("/api/startPump1",    HTTP_POST, handleStartPump1);
  server.on("/api/stopPump1",     HTTP_POST, handleStopPump1);
  server.on("/api/startPump2",    HTTP_POST, handleStartPump2);
  server.on("/api/stopPump2",     HTTP_POST, handleStopPump2);

  server.begin();
  Serial.println("Web server started.");
}

// ------------------------------------------------------------------------
//                             SETUP
// ------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Pins for analog sensors
  pinMode(TDS_SENSOR_PIN, INPUT);
  pinMode(PH_SENSOR_PIN,  INPUT);
  
  // Ultrasonic pins
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  // Pump relays (active-low)
  pinMode(PUMP1_RELAY_PIN, OUTPUT);
  pinMode(PUMP2_RELAY_PIN, OUTPUT);
  pump1Off();
  pump2Off();

  // Initialize LCD (16x2, address 0x27)
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Water Monitor");

  // Connect WiFi + Start Web Server
  setupWiFi();
  setupServer();
}

// ------------------------------------------------------------------------
//                               LOOP
// ------------------------------------------------------------------------
void loop() {
  // 1) Collect TDS samples ~every 40ms
  static unsigned long sampleTimer = millis();
  if (millis() - sampleTimer > 40U) {
    sampleTimer = millis();
    analogBuffer[analogBufferIndex] = analogRead(TDS_SENSOR_PIN);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) {
      analogBufferIndex = 0;
    }
  }

  // 2) Every ~800ms, compute TDS, read pH & level
  static unsigned long sensorTimer = millis();
  if (millis() - sensorTimer > 800U) {
    sensorTimer = millis();

    // (a) TDS median filtering
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    }
    tdsValue = readTDS();

    // (b) pH reading
    pHValue = readPH();

    // (c) Ultrasonic level (percentage based on user-set tankHeightCm)
    waterLevelPercent = readFilteredTankLevel();

    // Debug output
    Serial.print("TDS: ");
    Serial.print(tdsValue, 0);
    Serial.print(" ppm | pH: ");
    Serial.print(pHValue, 2);
    Serial.print(" | Lvl: ");
    Serial.print(waterLevelPercent, 1);
    Serial.print("% (Ht=");
    Serial.print(tankHeightCm, 1);
    Serial.print("cm) | Pump1:");
    Serial.print(pump1State ? "ON" : "OFF");
    Serial.print(" Pump2:");
    Serial.println(pump2State ? "ON" : "OFF");

    // (d) Update LCD with small summary
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TDS:");
    lcd.print(tdsValue, 0);
    lcd.print("ppm  pH:");
    lcd.print(pHValue, 1);

    lcd.setCursor(0, 1);
    lcd.print("Lvl:");
    lcd.print(waterLevelPercent, 0);
    lcd.print("% H:");
    lcd.print(tankHeightCm, 0);
    lcd.print("cm");
  }

  // 3) Handle incoming web requests
  server.handleClient();
}
