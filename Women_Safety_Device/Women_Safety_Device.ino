/*
 * ============================================================
 *  WOMEN SAFETY DEVICE — Arduino Nano  (Toggle Edition)
 * ============================================================
 *  Hardware:
 *    - Arduino Nano
 *    - SIM900A GSM Module  → TX/RX on pins 2, 3
 *    - NEO-6M GPS Module   → TX/RX on pins 4, 5
 *    - Buzzer              → pin 12
 *    - Call Button         → pin 8  (INPUT_PULLUP, active LOW)
 *    - SMS  Button         → pin 9  (INPUT_PULLUP, active LOW)
 *    - Buzzer Button       → pin 10 (INPUT_PULLUP, active LOW)
 *
 *  Button behaviour (all three are TOGGLE):
 *    Press once → action starts (ON)
 *    Press again → action stops (OFF)
 *    Only the transition from un-pressed → pressed is detected,
 *    so holding the button does nothing extra.
 *
 *  Wiring notes:
 *    - Connect each button between its pin and GND (no external
 *      resistor needed — internal pull-ups are enabled).
 *    - SIM900A TX → Nano pin 2,  RX → Nano pin 3 (via divider)
 *    - NEO-6M  TX → Nano pin 4,  RX → Nano pin 5
 *    - Power SIM900A with a separate 5 V / 2 A supply.
 *
 *  ⚠  Replace the placeholder phone number below.
 * ============================================================
 */

#include <SoftwareSerial.h>

// ---------------------- PIN DEFINITIONS ----------------------
#define GSM_TX_PIN   2
#define GSM_RX_PIN   3
#define GPS_TX_PIN   4
#define GPS_RX_PIN   5

#define BTN_CALL     8
#define BTN_SMS      9
#define BTN_BUZZER   10
#define BUZZER_PIN   12

// ------------------- CONFIGURATION ---------------------------
const char EMERGENCY_NUMBER[] = "+91XXXXXXXXXX";   // ← CHANGE THIS

const unsigned long DEBOUNCE_MS        = 50;
const unsigned long GPS_TIMEOUT_MS     = 2000;
const unsigned long SMS_REPEAT_INTERVAL = 30000;   // Re-send SMS every 30 s while ON

// ------------------- SERIAL PORTS ----------------------------
SoftwareSerial gsmSerial(GSM_TX_PIN, GSM_RX_PIN);
SoftwareSerial gpsSerial(GPS_TX_PIN, GPS_RX_PIN);

// ------------------- GPS DATA --------------------------------
float gpsLat   = 0.0;
float gpsLng   = 0.0;
bool  gpsValid = false;

// ------------------- BUTTON STATE ----------------------------
//  prev   = last raw reading (for edge detection)
//  state  = current toggle state (ON / OFF)
//  lastDb = last time a valid edge was registered (debounce)

bool prevCall   = HIGH,  stateCall   = false;
bool prevSms    = HIGH,  stateSms    = false;
bool prevBuzzer = HIGH,  stateBuzzer = false;

unsigned long lastDbCall   = 0;
unsigned long lastDbSms    = 0;
unsigned long lastDbBuzzer = 0;

// ------------------- SMS TIMING ------------------------------
unsigned long lastSmsSentAt = 0;

// =============================================================
//  EDGE-DETECT HELPER
//  Returns true ONCE per button press (falling edge, debounced).
//  Also toggles the associated state variable.
// =============================================================
bool toggleOnPress(int pin, bool &prev, bool &state, unsigned long &lastDb) {
  bool reading = digitalRead(pin);
  bool toggled = false;

  // Falling edge: was HIGH, now LOW (button just pressed)
  if (prev == HIGH && reading == LOW && (millis() - lastDb > DEBOUNCE_MS)) {
    state  = !state;       // Flip the toggle
    toggled = true;
    lastDb  = millis();
  }

  prev = reading;          // Remember for next iteration
  return toggled;
}

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(9600);

  pinMode(BTN_CALL,   INPUT_PULLUP);
  pinMode(BTN_SMS,    INPUT_PULLUP);
  pinMode(BTN_BUZZER, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // GSM init
  gsmSerial.begin(9600);
  delay(1000);
  gsmSendAT("AT",        1000);
  gsmSendAT("AT+CMGF=1", 1000);   // SMS text mode
  gsmSendAT("ATE0",       500);   // Echo off

  Serial.println(F("=== Women Safety Device Ready ==="));
  Serial.println(F("Press 8->CALL  |  9->SMS  |  10->BUZZER"));
  Serial.println(F("Each button toggles ON / OFF on every press."));
}

