/*
  =========================================================
  ESP32 Fingerprint Attendance Management System
  =========================================================
  Hardware : ESP32 Dev Board, R307 Fingerprint Sensor, 16x2 I2C LCD

  How it works:
  - Normal mode: place a registered finger -> logs attendance to
    the "Attendance" sheet via a Google Apps Script Web App.
  - Every POLL_INTERVAL_MS, the ESP32 asks the sheet if a new
    name has been added to the "Users" sheet without a
    FingerprintID yet. If found, it shows "New User Found" and
    waits up to 30 seconds for a finger to be placed.
      -> If a finger is placed within 30s: runs the full 2-scan
         enrollment, stores the template on the sensor, and
         writes the new FingerprintID back to the sheet.
      -> If no finger is placed within 30s: it silently aborts
         and returns to normal attendance scanning (it will try
         asking about this same pending user again next poll).

  Libraries required (install via Arduino Library Manager):
    - Adafruit Fingerprint Sensor Library (by Adafruit)
    - LiquidCrystal I2C (by Frank de Brabander / John Rickman - "LiquidCrystal_I2C")
    - ArduinoJson (by Benoit Blanchon, v6.x)
    - WiFi / HTTPClient / WiFiClientSecure -> bundled with ESP32 board package

  Wiring:
    R307 fingerprint sensor:
      VCC  -> 3.3V or 5V (check your module's rating)
      GND  -> GND
      TX   -> ESP32 GPIO16 (RX2)
      RX   -> ESP32 GPIO17 (TX2)
        (use a logic-level/voltage divider on sensor RX if your
         module is 3.3V logic only and ESP32 outputs 3.3V - usually fine)

    16x2 I2C LCD:
      VCC  -> 5V
      GND  -> GND
      SDA  -> ESP32 GPIO21
      SCL  -> ESP32 GPIO22
      (Run an I2C scanner sketch first if unsure of the address;
       common addresses are 0x27 or 0x3F)
  =========================================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>

// ================== USER CONFIGURATION ==================
const char* WIFI_SSID     = "AAA";
const char* WIFI_PASSWORD = "12345678";

// Paste your deployed Apps Script Web App URL here (ends in /exec)
const char* GAS_URL = "https://script.google.com/macros/s/AKfycbwHjr28fpmSdRbLpKd41O_I10KG7b0Ze3O-vFCctAulxedirZWId8pjlo4SqFtqpWqo_w/exec";

// LCD config - change address to 0x3F if 0x27 doesn't work
LiquidCrystal_I2C lcd(0x27, 16, 2);

// R307 on ESP32 hardware Serial2
HardwareSerial fingerSerial(2);
Adafruit_Fingerprint finger(&fingerSerial);

// ================== TIMING CONFIG ==================
unsigned long lastPollTime = 0;
const unsigned long POLL_INTERVAL_MS = 15000UL;  // check Users sheet every 15s
const unsigned long ENROLL_WAIT_MS   = 30000UL;  // wait 30s for finger to START enrollment
const unsigned long SECOND_SCAN_TIMEOUT_MS = 10000UL; // timeout for 2nd scan during enrollment

// =========================================================
//                         SETUP
// =========================================================
void setup() {
  Serial.begin(115200);

  Wire.begin(); // default SDA=21, SCL=22
  lcd.begin();
  lcd.backlight();
  lcdMsg("Attendance Sys", "Booting...");

  fingerSerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("R307 sensor detected OK");
  } else {
    Serial.println("R307 sensor NOT found. Check wiring/baud rate.");
    lcdMsg("Sensor Error!", "Check wiring");
    while (true) delay(1000);
  }

  connectWiFi();
  lcdMsg("System Ready", "Place Finger");
}

// =========================================================
//                        MAIN LOOP
// =========================================================
void loop() {
  // 1. Periodically check the Users sheet for a newly added,
  //    not-yet-enrolled user.
  if (millis() - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = millis();
    checkForPendingEnrollment();
  }

  // 2. Normal attendance scanning (non-blocking - runs every loop)
  handleAttendanceScan();

  delay(50);
}

// =========================================================
//                    WIFI / HTTP HELPERS
// =========================================================
void connectWiFi() {
  lcdMsg("Connecting WiFi", "");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    lcdMsg("WiFi Connected", WiFi.localIP().toString());
    delay(1000);
  } else {
    Serial.println("\nWiFi connection failed, will retry in loop");
    lcdMsg("WiFi Failed!", "Retrying...");
  }
}

// Simple GET request to the Apps Script Web App. Returns raw JSON body.
String httpGET(String url) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return "{\"status\":\"error\",\"message\":\"no wifi\"}";
  }

  WiFiClientSecure client;
  client.setInsecure(); // skip TLS cert validation (simplifies talking to script.google.com)

  HTTPClient https;
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Apps Script replies with a 302 first
  String payload = "{\"status\":\"error\",\"message\":\"request failed\"}";

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      payload = https.getString();
    } else {
      Serial.printf("HTTP GET failed, code: %d\n", httpCode);
    }
    https.end();
  } else {
    Serial.println("Unable to connect to GAS_URL");
  }
  return payload;
}

// URL-encodes a string (needed for names with spaces/special chars)
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  char code0, code1;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum((unsigned char)c)) {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// =========================================================
//                       LCD HELPER
// =========================================================
void lcdMsg(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

// =========================================================
//              CHECK GOOGLE SHEET FOR NEW USER
// =========================================================
void checkForPendingEnrollment() {
  String url = String(GAS_URL) + "?action=checkPending";
  String response = httpGET(url);

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.println("JSON parse error (checkPending): " + String(err.c_str()));
    return;
  }

  String status = doc["status"] | "none";
  if (status == "pending") {
    int row = doc["row"] | 0;
    String name = doc["name"] | "";
    if (row > 0 && name.length() > 0) {
      startEnrollmentFlow(name, row);
    }
  }
}

// =========================================================
//                    ENROLLMENT FLOW
// =========================================================
void startEnrollmentFlow(String name, int row) {
  Serial.println("New pending user: " + name + " (row " + String(row) + ")");
  lcdMsg("New User Found:", name);
  delay(1500);
  lcdMsg("Place finger to", "enroll (30s)");

  unsigned long waitStart = millis();
  bool fingerDetected = false;

  // Wait up to ENROLL_WAIT_MS for the FIRST finger placement.
  // If nothing happens in this window, we abandon enrollment for
  // now and fall back to normal attendance scanning.
  while (millis() - waitStart < ENROLL_WAIT_MS) {
    int p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      fingerDetected = true;
      break; // image 1 is now captured in the sensor buffer
    }
    delay(200);
  }

  if (!fingerDetected) {
    Serial.println("No finger placed within 30s - resuming normal attendance.");
    lcdMsg("Enrollment", "Timed Out");
    delay(1500);
    lcdMsg("System Ready", "Place Finger");
    return;
  }

  int newId = getNextFingerprintID();
  bool success = enrollFingerprint(newId);

  if (success) {
    String url = String(GAS_URL) + "?action=updateEnrollment&row=" + String(row) +
                 "&fpid=" + String(newId) + "&name=" + urlEncode(name);
    String response = httpGET(url);
    Serial.println("Update response: " + response);

    lcdMsg("Enrolled!", "ID: " + String(newId));
    delay(2000);
  } else {
    lcdMsg("Enrollment", "Failed. Retry");
    delay(2000);
  }

  lcdMsg("System Ready", "Place Finger");
}

// Picks the next fingerprint ID slot.
// NOTE: assumes sequential enrollment with no deletions in between.
// For heavier use, consider tracking used IDs explicitly (e.g. store
// the highest used ID in the sheet itself and read it back).
int getNextFingerprintID() {
  finger.getTemplateCount();
  return finger.templateCount + 1;
}

// Runs the standard Adafruit two-scan enrollment sequence.
// Assumes image #1 has ALREADY been captured by the caller
// (the finger that triggered startEnrollmentFlow's wait loop).
bool enrollFingerprint(int id) {
  int p;

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("image2Tz(1) failed, code=" + String(p));
    return false;
  }

  lcdMsg("Remove finger", "");
  delay(1500);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    delay(50);
  }

  lcdMsg("Place same", "finger again");
  unsigned long t0 = millis();
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (millis() - t0 > SECOND_SCAN_TIMEOUT_MS) {
      Serial.println("Timeout waiting for second scan");
      return false;
    }
    delay(100);
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("image2Tz(2) failed, code=" + String(p));
    return false;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("createModel failed - the two scans didn't match, code=" + String(p));
    lcdMsg("Fingers didn't", "match. Retry");
    delay(1500);
    return false;
  }

  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    Serial.println("storeModel failed, code=" + String(p));
    return false;
  }

  Serial.println("Enrollment success, ID = " + String(id));
  return true;
}

// =========================================================
//                 NORMAL ATTENDANCE SCANNING
// =========================================================
void handleAttendanceScan() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return; // no finger present / read error, just try again next loop

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    lcdMsg("Not Recognized", "Try Again");
    delay(1500);
    lcdMsg("System Ready", "Place Finger");
    return;
  }

  int matchedID = finger.fingerID;
  Serial.println("Match found, ID = " + String(matchedID));
  lcdMsg("Verifying...", "ID: " + String(matchedID));

  String response = sendLogAttendance(matchedID);

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, response);

  if (!err && String((const char*)(doc["status"] | "")) == "success") {
    String name = doc["name"] | "User";
    lcdMsg("Welcome!", name);
  } else {
    String msg = doc["message"] | "Log failed";
    lcdMsg("Error:", msg);
  }

  delay(2000);
  lcdMsg("System Ready", "Place Finger");
}

String sendLogAttendance(int fpid) {
  String url = String(GAS_URL) + "?action=logAttendance&fpid=" + String(fpid);
  return httpGET(url);
}
