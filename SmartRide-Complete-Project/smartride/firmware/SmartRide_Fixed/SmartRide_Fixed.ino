/* Detection update:
   Keep the MPU still in normal riding orientation during the 2-second boot calibration.
   Trigger: >=2.5g impact OR >=40 degrees tilt continuously for 300ms.
   Cancel on GPIO26, then return upright and hold Cancel for 3 seconds to re-arm.
   Serial Monitor: 115200. Thresholds are prototype starting values, not crash validation.
   Motor cannot restart until detection has been re-armed.
   Dashboard requests run in a separate task; buzzer timing is non-blocking.
   GSM commands/responses are logged at 115200 baud. Startup sends one test SMS.
   SMS routines wait for modem responses; motor stays OFF during startup/confirmed events.
   +CMGS and OK confirm submission, not delivery to the recipient's phone.
*/
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

#include <Arduino.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
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
const char* EMERGENCY_PHONE = "+918591819175";
const bool SEND_STARTUP_SMS = true; // One startup SMS attempt on each reboot.
const unsigned long GSM_NETWORK_WAIT_MS = 60000UL;
const unsigned long GSM_SMS_WAIT_MS = 60000UL;

// ================= PIN ASSIGNMENTS =================
// Helmet and safety inputs
const uint8_t HELMET_SWITCH_PIN = 27; // switch to GND when helmet is worn
const uint8_t CANCEL_BUTTON_PIN = 26; // accident cancel button to GND
const uint8_t START_BUTTON_PIN = 14;  // bike start/stop button to GND
const uint8_t MQ3_ANALOG_PIN = 34;    // input only; use a voltage divider
const uint8_t BATTERY_PIN = 35;       // input only; use a voltage divider

// Outputs
const uint8_t MOTOR_ENABLE_PIN = 13;  // motor driver/MOSFET logic input
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
// Prototype starting values; tune using the printed peak acceleration.
const float IMPACT_G_THRESHOLD = 2.5;
const float TILT_THRESHOLD_DEG = 40.0;
const unsigned long TILT_HOLD_MS = 300;
const unsigned long MPU_SAMPLE_MS = 10;
const unsigned long MPU_DEBUG_MS = 250;
const float REARM_TILT_DEG = 20.0;
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
bool imuHealthy = false;
float uprightX = 0, uprightY = 0, uprightZ = 1;

// Copy complete JSON snapshots to the network task. Sensor/state variables
// remain owned by loop(); the network task never reads them directly.
struct TelemetryPacket {
  char helmet[512];
  char bike[96];
};
QueueHandle_t telemetryQueue = nullptr;
float batteryPercent = 0;

unsigned long accidentStartedAt = 0;
unsigned long lastTelemetryAt = 0;
unsigned long lastStartPressAt = 0;
bool previousStartButton = HIGH;

String accidentStateName() {
  switch (accidentState) {
    case ACCIDENT_PENDING: return "pending";
    case ACCIDENT_CANCELLED: return "cancelled";
    case ACCIDENT_CONFIRMED: return "confirmed";
    default: return "normal";
  }
}

// Non-blocking buzzer: later notifications replace an unfinished pattern.
uint8_t beepsRemaining = 0;
bool buzzerOn = false;
uint16_t beepOnMs = 120;
unsigned long buzzerChangedAt = 0;

void beep(uint8_t count, uint16_t onMs = 120) {
  beepsRemaining = count;
  beepOnMs = onMs;
  buzzerOn = count > 0;
  buzzerChangedAt = millis();
  digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
}

void updateBuzzer() {
  if (beepsRemaining == 0) return;
  unsigned long now = millis();
  if (now - buzzerChangedAt < (buzzerOn ? beepOnMs : 100UL)) return;
  buzzerChangedAt = now;
  if (buzzerOn) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
    --beepsRemaining;
  } else {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOn = true;
  }
}

