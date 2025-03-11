/************************************************************
 * ESP32 Code with Modern Web UI
 * Measures pH, Turbidity, and DS18B20 temperature, then serves
 * a modern-styled web page that auto-refreshes and indicates
 * water quality.
 ************************************************************/

#include <WiFi.h>
#include <WebServer.h>

// Libraries for DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>

// -------- USER CONFIGURATIONS ---------
// Replace with your WiFi credentials
const char* ssid     = "MyProject";
const char* password = "12345678";

// Assign pins
const int pH_PIN         = 34;   // Analog pin for pH sensor
const int TURBIDITY_PIN  = 35;   // Analog pin for turbidity sensor
const int ONE_WIRE_BUS   = 4;    // Digital pin for DS18B20

// Create oneWire instance and DallasTemperature instance
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

// Create a web server on port 80
WebServer server(80);

// Global temperature variable
float temperatureC = 0;

// Function to read temperature from DS18B20
void readDS18B20Temperature() {
  DS18B20.requestTemperatures();
  temperatureC = DS18B20.getTempCByIndex(0);  // Single sensor => index 0
}

// Function to read pH sensor
float readPH() {
  // Read analog voltage from sensor
  int rawValue = analogRead(pH_PIN);
  
  // Convert raw reading to voltage (0..4095 -> 0..3.3V)
  float voltage = rawValue * (3.3 / 4095.0);
  
  /*******************************************************
   * Example conversion for a typical analog pH circuit:
   * pH = 7 + ((1.65 - voltage) / 0.18)
   * (Modify this calibration formula as needed.)
   *******************************************************/
  float pHValue = 7.0 + ((1.65 - voltage) / 0.18);
  return pHValue;
}

// Function to read Turbidity sensor
float readTurbidity() {
  // Read analog voltage
  int rawValue = analogRead(TURBIDITY_PIN);
  
  // Convert raw reading to a rough NTU value.
  // Adjust mapping as per your sensor’s datasheet.
  float turbidityNTU = map(rawValue, 0, 4095, 0, 5); // Rough scale 0..5 NTU
  return turbidityNTU;
}

// Handle HTTP GET for the root page
void handleRoot() {
  // Read sensors
  float phValue = readPH();
  float turbidityValue = readTurbidity();
  readDS18B20Temperature();

  // Decide if water is safe (example thresholds)
  bool isSafePH        = (phValue >= 6.5 && phValue <= 8.5);
  bool isSafeTurbidity = (turbidityValue < 5.0);

  // Status message and style class
  String statusMsg;
  String statusClass;
  if (isSafePH && isSafeTurbidity) {
    statusMsg = "WATER IS SAFE FOR DRINKING";
    statusClass = "safe";
  } else {
    statusMsg = "WATER IS NOT SAFE FOR DRINKING";
    statusClass = "not-safe";
  }

  // Construct the modern styled HTML page
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Water Quality Monitor</title>";
  // Import a modern Google Font
  html += "<link href='https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500&display=swap' rel='stylesheet'>";
  html += "<style>";
  html += "body { margin:0; padding:0; font-family: 'Roboto', sans-serif; ";
  html += "background: linear-gradient(135deg, #f6d365 0%, #fda085 100%); ";
  html += "display: flex; align-items: center; justify-content: center; height: 100vh; }";
  html += ".card { background: #fff; border-radius: 15px; box-shadow: 0 10px 20px rgba(0,0,0,0.2); ";
  html += "padding: 30px; max-width: 400px; width: 90%; text-align: center; }";
  html += "h1 { margin-bottom: 20px; color: #333; }";
  html += ".sensor { margin: 15px 0; font-size: 1.2em; }";
  html += ".sensor span { font-weight: 500; }";
  html += ".status { margin-top: 30px; padding: 15px; border-radius: 10px; ";
  html += "font-size: 1.3em; font-weight: 500; }";
  html += ".safe { background-color: #4caf50; color: #fff; }";
  html += ".not-safe { background-color: #f44336; color: #fff; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='card'>";
  html += "<h1>Water Quality Monitor</h1>";
  html += "<div class='sensor'><strong>pH:</strong> " + String(phValue, 2) + "</div>";
  html += "<div class='sensor'><strong>Turbidity (NTU):</strong> " + String(turbidityValue, 2) + "</div>";
  html += "<div class='sensor'><strong>Temperature (°C):</strong> " + String(temperatureC, 2) + "</div>";
  html += "<div class='status " + statusClass + "'>" + statusMsg + "</div>";
  html += "</div>";
  html += "</body></html>";

  // Send the response
  server.send(200, "text/html", html);
}

// Handle undefined routes
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

void setup() {
  Serial.begin(115200);

  // Initialize DS18B20 sensor
  DS18B20.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Define server routes
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  
  // Start the web server
  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  // Handle client requests
  server.handleClient();
}
