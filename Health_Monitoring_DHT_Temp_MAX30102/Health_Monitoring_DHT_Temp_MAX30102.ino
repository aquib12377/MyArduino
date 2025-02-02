/***************************************************
 * ESP32 code to read DHT11, DS18B20, and MAX30102
 * and send values to Blynk.
 * 
 * This code is based on:
 * - MAX30102 example from SparkFun
 * - DHT11, DS18B20 code snippets
 * - Blynk integration
 ***************************************************/

// -------------------- Include Libraries --------------------

#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3lGf3GiEt"
#define BLYNK_TEMPLATE_NAME "Health Monitoring"
#define BLYNK_AUTH_TOKEN "omM0Qlal7l_W4LvLY-DcWgIiPYf5pAP4"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// DHT11 Libraries
#include <DHT.h>
#define DHTPIN 4      // DHT data pin
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// DS18B20 Libraries
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_PIN 5  // DS18B20 data pin
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);

// MAX30102 Libraries (SparkFun)
#include "MAX30105.h"
#include "heartRate.h" // PBA Algorithm
MAX30105 particleSensor;

// -------------------- Blynk Auth & WiFi --------------------
char ssid[] = "MyProject";
char pass[] = "12345678";

// -------------------- Blynk Timer --------------------
BlynkTimer timer;

// -------------------- MAX30102 Variables --------------------
const byte RATE_SIZE = 4; //Number of samples for averaging BPM
byte rates[RATE_SIZE];    //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;         //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

// -------------------- Variables for Sensors --------------------
float dhtTemp = 0.0;
float dhtHum = 0.0;
float dsTemp = 0.0;

// Interval for reading sensors
const unsigned long readInterval = 5000; // 5 seconds

// -------------------- Function Prototypes --------------------
void readSensors();

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Setup DHT
  dht.begin();

  // Setup DS18B20
  ds18b20.begin();

  // Setup MAX30102 sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) { //Initialize sensor
    Serial.println("MAX30102 was not found. Check wiring.");
    while (1);
  }
  Serial.println("MAX30102 initialized. Place your finger on the sensor.");
  
  // Configure MAX30102
  particleSensor.setup(); //Configure with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);    //Set Red LED brightness
  particleSensor.setPulseAmplitudeGreen(0);     //Turn off Green LED for HR only

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Start Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Set a timer to read and send sensor data every 5 seconds
  timer.setInterval(readInterval, readSensors);
}

// -------------------- readSensors --------------------
// Reads DHT11, DS18B20 and also sends MAX30102 BPM/AvgBPM to Blynk
void readSensors() {
  // Read DHT11
  float newTemp = dht.readTemperature(); // Celsius
  float newHum = dht.readHumidity();
  if (!isnan(newTemp)) dhtTemp = newTemp;
  if (!isnan(newHum)) dhtHum = newHum;

  // Read DS18B20
  ds18b20.requestTemperatures();
  dsTemp = ds18b20.getTempCByIndex(0);

  // At this point, beatsPerMinute and beatAvg are updated in loop()

  // Print values to Serial
  Serial.print("DHT11: T=");
  Serial.print(dhtTemp);
  Serial.print("°C, H=");
  Serial.print(dhtHum);
  Serial.print("% | DS18B20: T=");
  Serial.print(dsTemp);
  Serial.print("°C | MAX30102: BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);
  Serial.println();

  // Send to Blynk
  Blynk.virtualWrite(V0, dhtTemp);
  Blynk.virtualWrite(V1, dhtHum);
  Blynk.virtualWrite(V7, dsTemp);
  Blynk.virtualWrite(V2, beatsPerMinute);
  Blynk.virtualWrite(V3, beatAvg);
}

// -------------------- Loop --------------------
void loop() {
  Blynk.run();
  timer.run();

  // MAX30102 loop for reading heart rate
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true) {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading
      rateSpot %= RATE_SIZE; //Wrap variable

      //Calculate average BPM
      int total = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        total += rates[x];
      beatAvg = total / RATE_SIZE;
    }
  }

  // Optional: Serial debug for MAX30102 (comment out if too noisy)
  // Serial.print("IR=");
  // Serial.print(irValue);
  // Serial.print(", BPM=");
  // Serial.print(beatsPerMinute);
  // Serial.print(", Avg BPM=");
  // Serial.print(beatAvg);
  // if (irValue < 50000) Serial.print(" No finger?");
  // Serial.println();
}
