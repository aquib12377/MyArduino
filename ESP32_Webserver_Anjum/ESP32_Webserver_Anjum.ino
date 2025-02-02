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

// Ultrasonic sensor objects
NewPing sonarObstacle(ULTRASONIC1_TRIG_PIN, ULTRASONIC1_ECHO_PIN, MAX_DISTANCE);
NewPing sonarDepth(ULTRASONIC2_TRIG_PIN, ULTRASONIC2_ECHO_PIN, MAX_DISTANCE);
int a,b,c,d;
// DS18B20 setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// TDS Sensor variables and constants
#define VREF 3.3
#define SCOUNT 30

int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25; // Default temperature

// Relay control variables with hysteresis
bool relayState = HIGH;
const int obstacleThreshold = 20;  // Distance to trigger relay in cm
const int hysteresisMargin = 5;    // Hysteresis margin in cm

// ------------------------------------------------------
// Setup
// ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  
  // Initialize DS18B20
  sensors.begin();

  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Set relay to inactive (ACTIVE LOW relay)

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

// ------------------------------------------------------
// Main Loop
// ------------------------------------------------------
void loop() {
  server.handleClient();
  readTDS();        // Continuously update TDS values
  handleRelay();    // Control relay based on obstacle distance
}

// ------------------------------------------------------
// TDS Sensor
// ------------------------------------------------------
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
  if ((iFilterLen & 1) > 0)
    return bTab[(iFilterLen - 1) / 2];
  else
    return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

void readTDS() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TDS_SENSOR_PIN);
    analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;
  }
  
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    }

    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4096.0;
    // Temperature compensation
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage = averageVoltage / compensationCoefficient;
    // Convert voltage to TDS
    tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage -
                255.86 * compensationVoltage * compensationVoltage +
                857.39 * compensationVoltage) * 0.5;

    Serial.print("TDS Value: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");
  }
}

// ------------------------------------------------------
// Other Sensor Reads
// ------------------------------------------------------
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
  float ph = 3.3 * voltage;  // Adjust or calibrate as needed
  Serial.print("pH Value: ");
  Serial.println(ph);
  return ph;
}

int readTurbidity() {
  int val = analogRead(TURBIDITY_SENSOR_PIN);
  // Simple mapping example:
  int turbidity = map(val, 0, 2800, 5, 1); 
  Serial.print("Turbidity Value: ");
  Serial.println(turbidity);
  return turbidity;
}

