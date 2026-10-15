/* SmartRide bike prototype ESP32 firmware
   Controls a small DC motor driver/relay. Do not connect directly to a real motorcycle ignition.
   Library: ArduinoJson
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* BIKE_API_URL = "http://192.168.1.100:5000/api/device/bike";
const char* DEVICE_KEY = "smartride-demo-device-key";

const int START_BUTTON_PIN = 27; // push button to GND
const int MOTOR_ENABLE_PIN = 26; // MOSFET/driver input, never the motor directly
const int GREEN_LED_PIN = 25;
const int RED_LED_PIN = 33;
const unsigned long POLL_MS = 750;

bool requestedRunning = false;
bool actualRunning = false;
bool lastButton = HIGH;
unsigned long lastPoll = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  digitalWrite(MOTOR_ENABLE_PIN, LOW); // fail safe
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) delay(300);
}

bool contactServer(bool postState) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http; http.setTimeout(1400); http.begin(BIKE_API_URL);
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int code;
  if (postState) {
    http.addHeader("Content-Type", "application/json");
    String body = String("{\"bike_running\":") + (requestedRunning ? "true}" : "false}");
    code = http.POST(body);
  } else code = http.GET();
  if (code != 200) { http.end(); return false; }
  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) return false;
  bool allowed = doc["ignition_allowed"] | false;
  actualRunning = requestedRunning && (doc["bike_running"] | false) && allowed;
  if (!allowed) requestedRunning = false;
  digitalWrite(MOTOR_ENABLE_PIN, actualRunning ? HIGH : LOW);
  digitalWrite(GREEN_LED_PIN, allowed ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, allowed ? LOW : HIGH);
  Serial.printf("Allowed=%d Requested=%d Running=%d\n", allowed, requestedRunning, actualRunning);
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_ENABLE_PIN, OUTPUT); pinMode(GREEN_LED_PIN, OUTPUT); pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(MOTOR_ENABLE_PIN, LOW); digitalWrite(RED_LED_PIN, HIGH);
  connectWiFi();
}

void loop() {
  connectWiFi();
  bool button = digitalRead(START_BUTTON_PIN);
  if (lastButton == HIGH && button == LOW) {
    requestedRunning = !requestedRunning;
    contactServer(true);
    delay(250);
  }
  lastButton = button;
  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    // POST desired state every poll to correct stale server state after reconnection.
    if (!contactServer(true)) {
      requestedRunning = actualRunning = false;
      digitalWrite(MOTOR_ENABLE_PIN, LOW); digitalWrite(GREEN_LED_PIN, LOW); digitalWrite(RED_LED_PIN, HIGH);
    }
  }
  delay(20);
}