// Run at boot with the sensor fixed in its normal riding orientation.
// This measures a gravity reference; it is not an accelerometer bias calibration.
bool calibrateUpright() {
  float sx = 0, sy = 0, sz = 0;
  float firstX = 0, firstY = 0, firstZ = 0;
  for (int i = 0; i < 100; ++i) {
    sensors_event_t a, g, t;
    if (!mpu.getEvent(&a, &g, &t)) return false;
    float x = a.acceleration.x / 9.80665f;
    float y = a.acceleration.y / 9.80665f;
    float z = a.acceleration.z / 9.80665f;
    float magnitude = sqrtf(x*x + y*y + z*z);
    float rotation = sqrtf(g.gyro.x*g.gyro.x + g.gyro.y*g.gyro.y + g.gyro.z*g.gyro.z);
    if (!isfinite(magnitude) || !isfinite(rotation) ||
        magnitude < 0.8f || magnitude > 1.2f || rotation > 0.2f) return false;
    if (i == 0) { firstX = x; firstY = y; firstZ = z; }
    float dx = x-firstX, dy = y-firstY, dz = z-firstZ;
    if (sqrtf(dx*dx + dy*dy + dz*dz) > 0.15f) return false;
    sx += x; sy += y; sz += z;
    delay(20);
  }
  float magnitude = sqrtf(sx*sx + sy*sy + sz*sz);
  if (magnitude < 1.0f) return false;
  uprightX = sx/magnitude; uprightY = sy/magnitude; uprightZ = sz/magnitude;
  imuHealthy = true;
  Serial.printf("[MPU] Upright reference ready: %.3f, %.3f, %.3f\n",
                uprightX, uprightY, uprightZ);
  return true;
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

void readSafetyInputs() {
  helmetWorn = digitalRead(HELMET_SWITCH_PIN) == LOW;
  alcoholDetected = analogRead(MQ3_ANALOG_PIN) >= MQ3_THRESHOLD;
  ignitionAllowed = helmetWorn && !alcoholDetected && imuHealthy &&
                    accidentState == ACCIDENT_NORMAL;

  // Immediately stop the prototype motor if either safety condition fails.
  if (!ignitionAllowed) bikeRunning = false;

  digitalWrite(MOTOR_ENABLE_PIN, !bikeRunning ? HIGH : LOW);
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
      bikeRunning = true;
      Serial.println("[BIKE] Started: helmet and alcohol checks passed");
      beep(2);
    } else {
      Serial.println("[BIKE] Start blocked: check helmet/alcohol, MPU and accident re-arm status");
      beep(3, 220);
    }

    digitalWrite(MOTOR_ENABLE_PIN, !bikeRunning ? HIGH : LOW);
  }

  previousStartButton = currentButton;
}

// Only setup()/loop() access simSerial. The web task never accesses the modem.
String gsmIdleLine;

void printGsmLine(String line) {
  line.trim();
  if (line.length() > 0) {
    Serial.print("[GSM RX] ");
    Serial.println(line);
  }
}

// Show unsolicited and late responses even when no SMS is being sent.
void pollGsmResponses() {
  while (simSerial.available()) {
    char c = (char)simSerial.read();
    if (c == '\n') {
      printGsmLine(gsmIdleLine);
      gsmIdleLine = "";
    } else if (c != '\r') {
      gsmIdleLine += c;
      if (gsmIdleLine.length() >= 256) {
        printGsmLine(gsmIdleLine);
        gsmIdleLine = "";
      }
    }
  }
}

void gsmPause(unsigned long durationMs) {
  unsigned long started = millis();
  while (millis() - started < durationMs) {
    pollGsmResponses();
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    updateBuzzer();
    delay(1);
  }
}

