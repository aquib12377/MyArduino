#include <Arduino.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WebServer.h>

// ------------------- WiFi Settings -------------------
const char* ssid = "MyProject";     // Replace with your WiFi SSID
const char* password = "12345678";  // Replace with your WiFi Password

// ------------------- Dustbin Information -------------------
const int dustbinNumber = 1;           // Change if you have multiple dustbins
volatile int globalFullness = 0;       // Updated fullness percentage (0 - 100)
volatile float globalLatitude = 18.961734;   // Latest latitude from GPS (default value)
volatile float globalLongitude = 72.816222;  // Latest longitude from GPS (default value)

// ------------------- PIN DEFINITIONS -------------------
// Ultrasonic Sensor (HC-SR04)
const int trigPin = 18;
const int echoPin = 19;

// LEDs
const int redLED = 32;
const int yellowLED = 33;
const int greenLED = 25;

// ------------------- GSM & GPS Setup -------------------
// GSM Module: Using Hardware Serial2 (ESP32 pins: TX=16, RX=17)
String phoneNumber = "+917977845638";  // Update with the actual phone number (include country code)

// GPS Module: Using Hardware Serial1 (only RX is needed)
// NOTE: Your comment mentions pin 34 but your code uses pin 4. Adjust if needed.
TinyGPSPlus gps;

// Flag to prevent repeated SMS sending
bool smsSent = false;

// ------------------- Web Server -------------------
WebServer server(80);

// ------------------- Function Prototypes -------------------
float measureDistance();
void updateLEDs(int fullness);
void sendSMS(String message, String phoneNumber);
void updateSerial();
void handleRoot();
void handleData();  // New endpoint to serve JSON data

// ===================== SETUP =====================
void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 GSM + GPS + Ultrasonic + Web Server Starting...");

  // ------------------- WiFi Initialization -------------------
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());

  // Set up web server routes and start the server
  server.on("/", handleRoot);   // Serves the main webpage
  server.on("/data", handleData); // Serves JSON data for AJAX calls
  server.begin();
  Serial.println("Web server started.");

  // ------------------- GSM Module Initialization -------------------
  // Using Hardware Serial2 with RX on GPIO16 and TX on GPIO17
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  delay(1000);
  Serial.println("Initializing GSM Module...");
  Serial2.println("AT");  // Basic test command

  // ------------------- GPS Module Initialization -------------------
  // Using Hardware Serial1. (Here using RX = 4; change to 34 if needed)
  Serial1.begin(9600, SERIAL_8N1, 4, -1);

  // ------------------- Sensor & LED Setup -------------------
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(redLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, LOW);
  digitalWrite(greenLED, LOW);
}

// ===================== LOOP =====================
void loop() {
  // Handle incoming web client requests
  server.handleClient();

  // --- Read and Parse GPS Data ---
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    gps.encode(c);
  }

  // --- Read Ultrasonic Sensor ---
  float distance = measureDistance();
  Serial.print("OG Distance: ");
  Serial.print(distance);

  // Limit the maximum reading to 30 cm if it goes beyond
  if (distance > 30) distance = 30;

  // Calculate dustbin fullness percentage:
  // When distance is 30 cm -> 0% full, when 0 cm -> 100% full.
  int fullness = (int)(((30 - distance) / 30.0) * 100);
  globalFullness = fullness;  // Update global variable for webpage

  Serial.print(" cm, Fullness: ");
  Serial.print(fullness);
  Serial.println(" %");

  // --- Update LED Status ---
  updateLEDs(fullness);

  // --- Update Global GPS Coordinates ---
  if (gps.location.isValid()) {
    globalLatitude = gps.location.lat();
    globalLongitude = gps.location.lng();
  }

  // --- Check if Dustbin is Almost Full & Send SMS if Needed ---
  if (fullness >= 95 && !smsSent) {
    // Prepare SMS message with GPS coordinates if available
    String lat = "N/A";
    String lng = "N/A";
    if (gps.location.isValid()) {
      lat = String(gps.location.lat(), 6);
      lng = String(gps.location.lng(), 6);
    }
    // Construct a Google Maps link for the location
    String message = "Dustbin #" + String(dustbinNumber) + " full (" + String(fullness) + "%).\nLocation: https://maps.google.com/?q=" + lat + "," + lng;
    sendSMS(message, phoneNumber);
    smsSent = true;  // Prevent repeated SMS until level drops
  }
  // Reset the SMS flag if fullness drops below 90% (hysteresis)
  else if (fullness < 90) {
    smsSent = false;
  }

  // --- Forward any GSM Module Responses (for debugging) ---
  if (Serial2.available()) {
    String gsmResponse = Serial2.readString();
    Serial.print("GSM Response: ");
    Serial.println(gsmResponse);
  }

  // Optionally, send a periodic AT command every 10 seconds to keep the GSM module active
  static unsigned long lastATTime = 0;
  if (millis() - lastATTime > 10000) {
    Serial.println("Sending periodic AT command...");
    Serial2.println("AT");
    lastATTime = millis();
    updateSerial();
  }

  delay(1000);  // Delay between sensor readings (adjust as needed)
}