int readOxygen() {
  int ADC_Raw = analogRead(OXYGEN_SENSOR_PIN);
  int oxygenLevel = (ADC_Raw * VREF) / 4096.0; // Adjust conversion if needed
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

// ------------------------------------------------------
// Relay Control with Hysteresis
// ------------------------------------------------------
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

// ------------------------------------------------------
// Web Pages
// ------------------------------------------------------
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>ESP32 Sensor Data</title>";
  // ------------------ Basic Styling ------------------
  html += "<style>";
  html += "body { font-family: Arial; background: #f4f4f9; margin: 0; padding: 0;} ";
  html += ".content { display: flex; flex-direction: column; align-items: center; } ";
  html += ".container { text-align: center; padding: 20px; width: 90%; max-width: 800px; background: #fff; box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1); border-radius: 8px; margin-top: 20px; }";
  html += ".sensor-data { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }";
  html += ".sensor-card { background: #007bff; color: #fff; padding: 15px; border-radius: 6px; }";
  html += ".sensor-card h2 { font-size: 1.2em; margin: 0; }";
  html += ".sensor-card p { font-size: 1.5em; margin: 5px 0 0; font-weight: bold; }";
  html += ".charts { width: 100%; max-width: 800px; margin-top: 20px; }";
  html += ".chart-container { margin-bottom: 20px; }";
  html += "</style>";

  // ------------------ Include Chart.js ------------------
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";

  // ------------------ JavaScript for Fetch & Charts ------------------
  html += "<script>";
  // We keep arrays of points for each graph: x=temperature, y=sensorValue
  html += "var dataPointsTDS = []; "
          "var dataPointsPH = []; "
          "var dataPointsOxygen = []; "
          "var dataPointsTurbidity = []; "
          // We'll store references to each chart object
          "var chartTDS, chartPH, chartOxygen, chartTurbidity;";

  // Create the charts on page load
  html += "function onPageLoad() {"
          "  createCharts();"
          "  fetchData();"
          "  setInterval(fetchData, 2000);"
          "}";

  // Chart creation
  html += "function createCharts() {"
          "  var ctxTDS = document.getElementById('chartTDS').getContext('2d');"
          "  chartTDS = new Chart(ctxTDS, {"
          "    type: 'line',"
          "    data: {"
          "      datasets: [{"
          "        label: 'TDS vs Temperature',"
          "        data: dataPointsTDS," // array of {x, y}
          "        borderColor: 'red',"
          "        fill: false"
          "      }]"
          "    },"
          "    options: {"
          "      scales: {"
          "        x: {"
          "          type: 'linear',"
          "          title: { display: true, text: 'Temperature (°C)' }"
          "        },"
          "        y: {"
          "          title: { display: true, text: 'TDS (ppm)' }"
          "        }"
          "      }"
          "    }"
          "  });"

          "  var ctxPH = document.getElementById('chartPH').getContext('2d');"
          "  chartPH = new Chart(ctxPH, {"
          "    type: 'line',"
          "    data: {"
          "      datasets: [{"
          "        label: 'pH vs Temperature',"
          "        data: dataPointsPH,"
          "        borderColor: 'blue',"
          "        fill: false"
          "      }]"
          "    },"
          "    options: {"
          "      scales: {"
          "        x: {"
          "          type: 'linear',"
          "          title: { display: true, text: 'Temperature (°C)' }"
          "        },"
          "        y: {"
          "          title: { display: true, text: 'pH' }"
          "        }"
          "      }"
          "    }"
          "  });"

          "  var ctxOxygen = document.getElementById('chartOxygen').getContext('2d');"
          "  chartOxygen = new Chart(ctxOxygen, {"
          "    type: 'line',"
          "    data: {"
          "      datasets: [{"
          "        label: 'Oxygen vs Temperature',"
          "        data: dataPointsOxygen,"
          "        borderColor: 'green',"
          "        fill: false"
          "      }]"
          "    },"
          "    options: {"
          "      scales: {"
          "        x: {"
          "          type: 'linear',"
          "          title: { display: true, text: 'Temperature (°C)' }"
          "        },"
          "        y: {"
          "          title: { display: true, text: 'Oxygen (ppm)' }"
          "        }"
          "      }"
          "    }"
          "  });"

          "  var ctxTurbidity = document.getElementById('chartTurbidity').getContext('2d');"
          "  chartTurbidity = new Chart(ctxTurbidity, {"
          "    type: 'line',"
          "    data: {"
          "      datasets: [{"
          "        label: 'Turbidity vs Temperature',"
          "        data: dataPointsTurbidity,"
          "        borderColor: 'orange',"
          "        fill: false"
          "      }]"
          "    },"
          "    options: {"
          "      scales: {"
          "        x: {"
          "          type: 'linear',"
          "          title: { display: true, text: 'Temperature (°C)' }"
          "        },"
          "        y: {"
          "          title: { display: true, text: 'Turbidity' }"
          "        }"
          "      }"
          "    }"
          "  });"
          "}";

  // Fetch and update data
  html += "function fetchData() {"
          "  fetch('/data')"
          "    .then(response => response.json())"
          "    .then(data => {"
          "      document.getElementById('temperature').innerText = data.temperature.toFixed(2) + ' °C';"
          "      document.getElementById('ph').innerText = data.ph.toFixed(2);"
          "      document.getElementById('oxygen').innerText = data.oxygen + ' ppm';"
          "      document.getElementById('tds').innerText = data.tds.toFixed(0) + ' ppm';"
          "      document.getElementById('turbidity').innerText = data.turbidity;"
          "      document.getElementById('obstacle').innerText = data.obstacle + ' cm';"
          "      document.getElementById('depth').innerText = data.depth + ' cm';"

          "      dataPointsTDS.push({ x: data.temperature, y: data.tds });"
          "      dataPointsPH.push({ x: data.temperature, y: data.ph });"
          "      dataPointsOxygen.push({ x: data.temperature, y: data.oxygen });"
          "      dataPointsTurbidity.push({ x: data.temperature, y: data.turbidity });"

          "      if (dataPointsTDS.length > 50) dataPointsTDS.shift();"
          "      if (dataPointsPH.length > 50) dataPointsPH.shift();"
          "      if (dataPointsOxygen.length > 50) dataPointsOxygen.shift();"
          "      if (dataPointsTurbidity.length > 50) dataPointsTurbidity.shift();"

          "      chartTDS.update();"
          "      chartPH.update();"
          "      chartOxygen.update();"
          "      chartTurbidity.update();"
          "    });"
          "}";
  html += "</script>";
  
  // ------------------ HTML Body ------------------
  html += "</head><body onload='onPageLoad()'>";
  html += "<div class='content'>";

  // Existing container with sensor cards
  html += "<div class='container'>";
  html += "<h1>ESP32 Sensor Data</h1>";
  html += "<div class='sensor-data'>";
  
  html += "<div class='sensor-card'><h2>Temperature</h2><p id='temperature'>0 °C</p></div>";
  html += "<div class='sensor-card'><h2>pH Level</h2><p id='ph'>0</p></div>";
  html += "<div class='sensor-card'><h2>Oxygen Level</h2><p id='oxygen'>0 ppm</p></div>";
  html += "<div class='sensor-card'><h2>TDS Level</h2><p id='tds'>0 ppm</p></div>";
  html += "<div class='sensor-card'><h2>Turbidity</h2><p id='turbidity'>0</p></div>";
  html += "<div class='sensor-card'><h2>Obstacle Distance</h2><p id='obstacle'>0 cm</p></div>";
  html += "<div class='sensor-card'><h2>Depth Distance</h2><p id='depth'>0 cm</p></div>";
  
  html += "</div>"; // close sensor-data
  html += "</div>"; // close container

  // New container for charts
  html += "<div class='container charts'>";
  html += "<h2>Real-Time Charts (Temperature on X-axis)</h2>";
  // Each chart in its own canvas
  html += "<div class='chart-container'><canvas id='chartTDS' width='400' height='200'></canvas></div>";
  html += "<div class='chart-container'><canvas id='chartPH' width='400' height='200'></canvas></div>";
  html += "<div class='chart-container'><canvas id='chartOxygen' width='400' height='200'></canvas></div>";
  html += "<div class='chart-container'><canvas id='chartTurbidity' width='400' height='200'></canvas></div>";
  html += "</div>"; // close charts container

  html += "</div>"; // close .content
  html += "</body></html>";
  Serial.println(html);
  //delay(10000);
  server.send(200, "text/html", html);
}

