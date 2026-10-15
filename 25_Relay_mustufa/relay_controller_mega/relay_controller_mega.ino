/*
 * ============================================================
 *  24-RELAY CONTROLLER — Arduino Mega
 *  Controlled via serial from a Python keypress application
 * ============================================================
 *  Protocol (newline-terminated):
 *    T01      → Toggle relay 01 (01–24)
 *    N01      → Turn ON relay 01
 *    F01      → Turn OFF relay 01
 *    S        → Report full state  (ST:000000000000000000000000)
 *    H        → Heartbeat ping     (HB:OK)
 *    A0       → All relays OFF
 *    A1       → All relays ON
 *
 *  Responses:
 *    OK:01:1  → Relay 01 is now ON
 *    OK:01:0  → Relay 01 is now OFF
 *    ST:...   → 24-char binary state string (1=ON, 0=OFF)
 *    HB:OK    → Heartbeat acknowledged
 *    ERR:msg  → Error description
 *
 *  Safety:
 *    - Heartbeat watchdog: if no valid command received within
 *      HEARTBEAT_TIMEOUT_MS, all relays are turned OFF.
 *    - Set HEARTBEAT_ENABLED = false to disable this feature.
 * ============================================================
 *  Board: Arduino Mega 2560
 *  Relay logic: LOW = ON (active-low modules). Change RELAY_ON
 *               / RELAY_OFF if your modules are active-high.
 * ============================================================
 */

// ─── Configuration ──────────────────────────────────────────
#define FW_VERSION          "1.0.0"
#define SERIAL_BAUD         115200
#define NUM_RELAYS          24
#define CMD_BUF_SIZE        16

// Relay logic level — most relay modules are active-LOW
constexpr uint8_t RELAY_ON  = LOW;
constexpr uint8_t RELAY_OFF = HIGH;

// Heartbeat watchdog
constexpr bool     HEARTBEAT_ENABLED    = true;
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 10000;  // 10 seconds

// ─── Pin Mapping ────────────────────────────────────────────
// Uses Mega digital pins 22–45 (all on the double-row header)
// Change this array if your wiring differs.
constexpr uint8_t relayPins[NUM_RELAYS] = {
  22, 23, 24, 25, 26, 27, 28, 29,   // Relay  1–8
  30, 31, 32, 33, 34, 35, 36, 37,   // Relay  9–16
  38, 39, 40, 41, 42, 43, 44, 45    // Relay 17–24
};

// ─── State ──────────────────────────────────────────────────
bool relayState[NUM_RELAYS];            // true = ON
char cmdBuf[CMD_BUF_SIZE];
uint8_t cmdIdx = 0;
uint32_t lastCommandMs = 0;
bool watchdogTripped = false;

// ─── Helpers ────────────────────────────────────────────────

void logMsg(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  Serial.print(tag);
  Serial.print(F(" "));
  Serial.println(msg);
}

void logMsgVal(const __FlashStringHelper* tag, const __FlashStringHelper* msg, int val) {
  Serial.print(tag);
  Serial.print(F(" "));
  Serial.print(msg);
  Serial.println(val);
}

// Apply relay hardware state from the relayState[] array
void applyRelay(uint8_t idx) {
  digitalWrite(relayPins[idx], relayState[idx] ? RELAY_ON : RELAY_OFF);
}

// Set a single relay and respond
void setRelay(uint8_t idx, bool state) {
  relayState[idx] = state;
  applyRelay(idx);
  // Response: OK:NN:S
  Serial.print(F("OK:"));
  if (idx + 1 < 10) Serial.print('0');
  Serial.print(idx + 1);
  Serial.print(':');
  Serial.println(state ? '1' : '0');
}

// Toggle a single relay and respond
void toggleRelay(uint8_t idx) {
  setRelay(idx, !relayState[idx]);
}

// Set all relays to the same state
void setAllRelays(bool state) {
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    relayState[i] = state;
    applyRelay(i);
  }
  sendState();
  Serial.print(F("[INFO] All relays "));
  Serial.println(state ? F("ON") : F("OFF"));
}

// Send full 24-bit state string
void sendState() {
  Serial.print(F("ST:"));
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    Serial.print(relayState[i] ? '1' : '0');
  }
  Serial.println();
}

// Parse a 2-digit relay number from buffer at offset, return 0-based index or -1
int8_t parseRelayNum(const char* buf, uint8_t offset) {
  if (buf[offset] < '0' || buf[offset] > '9') return -1;
  if (buf[offset + 1] < '0' || buf[offset + 1] > '9') return -1;
  int num = (buf[offset] - '0') * 10 + (buf[offset + 1] - '0');
  if (num < 1 || num > NUM_RELAYS) return -1;
  return (int8_t)(num - 1);  // 0-based
}