// =============================================================
//  MAIN LOOP
// =============================================================
void loop() {

  // ---------- CALL toggle (pin 8) ----------------------------
  if (toggleOnPress(BTN_CALL, prevCall, stateCall, lastDbCall)) {
    if (stateCall) {
      Serial.println(F("[CALL] ON  -> dialling..."));
      makeCall();
    } else {
      Serial.println(F("[CALL] OFF -> hanging up..."));
      hangUp();
    }
  }

  // ---------- SMS toggle (pin 9) -----------------------------
  if (toggleOnPress(BTN_SMS, prevSms, stateSms, lastDbSms)) {
    if (stateSms) {
      Serial.println(F("[SMS] ON  -> sending first SMS..."));
      updateGPS();
      sendSMS();
      lastSmsSentAt = millis();
    } else {
      Serial.println(F("[SMS] OFF -> stopped"));
    }
  }

  // While SMS state is ON, keep re-sending at intervals
  if (stateSms && (millis() - lastSmsSentAt >= SMS_REPEAT_INTERVAL)) {
    Serial.println(F("[SMS] Re-sending location update..."));
    updateGPS();
    sendSMS();
    lastSmsSentAt = millis();
  }

  // ---------- BUZZER toggle (pin 10) -------------------------
  if (toggleOnPress(BTN_BUZZER, prevBuzzer, stateBuzzer, lastDbBuzzer)) {
    Serial.print(F("[BUZZER] "));
    Serial.println(stateBuzzer ? F("ON") : F("OFF"));
  }

  // Drive buzzer siren while ON
  if (stateBuzzer) {
    tone(BUZZER_PIN, 3500);
    delay(120);
    tone(BUZZER_PIN, 4500);
    delay(120);
  } else {
    noTone(BUZZER_PIN);
  }
}

// =============================================================
//  GPS — Read a fresh GPRMC sentence and parse lat / lng
// =============================================================
void updateGPS() {
  Serial.println(F("[GPS] Reading..."));
  gpsValid = false;

  gpsSerial.begin(9600);
  delay(100);

  while (gpsSerial.available()) gpsSerial.read();   // Flush

  unsigned long start = millis();
  String sentence = "";

  while (millis() - start < GPS_TIMEOUT_MS) {
    while (gpsSerial.available()) {
      char c = gpsSerial.read();
      if (c == '$') sentence = "$";
      else          sentence += c;

      if (c == '\n') {
        if (sentence.startsWith("$GPRMC") || sentence.startsWith("$GNRMC")) {
          if (parseGPRMC(sentence)) {
            gpsSerial.end();
            return;
          }
        }
        sentence = "";
      }
    }
  }
  gpsSerial.end();
  Serial.println(F("[GPS] No valid fix within timeout"));
}

bool parseGPRMC(String s) {
  int idx = 0;
  String fields[13];
  int sp = 0;

  for (unsigned int i = 0; i < s.length() && idx < 13; i++) {
    if (s[i] == ',' || s[i] == '*') {
      fields[idx++] = s.substring(sp, i);
      sp = i + 1;
    }
  }

  if (idx < 7 || fields[2] != "A") return false;

  float rawLat = fields[3].toFloat();
  int   latDeg = (int)(rawLat / 100);
  gpsLat = latDeg + (rawLat - latDeg * 100) / 60.0;
  if (fields[4] == "S") gpsLat = -gpsLat;

  float rawLng = fields[5].toFloat();
  int   lngDeg = (int)(rawLng / 100);
  gpsLng = lngDeg + (rawLng - lngDeg * 100) / 60.0;
  if (fields[6] == "W") gpsLng = -gpsLng;

  gpsValid = true;
  Serial.print(F("[GPS] Fix: "));
  Serial.print(gpsLat, 6); Serial.print(F(", "));
  Serial.println(gpsLng, 6);
  return true;
}

// =============================================================
//  GSM — Voice call
// =============================================================
void makeCall() {
  gsmSerial.begin(9600);
  delay(300);
  String cmd = "ATD";
  cmd += EMERGENCY_NUMBER;
  cmd += ";";
  gsmSendAT(cmd.c_str(), 5000);
  Serial.println(F("[GSM] Call initiated"));
}

void hangUp() {
  gsmSerial.begin(9600);
  delay(300);
  gsmSendAT("ATH", 2000);
  Serial.println(F("[GSM] Call ended"));
}

// =============================================================
//  GSM — SMS with Google Maps link
// =============================================================
void sendSMS() {
  gsmSerial.begin(9600);
  delay(300);

  String msg = "!! EMERGENCY !!\n";
  msg += "I need help!\n";

  if (gpsValid) {
    msg += "Location:\n";
    msg += "https://maps.google.com/?q=";
    msg += String(gpsLat, 6);
    msg += ",";
    msg += String(gpsLng, 6);
  } else {
    msg += "(GPS unavailable)";
  }

  gsmSerial.listen();
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(EMERGENCY_NUMBER);
  gsmSerial.println("\"");
  delay(1000);

  gsmSerial.print(msg);
  gsmSerial.write(0x1A);
  delay(5000);

  while (gsmSerial.available()) Serial.write(gsmSerial.read());
  Serial.println(F("\n[GSM] SMS sent"));
}

// =============================================================
//  Utility — Send AT command and print reply
// =============================================================
void gsmSendAT(const char* cmd, unsigned long waitMs) {
  gsmSerial.listen();
  gsmSerial.println(cmd);
  unsigned long t = millis();
  while (millis() - t < waitMs) {
    while (gsmSerial.available()) Serial.write(gsmSerial.read());
  }
  Serial.println();
}
