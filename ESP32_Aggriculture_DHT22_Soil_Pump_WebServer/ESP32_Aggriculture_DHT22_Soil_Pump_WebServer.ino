/****************************************************
   ESP32 Smart Agriculture Example (Mobile Friendly)
   Components:
   - DHT22 on pin 15
   - Soil Moisture on pin 34
   - Relay (Pump) on pin 13, ACTIVE LOW
   - Basic Web Server for monitoring
   Turn on pump if soil moisture < 30%
****************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Arduino.h>
#include "DHT.h"

// -------------------- USER SETTINGS --------------------
const char* ssid     = "MyProject";
const char* password = "12345678";
// --------------------------------------------------------

// DHT22 Settings
#define DHTPIN   15
#define DHTTYPE  DHT22
DHT dht(DHTPIN, DHTTYPE);

// Soil Moisture
#define SOIL_PIN 34

// Relay
#define PUMP_PIN 13 // Active LOW

// Global variables for sensor data
float temperature = 0;
float humidity = 0;
float soilMoisturePercent = 0;
bool pumpOn = false;  // tracks whether the pump is ON (LOW) or OFF (HIGH)

// Create a WebServer object on port 80
WebServer server(80);

// Forward declaration of sensor reading function
void readSensorsAndUpdatePump();

// Handle the root URL "/"
void handleRoot() {
  // Simple HTML page with inline CSS and JavaScript to fetch data.
  // Added mobile-friendly viewport meta tag and responsive container width.
  String html = R"=====(

<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Smart Agriculture Monitor</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0; 
      padding: 0;
      background: #f4f4f4;
    }
    header {
      background: #8BC34A;
      color: #fff;
      padding: 1rem;
      font-size: 1.4rem;
    }
    #container {
      margin: 1rem auto;
      width: 90%;
      max-width: 600px;
      background: #fff;
      padding: 1rem;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    h2 {
      color: #4CAF50;
      margin-bottom: 1rem;
      font-size: 1.2rem;
    }
    .data-box {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin: 0.5rem 0;
      font-size: 1rem;
    }
    .label {
      font-weight: bold;
      margin-right: 1rem;
    }
    .value {
      font-weight: normal;
    }
    .pump-on {
      color: #e91e63;
      font-weight: bold;
    }
    .pump-off {
      color: #666;
      font-weight: bold;
    }
    footer {
      margin-top: 1rem;
      font-size: 0.8rem;
      color: #999;
    }
    @media (max-width: 400px) {
      .data-box {
        flex-direction: column;
        align-items: flex-start;
      }
      .label {
        margin-bottom: 0.3rem;
      }
    }
  </style>
</head>
<body>
  <header>Smart Agriculture Monitor</header>

  <div id="container">
    <h2>Real-Time Sensor Data</h2>
    
    <div class="data-box">
      <div class="label">Temperature:</div>
      <div id="tempVal" class="value">-- °C</div>
    </div>
    
    <div class="data-box">
      <div class="label">Humidity:</div>
      <div id="humVal" class="value">-- %</div>
    </div>
    
    <div class="data-box">
      <div class="label">Soil Moisture:</div>
      <div id="soilVal" class="value">-- %</div>
    </div>
    
    <div class="data-box">
      <div class="label">Pump Status:</div>
      <div id="pumpVal" class="value pump-off">OFF</div>
    </div>
  </div>

  <footer>ESP32 Smart Agriculture &copy; 2025</footer>
  
  <script>
    // Periodically fetch sensor data from /data endpoint
    setInterval(() => {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('tempVal').innerHTML = data.temperature.toFixed(1) + " °C";
          document.getElementById('humVal').innerHTML  = data.humidity.toFixed(1) + " %";
          document.getElementById('soilVal').innerHTML = data.soilMoisture.toFixed(1) + " %";

          let pump = document.getElementById('pumpVal');
          if(data.pumpOn){
            pump.innerHTML = "ON";
            pump.classList.remove("pump-off");
            pump.classList.add("pump-on");
          } else {
            pump.innerHTML = "OFF";
            pump.classList.remove("pump-on");
            pump.classList.add("pump-off");
          }
        })
        .catch(err => console.log('Error fetching data: ', err));
    }, 2000);
  </script>
</body>
</html>

)=====";

  server.send(200, "text/html", html);
}

// Handle the JSON data endpoint "/data"
void handleData() {
  // Create a small JSON with current sensor readings
  String jsonResponse = "{";
  jsonResponse += "\"temperature\":" + String(temperature, 1) + ",";
  jsonResponse += "\"humidity\":" + String(humidity, 1) + ",";
  jsonResponse += "\"soilMoisture\":" + String(soilMoisturePercent, 1) + ",";
  jsonResponse += "\"pumpOn\":" + String(pumpOn ? "true" : "false");
  jsonResponse += "}";

  server.send(200, "application/json", jsonResponse);
}

// Handle when the client requests an unknown page
void handleNotFound() {
  String message = "File Not Found\n\n";
  server.send(404, "text/plain", message);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Set up DHT
  dht.begin();

  // Relay pin
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH); // Relay OFF initially (because it's active LOW)

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println();
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Start the web server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started.");
}

unsigned long lastSensorReadTime = 0;
const unsigned long sensorInterval = 2000; // read sensors every 2 seconds

void loop() {
  // Handle any incoming client requests
  server.handleClient();

  // Periodically read sensors and update pump state
  unsigned long currentTime = millis();
  if (currentTime - lastSensorReadTime >= sensorInterval) {
    lastSensorReadTime = currentTime;
    readSensorsAndUpdatePump();
  }
}

// Reads DHT22 and soil moisture, updates global variables and pump state
void readSensorsAndUpdatePump() {
  // Read temperature/humidity
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }

  // Read soil moisture from ADC (0..4095)
  int soilADC = analogRead(SOIL_PIN);

  // Simple linear map:
  //   4095 -> 0% (completely dry)
  //   0    -> 100% (fully wet)
  float soilPercent = map(soilADC, 4095, 0, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
  soilMoisturePercent = soilPercent;

  // Control pump: if below 30% moisture, turn ON (LOW)
  if (soilMoisturePercent < 30.0) {
    digitalWrite(PUMP_PIN, LOW);
    pumpOn = true;
  } else {
    digitalWrite(PUMP_PIN, HIGH);
    pumpOn = false;
  }

  // Debug print
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" C, Hum: ");
  Serial.print(humidity);
  Serial.print(" %, Soil: ");
  Serial.print(soilMoisturePercent);
  Serial.print(" %, Pump: ");
  Serial.println(pumpOn ? "ON" : "OFF");
}
