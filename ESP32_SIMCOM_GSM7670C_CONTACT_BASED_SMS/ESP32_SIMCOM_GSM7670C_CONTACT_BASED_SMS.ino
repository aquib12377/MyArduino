/*
  ============================================================================
  A7670C DC-Supply-Fail Dry-Contact SMS Alert — ESP32 (Vasant Utsav DSS)
  ============================================================================
  Purpose : Monitor a single potential-free relay contact from a rectifier
            module. On a state change, SMS both configured recipients with
            a fixed, site-specific message. Always notifies on both
            directions (fault AND restore) — no mode selection.

  WIRING:
    SIM link:
      ESP32 GPIO16 (RX2) <-- module TXD
      ESP32 GPIO17 (TX2) --> module RXD
      ESP32 GND          --- module GND
      Module powered independently from its own 12V/2A jack supply.

    Dry contact input:
      ESP32 GPIO4  <---> one leg of the relay contact
      ESP32 GND    <---> other leg of the contact
      (Internal pull-up — contact CLOSED reads LOW, OPEN reads HIGH.)

  CONTACT -> MESSAGE MAPPING:
    Contact CLOSES  (LOW)  -> "220 V DC Supply failed..."   (fault)
    Contact OPENS   (HIGH) -> "220 V DC Supply restored..." (recovered)
    If this comes out backwards on-site, either swap which relay terminal
    pair (NO/NC) you're wired to, or swap MSG_FAILURE and MSG_RESTORED below.

  GPIO16/17 CAVEAT:
    Reserved for PSRAM on WROVER-class ESP32 boards. Assumes plain
    WROOM-32 (no PSRAM), same as your working test harness.

  BEHAVIOR NOTES:
    - Debounced in software (100ms) so relay chatter doesn't fire repeats.
    - Boot's starting contact state is not treated as a transition.
    - Startup SMS ("system active") only fires if network registration is
      confirmed within the boot timeout — it won't falsely claim
      "connected to network" if it isn't.
    - Every alert is sent to BOTH recipients independently; a failure on
      one number is retried once and logged, and does not block sending
      to the other number.
    - Each SMS best-effort includes a network-synced timestamp
      (AT+CCLK?) if available.
  ============================================================================
*/

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
constexpr uint8_t SIM_RX_PIN   = 16; // ESP32 RX2 <- module TXD
constexpr uint8_t SIM_TX_PIN   = 17; // ESP32 TX2 -> module RXD
constexpr uint32_t DEBUG_BAUD  = 115200;

constexpr uint8_t CONTACT_PIN  = 4;  // dry contact input (other leg -> GND)

constexpr uint32_t DEBOUNCE_MS = 100;
constexpr uint32_t REG_WAIT_TIMEOUT_MS = 30000; // boot-time registration wait

const char* ALERT_RECIPIENTS[] = { "+919029009501", "+918591062011","+917977845638" };
constexpr uint8_t NUM_RECIPIENTS = sizeof(ALERT_RECIPIENTS) / sizeof(ALERT_RECIPIENTS[0]);

const char* MSG_SYSTEM_ACTIVE = "Vasant Utsav DSS monitoring system is active and connected to network";
const char* MSG_FAILURE       = "220 V DC Supply failed at Vasant Utsav DSS (Rectifier Module failed)";
const char* MSG_RESTORED      = "220 V DC Supply restored at Vasant Utsav DSS (Rectifier Module ok)";

HardwareSerial simSerial(2); // ESP32 UART2

const long CANDIDATE_BAUDS[] = {115200, 57600, 38400, 19200, 9600};
const uint8_t NUM_BAUDS = sizeof(CANDIDATE_BAUDS) / sizeof(CANDIDATE_BAUDS[0]);
long simBaud = 0;

constexpr uint16_t RESP_BUF_LEN = 256;
char respBuf[RESP_BUF_LEN];

int lastRawReading;
int stableReading;
uint32_t lastDebounceTime = 0;

// ---------------------------------------------------------------------------
// Logging helper
// ---------------------------------------------------------------------------
void log(const char* level, const char* msg) {
  Serial.print('[');
  Serial.print(level);
  Serial.print(F("] "));
  Serial.println(msg);
}

