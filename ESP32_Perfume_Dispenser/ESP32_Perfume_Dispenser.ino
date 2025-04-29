/*************************************************************
  4-Load-Cell Perfume Dispenser
  Using normal WebServer (not Async) & minimal UI
  (Removes manual calibration & tare endpoints)
*************************************************************/
#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h"
#include "soc/rtc.h"  // CPU frequency fix

// ---------------------- Wi-Fi AP Config ----------------------
const char* AP_SSID     = "Load_Cell";
const char* AP_PASSWORD = "LoadCell1234";

// Standard web server on port 80
WebServer server(80);

// ---------------------- Load Cell Pins -----------------------
#define LOADCELL1_DOUT_PIN  4
#define LOADCELL1_SCK_PIN   16
#define LOADCELL2_DOUT_PIN  17
#define LOADCELL2_SCK_PIN   5
#define LOADCELL3_DOUT_PIN  18
#define LOADCELL3_SCK_PIN   19
#define LOADCELL4_DOUT_PIN  33
#define LOADCELL4_SCK_PIN   32

// ---------------------- HX711 Objects ------------------------
HX711 loadCell1;  // Source container 1
HX711 loadCell2;  // Source container 2
HX711 loadCell3;  // Source container 3
HX711 loadCell4;  // Target container

// ---------------------- Calibration Factors -------------------
// Adjust these for your actual hardware
float calFactor1 = -30917.333f / 74;  
float calFactor2 =  73468.666f / 197; 
float calFactor3 =  70436.666f / 197; 
float calFactor4 =  29166.333f / 74;  

// ---------------------- Pump Pins (Active LOW) ---------------
#define PUMP1 25
#define PUMP2 27
#define PUMP3 14

// ---------------------- Dispensing Logic ----------------------
float targetIncrement1 = 5.0f;   // grams from container1
float targetIncrement2 = 1.0f;   // grams from container2
float targetIncrement3 = 10.0f;  // grams from container3

float initialWeightContainer4 = 0.0f;

enum DispenseState { IDLE, DISPENSING, COMPLETE };
DispenseState dispenseState = IDLE;
int currentStep = 0;  // which container we're dispensing from (0..2)

// ---------------------- 5-Second Snapshot ----------------------
unsigned long lastMeasureTime = 0;
float storedW1 = 0.0f;
float storedW2 = 0.0f;
float storedW3 = 0.0f;
float storedW4 = 0.0f;

// ---------------------- CPU Frequency Fix ----------------------
void fixCpuFrequency() {
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_set_config(&config);
  rtc_clk_cpu_freq_set_config_fast(&config);
}

// ---------------------- Dispensing Logic ----------------------
void handleDispensing() {
  if (dispenseState != DISPENSING) return;
  Serial.println("Handling Dispense");

  float increments[3] = { targetIncrement1, targetIncrement2, targetIncrement3 };
  int pumpPins[3] = { PUMP1, PUMP2, PUMP3 };
  float cumulative[3];
  cumulative[0] = increments[0];
  cumulative[1] = increments[0] + increments[1];
  cumulative[2] = increments[0] + increments[1] + increments[2];

  // Real-time read for dispensing control
  loadCell4.power_up();
  float currentWeight4 = loadCell4.get_units(10);
loadCell4.power_down();
    Serial.println("CUrrent Weight - " +String(currentWeight4));

  // Skip zero increments if any
  while (currentStep < 3 && increments[currentStep] == 0) {
    currentStep++;
  }
  if (currentStep < 3) {
    digitalWrite(pumpPins[currentStep], LOW); // pump ON (active LOW)
    float actualGain = currentWeight4 - initialWeightContainer4;
    float required   = cumulative[currentStep];
  Serial.println("CUrrent Weight - " +String(currentWeight4));
    if (actualGain >= required) {
      // Turn pump off
      digitalWrite(pumpPins[currentStep], HIGH);
      currentStep++;
      // brief settle
      unsigned long t0 = millis();
      while (millis() - t0 < 50) {
        delay(1);
      }
    }
  } else {
    // All steps done
    dispenseState = COMPLETE;
    Serial.println("[+] Dispensing complete.");
    dispenseState = IDLE;
  }
}

// ---------------------- Start Dispense ----------------------
void startDispense(float t1, float t2, float t3) {
  targetIncrement1 = t1;
  targetIncrement2 = t2;
  targetIncrement3 = t3;
loadCell1.power_up();
    loadCell2.power_up();
    loadCell3.power_up();
    loadCell4.power_up();
  // Optional check if source containers have enough liquid
  float w1 = loadCell1.get_units(10);
  float w2 = loadCell2.get_units(10);
  float w3 = loadCell3.get_units(10);
  if ((t1 > 0 && w1 < (t1 + 5)) ||
      (t2 > 0 && w2 < (t2 + 5)) ||
      (t3 > 0 && w3 < (t3 + 5))) {
    Serial.println("[!] Insufficient liquid in at least one container");
    return; // do not start
  }

  // Record target container's baseline
  initialWeightContainer4 = loadCell4.get_units(5);

    currentStep   = 0;
    dispenseState = DISPENSING;
    Serial.printf("[+] Dispense started: t1=%.1f, t2=%.1f, t3=%.1f\n", t1,t2,t3);
    loadCell1.power_down();
    loadCell2.power_down();
    loadCell3.power_down();
    loadCell4.power_down();
}

