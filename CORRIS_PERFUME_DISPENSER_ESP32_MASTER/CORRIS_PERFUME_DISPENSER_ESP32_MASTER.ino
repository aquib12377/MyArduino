/*
  ESP32 MASTER – Classic BT SPP → UART2 → 74HC4051 MUX (one-way to UNO RX0)
  -------------------------------------------------------------------------
  Pair name : "Perfume_Master"  (PIN "8520", optional)
  Command   : "3 0.750"  or  "ALL 0.250"
  Sends     : ONLY "<litres>\n" to the selected UNO (matches UNO's original code)

  Hardware:
    - ESP32 TX2 (GPIO17) → 74HC4051 Z (common I/O)
    - 74HC4051 Y0..Y4 → UNO#1..#5 D0 (RX0)  [one-way, add 220–1k series R]
    - 74HC4051 S0,S1,S2 → ESP32 GPIO 25,26,27  (select 0..7)
    - 74HC4051 /EN → ESP32 GPIO 14 (drive LOW to enable)
    - VCC=3.3V, VEE=GND, GND common
*/

#include <Arduino.h>
#include <ctype.h>
#include <string.h>
#include "BluetoothSerial.h"
#include "esp_spp_api.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Classic BT not enabled in this ESP32 build. Enable Bluetooth in menuconfig."
#endif

/* ---------------- CONFIG ------------------- */
#define DEBUG_HEX_RX_BYTES   1
#define HEARTBEAT_MS         3000
#define LINE_TIMEOUT_MS 120

// UART2 (to MUX → UNO RX0)
HardwareSerial BUS(2);
static const int BUS_TX    = 17;       // TX2 pin
static const int BUS_RX    = 16;       // unused
static const long BUS_BAUD = 115200;   // UNO Serial baud (must match UNO)

// 74HC4051 pins (change if you like)
static const int MUX_S0 = 25;
static const int MUX_S1 = 26;
static const int MUX_S2 = 27;
static const int MUX_EN = 14;          // Active LOW

// Logical device IDs and channel map
static const uint8_t NUM_UNITS = 5;    // UNO#1..UNO#5
// Map ID 1..5 → 4051 channels 0..4
static inline int idToChannel(uint8_t id) {
  if (id >= 1 && id <= NUM_UNITS) return (int)(id - 1);
  return -1;
}

/* -------------- Bluetooth ------------------ */
BluetoothSerial BT;
static const char* BT_NAME = "Perfume_Master";
static const char* BT_PIN  = "8520";   // leave "" for no PIN

/* -------------- Utils/Debug ---------------- */
static uint32_t ms() { return (uint32_t)millis(); }

static void hexdump(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (i && (i % 16) == 0) Serial.println();
    Serial.printf("%02X ", p[i]);
  }
  Serial.println();
}

static void printPrintable(char c) {
  if (c == '\r') Serial.print("\\r");
  else if (c == '\n') Serial.print("\\n");
  else if (isprint((unsigned char)c)) Serial.print(c);
  else { Serial.print("\\x"); Serial.printf("%02X", (uint8_t)c); }
}

/* -------------- MUX control ---------------- */
static void muxEnable(bool en) {    // en=true → pass-through
  digitalWrite(MUX_EN, en ? LOW : HIGH);
}

static void muxSelect(uint8_t ch) { // 0..7
  digitalWrite(MUX_S0, (ch & 0x01) ? HIGH : LOW);
  digitalWrite(MUX_S1, (ch & 0x02) ? HIGH : LOW);
  digitalWrite(MUX_S2, (ch & 0x04) ? HIGH : LOW);
  Serial.printf("[%-8lu] MUX sel → ch=%u  (S2S1S0=%u%u%u)\n",
                ms(), ch,
                (unsigned)((ch>>2)&1), (unsigned)((ch>>1)&1), (unsigned)(ch&1));
}

/* -------------- Sending -------------------- */
// Send just "<litres>\n" to currently selected channel
static void sendLitresLine(float litres) {
  char line[24];
  int n = snprintf(line, sizeof(line), "%.3f\n", litres);

  Serial.printf("[%-8lu] BUS TX ch=cur (%d bytes): ", ms(), n);
  Serial.write((const uint8_t*)line, n);
  if (line[n-1] != '\n') Serial.println();
  Serial.print  ("           BUS TX HEX    : ");
  hexdump((const uint8_t*)line, (size_t)n);

  BUS.write((const uint8_t*)line, n);
  BUS.flush(); // push out before switching channels
}

