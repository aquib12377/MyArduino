#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// WiFi Credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// ThingSpeak details
const char* server = "http://api.thingspeak.com";
String writeAPIKey = "332LLLYUDGG6YSUM";
unsigned long channelID = 2781392;

// Pin assignments
const int BUTTON_PIN = 15;    // Digital input for start button
const int LED_PIN = 2;        // LED output pin
const int BUZZER_PIN = 4;     // Buzzer output pin

// MPU6050 instance
Adafruit_MPU6050 mpu;

bool monitoringStarted = false;
unsigned long lastUpdate = 0;
unsigned long updateInterval = 20000; // Post data every 20 seconds

// Threshold for "unusual vibration"
// 1g ≈ 9.81 m/s², let's say unusual > 15 m/s² (approx 1.5g)
// Adjust as needed after testing
float threshold = 30.0; 

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize MPU6050 using Adafruit library
  if (!mpu.begin()) {
    Serial.println("MPU6050 initialization failed!");
    while(1) { delay(1000); }
  }
  Serial.println("MPU6050 initialized successfully.");

  // Optional: Set accelerometer range (default is ±2g)
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);

  Serial.println("Press the button to start monitoring.");
}

void loop() {
  // Check if button is pressed to start monitoring
  if (!monitoringStarted) {
    // Button is INPUT_PULLUP, so LOW means pressed
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(BUTTON_PIN) == LOW) {
        monitoringStarted = true;
        Serial.println("Monitoring started!");
      }
    }
    return; // If not started, just return
  }

  // Read from MPU6050 via Adafruit library
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // a.acceleration.x, a.acceleration.y, a.acceleration.z are in m/s²
  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  // Calculate vector magnitude of acceleration
  // magnitude = sqrt(ax² + ay² + az²)
  float magnitude = sqrt(ax*ax + ay*ay + az*az);

  bool isUnusual = (magnitude > threshold);

  // Debug prints if unusual
  if(isUnusual) {
    Serial.print("ax: "); Serial.print(ax);
    Serial.print(" ay: "); Serial.print(ay);
    Serial.print(" az: "); Serial.print(az);
    Serial.print(" | magnitude: "); Serial.print(magnitude);
    Serial.print(" m/s² | threshold: "); Serial.print(threshold);
    Serial.print(" m/s² | isUnusual: "); Serial.println(isUnusual ? "YES" : "NO");
  }

  if (isUnusual) {
    digitalWrite(LED_PIN, HIGH);
    tone(BUZZER_PIN, 1000); // Buzzer ON at 1kHz tone
  } else {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }

  // Post data to ThingSpeak at defined intervals
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= updateInterval) {
    lastUpdate = currentMillis;
    // Convert magnitude to an integer if you like, or send as float
    // For ThingSpeak you can send integer or use String. We'll send int by truncation.
    postToThingSpeak((int)magnitude, isUnusual);
  }
}

void postToThingSpeak(int vibrationValue, bool isUnusual) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String postData = "api_key=" + writeAPIKey;
    postData += "&field1=" + String(vibrationValue);
    postData += "&field2=" + String(isUnusual ? "Unusual" : "Normal");

    String url = String(server) + "/update";
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    Serial.println("Posting to ThingSpeak...");
    Serial.println("POST Data: " + postData);
    
    int httpCode = http.POST(postData);
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.print("ThingSpeak response: ");
      Serial.println(payload);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(http.errorToString(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected. Could not post data.");
  }
}