// JSON Data endpoint
void handleData() {
  // Read all sensors once here, then return as JSON
  temperature = readTemperature();       // important to keep `temperature` updated for TDS compensation
  float phValue = readPH();
  int oxygenLevel = readOxygen();
  int turbidityLevel = readTurbidity();
  int obstacleDistance = readObstacleDistance();
  int depthDistance = readDepthDistance();

  // Construct JSON
  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"ph\":" + String(phValue) + ",";
  json += "\"oxygen\":" + String(oxygenLevel) + ",";
  json += "\"tds\":" + String(tdsValue) + ",";
  json += "\"turbidity\":" + String(turbidityLevel) + ",";
  json += "\"obstacle\":" + String(obstacleDistance) + ",";
  json += "\"depth\":" + String(depthDistance);
  json += "}";

  
  // String json = "{";
  // json += "\"temperature\":" + String(++a) + ",";
  // json += "\"ph\":" + String(++b) + ",";
  // json += "\"oxygen\":" + String(++c) + ",";
  // json += "\"tds\":" + String(++d) + ",";
  // json += "\"turbidity\":" + String((a+b)) + ",";
  // json += "\"obstacle\":" + String(obstacleDistance+random(0,5)) + ",";
  // json += "\"depth\":" + String(depthDistance+random(0,5));
  // json += "}";
  Serial.println(json);
  server.send(200, "application/json", json);
}
