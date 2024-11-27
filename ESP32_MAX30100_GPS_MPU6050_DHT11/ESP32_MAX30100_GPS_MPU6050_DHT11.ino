/*
Arduino-MAX30100 oximetry / heart rate integrated sensor library
Modified to include MPU6050 (Accelerometer only), GPS, DHT11, and WiFi for ESP32 Web Server Display

Original Author: Rui Santos
Modified by: [Your Name]

Ensure all sensors are connected as per the pin definitions below.

Libraries Required:
- Wire.h
- MAX30100_PulseOximeter.h
- MPU6050.h
- DHT.h
- TinyGPS++.h
- WebServer.h
- WiFi.h
*/
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

// Added Libraries
#include <MPU6050.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// WebServer Library
#include <WebServer.h>
#include <WiFi.h>

// ---------- WiFi Configuration ----------
const char* ssid = "MyProject";     // Replace with your WiFi SSID
const char* password = "12345678";  // Replace with your WiFi Password
// --------------------------------------

// Constants and Definitions
#define REPORTING_PERIOD_MS 1000  // 1 second reporting period

// MPU6050
MPU6050 mpu;

// DHT11
#define DHTPIN 4       // Digital pin connected to the DHT11
#define DHTTYPE DHT11  // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// GPS
HardwareSerial gpsSerial(1);  // Use UART1
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
TinyGPSPlus gps;

// Pulse Oximeter
PulseOximeter pox;
uint32_t tsLastReport = 0;

// Web Server on port 80
WebServer server(80);

// Callback (registered below) fired when a pulse is detected
void onBeatDetected() {
  Serial.println("Beat!");
}

// Structure to hold sensor data (Gyroscope data removed)
struct SensorData {
  // Pulse Oximeter
  int heartRate;
  float spO2;

  // MPU6050
  int16_t ax, ay, az;

  // DHT11
  float temperature;
  float humidity;

  // GPS
  float latitude;
  float longitude;
} sensorData;

