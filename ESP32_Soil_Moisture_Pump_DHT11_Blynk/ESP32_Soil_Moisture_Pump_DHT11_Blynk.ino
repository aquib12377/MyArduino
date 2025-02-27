/***************************************************
 *  Blynk Smart Agriculture Example
 *  
 *  Hardware:
 *   - ESP32
 *   - DHT11 on GPIO 25 (example)
 *   - Soil Moisture on GPIO 34 (analog)
 *   - Relay on GPIO 26 (active LOW)
 *
 *  Make sure to install:
 *   - Blynk library
 *   - DHT sensor library (e.g. "DHT sensor library" by Adafruit)
 ****************************************************/

// ========== BLYNK Setup ==========
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3o34aznwr"
#define BLYNK_TEMPLATE_NAME "Smart Aggriculture"
#define BLYNK_AUTH_TOKEN "bYo4wtPWdBQ9m1qyctF66ybtzuit7xFb"

// ========== WiFi Credentials ==========
char ssid[] = "MyProject";
char pass[] = "12345678";

// ========== Libraries ==========
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"

// ========== Pin Definitions ==========
#define DHTPIN 25             // DHT11 data pin connected to GPIO 25
#define DHTTYPE DHT11         // Using DHT11
#define SOIL_MOISTURE_PIN 34  // Soil Moisture sensor analog output
#define RELAY_PIN 26          // Relay control pin (active LOW)

// ========== Global Objects ==========
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// Variables for auto/manual control
bool autoMode = true;        // false = manual, true = auto
int manualPumpState = 0;      // 0 = OFF, 1 = ON (used in manual mode)

// --- BLYNK_WRITE for Manual Button (V1) ---
BLYNK_WRITE(V3)
{
  manualPumpState = param.asInt(); // 1 = ON, 0 = OFF
  Serial.print("Manual Button State: ");
  Serial.println(manualPumpState);
}

// --- BLYNK_WRITE for Auto/Manual Switch (V5) ---
BLYNK_WRITE(V4)
{
  autoMode = param.asInt(); // 1 = auto, 0 = manual
  Serial.print("Auto Mode: ");
  Serial.println(autoMode ? "ON" : "OFF");
}

// --- Function to read sensors and manage pump ---
void readSensors() {
  // 1) Read Soil Moisture (Analog)
  int soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN); 
  // Convert raw ADC (0-4095) to percentage (0-100)
  float soilMoisturePercent = map(soilMoistureRaw, 4095, 0, 0, 100);
  
  // 2) Read DHT11 (Temperature & Humidity)
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature(); // Celsius

  // Debug prints
  Serial.print("Soil Moisture Raw: ");
  Serial.print(soilMoistureRaw);
  Serial.print(" -> %: ");
  Serial.print(soilMoisturePercent);
  Serial.print("% | Temp: ");
  Serial.print(temperature);
  Serial.print("C | Hum: ");
  Serial.print(humidity);
  Serial.println("%");
  
  // 3) Send data to Blynk
  //    - V2: Soil Moisture
  //    - V3: Temperature
  //    - V4: Humidity
  Blynk.virtualWrite(V0, soilMoisturePercent);
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, digitalRead(RELAY_PIN) == LOW);

  // 4) Pump Control Logic
  //    If autoMode is ON => automatically control pump based on threshold
  //    If autoMode is OFF => use manualPumpState from Blynk button (V1)
   float moistureThreshold = 40.0;
    if (soilMoisturePercent < moistureThreshold) {
      // Turn pump ON (active LOW)
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Auto Mode: Pump ON (Soil moisture below threshold)");
    } else {
      // Turn pump OFF
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Auto Mode: Pump OFF (Soil moisture above threshold)");
    }

}

void setup() {
  // Start Serial
  Serial.begin(115200);
  
  // Initialize Relay pin (active LOW)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // ensure pump OFF initially
  
  // Initialize the DHT sensor
  dht.begin();
  
  // Setup Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Setup a function to be called every 2 seconds
  timer.setInterval(2000L, readSensors);
}

void loop() {
  Blynk.run();
  timer.run();
}