// Commands need a complete OK line; SMS submission needs BOTH +CMGS and OK.
// A '>' prompt is not newline-terminated, so handle it separately.
bool waitGsmResponse(String& response, unsigned long timeoutMs,
                     bool expectPrompt, bool expectSmsReference) {
  response = "";
  String line;
  bool gotSmsReference = false;
  unsigned long started = millis();
  while (millis() - started < timeoutMs) {
    while (simSerial.available()) {
      char c = (char)simSerial.read();
      if (response.length() < 2048) response += c;
      if (expectPrompt && c == '>') {
        Serial.println("[GSM RX] > (ready for SMS text)");
        return true;
      }
      if (c == '\n') {
        line.trim();
        printGsmLine(line);
        if (line == "ERROR" || line.startsWith("+CMS ERROR:") ||
            line.startsWith("+CME ERROR:")) {
          Serial.println("[GSM ERROR] Command rejected; see modem response above.");
          return false;
        }
        if (line.startsWith("+CMGS:")) gotSmsReference = true;
        if (line == "OK" && !expectPrompt) {
          if (!expectSmsReference || gotSmsReference) return true;
          Serial.println("[GSM ERROR] OK received without +CMGS; SMS submission is unconfirmed.");
          return false;
        }
        line = "";
      } else if (c != '\r') {
        line += c;
        if (line.length() > 256) line.remove(0, 128);
      }
    }
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    updateBuzzer();
    delay(1);
  }
  printGsmLine(line);
  Serial.printf("[GSM TIMEOUT] Expected response not completed within %lu ms.\n", timeoutMs);
  return false;
}

bool sendGsmAT(const String& command, String& response, unsigned long timeoutMs) {
  // Log already-buffered data so it cannot satisfy this new command.
  pollGsmResponses();
  if (gsmIdleLine.length() > 0) {
    printGsmLine(gsmIdleLine);
    gsmIdleLine = "";
  }
  Serial.print("[GSM TX] ");
  Serial.println(command);
  simSerial.print(command);
  simSerial.print("\r");
  return waitGsmResponse(response, timeoutMs, false, false);
}

int gsmRegistrationStatus(const String& response) {
  int start = response.indexOf("+CREG:");
  if (start < 0) return -1;
  String line = response.substring(start + 6);
  int mode = -1, status = -1;
  int fields = sscanf(line.c_str(), " %d , %d", &mode, &status);
  return fields == 2 ? status : -1; // AT+CREG? returns <n>,<stat>.
}

bool initializeGsm() {
  String response;
  Serial.println("[GSM] Checking modem at 9600 baud...");
  bool responsive = false;
  for (int attempt = 1; attempt <= 10; ++attempt) {
    Serial.printf("[GSM] AT attempt %d/10\n", attempt);
    if (sendGsmAT("AT", response, 1500)) { responsive = true; break; }
    gsmPause(1000);
  }
  if (!responsive) {
    Serial.println("[GSM ERROR] No AT reply. Check GSM power, common GND, TX/RX wiring and baud rate.");
    return false;
  }
  if (!sendGsmAT("ATE0", response, 2000)) return false;
  if (!sendGsmAT("AT+CMEE=2", response, 2000)) return false;
  if (!sendGsmAT("ATI", response, 2000)) return false;

  bool simReady = false;
  unsigned long started = millis();
  while (millis() - started < 20000UL) {
    if (sendGsmAT("AT+CPIN?", response, 2000)) {
      if (response.indexOf("+CPIN: READY") >= 0) { simReady = true; break; }
      if (response.indexOf("SIM PIN") >= 0 || response.indexOf("SIM PUK") >= 0) {
        Serial.println("[GSM ERROR] SIM is locked. Unlock the SIM before testing.");
        return false;
      }
    }
    gsmPause(1000);
  }
  if (!simReady) {
    Serial.println("[GSM ERROR] SIM not ready. Check the CPIN/error responses above.");
    return false;
  }

  Serial.println("[GSM] Waiting up to 60 seconds for network registration...");
  started = millis();
  bool registered = false;
  while (millis() - started < GSM_NETWORK_WAIT_MS) {
    if (!sendGsmAT("AT+CSQ", response, 2000)) return false;
    if (!sendGsmAT("AT+CREG?", response, 2000)) return false;
    int status = gsmRegistrationStatus(response);
    Serial.printf("[GSM] Registration=%d (1=home, 5=roaming, 2=searching, 3=denied)\n", status);
    if (status == 1 || status == 5) { registered = true; break; }
    if (status == 3) {
      Serial.println("[GSM ERROR] Network registration denied.");
      return false;
    }
    gsmPause(2000);
  }
  if (!registered) {
    Serial.println("[GSM ERROR] Network registration timed out; SMS not attempted.");
    return false;
  }
  if (!sendGsmAT("AT+COPS?", response, 2000)) return false;
  if (!sendGsmAT("AT+CSCA?", response, 2000)) return false;
  Serial.println("[GSM] SIM ready and registered on network.");
  return true;
}