// ---------------------------------------------------------------------------
// Send an AT command and block (up to timeoutMs) until `expect` substring
// or "ERROR" appears in the response. Result is left in respBuf.
// ---------------------------------------------------------------------------
bool sendAT(const char* cmd, uint16_t timeoutMs, const char* expect = "OK") {
  while (simSerial.available()) simSerial.read(); // flush stale bytes

  simSerial.print(cmd);
  simSerial.print("\r\n");

  uint16_t idx = 0;
  memset(respBuf, 0, RESP_BUF_LEN);
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      if (idx < RESP_BUF_LEN - 1) {
        respBuf[idx++] = c;
        respBuf[idx] = '\0';
      }
      if (strstr(respBuf, expect) != nullptr) return true;
      if (strstr(respBuf, "ERROR") != nullptr) return false;
    }
  }
  return false; // timeout
}

// ---------------------------------------------------------------------------
// Baud auto-detection
// ---------------------------------------------------------------------------
bool autoDetectBaud() {
  for (uint8_t i = 0; i < NUM_BAUDS; i++) {
    simSerial.end();
    delay(50);
    simSerial.begin(CANDIDATE_BAUDS[i], SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
    delay(100);
    while (simSerial.available()) simSerial.read();

    for (uint8_t attempt = 0; attempt < 3; attempt++) {
      if (sendAT("AT", 800)) {
        simBaud = CANDIDATE_BAUDS[i];
        return true;
      }
    }
  }
  return false;
}

bool waitForRegistration(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (sendAT("AT+CREG?", 2000, "OK")) {
      if (strstr(respBuf, "+CREG: 0,1") || strstr(respBuf, "+CREG: 0,5") ||
          strstr(respBuf, "+CREG: 1,1") || strstr(respBuf, "+CREG: 1,5") ||
          strstr(respBuf, "+CREG: 2,1") || strstr(respBuf, "+CREG: 2,5")) {
        log("INIT", "Network registered.");
        return true;
      }
    }
    log("INIT", "Waiting for network registration...");
    delay(2000);
  }
  log("WARN", "Not registered within boot timeout - startup SMS skipped, will keep retrying on each alert.");
  return false;
}

// ---------------------------------------------------------------------------
// Best-effort network timestamp for the SMS body
// ---------------------------------------------------------------------------
bool getNetworkTimestamp(char* out, size_t outLen) {
  if (!sendAT("AT+CCLK?", 2000, "+CCLK:")) return false;
  char* start = strchr(respBuf, '"');
  if (!start) return false;
  start++;
  char* end = strchr(start, '"');
  if (!end) return false;
  size_t len = end - start;
  if (len >= outLen) len = outLen - 1;
  strncpy(out, start, len);
  out[len] = '\0';
  // Reject obviously unsynced default dates (module never got NITZ time)
  if (strncmp(out, "80/01", 5) == 0 || strncmp(out, "00/01", 5) == 0) return false;
  return true;
}

// ---------------------------------------------------------------------------
// SMS sending - single recipient
// ---------------------------------------------------------------------------
bool sendSMSToNumber(const char* number, const char* message) {
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", number);

  while (simSerial.available()) simSerial.read();
  simSerial.print(cmd);
  simSerial.print("\r\n");

  uint32_t promptStart = millis();
  bool gotPrompt = false;
  while (millis() - promptStart < 3000) {
    if (simSerial.available()) {
      char c = simSerial.read();
      if (c == '>') { gotPrompt = true; break; }
    }
  }
  if (!gotPrompt) {
    log("ERROR", "No '>' prompt from module - SMS send aborted.");
    simSerial.write(0x1B); // ESC cancels pending SMS input
    return false;
  }

  simSerial.print(message);
  simSerial.write(0x1A); // Ctrl+Z submits the message

  uint16_t idx = 0;
  memset(respBuf, 0, RESP_BUF_LEN);
  uint32_t start = millis();
  bool sawCMGS = false;
  bool done = false;
  while (millis() - start < 15000 && !done) {
    while (simSerial.available()) {
      char c = simSerial.read();
      if (idx < RESP_BUF_LEN - 1) { respBuf[idx++] = c; respBuf[idx] = '\0'; }
      if (strstr(respBuf, "+CMGS:") != nullptr) sawCMGS = true;
      if (sawCMGS && strstr(respBuf, "OK") != nullptr) { done = true; break; }
      if (strstr(respBuf, "ERROR") != nullptr) { done = true; break; }
    }
  }
  return sawCMGS;
}