// ===================== WEB SERVER HANDLER =====================
// Serves a webpage displaying the dustbin number, fullness, and an interactive Leaflet map.
// The page uses AJAX to update the map marker and fullness without reloading.
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Dustbin Status</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  // Note: No meta refresh tag since we are using AJAX for live updates.
  // Include Leaflet CSS from CDN
  html += "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.7.1/dist/leaflet.css' />";
  html += "<style>body { font-family: Arial, sans-serif; margin: 0; padding: 0; } ";
  html += "#map { height: 600px; width: 100%; }</style></head>";
  html += "<body><h1>Dustbin Status</h1>";
  html += "<p>Dustbin Number: " + String(dustbinNumber) + "</p>";
  html += "<p>Dustbin Fullness: <span id='fullness'>" + String(globalFullness) + "%</span></p>";
  html += "<div id='map'></div>";
  // Include Leaflet JS from CDN
  html += "<script src='https://unpkg.com/leaflet@1.7.1/dist/leaflet.js'></script>";
  html += "<script>";
  // Initialize the map using the initial global coordinates (default values)
  html += "var map = L.map('map').setView([" + String(globalLatitude, 6) + ", " + String(globalLongitude, 6) + "], 15);";
  html += "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {";
  html += "    attribution: '&copy; OpenStreetMap contributors',";
  html += "    maxZoom: 20";
  html += "}).addTo(map);";
  // Create a marker with an initial position
  html += "var marker = L.marker([" + String(globalLatitude, 6) + ", " + String(globalLongitude, 6) + "]).addTo(map);";
  html += "marker.bindPopup('Dustbin #" + String(dustbinNumber) + "').openPopup();";
  // Function to update map and text using AJAX
  html += "function updateData() {";
  html += "  fetch('/data')"
       "    .then(response => response.json())"
       "    .then(data => {"
       "       var newLat = parseFloat(data.latitude);"
       "       var newLon = parseFloat(data.longitude);"
       "       marker.setLatLng([newLat, newLon]);"
       "       map.setView([newLat, newLon]);"
       "       document.getElementById('fullness').innerText = data.fullness + '%';"
       "    })"
       "    .catch(err => console.log('Error: ', err));";
  html += "}";
  // Call updateData every 5 seconds
  html += "setInterval(updateData, 1000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ===================== DATA HANDLER =====================
// Serves a JSON string with the current dustbin data for AJAX calls.
void handleData() {
  String json = "{";
  json += "\"dustbin\":" + String(dustbinNumber) + ",";
  json += "\"fullness\":" + String(globalFullness) + ",";
  json += "\"latitude\":" + String(globalLatitude, 6) + ",";
  json += "\"longitude\":" + String(globalLongitude, 6);
  json += "}";
  server.send(200, "application/json", json);
}

// ===================== HELPER FUNCTIONS =====================

// Measure distance (in cm) from the ultrasonic sensor
float measureDistance() {
  // Ensure trigger is LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Send a 10µs HIGH pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo time (timeout after 30000 µs)
  long duration = pulseIn(echoPin, HIGH, 30000);
  // Calculate distance in cm (speed of sound = 0.0343 cm/µs)
  float distance = duration * 0.0343 / 2;
  return distance;
}

// Update the LED indicators based on the dustbin fullness percentage
void updateLEDs(int fullness) {
  if (fullness < 50) {
    // Less than 50% full: Green LED ON
    digitalWrite(greenLED, HIGH);
    digitalWrite(yellowLED, LOW);
    digitalWrite(redLED, LOW);
  } else if (fullness >= 50 && fullness < 80) {
    // Between 50% and 80% full: Yellow LED ON
    digitalWrite(greenLED, LOW);
    digitalWrite(yellowLED, HIGH);
    digitalWrite(redLED, LOW);
  } else {
    // 80% or more full: Red LED ON
    digitalWrite(greenLED, LOW);
    digitalWrite(yellowLED, LOW);
    digitalWrite(redLED, HIGH);
  }
}

// Send an SMS message via the GSM module
void sendSMS(String message, String phoneNumber) {
  Serial.println("Configuring GSM module for SMS...");
  Serial2.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  updateSerial();

  Serial.println("Sending SMS...");
  Serial2.println("AT+CMGS=\"" + phoneNumber + "\"");  // Specify recipient
  delay(1000);
  updateSerial();

  Serial2.print(message);  // Send the SMS message text
  delay(100);
  updateSerial();

  Serial2.write(26);  // Send Ctrl+Z (ASCII 26) to indicate end of message
  delay(1000);
  updateSerial();
  Serial.println("SMS sent.");
}

// A helper function to forward any available data between Serial and GSM Serial2
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    Serial2.write(Serial.read());
  }
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}
