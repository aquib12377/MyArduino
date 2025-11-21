#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// ------------------ USER CONFIG ------------------
const char* PHONE_NUMBER = "+918591394504";   // <-- change to destination number
const unsigned long GPS_WAIT_MS = 5000;      // wait up to 15s for a fix
const double DEFAULT_LAT = 19.0760;           // <-- your fallback latitude
const double DEFAULT_LON = 72.8777;           // <-- your fallback longitude
const bool BUTTON_ACTIVE_LOW = true;

// Pins
const uint8_t PIN_BTN   = 2;   // button with INPUT_PULLUP
const uint8_t SIM_RX    = 7;   // Nano -> SIM TX (Nano's TX pin for SoftwareSerial is second arg)
const uint8_t SIM_TX    = 8;   // Nano <- SIM RX
const uint8_t GPS_RX    = 3;   // Nano -> GPS TX
const uint8_t GPS_TX    = 4;   // Nano <- GPS RX

// -------------------------------------------------

// SoftwareSerial(rxPin, txPin): from the Arduino's perspective
SoftwareSerial simSS(SIM_RX, SIM_TX); // to GSM (SIM800/900)
SoftwareSerial gpsSS(GPS_RX, GPS_TX); // to GPS (NEO-6M)

TinyGPSPlus gps;

enum ButtonState { BTN_IDLE, BTN_DEBOUNCE, BTN_PRESSED };
ButtonState btnState = BTN_IDLE;
unsigned long lastBtnChange = 0;
const unsigned long DEBOUNCE_MS = 40;

bool waitForGSMRegistration(unsigned long timeoutMs);
bool sendSMS(const char* number, const String& body);
bool getGPSFixWithin(double &lat, double &lon, unsigned long timeoutMs);
void flushSIM();
String buildMessage(double lat, double lon, bool isFallback);

void setup() {
  pinMode(PIN_BTN, INPUT_PULLUP);
  Serial.begin(115200);
  simSS.begin(9600); // SIM800 default (often 9600)
  gpsSS.begin(9600); // NEO-6M default (typically 9600)

  Serial.println(F("\n--- Nano: SMS with GPS or fallback ---"));
  Serial.println(F("Init GSM..."));

  // Basic GSM init
  flushSIM();
  simSS.println(F("AT"));
  delay(200);
  flushSIM();
  simSS.println(F("ATE0")); // echo off
  delay(200);
  flushSIM();
  simSS.println(F("AT+CMGF=1")); // SMS text mode
  delay(200);
  flushSIM();

  Serial.println(F("Setup done. Press the button to send SMS."));
}

void loop() {
  // --- Button FSM with debounce ---
  bool rawPressed = BUTTON_ACTIVE_LOW ? (digitalRead(PIN_BTN) == LOW) : (digitalRead(PIN_BTN) == HIGH);

  switch (btnState) {
    case BTN_IDLE:
      if (rawPressed) {
        btnState = BTN_DEBOUNCE;
        lastBtnChange = millis();
      }
      break;

    case BTN_DEBOUNCE:
      if (millis() - lastBtnChange >= DEBOUNCE_MS) {
        if (rawPressed) {
          btnState = BTN_PRESSED;
          // Handle press once
          handleButtonPress();
        } else {
          btnState = BTN_IDLE;
        }
      }
      break;

    case BTN_PRESSED:
      // wait for release
      if (!rawPressed) {
        btnState = BTN_IDLE;
      }
      break;
  }

  // Keep parsing GPS in background for better chances of quick fix
  while (gpsSS.available()) {
    gps.encode(gpsSS.read());
  }
}