// Send to one ID by selecting its channel
static bool sendToId(uint8_t id, float litres) {
  int ch = idToChannel(id);
  if (ch < 0) {
    Serial.printf("[%-8lu] ERROR: invalid id=%u\n", ms(), id);
    return false;
  }
  muxEnable(false);           // temporarily disable while switching (optional hygiene)
  muxSelect((uint8_t)ch);
  muxEnable(true);            // enable path
  delayMicroseconds(30);      // settle time for MUX

  Serial.printf("[%-8lu] Dispatch → id=%u (ch=%d), litres=%.3f\n",
                ms(), id, ch, litres);
  sendLitresLine(litres);
  return true;
}

// Broadcast = iterate IDs 1..NUM_UNITS
static void broadcastAll(float litres) {
  Serial.printf("[%-8lu] Broadcast ALL → litres=%.3f\n", ms(), litres);
  for (uint8_t id = 1; id <= NUM_UNITS; ++id) {
    sendToId(id, litres);
    delay(15);   // small gap between nodes
  }
}

/* ----------- Parsing & dispatch ----------- */
static inline bool isSpaceC(char c){ return c==' '||c=='\t'; }

bool parseAndDispatch(const char* ln) {
  Serial.printf("[%-8lu] LINE → '%s'\n", ms(), ln);

  char buf[64];
  size_t L = strnlen(ln, sizeof(buf)-1);
  memcpy(buf, ln, L); buf[L] = 0;

  char* p = buf;
  while (isSpaceC(*p)) ++p;
  if (*p == 0 || *p == '#') {
    Serial.printf("[%-8lu] Parse: blank/comment, ignored.\n", ms());
    return false;
  }

  // Uppercase first token to allow "all"
  char* q = p; while (*q && !isSpaceC(*q)) { *q = toupper((unsigned char)*q); ++q; }

  uint8_t id = 0xFF;
  float litres = 0.0f;

  if (strncmp(p, "ALL", 3) == 0 && isSpaceC(p[3])) {
    char* v = p + 3; while (isSpaceC(*v)) ++v;
    if (sscanf(v, "%f", &litres) == 1) { broadcastAll(litres); return true; }
  } else {
    if (*p == '@') ++p;
    unsigned tmpId = 0; float tmpL = 0.0f;
    if (sscanf(p, "%u %f", &tmpId, &tmpL) == 2) {
      if (tmpId >= 1 && tmpId <= NUM_UNITS && tmpL > 0.0f) {
        id = (uint8_t)tmpId; litres = tmpL;
        return sendToId(id, litres);
      }
    }
  }

  Serial.printf("[%-8lu] Parse FAIL. Use: '<id 1..%u> <L>' or 'ALL <L>'  e.g. 3 0.750\n",
                ms(), NUM_UNITS);
  return false;
}

/* -------------- Line accumulators ---------- */
struct LineIn {
  char buf[96];
  uint8_t i = 0;
  uint32_t lastByteMs = 0;

  void reset(){ i=0; buf[0]=0; lastByteMs = 0; }

  void dispatch() {
    buf[i] = 0;
    i = 0;
    if (buf[0]) parseAndDispatch(buf);
  }

  void push(char c){
#if DEBUG_HEX_RX_BYTES
    Serial.print("[BT BYTE ] ");
    Serial.printf("0x%02X '", (uint8_t)c);
    if (c=='\r') Serial.print("\\r");
    else if (c=='\n') Serial.print("\\n");
    else if (isprint((unsigned char)c)) Serial.print(c);
    else { Serial.print("\\x"); Serial.printf("%02X", (uint8_t)c); }
    Serial.println("'");
#endif
    lastByteMs = millis();

    if (c=='\r') { dispatch(); return; }     // now accept CR as EOL
    if (c=='\n') { dispatch(); return; }     // LF as EOL

    if (i < sizeof(buf)-1) buf[i++] = c;
    else {
      buf[sizeof(buf)-1] = 0;
      Serial.printf("[%-8lu] WARN: input line overflow (truncated): '%s'\n", (uint32_t)millis(), buf);
    }
  }

