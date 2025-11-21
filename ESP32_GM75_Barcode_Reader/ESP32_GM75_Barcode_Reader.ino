// ============================================================
// ESP32 + GM75 (UART Command Trigger) — Stop fix with "armed" gating
// - Dedup & ignore "31" are unchanged
// - Auto-retrigger happens only when 'armed' (set by Start, cleared by Stop)
// ============================================================

#include <Arduino.h>

// -------- UART --------
static const int UART_NUM = 2;
static const int PIN_RX   = 16;      // GM75 TX -> ESP32 RX2
static const int PIN_TX   = 17;      // ESP32 TX2 -> GM75 RX
static const uint32_t BAUD = 9600;
HardwareSerial GM(UART_NUM);

// -------- Options --------
static const bool USE_BUTTONS    = true;
static const int  PIN_BTN_START  = 4;      // INPUT_PULLUP; press -> GND
static const int  PIN_BTN_STOP   = 5;      // INPUT_PULLUP; press -> GND
static const bool LOG_FRAMES_HEX = false;

// Debounce
static const uint32_t DEBOUNCE_MS = 200;
uint32_t lastStartPress = 0, lastStopPress = 0;

// -------- Text framing (no CR/LF) --------
static const uint32_t IDLE_TIMEOUT_MS = 40;
static const size_t   BUF_CAP         = 256;
char     txtBuf[BUF_CAP];
size_t   tlen = 0;
uint32_t lastByteMs = 0;

// -------- Binary frame capture (0x7E ...) --------
static const size_t   FBUF_CAP = 128;
uint8_t  frmBuf[FBUF_CAP];
size_t   flen = 0;
bool     inFrame = false;

// -------- Dedup cache --------
static const size_t SEEN_MAX = 64;
String  seen[SEEN_MAX];
size_t  seenCount = 0;
size_t  seenHead  = 0;

// -------- Commands --------
const uint8_t CMD_START[]     = {0x7E,0x00,0x08,0x01,0x00,0x02,0x01,0xAB,0xCD};
const uint8_t CMD_STOP[]      = {0x7E,0x00,0x08,0x01,0x00,0x02,0x00,0xAB,0xCD};

// <<< changed >>> gate auto-retrigger with an armed flag
volatile bool armed = false;

void hexDump(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) { if (i) Serial.print(' '); if (p[i] < 16) Serial.print('0'); Serial.print(p[i], HEX); }
}

void clearText() { tlen = 0; }

void sendStart() {
  clearText();
  armed = true;                           // <<< changed
  GM.write(CMD_START, sizeof(CMD_START));
  if (LOG_FRAMES_HEX) { Serial.print("[GM75] TX START: "); hexDump(CMD_START, sizeof(CMD_START)); Serial.println(); }
}

void sendStop() {
  armed = false;                          // <<< changed: disarm first
  GM.write(CMD_STOP, sizeof(CMD_STOP));
  clearText();                            // <<< changed: drop any partial payload
  if (LOG_FRAMES_HEX) { Serial.print("[GM75] TX STOP : "); hexDump(CMD_STOP, sizeof(CMD_STOP)); Serial.println(); }
  // optional: drain any immediate ACK/status bytes
  delay(5);
  while (GM.available()) (void)GM.read();
}

bool seenContains(const String& s) {
  size_t n = (seenCount < SEEN_MAX) ? seenCount : SEEN_MAX;
  for (size_t i = 0; i < n; ++i) if (seen[i] == s) return true;
  return false;
}
void seenInsert(const String& s) {
  if (seenCount < SEEN_MAX) seen[seenCount++] = s;
  else { seen[seenHead] = s; seenHead = (seenHead + 1) % SEEN_MAX; }
}

// Completed barcode handler
void handleBarcodeLine(const String& code) {
  if (code == "31") return;               // <<< changed: ignore "31"
  if (code.length() == 0) return;

  if (!seenContains(code)) {
    seenInsert(code);
    Serial.print("BARCODE: "); Serial.println(code);
  }
}

// Called when a barcode text chunk is complete (by idle gap)
void processCompletedBarcode() {
  if (tlen == 0) return;

  size_t start = 0, end = tlen;
  while (start < end && (txtBuf[start] == ' ' || txtBuf[start] == '\t')) start++;
  while (end > start && (txtBuf[end - 1] == ' ' || txtBuf[end - 1] == '\t')) end--;

  String code;
  if (end > start) {
    code.reserve(end - start);
    for (size_t i = start; i < end; ++i) code += txtBuf[i];
  }
  tlen = 0;

  handleBarcodeLine(code);

  // <<< changed: only re-trigger if we are armed
  if (armed) {
    delay(10);
    GM.write(CMD_START, sizeof(CMD_START));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[ESP32 <-> GM75] Start/Stop fixed (armed), dedup, ignore \"31\"");
  if (USE_BUTTONS) {
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_BTN_STOP,  INPUT_PULLUP);
  }
  GM.begin(BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  Serial.printf("UART2 @ %lu 8N1  (RX=%d, TX=%d)\n", (unsigned long)BAUD, PIN_RX, PIN_TX);
  Serial.println("Controls: 't' start, 's' stop.");
}

void loop() {
  // Console controls
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 't' || c == 'T') sendStart();
    if (c == 's' || c == 'S') sendStop();
  }

  // Buttons
  if (USE_BUTTONS) {
    uint32_t now = millis();
    if (digitalRead(PIN_BTN_START) == LOW && (now - lastStartPress > DEBOUNCE_MS)) { lastStartPress = now; sendStart(); }
    if (digitalRead(PIN_BTN_STOP)  == LOW && (now - lastStopPress  > DEBOUNCE_MS)) { lastStopPress  = now; sendStop();  }
  }

  // Read stream
  while (GM.available() > 0) {
    int bi = GM.read();
    if (bi < 0) break;
    uint8_t b = (uint8_t)bi;

    // Binary frame?
    if (!inFrame && b == 0x7E) { inFrame = true; flen = 0; frmBuf[flen++] = b; continue; }
    if (inFrame) {
      if (flen < FBUF_CAP) frmBuf[flen++] = b;
      if (GM.available() == 0) {
        if (LOG_FRAMES_HEX) { Serial.print("[GM75] RX FRAME: "); hexDump(frmBuf, flen); Serial.println(); }
        inFrame = false; flen = 0;
      }
      continue;
    }

    // Decoded text path
    lastByteMs = millis();
    if (b == '\r' || b == '\n') continue;
    if (b >= 0x20 && b <= 0x7E) {
      if (tlen < BUF_CAP) txtBuf[tlen++] = (char)b;
      else { processCompletedBarcode(); txtBuf[0] = (char)b; tlen = 1; }
    }
  }

  // Idle-gap -> full barcode ended
  if (tlen > 0 && (millis() - lastByteMs) >= IDLE_TIMEOUT_MS) {
    processCompletedBarcode();
  }

  delay(1);
}