void handleButtonPress() {
  Serial.println(F("\n[BTN] Press detected. Preparing SMS..."));

  gpsSS.listen();
  // 1) Try to get GPS fix quickly
  double lat = DEFAULT_LAT, lon = DEFAULT_LON;
  bool gotFix = getGPSFixWithin(lat, lon, GPS_WAIT_MS);
  if (gotFix) {
    Serial.print(F("[GPS] Fix OK: "));
    Serial.print(lat, 6); Serial.print(F(", "));
    Serial.println(lon, 6);
  } else {
    Serial.print(F("[GPS] No fix in ")); Serial.print(GPS_WAIT_MS); Serial.println(F(" ms. Using fallback."));
    Serial.print(F("[GPS] Fallback: ")); Serial.print(lat, 6); Serial.print(F(", ")); Serial.println(lon, 6);
  }

  // 2) Ensure GSM is registered
  Serial.println(F("[GSM] Waiting for network..."));
  if (!waitForGSMRegistration(5000)) {
    Serial.println(F("[GSM] Not registered to network. Will try to send anyway (may fail)."));
  } else {
    Serial.println(F("[GSM] Registered."));
  }

  // 3) Build & send SMS
  String body = buildMessage(lat, lon, !gotFix);
  Serial.println(F("[SMS] Sending..."));
  if (sendSMS(PHONE_NUMBER, body)) {
    Serial.println(F("[SMS] Sent successfully."));
  } else {
    Serial.println(F("[SMS] Failed to send."));
  }
}

String buildMessage(double lat, double lon, bool isFallback) {
  String msg = F("Location Update");
  if (isFallback) msg += F(" (Fallback)");
  msg += F(":\n");
  msg += F("Lat: "); msg += String(lat, 6); msg += F("\n");
  msg += F("Lon: "); msg += String(lon, 6); msg += F("\n");
  msg += F("Map: https://maps.app.goo.gl/RVWKskk9LHz2B4n79");
  return msg;
}

bool getGPSFixWithin(double &lat, double &lon, unsigned long timeoutMs) {
  unsigned long start = millis();
  bool fixed = false;

  while (millis() - start < timeoutMs) {
    while (gpsSS.available()) {
      gps.encode(gpsSS.read());
    }

    if (gps.location.isUpdated() && gps.location.isValid()) {
      // Optional quality checks: age < 2s, HDOP, satellites, etc.
      lat = gps.location.lat();
      lon = gps.location.lng();
      fixed = true;
      break;
    }
  }
  return fixed;
}

bool waitForGSMRegistration(unsigned long timeoutMs) {
  unsigned long start = millis();
  simSS.listen();

  while (millis() - start < timeoutMs) {
    flushSIM();
    simSS.println(F("AT+CREG?"));
    delay(300);

    // Read response
    bool regOK = false;
    while (simSS.available()) {
      String line = simSS.readStringUntil('\n');
      line.trim();
      // +CREG: <n>,<stat>
      // stat: 1=home, 5=roaming
      if (line.startsWith(F("+CREG:"))) {
        int comma = line.lastIndexOf(',');
        if (comma > 0 && comma < (int)line.length() - 1) {
          char stat = line.charAt(comma + 1);
          if (stat == '1' || stat == '5') regOK = true;
        }
      }
    }
    if (regOK) return true;

    delay(500);
  }
  return false;
}

bool sendSMS(const char* number, const String& body) {
  //flushSIM();
  simSS.listen();
  // Ensure text mode
  simSS.println(F("AT+CMGF=1"));
  delay(200);
  //flushSIM();

  simSS.print(F("AT+CMGS=\""));
  simSS.print(number);
  simSS.println(F("\""));
  delay(200); // wait for '>' prompt (not strictly checked to keep it simple)

  simSS.print(body);
  simSS.write(26); // Ctrl+Z to send
  //simSS.println();

  // Wait a bit for +CMGS ack
  unsigned long start = millis();
  bool ok = false;
  while (millis() - start < 10000) {
    if (simSS.available()) {
      String resp = simSS.readString();
      Serial.println(resp);
      // Look for indications of success
      if (resp.indexOf(F("+CMGS:")) >= 0 || resp.indexOf(F("OK")) >= 0) {
        ok = true;
        break;
      }
      // If you want, check for "ERROR" too
      if (resp.indexOf(F("ERROR")) >= 0) {
        ok = false;
        break;
      }
    }
  }
  return ok;
}

void flushSIM() {
  while (simSS.available()) (void)simSS.read();
}