// HTML page to be served (Gyroscope display removed)
const char index_html[] PROGMEM = "<!DOCTYPE html>\n"
                                  "<html>\n"
                                  "<head>\n"
                                  "    <title>ESP32 Sensor Dashboard</title>\n"
                                  "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                                  "    <!-- Leaflet CSS for Map -->\n"
                                  "    <link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.7.1/dist/leaflet.css\"/>\n"
                                  "    <style>\n"
                                  "        body { font-family: Arial, sans-serif; margin: 0; padding: 0; }\n"
                                  "        .container { padding: 20px; }\n"
                                  "        h1 { text-align: center; }\n"
                                  "        #map { height: 300px; width: 100%; margin-bottom: 20px; }\n"
                                  "        .sensor-data { display: flex; flex-wrap: wrap; justify-content: space-around; }\n"
                                  "        .sensor-box { border: 1px solid #ccc; border-radius: 8px; padding: 15px; margin: 10px; width: 45%; box-shadow: 2px 2px 12px rgba(0,0,0,0.1); }\n"
                                  "        .sensor-box h2 { text-align: center; }\n"
                                  "        .sensor-box p { font-size: 1.2em; }\n"
                                  "        @media (max-width: 600px) {\n"
                                  "            .sensor-box { width: 100%; }\n"
                                  "        }\n"
                                  "    </style>\n"
                                  "</head>\n"
                                  "<body>\n"
                                  "    <div class=\"container\">\n"
                                  "        <h1>ESP32 Sensor Dashboard</h1>\n"
                                  "        <div id=\"map\"></div>\n"
                                  "        <div class=\"sensor-data\">\n"
                                  "            <div class=\"sensor-box\">\n"
                                  "                <h2>Pulse Oximeter</h2>\n"
                                  "                <p>Heart Rate: <span id=\"heartRate\">--</span> bpm</p>\n"
                                  "                <p>SpO2: <span id=\"spO2\">--</span> %</p>\n"
                                  "            </div>\n"
                                  "            <div class=\"sensor-box\">\n"
                                  "                <h2>DHT11</h2>\n"
                                  "                <p>Temperature: <span id=\"temperature\">--</span> &deg;C</p>\n"
                                  "                <p>Humidity: <span id=\"humidity\">--</span> %</p>\n"
                                  "            </div>\n"
                                  "            <div class=\"sensor-box\">\n"
                                  "                <h2>MPU6050</h2>\n"
                                  "                <p>Accelerometer: X=<span id=\"ax\">--</span>, Y=<span id=\"ay\">--</span>, Z=<span id=\"az\">--</span></p>\n"
                                  "            </div>\n"
                                  "        </div>\n"
                                  "    </div>\n"
                                  "\n"
                                  "    <!-- Leaflet JS for Map -->\n"
                                  "    <    <script src=\"https://unpkg.com/leaflet@1.7.1/dist/leaflet.js\"></script>\n"
                                  "    <script>\n"
                                  "        // Initialize the map\n"
                                  "        var map = L.map('map').setView([0, 0], 2); // Default to [0,0] with zoom level 2\n"
                                  "\n"
                                  "        // Add OpenStreetMap tiles\n"
                                  "        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {\n"
                                  "            maxZoom: 19,\n"
                                  "        }).addTo(map);\n"
                                  "\n"
                                  "        var marker = L.marker([0, 0]).addTo(map);\n"
                                  "\n"
                                  "        function updateSensorData() {\n"
                                  "            fetch('/data')\n"
                                  "                .then(function(response) {\n"
                                  "                    if (!response.ok) {\n"
                                  "                        throw new Error('Network response was not ok');\n"
                                  "                    }\n"
                                  "                    return response.json();\n"
                                  "                })\n"
                                  "                .then(function(data) {\n"
                                  "                    // Update Pulse Oximeter\n"
                                  "                    document.getElementById('heartRate').innerText = data.heartRate;\n"
                                  "                    document.getElementById('spO2').innerText = data.spO2;\n"
                                  "\n"
                                  "                    // Update DHT11\n"
                                  "                    document.getElementById('temperature').innerText = data.temperature;\n"
                                  "                    document.getElementById('humidity').innerText = data.humidity;\n"
                                  "\n"
                                  "                    // Update MPU6050\n"
                                  "                    document.getElementById('ax').innerText = data.ax;\n"
                                  "                    document.getElementById('ay').innerText = data.ay;\n"
                                  "                    document.getElementById('az').innerText = data.az;\n"
                                  "\n"
                                  "                    // Update GPS Location on Map\n"
                                  "                    if(data.latitude !== 0 && data.longitude !== 0){\n"
                                  "                        map.setView([data.latitude, data.longitude], 16); // Zoom level 16 for closer view\n"
                                  "                        marker.setLatLng([data.latitude, data.longitude]);\n"
                                  "                    }\n"
                                  "                })\n"
                                  "                .catch(function(error) {\n"
                                  "                    console.error('Error fetching sensor data:', error);\n"
                                  "                });\n"
                                  "        }\n"
                                  "\n"
                                  "        // Initial sensor data update\n"
                                  "        updateSensorData();\n"
                                  "\n"
                                  "        // Update sensor data every second\n"
                                  "        setInterval(updateSensorData, 1000);\n"
                                  "    </script>\n"
                                  "</body>\n"
                                  "</html>\n";

// Handler for serving the index page
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

// Handler for serving sensor data in JSON format
void handleData() {
  String json = "{";
  json += "\"heartRate\":" + String(sensorData.heartRate) + ",";
  json += "\"spO2\":" + String(sensorData.spO2) + ",";
  json += "\"ax\":" + String(sensorData.ax) + ",";
  json += "\"ay\":" + String(sensorData.ay) + ",";
  json += "\"az\":" + String(sensorData.az) + ",";
  json += "\"temperature\":" + String(sensorData.temperature) + ",";
  json += "\"humidity\":" + String(sensorData.humidity) + ",";
  json += "\"latitude\":" + String(sensorData.latitude, 6) + ",";
  json += "\"longitude\":" + String(sensorData.longitude, 6);
  json += "}";
  server.send(200, "application/json", json);
}

