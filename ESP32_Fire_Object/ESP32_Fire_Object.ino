#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL34fENc1RU"
#define BLYNK_TEMPLATE_NAME "Fire and Object Detection"
#define BLYNK_AUTH_TOKEN "ZuTjCkWYd5HFlHn5lc-XtPxswEJfdAwk"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Blynk credentials
char ssid[] = "MyProject";    // Replace with your WiFi SSID
char pass[] = "12345678";     // Replace with your WiFi Password

// Pin Definitions
#define fireSensorPin 12  // Fire sensor connected to pin 12
#define irSensorPin 13    // IR sensor connected to pin 13
#define oneWireBus 14     // DS18B20 sensor connected to pin 14

// Setup a OneWire instance for DS18B20
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

BlynkTimer timer;

// Function to read sensors and send data to Blynk
void readSensors() {
  // Read Fire Sensor
  int fireDetected = digitalRead(fireSensorPin);
  
  // Read IR Sensor
  int obstacleDetected = digitalRead(irSensorPin);
  
  // Request temperature from DS18B20
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0); // Get temperature from the first DS18B20 sensor

  // Send Fire sensor status to Blynk (V0)
  Blynk.virtualWrite(V0, fireDetected == LOW ? "No" : "Yes");

  // Send IR sensor status to Blynk (V1)
  Blynk.virtualWrite(V1, obstacleDetected == HIGH ? "No" : "Yes");

  // Send Temperature to Blynk (V2)
  Blynk.virtualWrite(V2, temperature);

  // Debugging output on the Serial Monitor
  Serial.print("Fire Detected: ");
  Serial.println(fireDetected == LOW ? "No" : "Yes");
  
  Serial.print("Obstacle Detected: ");
  Serial.println(obstacleDetected == HIGH ? "No" : "Yes");

  Serial.print("Temperature: ");
  Serial.println(temperature);
}

void setup() {
  // Setup Serial Monitor
  Serial.begin(115200);

  // Initialize Fire and IR sensor pins
  pinMode(fireSensorPin, INPUT);
  pinMode(irSensorPin, INPUT);

  // Start communication with DS18B20 temperature sensor
  sensors.begin();

  // Connect to Wi-Fi and Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup a timer to read sensor data every 2 seconds
  timer.setInterval(2000L, readSensors);
}

void loop() {
  Blynk.run();
  timer.run();
}
