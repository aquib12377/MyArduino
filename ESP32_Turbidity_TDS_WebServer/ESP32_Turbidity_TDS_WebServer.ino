#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Web server on port 80
WebServer server(80);

// Sensor pins
#define TDS_SENSOR_PIN 34          // TDS sensor pin
#define TURBIDITY_SENSOR_PIN 36    // Turbidity sensor pin
#define VREF 3.3                   // Analog reference voltage (Volt) of the ADC
#define SCOUNT  30                 // Number of samples for TDS

// TDS Sensor Variables
int analogBuffer[SCOUNT];         // Store analog values from the ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;

// Turbidity Sensor Variables
int turbidityValue = 0;

// Timing variables
unsigned long previousTdsMillis = 0;
const unsigned long TDS_INTERVAL = 800;      // 800 ms for TDS processing
unsigned long previousTurbidityMillis = 0;
const unsigned long TURBIDITY_INTERVAL = 2000; // 2000 ms for Turbidity processing

void setup() {
  Serial.begin(115200);

  // Initialize sensor pins
  pinMode(TDS_SENSOR_PIN, INPUT);
  pinMode(TURBIDITY_SENSOR_PIN, INPUT);

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
  readTDS();          // Continuously update TDS values
  readTurbidity();    // Continuously update Turbidity values
}

// TDS sensor median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  
  // Simple Bubble Sort
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
  // Return the median
  if (iFilterLen & 1) { // Odd number of elements
    return bTab[(iFilterLen - 1) / 2];
  } else { // Even number of elements
    return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
}

// Function to continuously read TDS sensor data
void readTDS() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousTdsMillis > 40U) {   // every 40 ms
    previousTdsMillis = currentMillis;
    analogBuffer[analogBufferIndex] = analogRead(TDS_SENSOR_PIN); // read and store ADC value
    analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;  // Increment and wrap index
  }
  
  if (currentMillis - previousTdsMillis >= TDS_INTERVAL) { // every 800 ms
    // Copy buffer to temporary array
    for (int i = 0; i < SCOUNT; i++) {
      analogBufferTemp[i] = analogBuffer[i];
    }

    // Calculate median voltage
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4096.0;

    // Convert voltage to TDS value (ppm)
    tdsValue = (133.42 * averageVoltage * averageVoltage * averageVoltage -
               255.86 * averageVoltage * averageVoltage + 857.39 * averageVoltage) * 0.5;

    Serial.print("TDS Value: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");
  }
}

// Function to read Turbidity sensor data
void readTurbidity() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousTurbidityMillis >= TURBIDITY_INTERVAL) { // every 2000 ms
    previousTurbidityMillis = currentMillis;
    int analogValue = analogRead(TURBIDITY_SENSOR_PIN);
    // Map the analog value to turbidity (adjust based on calibration)
    turbidityValue = map(analogValue, 0, 4095, 0, 100); // Example mapping: 0-4095 -> 0-100 NTU
    Serial.print("Turbidity Value: ");
    Serial.print(turbidityValue);
    Serial.println(" NTU");
  }
}

// Handler for the root path
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='X-UA-Compatible' content='IE=edge'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Water Quality Monitor</title>";
  
  // CSS Styling
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(to right, #74ebd5, #acb6e5); margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
  html += ".container { background: rgba(255, 255, 255, 0.9); padding: 30px; border-radius: 15px; box-shadow: 0 8px 16px rgba(0,0,0,0.3); text-align: center; width: 90%; max-width: 500px; }";
  html += "h1 { color: #333; margin-bottom: 20px; }";
  html += ".sensor { margin: 20px 0; }";
  html += ".sensor h2 { color: #555; margin-bottom: 10px; }";
  html += ".value { font-size: 2em; color: #007BFF; }";
  html += "@media (max-width: 600px) { .container { padding: 20px; } .value { font-size: 1.5em; } }";
  html += "</style>";

  // JavaScript for fetching data
  html += "<script>";
  html += "function fetchData() {";
  html += "fetch('/data').then(response => response.json()).then(data => {";
  html += "document.getElementById('tds').innerText = data.tds + ' ppm';";
  html += "document.getElementById('turbidity').innerText = data.turbidity + ' NTU';";
  html += "}).catch(err => console.error(err));";
  html += "}";
  html += "setInterval(fetchData, 2000);"; // Fetch data every 2 seconds
  html += "window.onload = fetchData;";
  html += "</script>";

  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Water Quality Monitor</h1>";
  
  // TDS Display
  html += "<div class='sensor'>";
  html += "<h2>Total Dissolved Solids (TDS)</h2>";
  html += "<p class='value' id='tds'>-- ppm</p>";
  html += "</div>";
  
  // Turbidity Display
  html += "<div class='sensor'>";
  html += "<h2>Turbidity</h2>";
  html += "<p class='value' id='turbidity'>-- NTU</p>";
  html += "</div>";
  
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Handler for the data path
void handleData() {
  // Construct JSON response
  String json = "{";
  json += "\"tds\":" + String(tdsValue, 0) + ",";
  json += "\"turbidity\":" + String(turbidityValue);
  json += "}";
  
  server.send(200, "application/json", json);
}
