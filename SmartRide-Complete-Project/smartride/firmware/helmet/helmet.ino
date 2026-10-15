/* SmartRide helmet ESP32 firmware
   Libraries: ArduinoJson, Adafruit MPU6050, Adafruit Unified Sensor, TinyGPSPlus
   Board: ESP32 Dev Module
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>
#include <time.h>

// ---------- Change these values ----------
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://192.168.1.100:5000/api/device/helmet";
const char* DEVICE_KEY = "smartride-demo-device-key";
const char* EMERGENCY_PHONE = "+919999999999";

// ---------- Pins ----------
const int HELMET_SWITCH_PIN = 27; // switch closes to GND when helmet is worn
const int CANCEL_BUTTON_PIN = 26; // button to GND
const int MQ3_ANALOG_PIN = 34;    // through voltage divider; never exceed 3.3 V
const int BATTERY_PIN = 35;       // through voltage divider
const int BUZZER_PIN = 25;
const int GPS_RX = 16, GPS_TX = 17;
const int SIM_RX = 32, SIM_TX = 33;
const int SDA_PIN = 21, SCL_PIN = 22;

const int MQ3_THRESHOLD = 1800;       // calibrate after MQ-3 warm-up
const float IMPACT_G_THRESHOLD = 3.0; // prototype threshold
const float TILT_THRESHOLD_DEG = 65.0;
const unsigned long ALERT_MS = 120000;
const unsigned long POST_INTERVAL_MS = 1000;

Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial simSerial(2);

enum AccidentState { NORMAL, PENDING, CANCELLED, CONFIRMED };
AccidentState accidentState = NORMAL;
unsigned long accidentStarted = 0;
unsigned long lastPost = 0;
bool smsSent = false;
float batteryPercent = 0;

String stateName() {
  switch (accidentState) {
    case PENDING: return "pending";
    case CANCELLED: return "cancelled";
    case CONFIRMED: return "confirmed";
    default: return "normal";
  }
}

String isoDeadline() {
  if (accidentState != PENDING) return "";
  time_t now; time(&now);
  if (now < 1700000000) return ""; // NTP not ready
  unsigned long elapsed = millis() - accidentStarted;
  time_t deadline = now + ((ALERT_MS - min(elapsed, ALERT_MS)) / 1000);
  struct tm utc; gmtime_r(&deadline, &utc);
  char out[25]; strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(out);
}

void beep(int count, int onMs = 120) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(onMs);
    digitalWrite(BUZZER_PIN, LOW); delay(100);
  }
}

void sendAT(const String &command, unsigned long waitMs = 500) {
  simSerial.println(command);
  unsigned long start = millis();
  while (millis() - start < waitMs) {
    while (simSerial.available()) Serial.write(simSerial.read());
  }
}

void sendEmergencySMS() {
  String location = "GPS location unavailable";
  if (gps.location.isValid()) {
    location = "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  }
  sendAT("AT");
  sendAT("AT+CMGF=1");
  simSerial.print("AT+CMGS=\""); simSerial.print(EMERGENCY_PHONE); simSerial.println("\"");
  delay(700);
  simSerial.print("SmartRide emergency: possible rider accident. Location: ");
  simSerial.print(location);
  simSerial.write(26); // Ctrl+Z
  delay(5000);
  smsSent = true;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting Wi-Fi");
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) { delay(300); Serial.print('.'); }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " failed");
}

void detectAccident() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  float ax = accel.acceleration.x / 9.80665;
  float ay = accel.acceleration.y / 9.80665;
  float az = accel.acceleration.z / 9.80665;
  float totalG = sqrt(ax * ax + ay * ay + az * az);
  float tilt = atan2(sqrt(ax * ax + ay * ay), fabs(az)) * 180.0 / PI;

  if (accidentState == NORMAL && (totalG >= IMPACT_G_THRESHOLD || tilt >= TILT_THRESHOLD_DEG)) {
    accidentState = PENDING;
    accidentStarted = millis();
    smsSent = false;
    beep(4, 180);
    Serial.printf("Possible accident: %.2fg, tilt %.1f deg\n", totalG, tilt);
  }
  if (accidentState == PENDING && digitalRead(CANCEL_BUTTON_PIN) == LOW) {
    accidentState = CANCELLED;
    beep(2);
    delay(250); // debounce
  }
  if (accidentState == PENDING && millis() - accidentStarted >= ALERT_MS) {
    accidentState = CONFIRMED;
    sendEmergencySMS();
    beep(8, 200);
  }
  // Hold cancel for 3 seconds after cancelled/confirmed to re-arm.
  static unsigned long holdStart = 0;
  if (accidentState != NORMAL && accidentState != PENDING && digitalRead(CANCEL_BUTTON_PIN) == LOW) {
    if (!holdStart) holdStart = millis();
    if (millis() - holdStart > 3000) { accidentState = NORMAL; holdStart = 0; beep(1); }
  } else holdStart = 0;
}

void postTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return;
  StaticJsonDocument<512> doc;
  doc["helmet_worn"] = digitalRead(HELMET_SWITCH_PIN) == LOW;
  doc["alcohol_detected"] = analogRead(MQ3_ANALOG_PIN) >= MQ3_THRESHOLD;
  doc["battery"] = batteryPercent;
  doc["accident_status"] = stateName();
  doc["accident_deadline"] = isoDeadline();
  doc["sms_status"] = smsSent ? "Sent by SIM800L" : "";
  if (gps.location.isValid()) { doc["latitude"] = gps.location.lat(); doc["longitude"] = gps.location.lng(); }
  doc["speed"] = gps.speed.isValid() ? gps.speed.kmph() : 0;
  String body; serializeJson(doc, body);
  HTTPClient http;
  http.setTimeout(1500);
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int code = http.POST(body);
  Serial.printf("POST %d: %s\n", code, body.c_str());
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(HELMET_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  analogReadResolution(12);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!mpu.begin()) { Serial.println("MPU6050 not found"); while (true) { beep(1, 500); delay(500); } }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  simSerial.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);
  connectWiFi();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  beep(2);
}

void loop() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  connectWiFi();
  detectAccident();
  int rawBattery = analogRead(BATTERY_PIN);
  float batteryV = (rawBattery / 4095.0) * 3.3 * 2.0; // for equal-value divider
  batteryPercent = constrain((batteryV - 3.2) * 100.0 / (4.2 - 3.2), 0, 100);
  if (millis() - lastPost >= POST_INTERVAL_MS) { lastPost = millis(); postTelemetry(); }
  delay(20);
}

