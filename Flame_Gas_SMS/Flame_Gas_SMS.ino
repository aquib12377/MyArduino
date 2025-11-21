#include <SoftwareSerial.h>

/* ================== USER CONFIG ================== */
// Phone number to alert (international format recommended)
const char* ALERT_NUMBER = "+919767771805";

// Pins
const uint8_t PIN_FLAME = 2;       // Flame sensor digital OUT
const uint8_t PIN_GAS   = 3;       // Gas sensor digital D0 OUT
const uint8_t GSM_RX    = 7;       // Arduino RX  (connect to SIM TX)
const uint8_t GSM_TX    = 8;       // Arduino TX  (through level shift to SIM RX)

// Sensor polarity (adjust to your modules):
// Many flame modules are ACTIVE LOW when flame is present.
// Many MQ D0 comparators drive HIGH when gas above threshold.
const bool FLAME_ACTIVE_LOW = true;
const bool GAS_ACTIVE_HIGH  = true;

// Debounce / persistence (ms)
const unsigned long DETECT_PERSIST_MS = 150;  // must stay in "triggered" state this long

// Cooldowns to avoid SMS spam (ms)
const unsigned long SMS_COOLDOWN_MS_FLAME = 60000;  // 60s
const unsigned long SMS_COOLDOWN_MS_GAS   = 60000;  // 60s
const unsigned long SMS_COOLDOWN_MS_BOTH  = 90000;  // 90s (if both triggered at once)

/* ================================================= */

SoftwareSerial gsm(GSM_RX, GSM_TX); // RX, TX

// Latching timers
unsigned long flameStateSince = 0;
unsigned long gasStateSince   = 0;
bool          flameLatched    = false;
bool          gasLatched      = false;

// SMS cooldown timers
unsigned long lastSMS_Flame = 0;
unsigned long lastSMS_Gas   = 0;
unsigned long lastSMS_Both  = 0;

bool readFlameRaw() {
  int v = digitalRead(PIN_FLAME);
  Serial.print("Flame: ");
  Serial.println(v);
  return FLAME_ACTIVE_LOW ? (v == LOW) : (v == HIGH); // true when flame detected
}

bool readGasRaw() {
  int v = digitalRead(PIN_GAS);
  Serial.print("Gas: ");
  Serial.println(v);
  return GAS_ACTIVE_HIGH ? (v == LOW) : (v == HIGH);  // true when gas detected
}

void waitForGsmReady() {
  // Basic init sequence for SMS in text mode
  // Keep it simple and robust: send commands with short waits.
  sendAT("AT");                 delay(300);
  sendAT("ATE0");               delay(300); // echo off
  sendAT("AT+CMGF=1");          delay(300); // SMS text mode
  sendAT("AT+CSCS=\"GSM\"");    delay(300); // 7-bit default
  sendAT("AT+CNMI=1,2,0,0,0");  delay(300); // direct indications (not strictly required)
}

void sendAT(const char* cmd) {
  gsm.print(cmd);
  gsm.print("\r");
}

bool sendSMS(const char* number, const String& message) {
  // Returns true if we at least reached Ctrl+Z send
  gsm.print("AT+CMGF=1\r");           delay(200);
  gsm.print("AT+CMGS=\""); gsm.print(number); gsm.print("\"\r");
  delay(300); // wait for '>' prompt (simple delay is okay for basic use)
  gsm.print(message);
  gsm.write(26); // Ctrl+Z
  // Give the modem time to send
  unsigned long t0 = millis();
  while (millis() - t0 < 8000) { // up to 8s
    // Optionally read responses for "OK" / "+CMGS:"
    if (gsm.available()) (void)gsm.read();
  }
  return true;
}

void setup() {
  pinMode(PIN_FLAME, INPUT_PULLUP); // works for both active-low/high modules with on-board pullups
  pinMode(PIN_GAS,   INPUT_PULLUP);

  Serial.begin(115200);
  gsm.begin(9600);  // Many SIM800/SIM900 default to 9600

  Serial.println(F("GSM SMS Alarm: Flame/Gas"));

  waitForGsmReady();
  Serial.println(F("GSM initialized."));
  sendSMS(ALERT_NUMBER,"Proejct Ready");
}

void loop() {
  unsigned long now = millis();

  // ---- Read sensors with persistence (debounce) ----
  bool flameNow = readFlameRaw();
  bool gasNow   = readGasRaw();

  // Track persistence windows
  if(flameNow && gasNow)
  {
String msg = F("ALERT: FLAME and GAS detected!\nTake immediate action.");
      Serial.println(msg);
      sendSMS(ALERT_NUMBER, msg);
  }
  else if (flameNow) {
    String msg = F("ALERT: FLAME detected!");
      Serial.println(msg);
      sendSMS(ALERT_NUMBER, msg);
  } else if (gasNow) {
    String msg = F("ALERT: GAS leak detected!");
      Serial.println(msg);
      sendSMS(ALERT_NUMBER, msg);
  } 
    delay(20);
}
