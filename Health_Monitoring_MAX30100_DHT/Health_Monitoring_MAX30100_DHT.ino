#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

#include <WiFi.h>        // For ESP32
#include <WebServer.h>   // For ESP32

#include <DHT.h>

// ------------------- Adjust these as needed -------------------
#define DHTPIN  4        // GPIO pin for DHT11 data
#define DHTTYPE DHT11    // or DHT22, etc.
DHT dht(DHTPIN, DHTTYPE);

// MAX30100
PulseOximeter pox;

// Wi-Fi credentials
const char* SSID     = "MyProject";
const char* PASSWORD = "12345678";

// Web server on port 80
WebServer server(80);

// Sensor variables
float temperature = 0.0;
float humidity    = 0.0;

// ------------------- Pulse detection callback -------------------
void onBeatDetected()
{
  Serial.println("Beat!");
}

// ------------------- Webserver Handlers -------------------

// HTML page that uses JavaScript to periodically fetch /data and update the DOM.
// Includes inline CSS for a modern, responsive card layout and styling.
void handleRoot() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta charset='utf-8'/>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"; // Responsive
  page += "<title>MAX30100 + DHT11 Data</title>";

  // Inline CSS for a modern design
  page += "<style>";
  page += "body { margin:0; padding:0; font-family:Arial, sans-serif; background:#f4f4f4; color:#333; }";
  page += ".header { background:#293846; color:#fff; padding:1em; text-align:center; }";
  page += ".header h1 { margin:0; font-size:1.5em; }";
  page += ".card-container { display:flex; flex-wrap:wrap; justify-content:center; max-width:1200px; margin:20px auto; }";
  page += ".card { background:#fff; border-radius:8px; box-shadow:0 2px 5px rgba(0,0,0,0.15); margin:10px; padding:20px; flex:1; min-width:220px; max-width:300px; text-align:center; }";
  page += ".card h2 { margin:0 0 10px; font-size:1.1em; color:#555; }";
  page += ".value { font-size:2em; margin:10px 0; font-weight:bold; }";
  page += ".footer { text-align:center; color:#666; margin:20px 0; }";
  page += ".footer a { color:#293846; text-decoration:none; }";
  page += ".footer a:hover { text-decoration:underline; }";
  page += "</style>";

  // Inline JavaScript to periodically fetch /data and update the DOM
  page += "<script>";
  page += "function getSensorData() {";
  page += "  fetch('/data')"
            ".then(response => response.json())"
            ".then(data => {"
            "    document.getElementById('temp').innerHTML = data.temperature + ' &deg;C';"
            "    document.getElementById('hum').innerHTML = data.humidity + ' %';"
            "    document.getElementById('hr').innerHTML = data.heart_rate + ' bpm';"
            "    document.getElementById('spo2').innerHTML = data.spo2 + ' %';"
            "});"
          "}";
  // Call `getSensorData()` once on load, then every 1000 ms (1 second)
  page += "window.onload = function() { getSensorData(); setInterval(getSensorData, 1000); }";
  page += "</script>";
  page += "</head><body>";

  page += "<div class='header'><h1>MAX30100 + DHT11 Dashboard</h1></div>";

  page += "<div class='card-container'>";

  page += "  <div class='card'>";
  page += "    <h2>Temperature</h2>";
  page += "    <div class='value' id='temp'>N/A</div>";
  page += "  </div>";

  page += "  <div class='card'>";
  page += "    <h2>Humidity</h2>";
  page += "    <div class='value' id='hum'>N/A</div>";
  page += "  </div>";

  page += "  <div class='card'>";
  page += "    <h2>Heart Rate</h2>";
  page += "    <div class='value' id='hr'>N/A</div>";
  page += "  </div>";

  page += "  <div class='card'>";
  page += "    <h2>SpO2</h2>";
  page += "    <div class='value' id='spo2'>N/A</div>";
  page += "  </div>";

  page += "</div>"; // .card-container

  page += "<div class='footer'>";
  page += "<p>JSON Data Endpoint: <a href='/data'>/data</a></p>";
  page += "</div>";

  page += "</body></html>";

  server.send(200, "text/html", page);
}

// Return JSON data for external or AJAX clients
void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":"    + String(humidity)    + ",";
  json += "\"heart_rate\":"  + String(pox.getHeartRate()) + ",";
  json += "\"spo2\":"        + String(pox.getSpO2());
  json += "}";

  Serial.println(json);  // Debug print
  server.send(200, "application/json", json);
}

// ------------------- Setup -------------------
void setup()
{
  Serial.begin(115200);
  delay(100);

  // Initialize WiFi
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize MAX30100
  Serial.print("Initializing pulse oximeter...");
  if (!pox.begin()) {
    Serial.println("FAILED");
    // Halt if sensor init fails
    //while (1) { delay(1); }
  } else {
    Serial.println("SUCCESS");
  }
  pox.setOnBeatDetectedCallback(onBeatDetected);

  // Initialize the DHT sensor
  dht.begin();

  // Configure webserver routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started.");
}

// ------------------- Main Loop -------------------
void loop()
{
  // Handle any incoming HTTP requests
  server.handleClient();

  // Update pulse oximeter readings continuously
  pox.update();

  // Read DHT sensor in each loop (or you can limit reading frequency if desired)
  humidity    = dht.readHumidity();
  temperature = dht.readTemperature();
}
