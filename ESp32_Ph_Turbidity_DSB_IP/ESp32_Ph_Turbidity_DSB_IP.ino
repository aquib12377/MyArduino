#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Replace with your network credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Define sensor pins
#define ONE_WIRE_BUS 4  // DS18B20 Data pin
#define TURBIDITY_SENSOR_PIN 34  // Analog pin for turbidity sensor
#define PH_SENSOR_PIN 35  // Analog pin for pH sensor

// OneWire and DS18B20 setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Create AsyncWebServer on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float temperature = 0.0;
float turbidity = 0.0;
float pH = 0.0;
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script>
        var socket = new WebSocket("ws://" + window.location.host + "/ws");
        socket.onmessage = function(event) {
            var data = JSON.parse(event.data);
            document.getElementById("temp").innerText = data.temperature + " °C";
            document.getElementById("turbidity").innerText = data.turbidity + " NTU";
            document.getElementById("ph").innerText = data.ph;
        };
    </script>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; }
        .card { padding: 20px; margin: 10px; border-radius: 10px; box-shadow: 2px 2px 10px gray; }
    </style>
</head>
<body>
    <h2>ESP32 Sensor Data</h2>
    <div class="card">
        <h3>Temperature</h3>
        <p id="temp">-- °C</p>
    </div>
    <div class="card">
        <h3>Turbidity</h3>
        <p id="turbidity">-- NTU</p>
    </div>
    <div class="card">
        <h3>pH Level</h3>
        <p id="ph">--</p>
    </div>
</body>
</html>
)rawliteral";

void readSensors() {
    sensors.requestTemperatures();
    temperature = sensors.getTempCByIndex(0);
    turbidity = analogRead(TURBIDITY_SENSOR_PIN) * (5.0 / 4095.0);
    pH = analogRead(PH_SENSOR_PIN) * (14.0 / 4095.0);
    
    String json = "{";
    json += "\"temperature\":" + String(temperature, 2) + ",";
    json += "\"turbidity\":" + String(turbidity, 2) + ",";
    json += "\"ph\":" + String(pH, 2) + "}";
    ws.textAll(json);
}

void notifyClients() {
    readSensors();
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        notifyClients();
    }
}

void setup() {
    Serial.begin(115200);
    sensors.begin();
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    server.begin();
}

void loop() {
    notifyClients();
    delay(2000); // Update every 2 seconds
}

