#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// ================== GM75 on SoftwareSerial (Nano) ==================
static const uint8_t GM_RX_PIN = 10;   // GM75 TX -> Nano D10 (SoftwareSerial RX)
static const uint8_t GM_TX_PIN = 11;   // Nano D11 (SoftwareSerial TX) -> GM75 RX
SoftwareSerial GM(GM_RX_PIN, GM_TX_PIN); // RX, TX
static const uint32_t GM_BAUD = 9600;

// GM75 host-trigger frames
const uint8_t CMD_START[] = { 0x7E,0x00,0x08,0x01,0x00,0x02,0x01,0xAB,0xCD };
const uint8_t CMD_STOP[]  = { 0x7E,0x00,0x08,0x01,0x00,0x02,0x00,0xAB,0xCD };

// ================== I/O mapping ==================
static const uint8_t PIN_BTN_RESET = 2; // to GND, INPUT_PULLUP
static const uint8_t PIN_BTN_LIGHT = 3; // to GND, INPUT_PULLUP

// Relay pins mapped by slot index 0..3
static const uint8_t RELAY_PINS[4] = { 4, 4, 4, 4 };

// Change if your relay modules are active-high
static const bool RELAY_ACTIVE_LEVEL = LOW;

// ================== Debounce ==================
static const uint32_t DEBOUNCE_MS = 200;
uint32_t lastResetMs = 0, lastLightMs = 0;

// ================== Scanner text framing ==================
static const size_t BUF_CAP = 128;
char txtBuf[BUF_CAP];
size_t tlen = 0;
uint32_t lastByteMs = 0;
static const uint32_t IDLE_GAP_MS = 60; // ms without bytes => one scan complete

// ================== States ==================
bool relLatched[4] = {false, false, false, false};
bool scanLightOn   = false;

// ================== 4 software QR codes (EEPROM) ==================
// EEPROM layout: [0]=MAGIC, then 4 slots of (len byte + 32 chars)
static const uint8_t EE_MAGIC_ADDR = 0;
static const uint8_t EE_MAGIC_VAL  = 0x42;
static const int SLOT_BASE   = 1;          // payload starts here
static const uint8_t SLOT_MAXLEN = 32;
static const int SLOT_STRIDE = 1 + SLOT_MAXLEN; // len + data

int slotAddr(uint8_t slot1to4) {
  return SLOT_BASE + (slot1to4 - 1) * SLOT_STRIDE;
}

char codeSlots[4][SLOT_MAXLEN + 1]; // C strings, null-terminated

// ---------------- EEPROM helpers ----------------
void loadCodes() {
  if (EEPROM.read(EE_MAGIC_ADDR) != EE_MAGIC_VAL) {
    const char* def0 = "AQUA-ALPHA";
    const char* def1 = "AQUA-BETA";
    const char* def2 = "AQUA-GAMMA";
    const char* def3 = "AQUA-DELTA";
    strncpy(codeSlots[0], def0, SLOT_MAXLEN); codeSlots[0][SLOT_MAXLEN] = 0;
    strncpy(codeSlots[1], def1, SLOT_MAXLEN); codeSlots[1][SLOT_MAXLEN] = 0;
    strncpy(codeSlots[2], def2, SLOT_MAXLEN); codeSlots[2][SLOT_MAXLEN] = 0;
    strncpy(codeSlots[3], def3, SLOT_MAXLEN); codeSlots[3][SLOT_MAXLEN] = 0;

    EEPROM.write(EE_MAGIC_ADDR, EE_MAGIC_VAL);
    for (uint8_t i=1;i<=4;i++) {
      int base = slotAddr(i);
      const char* p = codeSlots[i-1];
      uint8_t L = (uint8_t)strlen(p);
      if (L > SLOT_MAXLEN) L = SLOT_MAXLEN;
      EEPROM.write(base, L);
      for (uint8_t k=0;k<SLOT_MAXLEN;k++) {
        char c = (k < L) ? p[k] : 0;
        EEPROM.write(base+1+k, c);
      }
    }
  } else {
    for (uint8_t i=1;i<=4;i++) {
      int base = slotAddr(i);
      uint8_t L = EEPROM.read(base);
      if (L > SLOT_MAXLEN) L = SLOT_MAXLEN;
      for (uint8_t k=0;k<SLOT_MAXLEN;k++) {
        char c = EEPROM.read(base+1+k);
        codeSlots[i-1][k] = (k < L) ? c : 0;
      }
      codeSlots[i-1][SLOT_MAXLEN] = 0;
    }
  }
}