// ─── Command Processor ─────────────────────────────────────

void processCommand(const char* cmd, uint8_t len) {
  if (len == 0) return;

  lastCommandMs = millis();  // any valid parse attempt resets watchdog

  // Reset watchdog trip flag on new communication
  if (watchdogTripped) {
    watchdogTripped = false;
    logMsg(F("[RECOVERY]"), F("Serial communication restored"));
  }

  switch (cmd[0]) {

    case 'T':  // Toggle: T01 .. T24
    case 't': {
      if (len < 3) { Serial.println(F("ERR:T needs relay number (T01-T24)")); return; }
      int8_t idx = parseRelayNum(cmd, 1);
      if (idx < 0)  { Serial.println(F("ERR:Invalid relay number")); return; }
      toggleRelay(idx);
      break;
    }

    case 'N':  // ON: N01 .. N24
    case 'n': {
      if (len < 3) { Serial.println(F("ERR:N needs relay number (N01-N24)")); return; }
      int8_t idx = parseRelayNum(cmd, 1);
      if (idx < 0)  { Serial.println(F("ERR:Invalid relay number")); return; }
      setRelay(idx, true);
      break;
    }

    case 'F':  // OFF: F01 .. F24
    case 'f': {
      if (len < 3) { Serial.println(F("ERR:F needs relay number (F01-F24)")); return; }
      int8_t idx = parseRelayNum(cmd, 1);
      if (idx < 0)  { Serial.println(F("ERR:Invalid relay number")); return; }
      setRelay(idx, false);
      break;
    }

    case 'S':  // Status
    case 's':
      sendState();
      break;

    case 'H':  // Heartbeat
    case 'h':
      Serial.println(F("HB:OK"));
      break;

    case 'A':  // All: A0 = all off, A1 = all on
    case 'a': {
      if (len < 2) { Serial.println(F("ERR:A needs 0 or 1 (A0/A1)")); return; }
      if (cmd[1] == '1')      setAllRelays(true);
      else if (cmd[1] == '0') setAllRelays(false);
      else Serial.println(F("ERR:A expects 0 or 1"));
      break;
    }

    default:
      Serial.print(F("ERR:Unknown command '"));
      Serial.print(cmd[0]);
      Serial.println('\'');
      break;
  }
}

// ─── Setup ──────────────────────────────────────────────────

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) { ; }  // Wait for USB serial on Mega

  // Boot banner
  Serial.println(F("========================================"));
  Serial.println(F("[BOOT] 24-Relay Controller — Arduino Mega"));
  Serial.print(F("[BOOT] Firmware v"));
  Serial.println(F(FW_VERSION));
  logMsgVal(F("[BOOT]"), F("Baud: "), SERIAL_BAUD);
  logMsgVal(F("[BOOT]"), F("Relays: "), NUM_RELAYS);
  Serial.print(F("[BOOT] Relay logic: "));
  Serial.println(RELAY_ON == LOW ? F("Active-LOW") : F("Active-HIGH"));
  Serial.print(F("[BOOT] Heartbeat watchdog: "));
  Serial.println(HEARTBEAT_ENABLED ? F("ENABLED") : F("DISABLED"));
  if (HEARTBEAT_ENABLED) {
    logMsgVal(F("[BOOT]"), F("Watchdog timeout (ms): "), HEARTBEAT_TIMEOUT_MS);
  }

  // Initialize relay pins — all OFF
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    relayState[i] = false;
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_OFF);
  }
  logMsg(F("[INIT]"), F("All 24 relay pins configured — OFF"));

  Serial.println(F("[INIT] Ready — awaiting commands"));
  Serial.println(F("========================================"));

  lastCommandMs = millis();
}

// ─── Loop ───────────────────────────────────────────────────

void loop() {
  // ── Read serial commands ──
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdIdx > 0) {
        cmdBuf[cmdIdx] = '\0';
        processCommand(cmdBuf, cmdIdx);
        cmdIdx = 0;
      }
    } else {
      if (cmdIdx < CMD_BUF_SIZE - 1) {
        cmdBuf[cmdIdx++] = c;
      } else {
        // Buffer overflow — discard
        cmdIdx = 0;
        Serial.println(F("ERR:Command too long"));
      }
    }
  }

  // ── Heartbeat watchdog ──
  if (HEARTBEAT_ENABLED && !watchdogTripped) {
    if (millis() - lastCommandMs > HEARTBEAT_TIMEOUT_MS) {
      watchdogTripped = true;
      logMsg(F("[WARN]"), F("Heartbeat timeout — setting all relays OFF"));
      for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        relayState[i] = false;
        applyRelay(i);
      }
      sendState();
    }
  }
}