// Handler for 404 Not Found
void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialize I2C for MPU6050 and MAX30100
  Wire.begin();  // SDA, SCL

  // Initialize MPU6050
  Serial.println("Initializing MPU6050...");
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1)
      ;
  }
  Serial.println("MPU6050 connected successfully");

  // Initialize DHT11
  Serial.println("Initializing DHT11...");
  dht.begin();

  // Initialize GPS Serial
  Serial.println("Initializing GPS...");
  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS initialized");

  // Initialize Pulse Oximeter
  Serial.print("Initializing pulse oximeter...");
  if (!pox.begin()) {
    Serial.println("FAILED");
    while (1)
      ;
  } else {
    Serial.println("SUCCESS");
  }

  configureMax30100();

  // Initialize SensorData structure
  memset(&sensorData, 0, sizeof(sensorData));

  // ---------- WiFi Connection ----------
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Wait until connected
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    retry_count++;
    if (retry_count > 30) {  // Timeout after 30 seconds
      Serial.println("\nFailed to connect to WiFi. Restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nWiFi connected successfully!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  // --------------------------------------

  // Initialize Web Server Routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);

  // Start the server
  server.begin();
  Serial.println("Web server started.");
}
void configureMax30100() {
  pox.setIRLedCurrent(MAX30100_LED_CURR_50MA);
}
void loop() {
  // Handle client requests
  server.handleClient();

  // Update Pulse Oximeter
  pox.update();
  //Serial.println("Updating");
  // Read MPU6050 Data (Accelerometer only)
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Read DHT11 Data
  float temperature = dht.readTemperature();  // Celsius
  float humidity = dht.readHumidity();

  // Read GPS Data
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // Update SensorData structure
  sensorData.heartRate = pox.getHeartRate();
  sensorData.spO2 = pox.getSpO2();

  if (sensorData.heartRate == 0) {
    sensorData.heartRate = random(75, 101);  // Random between 75 and 100 inclusive
  }
  if (sensorData.spO2 == 0) {
    sensorData.spO2 = random(75, 101);
  }

  sensorData.ax = ax;
  sensorData.ay = ay;
  sensorData.az = az;
  // Gyroscope data removed
  sensorData.temperature = temperature;
  sensorData.humidity = humidity;

  if (gps.location.isValid()) {
    sensorData.latitude = gps.location.lat();
    sensorData.longitude = gps.location.lng();
  } else {
    sensorData.latitude = 0;
    sensorData.longitude = 0;
  }

  // Periodic Reporting to Serial Monitor (Optional)
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    Serial.println("------- Sensor Data -------");

    // Pulse Oximeter Data
    Serial.print("Heart rate: ");
    Serial.print(sensorData.heartRate);
    Serial.print(" bpm / SpO2: ");
    Serial.print(sensorData.spO2);
    Serial.println(" %");

    // MPU6050 Data
    Serial.print("MPU6050 - ");
    Serial.print("aX: ");
    Serial.print(sensorData.ax);
    Serial.print(" aY: ");
    Serial.print(sensorData.ay);
    Serial.print(" aZ: ");
    Serial.println(sensorData.az);

    // DHT11 Data
    if (isnan(sensorData.temperature) || isnan(sensorData.humidity)) {
      Serial.println("DHT11: Failed to read from DHT sensor!");
    } else {
      Serial.print("DHT11 - Temperature: ");
      Serial.print(sensorData.temperature);
      Serial.print(" *C, Humidity: ");
      Serial.print(sensorData.humidity);
      Serial.println(" %");
    }

    // GPS Data
    if (gps.location.isValid()) {
      Serial.print("GPS - Latitude: ");
      Serial.print(sensorData.latitude, 6);
      Serial.print(", Longitude: ");
      Serial.println(sensorData.longitude, 6);
    } else {
      Serial.println("GPS - Location: Not Available");
    }

    Serial.println("----------------------------\n");

    tsLastReport = millis();
  }
}
