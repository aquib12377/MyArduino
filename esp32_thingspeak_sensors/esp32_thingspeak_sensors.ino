/*
  ESP32 Multi-Sensor Data Logger for ThingSpeak
  
  This code reads data from three sensors and sends it to a ThingSpeak channel.
  - Sensor 1: ADXL345 Accelerometer (reads X and Y axis via I2C)
  - Sensor 2: Digital Tilt Sensor (reads a binary state - Tilted or Not)
  - Sensor 3: 50kg Load Cell with HX711 Amplifier (reads weight)

  ThingSpeak Channel Fields:
  - Field 1: ADXL345 - X-axis Acceleration (m/s^2)
  - Field 2: ADXL345 - Y-axis Acceleration (m/s^2)
  - Field 3: Tilt Sensor Status (0 = Tilted, 1 = Not Tilted)
  - Field 4: Load Cell Weight (kg)
  
  Make sure to install the required libraries via the Arduino IDE Library Manager:
  1. "Adafruit ADXL345" by Adafruit
  2. "Adafruit Unified Sensor" by Adafruit
  3. "HX711_Arduino_Library" by bogde
  4. "ThingSpeak" by MathWorks
*/

// --- LIBRARIES ---
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "HX711.h"
#include "ThingSpeak.h"

// --- CONFIGURATION ---

// 1. WiFi Network Credentials
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASSWORD = "12345678";

// 2. ThingSpeak Channel Credentials
// Replace with your Channel ID and Write API Key
unsigned long THINGSPEAK_CHANNEL_ID = 3096922; 
const char* THINGSPEAK_WRITE_API_KEY = "2J2DZ17D78AAULYR";
// unsigned long THINGSPEAK_CHANNEL_ID = 3096878; 
// const char* THINGSPEAK_WRITE_API_KEY = "1M0AJNHDSEGPS8QZ";

// 3. Pin Definitions
// I2C pins for ADXL345 are typically GPIO 21 (SDA) and 22 (SCL) on most ESP32 boards
const int TILT_SENSOR_PIN = 4;      // Digital pin for the tilt sensor
const int LOADCELL_DOUT_PIN = 16;   // HX711 data output pin
const int LOADCELL_SCK_PIN = 17;    // HX711 clock pin

// --- GLOBAL OBJECTS ---
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345); // Unique ID for the sensor
HX711 scale;
WiFiClient client; // Client for ThingSpeak connection

// --- LOAD CELL CALIBRATION ---
// This value is crucial for accurate weight readings.
// You must calibrate this value for your specific load cell setup.
// See the README for instructions on how to find this value.
float calibration_factor = -22000; // Start with a value like -22000 and adjust.

// --- TIMING ---
// ThingSpeak's free plan allows updates every 15 seconds. 20 seconds is a safe interval.
const long updateInterval = 20000; // 20 seconds in milliseconds
unsigned long lastUpdateTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection
  Serial.println("\nESP32 Multi-Sensor ThingSpeak Logger");

  // --- Initialize WiFi ---
  connectWiFi();

  // --- Initialize ThingSpeak ---
  ThingSpeak.begin(client);

  // --- Initialize ADXL345 Accelerometer ---
  Wire.begin(); // Start I2C communication
  if (!accel.begin()) {
    Serial.println("Error: Could not find a valid ADXL345 sensor. Check wiring!");
    while (1); // Halt execution
  }
  // Set the measurement range
  // Options: ADXL345_RANGE_2_G, ADXL345_RANGE_4_G, ADXL345_RANGE_8_G, ADXL345_RANGE_16_G
  accel.setRange(ADXL345_RANGE_2_G);
  Serial.println("ADXL345 Initialized.");

  // --- Initialize Tilt Sensor ---
  // Using INPUT_PULLUP, so no external resistor is needed.
  // The sensor should connect the pin to GND when tilted.
  pinMode(TILT_SENSOR_PIN, INPUT_PULLUP);
  Serial.println("Tilt Sensor Initialized.");

  // --- Initialize HX711 Load Cell ---
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor); // Set the calibration factor
  scale.tare(); // Reset the scale to 0
  Serial.println("Load Cell Initialized. Please remove any weight for taring.");
}

void loop() {
  // Check if it's time to send a new update
  if (millis() - lastUpdateTime > updateInterval) {
    lastUpdateTime = millis();

    // --- 1. Read ADXL345 Accelerometer ---
    sensors_event_t event;
    accel.getEvent(&event);
    float accelX = event.acceleration.x;
    float accelY = event.acceleration.y;

    Serial.print("Accel X: "); Serial.print(accelX);
    Serial.print(" m/s^2, Accel Y: "); Serial.print(accelY); Serial.println(" m/s^2");

    // --- 2. Read Tilt Sensor ---
    // digitalRead will be LOW (0) when tilted (circuit closed to GND)
    // and HIGH (1) when not tilted (due to internal pull-up resistor).
    int tiltStatus = digitalRead(TILT_SENSOR_PIN);
    Serial.print("Tilt Detected: "); Serial.println(tiltStatus == LOW ? "1" : "0");

    // --- 3. Read Load Cell ---
    // We take an average of 10 readings for stability.
    float weight = scale.get_units(10);
    // Ensure weight is not negative (can happen due to drift)
    if (weight < 0) {
      weight = 0.0;
    }
    Serial.print("Weight: "); Serial.print(weight, 2); Serial.println(" kg");

    // --- 4. Send Data to ThingSpeak ---
    Serial.println("Sending data to ThingSpeak...");

    // Set the fields with the sensor data
    ThingSpeak.setField(1, accelX);
    ThingSpeak.setField(2, accelY);
    ThingSpeak.setField(3, tiltStatus); // 0 for Tilted, 1 for Not Tilted
    ThingSpeak.setField(4, weight);

    // Write the fields to the channel
    int httpCode = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);

    if (httpCode == 200) {
      Serial.println("ThingSpeak update successful.");
    } else {
      Serial.println("Error updating ThingSpeak. HTTP error code: " + String(httpCode));
    }
    Serial.println("------------------------------------");
  }
}

// --- Helper Functions ---

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retries++;
    if (retries > 20) {
       Serial.println("\nFailed to connect to WiFi. Please check credentials.");
       // Consider adding logic to restart the ESP32 here if needed.
       return; 
    }
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}
