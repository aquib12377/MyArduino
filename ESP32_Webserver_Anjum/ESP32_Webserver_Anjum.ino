#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <NewPing.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Web server on port 80
WebServer server(80);

// Sensor pins
#define PH_SENSOR_PIN 32           // pH sensor pin
#define OXYGEN_SENSOR_PIN 35       // Oxygen sensor pin (DO sensor)
#define TDS_SENSOR_PIN 34          // TDS sensor pin
#define TURBIDITY_SENSOR_PIN 36    // Turbidity sensor pin
#define ULTRASONIC1_TRIG_PIN 27    // Obstacle detection ultrasonic trigger pin
#define ULTRASONIC1_ECHO_PIN 26    // Obstacle detection ultrasonic echo pin
#define ULTRASONIC2_TRIG_PIN 12    // Depth detection ultrasonic trigger pin
#define ULTRASONIC2_ECHO_PIN 14    // Depth detection ultrasonic echo pin
#define MAX_DISTANCE 200           // Max distance for ultrasonic sensor in cm
#define RELAY_PIN 33               // Relay pin (ACTIVE LOW)
#define ONE_WIRE_BUS 4             // DS18B20 data pin

// Ultrasonic sensor objects using NewPing
NewPing sonarObstacle(ULTRASONIC1_TRIG_PIN, ULTRASONIC1_ECHO_PIN, MAX_DISTANCE);
NewPing sonarDepth(ULTRASONIC2_TRIG_PIN, ULTRASONIC2_ECHO_PIN, MAX_DISTANCE);

// DS18B20 setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// TDS Sensor Variables and Constants
#define VREF 3.3              // Analog reference voltage (Volt) of the ADC
#define SCOUNT  30            // Sum of sample points for TDS

int analogBuffer[SCOUNT];     // Store analog values from the ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;       // Default temperature value

// Relay control variables with hysteresis
bool relayState = HIGH;
const int obstacleThreshold = 20;  // Distance to trigger relay in cm
const int hysteresisMargin = 5;    // Hysteresis margin in cm

void setup() {
  Serial.begin(115200);

  // Initialize DS18B20
  sensors.begin();

  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Set relay to inactive (HIGH for ACTIVE LOW relay)

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  readTDS();  // Continuously update TDS values in the background
  handleRelay();  // Control relay based on obstacle distance with hysteresis
}

// TDS sensor median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  
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
  return (iFilterLen & 1) > 0 ? bTab[(iFilterLen - 1) / 2] : (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

// Function to continuously read TDS sensor data
void readTDS() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {   // every 40 ms
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TDS_SENSOR_PIN); // read and store ADC value
    analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;  // Increment and wrap index
  }
  
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {         // every 800 ms
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    }

    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4096.0;

    // Temperature compensation
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage = averageVoltage / compensationCoefficient;

    // Convert voltage to TDS value
    tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage -
               255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

    Serial.print("TDS Value: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");
  }
}

// Function to read temperature from DS18B20
float readTemperature() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" °C");
  return temp;
}

float readPH() {
  int value = analogRead(PH_SENSOR_PIN);
  float voltage = value * (3.3 / 4095.0);
  float ph = 3.3 * voltage;
  Serial.print("pH Value: ");
  Serial.println(ph);
  return ph;
}

int readTurbidity() {
  int val = analogRead(TURBIDITY_SENSOR_PIN);
  int turbidity = map(val, 0, 2800, 5, 1);
  Serial.print("Turbidity Value: ");
  Serial.println(turbidity);
  return turbidity;
}

int readOxygen() {
  int ADC_Raw = analogRead(OXYGEN_SENSOR_PIN);
  int oxygenLevel = (ADC_Raw * VREF) / 4096.0;
  Serial.print("Oxygen Level: ");
  Serial.println(oxygenLevel);
  return oxygenLevel;
}

int readObstacleDistance() {
  int distance = sonarObstacle.ping_cm();
  Serial.print("Obstacle Distance: ");
  Serial.println(distance);
  return distance;
}

int readDepthDistance() {
  int distance = sonarDepth.ping_cm();
  Serial.print("Depth Distance: ");
  Serial.println(distance);
  return distance;
}

// Control relay based on obstacle distance with hysteresis
void handleRelay() {
  int obstacleDistance = readObstacleDistance();
  
  if (obstacleDistance > 0 && obstacleDistance < obstacleThreshold && relayState == HIGH) {
    digitalWrite(RELAY_PIN, LOW);  // Trigger relay (ACTIVE LOW)
    relayState = LOW;
    Serial.println("Relay triggered: Object detected within 20 cm");
  } 
  else if (obstacleDistance > obstacleThreshold + hysteresisMargin && relayState == LOW) {
    digitalWrite(RELAY_PIN, HIGH); // Deactivate relay
    relayState = HIGH;
    Serial.println("Relay deactivated: Object out of range");
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>ESP32 Sensor Data</title>";
  html += "<style>body { font-family: Arial; background: #f4f4f9; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }";
  html += ".container { text-align: center; padding: 20px; width: 90%; max-width: 500px; background: #fff; box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1); border-radius: 8px; }";
  html += ".sensor-data { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }";
  html += ".sensor-card { background: #007bff; color: #fff; padding: 15px; border-radius: 6px; }";
  html += ".sensor-card h2 { font-size: 1.2em; margin: 0; } .sensor-card p { font-size: 1.5em; margin: 5px 0 0; font-weight: bold; }</style>";
  html += "<script>function fetchData() { fetch('/data').then(response => response.json()).then(data => { document.getElementById('temperature').innerText = data.temperature + '°C'; document.getElementById('ph').innerText = data.ph; document.getElementById('oxygen').innerText = data.oxygen + ' ppm';";
  html += "document.getElementById('tds').innerText = data.tds + ' ppm'; document.getElementById('turbidity').innerText = data.turbidity; document.getElementById('obstacle').innerText = data.obstacle + ' cm';";
  html += "document.getElementById('depth').innerText = data.depth + ' cm'; }); } setInterval(fetchData, 2000);</script></head><body onload='fetchData()'><div class='container'>";
  html += "<h1>ESP32 Sensor Data</h1><div class='sensor-data'><div class='sensor-card'><h2>Temperature</h2><p id='temperature'>0 °C</p></div>";
  html += "<div class='sensor-card'><h2>pH Level</h2><p id='ph'>0</p></div><div class='sensor-card'><h2>Oxygen Level</h2><p id='oxygen'>0 ppm</p></div>";
  html += "<div class='sensor-card'><h2>TDS Level</h2><p id='tds'>0 ppm</p></div><div class='sensor-card'><h2>Turbidity</h2><p id='turbidity'>0</p></div>";
  html += "<div class='sensor-card'><h2>Obstacle Distance</h2><p id='obstacle'>0 cm</p></div><div class='sensor-card'><h2>Depth Distance</h2><p id='depth'>0 cm</p></div>";
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

void handleData() {
  temperature = readTemperature();
  float phValue = readPH();
  int oxygenLevel = readOxygen();
  int turbidityLevel = readTurbidity();
  int obstacleDistance = readObstacleDistance();
  int depthDistance = readDepthDistance();

  String json = "{\"temperature\":" + String(temperature) + ",\"ph\":" + String(phValue) + ",\"oxygen\":" + String(oxygenLevel) + ",\"tds\":" + String(tdsValue);
  json += ",\"turbidity\":" + String(turbidityLevel) + ",\"obstacle\":" + String(obstacleDistance) + ",\"depth\":" + String(depthDistance) + "}";
  server.send(200, "application/json", json);
}
