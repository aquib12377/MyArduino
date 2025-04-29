#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <RBDdimmer.h>

// --- WiFi STA Mode (connect to an external hotspot) ---
const char *ssid = "MyProject";      // Your Hotspot SSID
const char *password = "12345678";    // Your Hotspot Password

// DHT11 Configuration
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Current Sensor Pin
#define CURRENT_SENSOR_PIN 36  // GPIO 36

// IR Sensors for Swipe Detection
#define IR_SWIPE1_PIN 32
#define IR_SWIPE2_PIN 5

// Variables for swipe detection
int swipeState = 0;   // 0: idle, 1: IR_SWIPE1 triggered, 2: IR_SWIPE2 triggered
unsigned long swipeStartTime = 0;
const unsigned long swipeTimeout = 500; // Increased timeout (ms) to complete a swipe
unsigned long lastSwipeDetected = 0;
const unsigned long swipeCooldown = 500;  // ms cooldown between swipes

// Variables for edge detection (previous sensor readings)
int lastIR1State = HIGH;
int lastIR2State = HIGH;

// Fan Control using AC Light Dimmer Module via RBDdimmer library
// The module has four pins: Vcc, GND, ZC, and PWM.
#define FAN_DIMMER_PIN 25
#define FAN_ZC_PIN 26
// Create a dimmerLamp object on the given pins: PWM and Zero-cross.
dimmerLamp dimmer(FAN_DIMMER_PIN, FAN_ZC_PIN);

bool fanOn = false;
int fanSpeedLevel = 1; // Levels: 1 to 4

WebServer server(80);

// Update the dimmer power based on fan state and speed level.
// Mapping: fanSpeedLevel * 20 capped at 80%.
// When fan is OFF, brightness is 0.
void updateFanDimmer() {
  int brightness = 0;
  if (fanOn) {
    brightness = fanSpeedLevel * 20 + 20;
    if (brightness > 80) {
      brightness = 80;
    }
  }
  dimmer.setPower(brightness);
  Serial.print("Dimmer set to ");
  Serial.print(brightness);
  Serial.println("%");
}

// Prepare JSON sensor data (includes sensor readings and fan info)
String getSensorDataJSON() {
  int raw = analogRead(CURRENT_SENSOR_PIN);  // Range 0-4095
  float voltage = raw * (3.3 / 4095.0);
  float currentVal = (voltage - 2.5) / 0.066;
  if (currentVal < 0)
    currentVal = 0;
  
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  String json = "{";
  json += "\"current\":" + String(currentVal, 2) + ",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"fanOn\":" + String(fanOn ? "true" : "false") + ",";
  json += "\"fanSpeedLevel\":" + String(fanSpeedLevel);
  json += "}";
  return json;
}

void handleAPI() {
  server.send(200, "application/json", getSensorDataJSON());
}