  // Call this regularly from loop()
  void checkTimeout() {
    if (i > 0 && (millis() - lastByteMs) >= LINE_TIMEOUT_MS) {
      Serial.printf("[%-8lu] EOL by timeout (%ums)\n", (uint32_t)millis(), LINE_TIMEOUT_MS);
      dispatch();
    }
  }
} btLine, usbLine;


/* -------------- BT events ------------------ */
static void btEvtCB(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_INIT_EVT:  Serial.printf("[%-8lu] [BT] SPP INIT (OK)\n", ms()); break;
    case ESP_SPP_START_EVT: Serial.printf("[%-8lu] [BT] SPP SERVER STARTED\n", ms()); break;
    case ESP_SPP_SRV_OPEN_EVT:
      Serial.printf("[%-8lu] [BT] CLIENT CONNECT: handle=%u, rem=",
                    ms(), (unsigned)param->srv_open.handle);
      for (int i=0;i<6;i++){ Serial.printf("%02X%s", param->srv_open.rem_bda[i], i<5?":":""); }
      Serial.println(); break;
    case ESP_SPP_CLOSE_EVT:
      Serial.printf("[%-8lu] [BT] CLIENT DISCONNECT: handle=%u\n",
                    ms(), (unsigned)param->close.handle); break;
    case ESP_SPP_WRITE_EVT:
      Serial.printf("[%-8lu] [BT] WRITE EVT: len=%u cong=%u\n",
                    ms(), (unsigned)param->write.len, (unsigned)param->write.cong); break;
    default:
      Serial.printf("[%-8lu] [BT] EVENT %d\n", ms(), (int)event); break;
  }
}

/* ----------------- Setup -------------------- */
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("====================================================");
  Serial.println(" ESP32 Perfume Master (BT SPP → UART2 → 4051 MUX) ");
  Serial.println("====================================================");

  // UART2 to MUX
  BUS.begin(BUS_BAUD, SERIAL_8N1, BUS_RX, BUS_TX);
  Serial.printf("[%-8lu] BUS init: baud=%ld TX=GPIO%d RX=GPIO%d\n",
                ms(), BUS_BAUD, BUS_TX, BUS_RX);

  // MUX pins
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_EN, OUTPUT);
  muxEnable(false);   // disable while we set initial select
  muxSelect(0);       // default to channel 0 (UNO#1)
  muxEnable(true);    // enable pass-through

  // Classic Bluetooth
  BT.register_callback(btEvtCB);
  // if (strlen(BT_PIN) > 0) {
  //   BT.enableSSP();
  //   BT.setPin((uint8_t*)BT_PIN);  // legacy PIN fallback for some phones
  // }
  bool ok = BT.begin(BT_NAME);
  Serial.printf("[%-8lu] BT.begin('%s') → %s\n", ms(), BT_NAME, ok ? "OK" : "FAIL");
  if (!ok) {
    Serial.println("[ERROR   ] Bluetooth init failed. Rebooting in 3s...");
    delay(3000);
    ESP.restart();
  }

  Serial.println();
  Serial.println("Commands over Bluetooth (SPP):");
  Serial.printf("  <id 1..%u> <L>   e.g.  3 0.750\n", NUM_UNITS);
  Serial.println("  ALL <L>          e.g.  ALL 0.250");
  Serial.println("[Hint] You can also type the same here via USB Serial.");
}

/* ----------------- Loop --------------------- */
void loop() {
  // BT → lines
  while (BT.available()) { btLine.push((char)BT.read()); }
  // USB → lines (optional)
  while (Serial.available()) { usbLine.push((char)Serial.read()); }
  btLine.checkTimeout();
  usbLine.checkTimeout();
  // Heartbeat
  static uint32_t nextBeat = 0;
  if (millis() - nextBeat >= HEARTBEAT_MS) {
    nextBeat = millis();
    bool hasClient = BT.hasClient();
    Serial.printf("[%-8lu] ♥ HB: BT.connected? %s, hasClient=%s, BUS=%ldbps, Units=%u\n",
                  ms(), hasClient ? "yes":"no", hasClient ? "yes":"no", BUS_BAUD, NUM_UNITS);
  }
}