bool sendSms(const String& message, const char* label) {
  String response;
  Serial.printf("[GSM] %s SMS to %s\n", label, EMERGENCY_PHONE);
  if (!sendGsmAT("AT+CMGF=1", response, 2000)) return false;
  if (!sendGsmAT("AT+CSCS=\"GSM\"", response, 2000)) return false;
  pollGsmResponses();
  String command = String("AT+CMGS=\"") + EMERGENCY_PHONE + "\"";
  Serial.print("[GSM TX] ");
  Serial.println(command);
  simSerial.print(command);
  simSerial.print("\r");
  if (!waitGsmResponse(response, 10000UL, true, false)) {
    simSerial.write(27); // ESC cancels text entry if a late prompt arrives.
    Serial.println("[GSM ERROR] No SMS text prompt; message body was not sent.");
    gsmPause(500);
    return false;
  }
  Serial.print("[GSM TX BODY] ");
  Serial.println(message);
  simSerial.print(message);
  simSerial.write(26); // Ctrl+Z submits the message.
  Serial.println("[GSM TX] Ctrl+Z; waiting for +CMGS and OK...");
  bool accepted = waitGsmResponse(response, GSM_SMS_WAIT_MS, false, true);
  if (accepted) {
    Serial.printf("[GSM SUCCESS] %s SMS submission confirmed (+CMGS and OK).\n", label);
    Serial.println("[GSM] Check the receiving phone to confirm delivery.");
  } else {
    Serial.printf("[GSM ERROR] %s SMS submission failed or is unconfirmed; no automatic resend.\n", label);
  }
  return accepted;
}

void sendStartupSMS() {
  // Intentionally does not change smsSent: that flag belongs to accident SMS.
  Serial.println("[GSM] Starting project startup SMS check...");
  if (!initializeGsm()) {
    Serial.println("[GSM STARTUP] GSM check failed. Continuing project without startup SMS.");
    return;
  }
  bool accepted = sendSms("SmartRide project started. ESP32 is powered ON. This is the GSM startup test message.", "STARTUP");
  Serial.println(accepted ? "[GSM STARTUP] Test SMS submitted successfully."
                          : "[GSM STARTUP] Test SMS not confirmed. Check the GSM responses above.");
}

void sendEmergencySMS() {
  smsSent = false;
  if (!initializeGsm()) {
    Serial.println("[ACCIDENT] GSM not ready; emergency SMS was not sent.");
    return;
  }
  String location = "GPS location unavailable";
  if (gps.location.isValid()) {
    location = "https://maps.google.com/?q=" +
               String(gps.location.lat(), 6) + "," +
               String(gps.location.lng(), 6);
  }
  else{
    location = "https://maps.app.goo.gl/DhKfFVuRoFbJivby7";
  }
  smsSent = sendSms(String("SmartRide emergency: possible rider accident. Location: ") + location,
                    "EMERGENCY");
}