// ---------------------- Web Handlers ----------------------
void handleRoot() {
  // Minimal redesigned UI (no manual calibration/tare)
  String html = R"=====(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="utf-8">
    <title>4-Load-Cell Dispenser</title>
    <style>
      body {
        font-family: Arial, sans-serif; 
        margin: 20px;
      }
      .container {
        max-width: 400px;
        margin: auto;
      }
      h1, h2 {
        text-align: center;
      }
      label {
        display: inline-block; 
        width: 150px;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Perfume Dispenser</h1>
      <p>Values update approximately every 5 seconds (snapshot).</p>
      <h2>Current Weights</h2>
      <p>Container 1: <span id="w1">0.0</span> g</p>
      <p>Container 2: <span id="w2">0.0</span> g</p>
      <p>Container 3: <span id="w3">0.0</span> g</p>
      <p>Container 4 (Target): <span id="w4">0.0</span> g</p>

      <h2>Start Dispense</h2>
      <p>
        <label>Container 1 (g):</label>
        <input id="t1" type="number" step="0.1" value="5">
      </p>
      <p>
        <label>Container 2 (g):</label>
        <input id="t2" type="number" step="0.1" value="1">
      </p>
      <p>
        <label>Container 3 (g):</label>
        <input id="t3" type="number" step="0.1" value="10">
      </p>
      <button onclick="startDispense()">Start Dispense</button>
    </div>

    <script>
      // Fetch snapshot /status every second
      setInterval(()=>{
        fetch('/status')
          .then(r=>r.json())
          .then(d=>{
            document.getElementById('w1').textContent = d.w1.toFixed(1);
            document.getElementById('w2').textContent = d.w2.toFixed(1);
            document.getElementById('w3').textContent = d.w3.toFixed(1);
            document.getElementById('w4').textContent = d.w4.toFixed(1);
          });
      },1000);

      function startDispense(){
        let t1 = parseFloat(document.getElementById('t1').value) || 0;
        let t2 = parseFloat(document.getElementById('t2').value) || 0;
        let t3 = parseFloat(document.getElementById('t3').value) || 0;
        fetch(`/startDispense?target1=${t1}&target2=${t2}&target3=${t3}`)
          .then(r=>r.text())
          .then(alert)
          .catch(e=>console.error(e));
      }
    </script>
  </body>
  </html>
  )=====";
  server.send(200, "text/html", html);
}

void handleStatus() {
  // Return the 5s snapshot
  String json = "{";
  json += "\"w1\":" + String(storedW1, 1) + ",";
  json += "\"w2\":" + String(storedW2, 1) + ",";
  json += "\"w3\":" + String(storedW3, 1) + ",";
  json += "\"w4\":" + String(storedW4, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handleStartDispense() {
  if (!server.hasArg("target1") ||
      !server.hasArg("target2") ||
      !server.hasArg("target3")) {
    server.send(400, "text/plain", "Missing target1/2/3");
    return;
  }
  float t1 = server.arg("target1").toFloat();
  float t2 = server.arg("target2").toFloat();
  float t3 = server.arg("target3").toFloat();

  startDispense(t1, t2, t3);
  String msg = "Dispense started: t1=" + String(t1) +
               ", t2=" + String(t2) +
               ", t3=" + String(t3);
  server.send(200, "text/plain", msg);
}

// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);

  fixCpuFrequency(); // Helps avoid NaN from HX711 on some boards

  // Start AP
  WiFi.mode(WIFI_MODE_AP);
  IPAddress localIP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println("[+] Wi-Fi AP started");
  Serial.print("    SSID: ");  Serial.println(AP_SSID);
  Serial.print("    PASS: ");  Serial.println(AP_PASSWORD);
  Serial.print("    IP  : ");  Serial.println(WiFi.softAPIP());
  Serial.print("    IP  : ");  Serial.println(WiFi.localIP());

  // Pumps off (active LOW)
  pinMode(PUMP1, OUTPUT); digitalWrite(PUMP1, HIGH);
  pinMode(PUMP2, OUTPUT); digitalWrite(PUMP2, HIGH);
  pinMode(PUMP3, OUTPUT); digitalWrite(PUMP3, HIGH);

  // Init HX711
  loadCell1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  loadCell2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  loadCell3.begin(LOADCELL3_DOUT_PIN, LOADCELL3_SCK_PIN);
  loadCell4.begin(LOADCELL4_DOUT_PIN, LOADCELL4_SCK_PIN);

  // Set scale factors (pre-determined)
  loadCell1.set_scale(calFactor1);
  loadCell2.set_scale(calFactor2);
  loadCell3.set_scale(calFactor3);
  loadCell4.set_scale(calFactor4);

  loadCell1.set_gain(128);
  loadCell2.set_gain(128);
  loadCell3.set_gain(128);
  loadCell4.set_gain(128);
  // Tare once at  startup (optional)
  loadCell1.tare();
  loadCell2.tare();
  loadCell3.tare();
  loadCell4.tare();

  // Web routes
  server.on("/",          HTTP_GET, handleRoot);
  server.on("/status",    HTTP_GET, handleStatus);
  server.on("/startDispense", HTTP_GET, handleStartDispense);

  // Start the server
  server.begin();
  Serial.println("[+] Server started");
}

// ---------------------- Main Loop ----------------------
void loop() {
  // 1) Handle incoming HTTP clients
  server.handleClient();

  // 2) Update the 5-second snapshot if needed
  if (millis() - lastMeasureTime >= 5000) {
    loadCell1.power_up();
    loadCell2.power_up();
    loadCell3.power_up();
    loadCell4.power_up();
    lastMeasureTime = millis();
    storedW1 = loadCell1.get_units(10);
    storedW2 = loadCell2.get_units(10);
    storedW3 = loadCell3.get_units(10);
    storedW4 = loadCell4.get_units(10);
    loadCell1.power_down();
    loadCell2.power_down();
    loadCell3.power_down();
    loadCell4.power_down();
    Serial.println("[~] Updated 5s snapshot");
  }
  //Serial.pri
  // 3) Run dispensing logic (non-blocking)
  handleDispensing();
}
