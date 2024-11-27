// Include Required Libraries
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <NewPing.h>
#include <ZMPT101B.h>
#define SENSITIVITY 648.5f
#define VOLTAGE_FREQUENCY 50.0

// Wi-Fi credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Web server on port 80
WebServer server(80);

// Sensor pins
#define ACS712_PIN_PRIMARY 34    // Analog pin for primary current sensor
#define ACS712_PIN_SECONDARY 35  // Analog pin for secondary current sensor
#define ZMPT_PIN 32              // Analog pin for voltage sensor
#define DHT_PIN 27
#define DHT_TYPE DHT11
#define TRIG_PIN 25
#define ECHO_PIN 26
#define RELAY_PIN 12
#define SD_CS_PIN 5  // Chip select pin for SD card module
NewPing sonar(TRIG_PIN, ECHO_PIN, 200); // NewPing setup of pins and maximum distance.

// Create DHT sensor object
DHT dht(DHT_PIN, DHT_TYPE);
ZMPT101B voltageSensor1(ZMPT_PIN, VOLTAGE_FREQUENCY);

// Variables to store sensor data
float currentPrimary = 0.0;
float currentSecondary = 0.0;
float voltage = 0.0;
float temperature = 0.0;
float oilDepth = 0.0;

// Data storage
String dataString = "";

void setup() {
  Serial.begin(115200);

  // Initialize sensors
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(RELAY_PIN, LOW);

  voltageSensor1.setSensitivity(SENSITIVITY);

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start the server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/table", handleTable);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  readSensors();
  saveDataToSD();
  delay(1000);  // Adjust the delay as needed
}

void readSensors() {
  // Read currents
  currentPrimary = readACS712(ACS712_PIN_PRIMARY);
  currentSecondary = readACS712(ACS712_PIN_SECONDARY);

  // Read voltage
  voltage = readZMPT101B(ZMPT_PIN);

  // Read temperature
  temperature = dht.readTemperature();

  // Read oil depth
  oilDepth = readUltrasonic(TRIG_PIN, ECHO_PIN);

  if(abs(currentPrimary - currentSecondary) > 1 || oilDepth > 30 || temperature > 40)
  {
      digitalWrite(RELAY_PIN, HIGH);
  }
  else{
      digitalWrite(RELAY_PIN, LOW);
  }

  // Create a CSV-formatted string
  dataString = String(currentPrimary) + "," + String(currentSecondary) + "," + String(voltage) + "," + String(temperature) + "," + String(oilDepth);
}

float readACS712(int pin) {
  int adc = analogRead(pin);
  float voltage = adc * (3.3 / 4095.0);
  float current = (voltage - 1.65) / 0.100;  // Adjust the formula based on your sensor
  current = constrain(current, 0, 20.0);  // Assuming maximum current is 20A
  return current;
}

float readZMPT101B(int pin) {
  return voltageSensor1.getRmsVoltage();;
}

float readUltrasonic(int trigPin, int echoPin) {
  float cm = sonar.ping_cm();
  Serial.println("Oil Depth: "+String(cm));
  return cm;
}

