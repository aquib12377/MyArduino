/*
  SmartRide - Single ESP32 prototype

  One ESP32 reads every helmet sensor and controls the prototype bike motor.
  The start permission is decided locally:
      helmet worn AND alcohol not detected = start allowed

  Wi-Fi is only used to update the Flask dashboard and store ride history.
  A Wi-Fi/server failure therefore does not bypass the safety conditions.

  NETWORK MODE: the ESP32 creates its OWN Wi-Fi hotspot (SoftAP) instead of
  joining a router/phone hotspot. Its own IP is always 192.168.4.1, so it
  never changes across reboots or locations. The PC running Flask (and any
  phone viewing the dashboard) must join this hotspot instead of the internet.
  See README section 6 for giving the PC a matching fixed IP.
  Because there is no internet uplink, the ESP32 cannot reach NTP time
  servers - the Flask server computes the accident countdown deadline from
  its own clock instead (it already falls back to this whenever the device
  sends no deadline).

  Required libraries:
    ArduinoJson
    Adafruit MPU6050
    Adafruit Unified Sensor
    TinyGPSPlus
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>

// ================= USER SETTINGS =================
// Hotspot the ESP32 itself broadcasts. Every SmartRide device (PC, phones)
// connects to THIS network - none of them connect to your home/mobile Wi-Fi.
const char* AP_SSID = "SmartRide-Helmet";     // change per unit if running more than one
const char* AP_PASSWORD = "smartride123";     // WPA2 needs 8+ chars; change before real use
const IPAddress AP_LOCAL_IP(192, 168, 4, 1);  // ESP32's fixed address - do not change
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// The PC running Flask must join AP_SSID and be given the FIXED static IP
// below on that connection (README section 6). That keeps this address from
// ever needing to change, since the ESP32's own DHCP could otherwise hand
// the PC a different address on a different boot.
const char* SERVER_BASE_URL = "http://192.168.4.2:5000";
const char* DEVICE_KEY = "smartride-demo-device-key";
const char* EMERGENCY_PHONE = "+919702846144";

// ================= PIN ASSIGNMENTS =================
// Helmet and safety inputs
const uint8_t HELMET_SWITCH_PIN = 27; // switch to GND when helmet is worn
const uint8_t CANCEL_BUTTON_PIN = 26; // accident cancel button to GND
const uint8_t START_BUTTON_PIN = 14;  // bike start/stop button to GND
const uint8_t MQ3_ANALOG_PIN = 34;    // input only; use a voltage divider
const uint8_t BATTERY_PIN = 35;       // input only; use a voltage divider

// Outputs
const uint8_t MOTOR_ENABLE_PIN = 13;  // motor driver/relay logic input
const bool MOTOR_ACTIVE_LOW = true;  // current wiring; set false for a HIGH-enable driver
const uint8_t BUZZER_PIN = 25;
const uint8_t GREEN_LED_PIN = 18;     // start allowed
const uint8_t RED_LED_PIN = 19;       // start blocked

// I2C MPU6050
const uint8_t SDA_PIN = 21;
const uint8_t SCL_PIN = 22;

// UART1 GPS
const uint8_t GPS_RX_PIN = 16; // connect GPS TX here
const uint8_t GPS_TX_PIN = 17; // connect GPS RX here (optional)

// UART2 SIM800L
const uint8_t SIM_RX_PIN = 32; // connect SIM800L TX here
const uint8_t SIM_TX_PIN = 33; // connect SIM800L RX here

// ================= CALIBRATION =================
const int MQ3_THRESHOLD = 1800;
const float IMPACT_G_THRESHOLD = 1.5; // above the approximately 1 g at rest
const float TILT_THRESHOLD_DEG = 70.0;
const unsigned long TILT_HOLD_MS = 1000UL; // ignore brief tilts
const unsigned long ACCIDENT_CANCEL_MS = 120000UL;
const unsigned long TELEMETRY_INTERVAL_MS = 1000UL;
const unsigned long BUTTON_DEBOUNCE_MS = 250UL;

Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial simSerial(2);

enum AccidentState { ACCIDENT_NORMAL, ACCIDENT_PENDING, ACCIDENT_CANCELLED, ACCIDENT_CONFIRMED };
AccidentState accidentState = ACCIDENT_NORMAL;

bool helmetWorn = false;
bool alcoholDetected = false;
bool ignitionAllowed = false;
bool bikeRunning = false;
bool smsSent = false;
float batteryPercent = 0;

unsigned long accidentStartedAt = 0;
unsigned long lastTelemetryAt = 0;
unsigned long lastStartPressAt = 0;
bool previousStartButton = HIGH;
bool previousCancelButton = HIGH;
bool tiltTiming = false;
unsigned long tiltStartedAt = 0;

String accidentStateName() {
  switch (accidentState) {
    case ACCIDENT_PENDING: return "pending";
    case ACCIDENT_CANCELLED: return "cancelled";
    case ACCIDENT_CONFIRMED: return "confirmed";
    default: return "normal";
  }
}

void beep(uint8_t count, uint16_t onMs = 120) {
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[WiFi] Hotspot \"");
  Serial.print(AP_SSID);
  Serial.print("\" up. ESP32 IP: ");
  Serial.println(WiFi.softAPIP());
}

bool hasClient() {
  return WiFi.softAPgetStationNum() > 0;
}

void updateMotorOutput() {
  digitalWrite(MOTOR_ENABLE_PIN, bikeRunning == MOTOR_ACTIVE_LOW ? LOW : HIGH);
}

void readSafetyInputs() {
  helmetWorn = digitalRead(HELMET_SWITCH_PIN) == LOW;
  alcoholDetected = analogRead(MQ3_ANALOG_PIN) >= MQ3_THRESHOLD;
  ignitionAllowed = helmetWorn && !alcoholDetected && accidentState != ACCIDENT_PENDING;

  // Immediately stop the prototype motor if either safety condition fails.
  if (!ignitionAllowed) bikeRunning = false;

  updateMotorOutput();
  digitalWrite(GREEN_LED_PIN, ignitionAllowed ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, ignitionAllowed ? LOW : HIGH);
}

void handleStartButton() {
  bool currentButton = digitalRead(START_BUTTON_PIN);

  if (previousStartButton == HIGH && currentButton == LOW &&
      millis() - lastStartPressAt >= BUTTON_DEBOUNCE_MS) {
    lastStartPressAt = millis();

    if (bikeRunning) {
      bikeRunning = false;
      Serial.println("[BIKE] Stopped by button");
      beep(1);
    } else if (ignitionAllowed) {
      // Starting a new ride also re-arms detection. No reset gesture needed.
      accidentState = ACCIDENT_NORMAL;
      smsSent = false;
      tiltTiming = false;
      bikeRunning = true;
      Serial.println("[BIKE] Started: helmet and alcohol checks passed");
      beep(2);
    } else {
      Serial.println("[BIKE] Start blocked: check helmet/alcohol or cancel the pending alert");
      beep(3, 220);
    }

    updateMotorOutput();
  }

  previousStartButton = currentButton;
}

void sendAT(const String& command, unsigned long waitMs = 500) {
  simSerial.println(command);
  Serial.print("[SIM] ");
  Serial.println(command);

  unsigned long started = millis();
  while (millis() - started < waitMs) {
    while (simSerial.available()) Serial.write(simSerial.read());
  }
}

void sendEmergencySMS() {
  String location = "GPS location unavailable";

  if (gps.location.isValid()) {
    location = "https://maps.google.com/?q=" +
               String(gps.location.lat(), 6) + "," +
               String(gps.location.lng(), 6);
  }

  sendAT("AT");
  sendAT("AT+CMGF=1");
  simSerial.print("AT+CMGS=\"");
  simSerial.print(EMERGENCY_PHONE);
  simSerial.println("\"");
  delay(700);
  simSerial.print("SmartRide emergency: possible rider accident. Location: ");
  simSerial.print(location);
  simSerial.write(26); // Ctrl+Z sends the message
  delay(5000);

  smsSent = true;
  Serial.println("[ACCIDENT] Emergency SMS command completed");
}

void handleAccidentDetection() {
  unsigned long now = millis();
  bool cancelButton = digitalRead(CANCEL_BUTTON_PIN);
  bool cancelPressed = previousCancelButton == HIGH && cancelButton == LOW;
  previousCancelButton = cancelButton;
sensors_event_t acceleration, gyro, temperature;
    mpu.getEvent(&acceleration, &gyro, &temperature);
  // Only an active ride can create a new alert. A pending countdown keeps
  // running after the motor stops, independently of helmet/alcohol inputs.
  if (bikeRunning && accidentState == ACCIDENT_NORMAL) {
    
    float ax = acceleration.acceleration.x / 9.80665;
    float ay = acceleration.acceleration.y / 9.80665;
    float az = acceleration.acceleration.z / 9.80665;
    float totalG = sqrt(ax * ax + ay * ay + az * az);
    // Mount with the sensor Z axis pointing up; inversion is also a tilt.
    float tiltDeg = atan2(sqrt(ax * ax + ay * ay), az) * 180.0 / PI;
    bool validTilt = totalG >= 0.5 && totalG <= 1.5;
    if (validTilt && tiltDeg >= TILT_THRESHOLD_DEG) {
      if (!tiltTiming) { tiltTiming = true; tiltStartedAt = now; }
    } else {
      tiltTiming = false;
    }
    bool impact = totalG >= IMPACT_G_THRESHOLD;
    bool sustainedTilt = tiltTiming && now - tiltStartedAt >= TILT_HOLD_MS;
    if (impact || sustainedTilt) {
      accidentState = ACCIDENT_PENDING;
      ignitionAllowed = false;
      accidentStartedAt = now;
      smsSent = false;
      bikeRunning = false;
      tiltTiming = false;
      updateMotorOutput();
      Serial.printf("[ACCIDENT] Possible accident: %.2fg, tilt %.1f degrees. Press Cancel within 120 seconds.\n", totalG, tiltDeg);
      beep(4, 180);
    }
  } else {
    tiltTiming = false;
  }

  if (accidentState == ACCIDENT_PENDING && cancelPressed) {
    accidentState = ACCIDENT_CANCELLED;
    Serial.println("[ACCIDENT] Cancelled. Press Start when ready for another ride.");
    beep(2);
  }

  if (accidentState == ACCIDENT_PENDING &&
      millis() - accidentStartedAt >= ACCIDENT_CANCEL_MS) {
    accidentState = ACCIDENT_CONFIRMED;
    sendEmergencySMS();
    Serial.println("[ACCIDENT] Confirmed. Press Start when ready for another ride.");
    beep(8, 200);
  }
}

void updateBatteryReading() {
  int raw = analogRead(BATTERY_PIN);
  float batteryVoltage = (raw / 4095.0) * 3.3 * 2.0; // equal-value divider
  batteryPercent = constrain((batteryVoltage - 3.2) * 100.0 / (4.2 - 3.2), 0, 100);
}

int postJson(const String& path, const String& body) {
  if (!hasClient()) return -1; // nothing joined the hotspot yet

  HTTPClient http;
  http.setTimeout(700);
  http.begin(String(SERVER_BASE_URL) + path);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int status = http.POST(body);
  http.end();
  return status;
}

void postTelemetry() {
  if (!hasClient()) return; // PC/phone not on the hotspot yet - safety logic keeps running regardless

  StaticJsonDocument<512> helmetDoc;
  helmetDoc["helmet_worn"] = helmetWorn;
  helmetDoc["alcohol_detected"] = alcoholDetected;
  helmetDoc["battery"] = batteryPercent;
  helmetDoc["accident_status"] = accidentStateName();
  // No accident_deadline field: the ESP32 has no internet for NTP, so it
  // can't compute a real timestamp. Flask fills this in from its own clock.
  helmetDoc["sms_status"] = smsSent ? "Sent by SIM800L" : "";
  helmetDoc["speed"] = gps.speed.isValid() ? gps.speed.kmph() : 0;

  if (gps.location.isValid()) {
    helmetDoc["latitude"] = gps.location.lat();
    helmetDoc["longitude"] = gps.location.lng();
  }

  String helmetBody;
  serializeJson(helmetDoc, helmetBody);
  int helmetStatus = postJson("/api/device/helmet", helmetBody);

  StaticJsonDocument<96> bikeDoc;
  bikeDoc["bike_running"] = bikeRunning;
  String bikeBody;
  serializeJson(bikeDoc, bikeBody);
  int bikeStatus = postJson("/api/device/bike", bikeBody);

  Serial.printf(
    "[WEB] helmet=%d bike=%d worn=%d alcohol=%d allowed=%d running=%d battery=%.0f%% accident=%s\n",
    helmetStatus,
    bikeStatus,
    helmetWorn,
    alcoholDetected,
    ignitionAllowed,
    bikeRunning,
    batteryPercent,
    accidentStateName().c_str()
  );
}

void setup() {
  Serial.begin(115200);

  pinMode(HELMET_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  updateMotorOutput();
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);

  analogReadResolution(12);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!mpu.begin()) {
    Serial.println("[ERROR] MPU6050 not found. Check SDA/SCL and power.");
    while (true) {
      beep(1, 500);
      delay(500);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  simSerial.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

  startAccessPoint();
  readSafetyInputs();
  beep(2);

  Serial.println("[READY] SmartRide single-ESP32 controller started");
}

void loop() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  readSafetyInputs();
  handleStartButton();
  handleAccidentDetection();
  updateBatteryReading();

  if (millis() - lastTelemetryAt >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryAt = millis();
    postTelemetry();
  }

  delay(20);
}
