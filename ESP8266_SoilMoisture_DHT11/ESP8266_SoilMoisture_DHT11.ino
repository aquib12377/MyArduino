#define BLYNK_TEMPLATE_ID   "TMPL3F3jdElqI"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation"
#define BLYNK_AUTH_TOKEN    "ZMjY8lckt-culVWWwHEJkBdl4iZCnEm_"

// Wi-Fi credentials
#define WIFI_SSID "MyProject"
#define WIFI_PASS "12345678"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>

// DHT settings
#define DHTPIN   2        // GPIO2 on NodeMCU (change as needed)
#define DHTTYPE  DHT11     // DHT11 or DHT22
DHT dht(DHTPIN, DHTTYPE);

// Soil moisture sensor
#define SOIL_PIN A0       // analog pin

BlynkTimer timer;

// Read sensors and push to Blynk
void sendSensorData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int soilRaw = analogRead(SOIL_PIN);
  // map soil value to percentage (wet=100%, dry=0%)
  float soilPct = map(soilRaw, 1023, 0, 0, 100);

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("T=");
  Serial.print(t);
  Serial.print("°C, H=");
  Serial.print(h);
  Serial.print("%, Soil=");
  Serial.print(soilPct);
  Serial.println("%");

  Blynk.virtualWrite(V1, t);        // Temperature → Virtual Pin V1
  Blynk.virtualWrite(V2, h);        // Humidity    → Virtual Pin V2
  Blynk.virtualWrite(V0, soilPct);  // Soil %      → Virtual Pin V3
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  // Send sensor data every 2 seconds
  timer.setInterval(2000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}
