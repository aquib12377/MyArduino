#define BLYNK_TEMPLATE_ID "TMPL3E5t75LZG"
#define BLYNK_TEMPLATE_NAME "Smart Agriculture"
#define BLYNK_AUTH_TOKEN "HIhIX3vTf8N1kMKA1xAGS7m7QrwAuNq7"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Constants
#define DHTPIN 4             // GPIO pin for DHT11
#define DHTTYPE DHT11        // Define the DHT sensor type
#define SOIL_MOISTURE_PIN 34 // Analog pin for soil moisture sensor
#define RELAY_PIN 13         // GPIO pin for relay
#define BUTTON_PIN 12        // GPIO pin for manual motor control button

// WiFi credentials
char ssid[] = "MyProject";      // Replace with your Wi-Fi SSID
char pass[] = "12345678";  // Replace with your Wi-Fi Password

// Blynk Virtual Pins
#define VPIN_SOIL_MOISTURE V2
#define VPIN_TEMPERATURE V0
#define VPIN_HUMIDITY V1
#define VPIN_MOTOR V3

// Sensor and relay objects
DHT dht(DHTPIN, DHTTYPE);
bool motorStatus = false; // Motor status variable

// I2C LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 to your I2C LCD address

BlynkTimer timer; // Create a Blynk Timer

// Function to read and send sensor data to Blynk
void sendSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilMoisture = map(analogRead(SOIL_MOISTURE_PIN), 0, 4095, 100, 0);

  // Control motor automatically based on soil moisture
  if (soilMoisture < 50) {
    digitalWrite(RELAY_PIN, LOW); // Turn ON motor
    motorStatus = true;
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Turn OFF motor
    motorStatus = false;
  }

  // Send data to Blynk
  Blynk.virtualWrite(VPIN_SOIL_MOISTURE, soilMoisture);
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity);
  Blynk.virtualWrite(VPIN_MOTOR, motorStatus ? 1 : 0);

  // Update LCD display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:" + String(temperature) + "C H:" + String(humidity) + "%");
  lcd.setCursor(0, 1);
  lcd.print("SM:" + String(soilMoisture) + "% M:" + (motorStatus ? "ON" : "OFF"));
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize sensors
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH); // Motor OFF initially

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Connect to Wi-Fi and Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Schedule sensor data reading every 2 seconds
  timer.setInterval(2000L, sendSensorData);
}

void loop() {
  Blynk.run();    // Run Blynk
  timer.run();    // Run Blynk Timer

  // Handle manual motor control button
  if (digitalRead(BUTTON_PIN) == LOW) {
    motorStatus = !motorStatus; // Toggle motor state
    digitalWrite(RELAY_PIN, motorStatus ? LOW : HIGH); // Active LOW relay
    delay(500); // Debounce delay
  }
}