void handleAccidentDetection() {
  static unsigned long lastSampleAt = 0;
  static unsigned long lastDebugAt = 0;
  static unsigned long tiltStartedAt = 0;
  static unsigned long resetHoldStarted = 0;
  static bool tiltTiming = false;
  static bool resetTiming = false;
  static float peakG = 0;
  unsigned long now = millis();
  if (now - lastSampleAt < MPU_SAMPLE_MS) return;
  unsigned long sampleGap = now - lastSampleAt;
  lastSampleAt = now;
  // Never count an unobserved pause as continuous tilt or reset-button hold.
  if (sampleGap > 100) { tiltTiming = false; resetTiming = false; }

  sensors_event_t acceleration = {}, gyro = {}, temperature = {};
  bool readOk = mpu.getEvent(&acceleration, &gyro, &temperature);
  float ax = acceleration.acceleration.x / 9.80665f;
  float ay = acceleration.acceleration.y / 9.80665f;
  float az = acceleration.acceleration.z / 9.80665f;
  float totalG = sqrtf(ax*ax + ay*ay + az*az);
  if (!readOk || !isfinite(totalG)) {
    imuHealthy = false;
    bikeRunning = false;
    digitalWrite(MOTOR_ENABLE_PIN, HIGH);
    tiltTiming = false;
    resetTiming = false;
    if (now - lastDebugAt >= MPU_DEBUG_MS) {
      lastDebugAt = now;
      Serial.println("[MPU ERROR] Invalid reading; check sensor wiring/power.");
    }
    return;
  }
  imuHealthy = true;
  if (totalG > peakG) peakG = totalG;

  // Angle to startup gravity reference: 0 = upright, 90 = sideways,
  // 180 = inverted. Do not use fabs(az), which folds inversion back to 0.
  float tiltDeg = 0;
  bool tiltValid = totalG >= 0.5f && totalG <= 1.5f;
  if (totalG > 0.01f) {
    float cosine = (ax*uprightX + ay*uprightY + az*uprightZ) / totalG;
    cosine = constrain(cosine, -1.0f, 1.0f);
    tiltDeg = acosf(cosine) * 180.0f / PI;
  }

  if (accidentState == ACCIDENT_NORMAL) {
    bool impact = totalG >= IMPACT_G_THRESHOLD;
    if (tiltValid && tiltDeg >= TILT_THRESHOLD_DEG) {
      if (!tiltTiming) { tiltStartedAt = now; tiltTiming = true; }
    } else {
      tiltTiming = false;
    }
    bool sustainedTilt = tiltTiming && now - tiltStartedAt >= TILT_HOLD_MS;
    if (impact || sustainedTilt) {
      accidentState = ACCIDENT_PENDING;
      accidentStartedAt = now;
      smsSent = false;
      bikeRunning = false;
      digitalWrite(MOTOR_ENABLE_PIN, HIGH);
      tiltTiming = false;
      Serial.printf("[ACCIDENT] Trigger=%s G=%.2f tilt=%.1f. Cancel within 120 seconds.\n",
                    impact ? "IMPACT" : "TILT", totalG, tiltDeg);
      beep(4, 180);
    }
  } else {
    tiltTiming = false;
  }

  bool cancelPressed = digitalRead(CANCEL_BUTTON_PIN) == LOW;
  if (accidentState == ACCIDENT_PENDING && cancelPressed) {
    accidentState = ACCIDENT_CANCELLED;
    Serial.println("[ACCIDENT] Cancelled. Return upright and hold Cancel 3 seconds to re-arm.");
    beep(2);
  }
  if (accidentState == ACCIDENT_PENDING && now - accidentStartedAt >= ACCIDENT_CANCEL_MS) {
    accidentState = ACCIDENT_CONFIRMED;
    Serial.println("[ACCIDENT] Confirmed. Attempting SMS; return upright and hold Cancel to re-arm.");
    sendEmergencySMS(); // Waits for GSM; motor remains OFF in confirmed state.
    beep(8, 200);
    return;
  }

  bool resettable = accidentState == ACCIDENT_CANCELLED || accidentState == ACCIDENT_CONFIRMED;
  bool upright = tiltValid && tiltDeg < REARM_TILT_DEG && totalG >= 0.8f && totalG <= 1.2f;
  if (resettable && cancelPressed && upright) {
    if (!resetTiming) { resetTiming = true; resetHoldStarted = now; }
    if (now - resetHoldStarted >= 3000) {
      accidentState = ACCIDENT_NORMAL;
      resetTiming = false;
      smsSent = false;
      peakG = totalG;
      Serial.println("[ACCIDENT] Detection ARMED. Ready for another test; motor remains OFF.");
      beep(1);
    }
  } else {
    resetTiming = false;
  }

  if (now - lastDebugAt >= MPU_DEBUG_MS) {
    lastDebugAt = now;
    unsigned long remaining = 0;
    if (accidentState == ACCIDENT_PENDING) {
      unsigned long elapsed = now - accidentStartedAt;
      if (elapsed < ACCIDENT_CANCEL_MS) remaining = (ACCIDENT_CANCEL_MS-elapsed+999)/1000;
    }
    Serial.printf("[MPU] G=%.2f peak=%.2f tilt=%.1f valid=%d state=%s cancel=%d left=%lus\n",
                  totalG, peakG, tiltDeg, tiltValid, accidentStateName().c_str(), cancelPressed, remaining);
    if (resettable) Serial.println("[MPU] NOT ARMED: return upright, hold Cancel for 3 seconds.");
    peakG = totalG;
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

void telemetryTask(void* parameter) {
  TelemetryPacket packet;
  for (;;) {
    if (xQueueReceive(telemetryQueue, &packet, portMAX_DELAY) == pdTRUE) {
      int helmetStatus = postJson("/api/device/helmet", String(packet.helmet));
      int bikeStatus = postJson("/api/device/bike", String(packet.bike));
      Serial.printf("[WEB] helmet=%d bike=%d\n", helmetStatus, bikeStatus);
    }
  }
}

void postTelemetry() {
  if (telemetryQueue == nullptr || !hasClient()) return; // PC/phone not on the hotspot yet - safety logic keeps running regardless

  StaticJsonDocument<512> helmetDoc;
  helmetDoc["helmet_worn"] = helmetWorn;
  helmetDoc["alcohol_detected"] = alcoholDetected;
  helmetDoc["battery"] = batteryPercent;
  helmetDoc["accident_status"] = accidentStateName();
  // No accident_deadline field: the ESP32 has no internet for NTP, so it
  // can't compute a real timestamp. Flask fills this in from its own clock.
  helmetDoc["sms_status"] = smsSent ? "SMS submission confirmed (delivery not verified)" :
      (accidentState == ACCIDENT_CONFIRMED ? "SMS not confirmed; check GSM Serial log" : "");
  helmetDoc["speed"] = gps.speed.isValid() ? gps.speed.kmph() : 0;

  if (gps.location.isValid()) {
    helmetDoc["latitude"] = gps.location.lat();
    helmetDoc["longitude"] = gps.location.lng();
  }else {
    helmetDoc["latitude"] = 19.0864505;
    helmetDoc["longitude"] = 72.8279212;
  }

  TelemetryPacket packet = {};
  serializeJson(helmetDoc, packet.helmet, sizeof(packet.helmet));

  StaticJsonDocument<96> bikeDoc;
  bikeDoc["bike_running"] = bikeRunning;
  serializeJson(bikeDoc, packet.bike, sizeof(packet.bike));
  xQueueOverwrite(telemetryQueue, &packet);
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

  digitalWrite(MOTOR_ENABLE_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);

  analogReadResolution(12);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!mpu.begin()) {
    Serial.println("[ERROR] MPU6050 not found. Check SDA/SCL and power.");
    while (true) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(500);
      digitalWrite(BUZZER_PIN, LOW);
      delay(500);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
  Serial.println("[MPU] Keep still in NORMAL RIDING POSITION for upright calibration (2 seconds).");
  delay(500);
  while (!calibrateUpright()) {
    Serial.println("[MPU] Calibration rejected: keep the sensor still in its normal position. Retrying...");
    delay(500);
  }
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  simSerial.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

  startAccessPoint();
  // Startup waits for GSM with the motor OFF. This attempt happens once per boot.
  if (SEND_STARTUP_SMS) sendStartupSMS();
  telemetryQueue = xQueueCreate(1, sizeof(TelemetryPacket));
  if (telemetryQueue != nullptr) {
    if (xTaskCreate(telemetryTask, "telemetry", 8192, nullptr, 1, nullptr) != pdPASS) {
      vQueueDelete(telemetryQueue);
      telemetryQueue = nullptr;
    }
  }
  if (telemetryQueue == nullptr) Serial.println("[WEB ERROR] Background task unavailable; telemetry disabled.");
  readSafetyInputs();
  beep(2);

  Serial.println("[READY] SmartRide single-ESP32 controller started");
}

void loop() {
  pollGsmResponses();
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  updateBuzzer();
  handleAccidentDetection();
  readSafetyInputs();
  handleStartButton();
  updateBatteryReading();

  if (millis() - lastTelemetryAt >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryAt = millis();
    postTelemetry();
  }

  delay(1);
}