void saveDataToSD() {
  File dataFile = SD.open("/data.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    Serial.println("Data saved: " + dataString);
  } else {
    Serial.println("Failed to open file for writing");
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Transformer Monitoring</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body {font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f0f0f0;}";
  html += "header {background-color: #4CAF50; color: white; padding: 20px; text-align: center;}";
  html += "section {padding: 20px;}";
  html += ".chart-container {position: relative; margin: auto; height: 40vh; width: 80vw;}";
  html += ".oil-depth {font-size: 24px; font-weight: bold; text-align: center; margin: 20px 0;}";
  html += ".label {display: inline-block; width: 150px;}";
  html += "</style>";
  html += "</head><body>";
  html += "<header><h1>Transformer Monitoring System</h1></header>";
  html += "<section>";

  // Oil Depth Display
  html += "<div class='oil-depth'>Oil Depth: <span id='oilDepth'>--</span> cm</div>";

  // Current Chart
  html += "<div class='chart-container'><canvas id='currentChart'></canvas></div>";
  // Voltage Chart
  html += "<div class='chart-container'><canvas id='voltageChart'></canvas></div>";
  // Temperature Chart
  html += "<div class='chart-container'><canvas id='temperatureChart'></canvas></div>";

  html += "</section>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "<script>";
  html += "var timeLabels = [];";
  html += "var oilDepthLabel = document.getElementById('oilDepth');";

  // Current Chart Setup
  html += "var currentCtx = document.getElementById('currentChart').getContext('2d');";
  html += "var currentChart = new Chart(currentCtx, {";
  html += "type: 'line',";
  html += "data: {";
  html += "labels: timeLabels,";
  html += "datasets: [";
  html += "{label: 'Primary Current (A)', borderColor: 'red', data: [], fill: false},";
  html += "{label: 'Secondary Current (A)', borderColor: 'blue', data: [], fill: false}";
  html += "]";
  html += "},";
  html += "options: {";
  html += "responsive: true,";
  html += "scales: {";
  html += "x: {display: true, title: {display: true, text: 'Time'}},";
  html += "y: {display: true, title: {display: true, text: 'Current (A)'}}";
  html += "}";
  html += "}";
  html += "});";

  // Voltage Chart Setup
  html += "var voltageCtx = document.getElementById('voltageChart').getContext('2d');";
  html += "var voltageChart = new Chart(voltageCtx, {";
  html += "type: 'line',";
  html += "data: {";
  html += "labels: timeLabels,";
  html += "datasets: [";
  html += "{label: 'Voltage (V)', borderColor: 'green', data: [], fill: false}";
  html += "]";
  html += "},";
  html += "options: {";
  html += "responsive: true,";
  html += "scales: {";
  html += "x: {display: true, title: {display: true, text: 'Time'}},";
  html += "y: {display: true, title: {display: true, text: 'Voltage (V)'}}";
  html += "}";
  html += "}";
  html += "});";

  // Temperature Chart Setup
  html += "var temperatureCtx = document.getElementById('temperatureChart').getContext('2d');";
  html += "var temperatureChart = new Chart(temperatureCtx, {";
  html += "type: 'line',";
  html += "data: {";
  html += "labels: timeLabels,";
  html += "datasets: [";
  html += "{label: 'Temperature (°C)', borderColor: 'orange', data: [], fill: false}";
  html += "]";
  html += "},";
  html += "options: {";
  html += "responsive: true,";
  html += "scales: {";
  html += "x: {display: true, title: {display: true, text: 'Time'}},";
  html += "y: {display: true, title: {display: true, text: 'Temperature (°C)'}}";
  html += "}";
  html += "}";
  html += "});";

  // Update Data Function
  html += "function updateData() {";
  html += "fetch('/data').then(response => response.json()).then(data => {";
  html += "var time = new Date().toLocaleTimeString();";
  html += "timeLabels.push(time);";
  html += "if(timeLabels.length > 20){timeLabels.shift();}";

  // Update Oil Depth
  html += "oilDepthLabel.textContent = data.oilDepth.toFixed(2);";

  // Update Current Chart
  html += "currentChart.data.datasets[0].data.push(data.currentPrimary);";
  html += "if(currentChart.data.datasets[0].data.length > 20){currentChart.data.datasets[0].data.shift();}";
  html += "currentChart.data.datasets[1].data.push(data.currentSecondary);";
  html += "if(currentChart.data.datasets[1].data.length > 20){currentChart.data.datasets[1].data.shift();}";
  html += "currentChart.update();";

  // Update Voltage Chart
  html += "voltageChart.data.datasets[0].data.push(data.voltage);";
  html += "if(voltageChart.data.datasets[0].data.length > 20){voltageChart.data.datasets[0].data.shift();}";
  html += "voltageChart.update();";

  // Update Temperature Chart
  html += "temperatureChart.data.datasets[0].data.push(data.temperature);";
  html += "if(temperatureChart.data.datasets[0].data.length > 20){temperatureChart.data.datasets[0].data.shift();}";
  html += "temperatureChart.update();";
  html += "});";
  html += "}";
  html += "setInterval(updateData, 1000);";
  html += "</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}



void handleData() {
  String json = "{";
  json += "\"currentPrimary\":" + String(currentPrimary) + ",";
  json += "\"currentSecondary\":" + String(currentSecondary) + ",";
  json += "\"voltage\":" + String(voltage) + ",";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"oilDepth\":" + String(oilDepth);
  json += "}";
  server.send(200, "application/json", json);
}

void handleTable() {
  File dataFile = SD.open("/data.csv");
  if (!dataFile) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }

  // Read the last 100 lines
  const int maxLines = 100;
  String lines[maxLines];
  int lineCount = 0;

  while (dataFile.available()) {
    String line = dataFile.readStringUntil('\n');
    lines[lineCount % maxLines] = line;
    lineCount++;
  }
  dataFile.close();

  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Data Table</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body {font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f0f0f0;}";
  html += "header {background-color: #4CAF50; color: white; padding: 20px; text-align: center;}";
  html += "section {padding: 20px;}";
  html += "table {width: 100%; border-collapse: collapse; margin-bottom: 20px;}";
  html += "th, td {padding: 12px; border-bottom: 1px solid #ddd; text-align: center;}";
  html += "th {background-color: #4CAF50; color: white;}";
  html += "tr:hover {background-color: #f1f1f1;}";
  html += "a {display: inline-block; padding: 10px 20px; margin: 10px 0; background-color: #4CAF50; color: white; text-decoration: none; border-radius: 4px;}";
  html += "a:hover {background-color: #45a049;}";
  html += "</style>";
  html += "</head><body>";
  html += "<header><h1>Last 100 Data Entries</h1></header>";
  html += "<section>";
  html += "<a href='/'>Back to Dashboard</a>";
  html += "<table>";
  html += "<tr><th>Primary Current (A)</th><th>Secondary Current (A)</th><th>Voltage (V)</th><th>Temperature (°C)</th><th>Oil Depth (cm)</th></tr>";

  int start = lineCount >= maxLines ? (lineCount % maxLines) : 0;
  int totalLines = lineCount >= maxLines ? maxLines : lineCount;

  for (int i = 0; i < totalLines; i++) {
    int index = (start + i) % maxLines;
    html += "<tr>";
    String line = lines[index];
    int commaIndex = 0;
    for (int j = 0; j < 5; j++) {
      int nextComma = line.indexOf(',', commaIndex);
      if (nextComma == -1) nextComma = line.length();
      String data = line.substring(commaIndex, nextComma);
      html += "<td>" + data + "</td>";
      commaIndex = nextComma + 1;
    }
    html += "</tr>";
  }

  html += "</table>";
  html += "</section>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