void saveCodes() {
  EEPROM.write(EE_MAGIC_ADDR, EE_MAGIC_VAL);
  for (uint8_t i=1;i<=4;i++) {
    int base = slotAddr(i);
    const char* p = codeSlots[i-1];
    uint8_t L = (uint8_t)strlen(p);
    if (L > SLOT_MAXLEN) L = SLOT_MAXLEN;
    EEPROM.write(base, L);
    for (uint8_t k=0;k<SLOT_MAXLEN;k++) {
      char c = (k < L) ? p[k] : 0;
      EEPROM.write(base+1+k, c);
    }
  }
  Serial.println(F("[SAVE] EEPROM updated."));
}

// ---------------- Relay helpers ----------------
void relayOn(uint8_t idx)  { digitalWrite(RELAY_PINS[idx], RELAY_ACTIVE_LEVEL); }
void relayOff(uint8_t idx) { digitalWrite(RELAY_PINS[idx], !RELAY_ACTIVE_LEVEL); }

void allRelaysOff() {
  for (uint8_t i=0;i<4;i++) {
    relayOff(i);
    relLatched[i] = false;
  }
}

// ---------------- GM75 helpers ----------------
void gmStart() {
  GM.write(CMD_START, sizeof(CMD_START));
  scanLightOn = true;
  Serial.println(F("[GM75] START (light ON)"));
}
void gmStop() {
  GM.write(CMD_STOP, sizeof(CMD_STOP));
  scanLightOn = false;
  Serial.println(F("[GM75] STOP (light OFF)"));
}

// ---------------- Logic ----------------
void printCodes() {
  Serial.println(F("[CODES] ===== List ====="));
  for (uint8_t i=0;i<4;i++) {
    Serial.print(F("Slot ")); Serial.print(i+1);
    Serial.print(F(": "));
    Serial.println(codeSlots[i]);
  }
}
void printStatus() {
  Serial.print(F("[STATUS] Light: "));
  Serial.print(scanLightOn ? F("ON") : F("OFF"));
  Serial.print(F(" | Relays: "));
  for (uint8_t i=0;i<4;i++) {
    Serial.print(relLatched[i] ? F("ON") : F("OFF"));
    if (i<3) Serial.print(' ');
  }
  Serial.println();
}

// returns slot index 0..3 if matches; else -1
int matchSlot(const String& s) {
  for (uint8_t i=0;i<4;i++) {
    if (codeSlots[i][0] == 0) continue;
    if (s.equals(String(codeSlots[i]))) return (int)i;
  }
  return -1;
}

void handleScanChunk(const String& raw) {
  if (!raw.length()) return;

  String s = raw;
  s.trim();

  // ---- Filter out "31" noise from GM75 ----
  // 1) Ignore a bare "31"
  if (s == "31") {
    Serial.println(F("[NOISE] Ignoring bare 31"));
    return;
  }

  // 2) If string starts with "31" and has more data, strip it
  if (s.startsWith("31") && s.length() > 2) {
    Serial.print(F("[CLEAN] Stripping leading 31: \""));
    Serial.print(s);
    Serial.print(F("\" -> \""));
    s = s.substring(2);   // drop first two chars
    Serial.print(s);
    Serial.println(F("\""));
  }

  // (Optional) If you ever see it at the end instead of the start, you can also do:
  // if (s.endsWith("31") && s.length() > 2) {
  //   Serial.println(F("[CLEAN] Stripping trailing 31"));
  //   s.remove(s.length() - 2);
  // }

  Serial.print(F("[SCAN] \"")); Serial.print(s); Serial.println(F("\""));

  int idx = matchSlot(s);
  if (idx >= 0) {
    // Latch the corresponding relay
    relayOn(idx);
    relLatched[idx] = true;
    Serial.print(F("[MATCH] Slot ")); Serial.print(idx+1);
    Serial.println(F(" -> Relay latched"));
    // Turn off scanner light after a successful scan
    gmStop();
  } else {
    Serial.println(F("[NO MATCH] Not in the 4 software codes."));
  }
}


void completeChunkIfIdle() {
  if (tlen == 0) return;
  if (millis() - lastByteMs < IDLE_GAP_MS) return;

  // Build chunk safely (no String(char*,len) on AVR)
  String chunk;
  chunk.reserve(tlen);
  for (size_t i = 0; i < tlen; ++i) chunk += txtBuf[i];
  tlen = 0;

  handleScanChunk(chunk);
}