// ---------------------------------------------------------------------------
// SMS sending - all configured recipients, with per-recipient retry
// ---------------------------------------------------------------------------
void sendAlertToAllRecipients(const char* baseMessage) {
  char message[160];
  char ts[32];
  if (getNetworkTimestamp(ts, sizeof(ts))) {
    snprintf(message, sizeof(message), "%s at %s", baseMessage, ts);
  } else {
    snprintf(message, sizeof(message), "%s", baseMessage);
  }

  for (uint8_t i = 0; i < NUM_RECIPIENTS; i++) {
    const char* number = ALERT_RECIPIENTS[i];
    bool ok = sendSMSToNumber(number, message);

    if (!ok) {
      Serial.print(F("[WARN] SMS failed to "));
      Serial.print(number);
      Serial.println(F(" - retrying once..."));
      delay(2000);
      ok = sendSMSToNumber(number, message);
    }

    if (ok) {
      Serial.print(F("[INFO] SMS sent to "));
      Serial.print(number);
      Serial.print(F(": "));
      Serial.println(message);
    } else {
      Serial.print(F("[ERROR] SMS permanently failed to "));
      Serial.println(number);
    }
  }
}

// ---------------------------------------------------------------------------
// Contact transition handling (always BOTH directions)
// ---------------------------------------------------------------------------
void handleContactTransition(bool connected) {
  Serial.print(F("[STATE] Contact "));
  Serial.println(connected ? "CLOSED (fault)" : "OPEN (restored)");

  if (connected) {
    sendAlertToAllRecipients(MSG_FAILURE);
  } else {
    sendAlertToAllRecipients(MSG_RESTORED);
  }
}

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(300);

  pinMode(CONTACT_PIN, INPUT_PULLUP);

  // Establish starting state WITHOUT treating it as a transition
  lastRawReading = digitalRead(CONTACT_PIN);
  stableReading = lastRawReading;

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("[BOOT] A7670C DC-Supply-Fail SMS Alert | Vasant Utsav DSS"));
  Serial.print(F("[BOOT] Starting contact state: "));
  Serial.println(stableReading == LOW ? "CLOSED (fault)" : "OPEN (normal)");
  Serial.println(F("=================================================="));

  log("INIT", "Starting module link auto-baud detection...");
  if (autoDetectBaud()) {
    Serial.print(F("[INIT] Module responded at "));
    Serial.print(simBaud);
    Serial.println(F(" baud."));
  } else {
    Serial.println(F("[ERROR] No response from module at any candidate baud rate."));
    Serial.println(F("[ERROR] Contact monitoring will still run, but SMS sends will fail"));
    Serial.println(F("[ERROR] until the module link is fixed. Check DC-PWR LED and wiring."));
  }

  sendAT("ATE0", 1000);
  sendAT("AT+CMEE=2", 1000);
  if (sendAT("AT+CPIN?", 3000, "+CPIN: READY")) {
    log("INIT", "SIM card ready.");
  } else {
    log("WARN", "SIM not confirmed ready - check SIM insertion / PIN lock.");
  }
  sendAT("AT+CMGF=1", 1000); // SMS text mode

  bool registered = waitForRegistration(REG_WAIT_TIMEOUT_MS);
  if (registered) {
    sendAlertToAllRecipients(MSG_SYSTEM_ACTIVE);
  } else {
    log("WARN", "Skipping startup SMS - will still monitor contact and attempt alerts later.");
  }

  log("INIT", "Setup complete - monitoring contact.");
}

void loop() {
  int rawReading = digitalRead(CONTACT_PIN);

  if (rawReading != lastRawReading) {
    lastDebounceTime = millis();
    lastRawReading = rawReading;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (rawReading != stableReading) {
      stableReading = rawReading;
      bool connected = (stableReading == LOW); // closed/grounded = LOW
      handleContactTransition(connected);
    }
  }

  // Passively drain any unexpected module output so the UART buffer never
  // fills up while idle.
  while (simSerial.available()) simSerial.read();
}