void handleToggleFan() {
  fanOn = !fanOn;
  // For demonstration, when toggling ON, set fan speed to level 4.
  if (fanOn) {
    fanSpeedLevel = 4;
  }
  updateFanDimmer();
  server.send(200, "text/plain", fanOn ? "Fan ON" : "Fan OFF");
}

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32 Sensor Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; text-align: center; margin-top: 50px; }
      .card { border: 1px solid #ccc; padding: 20px; margin: 10px; display: inline-block; width: 200px; }
      button { padding: 10px 20px; font-size: 16px; }
    </style>
  </head>
  <body>
    <h1>ESP32 Sensor Dashboard</h1>
    <div class="card">
      <h3>Current</h3>
      <p id="current">--</p>
    </div>
    <div class="card">
      <h3>Temperature</h3>
      <p id="temperature">--</p>
    </div>
    <div class="card">
      <h3>Humidity</h3>
      <p id="humidity">--</p>
    </div>
    <div class="card">
      <h3>Fan</h3>
      <p id="fanStatus">OFF</p>
      <p>Speed Level: <span id="fanSpeedLevel">1</span></p>
      <button onclick="toggleFan()">Toggle Fan</button>
    </div>
    <script>
      function updateData() {
        fetch('/api/sensors')
          .then(res => res.json())
          .then(data => {
            document.getElementById('current').innerText = data.current + " A";
            document.getElementById('temperature').innerText = data.temperature + " °C";
            document.getElementById('humidity').innerText = data.humidity + " %";
            document.getElementById('fanStatus').innerText = data.fanOn ? "ON" : "OFF";
            document.getElementById('fanSpeedLevel').innerText = data.fanSpeedLevel;
          })
          .catch(err => console.error("API error: ", err));
      }
      function toggleFan(){
        fetch('/fan/toggle')
          .then(res => res.text())
          .then(status => {
            alert("Fan status: " + status);
            updateData();
          })
          .catch(err => console.error("Fan toggle error:", err));
      }
      setInterval(updateData, 5000);
      updateData();
    </script>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize IR swipe sensor pins
  pinMode(IR_SWIPE1_PIN, INPUT_PULLUP);
  pinMode(IR_SWIPE2_PIN, INPUT_PULLUP);
  
  dht.begin();
  
  // Connect to external WiFi Hotspot
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server endpoints
  server.on("/", handleRoot);
  server.on("/api/sensors", handleAPI);
  server.on("/fan/toggle", handleToggleFan);
  server.begin();
  
  // Initialize the dimmer module
  fanOn = false;
  fanSpeedLevel = 0;
  dimmer.begin(NORMAL_MODE, ON);
  updateFanDimmer();
}

void loop() {
  server.handleClient();
        updateFanDimmer();
  // Read current states of IR sensors
  int currentIR1 = digitalRead(IR_SWIPE1_PIN);
  int currentIR2 = digitalRead(IR_SWIPE2_PIN);
  
  // If no swipe is in progress, check for a falling edge on either sensor.
  if (swipeState == 0) {
    if (currentIR1 == LOW && lastIR1State == HIGH) {
      swipeState = 1; // Starting swipe from sensor 1.
      swipeStartTime = millis();
      Serial.println("IR_SWIPE1 triggered");
    }
    else if (currentIR2 == LOW && lastIR2State == HIGH) {
      swipeState = 2; // Starting swipe from sensor 2.
      swipeStartTime = millis();
      Serial.println("IR_SWIPE2 triggered");
    }
  }
  // If waiting for a right swipe (starting at IR_SWIPE1), check for sensor 2 trigger.
  else if (swipeState == 1) {
    if (currentIR2 == LOW) { 
      // Right swipe detected: increase fan speed if below maximum.
      if (fanSpeedLevel < 4) {
        fanSpeedLevel++;
        updateFanDimmer();
        Serial.print("Swipe Right: Increasing fan speed to level ");
        Serial.println(fanSpeedLevel);
      } else {
        Serial.println("Swipe Right: Already at maximum speed");
      }
      lastSwipeDetected = millis();
      swipeState = 0;
    }
  }
  // If waiting for a left swipe (starting at IR_SWIPE2), check for sensor 1 trigger.
  else if (swipeState == 2) {
    if (currentIR1 == LOW) {
      // Left swipe detected: decrease fan speed if above minimum.
      if (fanSpeedLevel > 1) {
        fanSpeedLevel--;
        updateFanDimmer();
        Serial.print("Swipe Left: Decreasing fan speed to level ");
        Serial.println(fanSpeedLevel);
      } else {
        Serial.println("Swipe Left: Already at minimum speed");
      }
      lastSwipeDetected = millis();
      swipeState = 0;
    }
  }
  
  // If swipe state is active and the timeout expires, reset the state.
  if (swipeState != 0 && (millis() - swipeStartTime) > swipeTimeout) {
    Serial.println("Swipe timeout - resetting state");
    swipeState = 0;
  }
  
  // Update last sensor states for edge detection.
  lastIR1State = currentIR1;
  lastIR2State = currentIR2;
}