// ---------------- Serial CLI ----------------
// LIST | SET n value | CLR n | SAVE | START | STOP | RESET | STATUS | ON n | OFF n
void processSerialCLI() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String cmd = line; line = "";
      cmd.trim();
      if (!cmd.length()) return;

      if (cmd.equalsIgnoreCase("LIST")) {
        printCodes();
      } else if (cmd.equalsIgnoreCase("STATUS")) {
        printStatus();
      } else if (cmd.startsWith("SET ")) {
        int sp = cmd.indexOf(' ', 4);
        if (sp > 0) {
          int idx = cmd.substring(4, sp).toInt();     // 1..4
          String val = cmd.substring(sp+1); val.trim();
          if (idx >= 1 && idx <= 4 && val.length() > 0 && val.length() <= SLOT_MAXLEN) {
            val.toCharArray(codeSlots[idx-1], SLOT_MAXLEN+1);
            Serial.print(F("[SET] Slot ")); Serial.print(idx);
            Serial.print(F(" = \"")); Serial.print(codeSlots[idx-1]); Serial.println(F("\""));
          } else {
            Serial.println(F("[ERR] Usage: SET <1..4> <value ≤32 chars>"));
          }
        } else {
          Serial.println(F("[ERR] Usage: SET <1..4> <value>"));
        }
      } else if (cmd.startsWith("CLR ")) {
        int idx = cmd.substring(4).toInt();
        if (idx >= 1 && idx <= 4) {
          codeSlots[idx-1][0] = 0;
          Serial.print(F("[CLR] Slot ")); Serial.print(idx); Serial.println(F(" cleared"));
        } else {
          Serial.println(F("[ERR] Usage: CLR <1..4>"));
        }
      } else if (cmd.equalsIgnoreCase("SAVE")) {
        saveCodes();
      } else if (cmd.equalsIgnoreCase("START")) {
        gmStart();
      } else if (cmd.equalsIgnoreCase("STOP")) {
        gmStop();
      } else if (cmd.equalsIgnoreCase("RESET")) {
        allRelaysOff();
        Serial.println(F("[RESET] All relays OFF; ready."));
      } else if (cmd.startsWith("ON ")) {
        int idx = cmd.substring(3).toInt(); // 1..4
        if (idx>=1 && idx<=4) { relayOn(idx-1); relLatched[idx-1] = true; Serial.print("[ON] Relay "+String(idx)+"\n"); }
        else Serial.println(F("[ERR] Usage: ON <1..4>"));
      } else if (cmd.startsWith("OFF ")) {
        int idx = cmd.substring(4).toInt(); // 1..4
        if (idx>=1 && idx<=4) { relayOff(idx-1); relLatched[idx-1] = false; Serial.print("[OFF] Relay "+String(idx)+"\n"); }
        else Serial.println(F("[ERR] Usage: OFF <1..4>"));
      } else {
        Serial.println(F("[INFO] Commands: LIST | SET n val | CLR n | SAVE | START | STOP | RESET | STATUS | ON n | OFF n"));
      }
      return;
    }
    if (line.length() < 96) line += c;
  }
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[NANO QR LATCH x4] 4-code matcher, 4 relay latches, light toggle"));

  loadCodes();
  printCodes();

  for (uint8_t i=0;i<4;i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    relayOff(i);
    relLatched[i] = false;
  }

  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_BTN_LIGHT, INPUT_PULLUP);

  GM.begin(GM_BAUD);
  Serial.println(F("[UART] GM75 via SoftwareSerial @ 9600 (D10=RX, D11=TX)"));

  // Ensure light off at boot
  gmStop();
}

void loop() {
  processSerialCLI();

  // Buttons (debounced)
  uint32_t now = millis();
  if (digitalRead(PIN_BTN_RESET) == LOW && (now - lastResetMs) > DEBOUNCE_MS) {
    lastResetMs = now;
    allRelaysOff();
    Serial.println(F("[BTN] RESET -> All relays OFF; ready."));
  }
  if (digitalRead(PIN_BTN_LIGHT) == LOW && (now - lastLightMs) > DEBOUNCE_MS) {
    lastLightMs = now;
    if (scanLightOn) gmStop(); else gmStart();
  }

  // Read scanner bytes
  while (GM.available() > 0) {
    int bi = GM.read();
    if (bi < 0) break;
    uint8_t b = (uint8_t)bi;
    if (b == '\r' || b == '\n') continue; // tails should be disabled in scanner
    if (b >= 0x20 && b <= 0x7E) {
      if (tlen < BUF_CAP) {
        txtBuf[tlen++] = (char)b;
        lastByteMs = millis();
      } else {
        // overflow -> flush current chunk (build safely)
        String chunk;
        chunk.reserve(tlen);
        for (size_t i = 0; i < tlen; ++i) chunk += txtBuf[i];
        tlen = 0;
        handleScanChunk(chunk);
      }
    }
  }

  // End of scan when idle gap passes
  completeChunkIfIdle();
}
