/* ============================================================================
 *  LED-Ctrl  —  ESP32-S3 BLE receiver firmware
 *
 *  Role     : BLE peripheral (GATT server). Advertises by name, accepts ONE
 *             central (your Flutter / flutter_blue_plus app), receives framed
 *             commands by characteristic WRITE, and drives a WS2812 strip.
 *
 *  Targets  : - ESP32-S3 N16R8 generic dev module
 *             - Waveshare ESP32-S3-Nano  (ESP32-S3R8)
 *             Same silicon on both. Flash BOTH with the SAME profile (below) so
 *             the GPIO numbers in this file are literal and identical.
 *
 *  Frame    : '#' <CMD> ':' <fields...> '*'   (UTF-8 bytes)
 *               ON  -> #LD:1:<R>:<G>:<B>:<BRIGHT>*   e.g. #LD:1:255:0:128:200*
 *               OFF -> #LD:0:0:0:0:0*
 *
 *  Toolchain (pin these versions):
 *    Arduino-ESP32 core .. 3.x   (ESP-IDF 5.x)
 *    NimBLE-Arduino ...... 2.2.0+ (2.x API: callbacks take NimBLEConnInfo&)
 *    Adafruit_NeoPixel ... 1.12.x
 *    Preferences ......... bundled with the core (NVS)
 *
 *  Arduino IDE  Tools  settings — IDENTICAL for BOTH boards:
 *    Board ............ "ESP32S3 Dev Module"   <-- use for the Nano too, so the
 *                       Nano's Dx pin-remap does NOT move LED_DATA_PIN.
 *    USB CDC On Boot .. Enabled                 <-- needed for Serial over native USB
 *    Flash Size ....... 16MB (128Mb)
 *    PSRAM ............ Disabled                 <-- not needed here; avoids a
 *                       boot-loop from a wrong OPI/QSPI choice. (Both are R8/OPI
 *                       if you ever need it.)
 *    Partition Scheme . Default 4MB w/ spiffs (or any 16M scheme). App ~1.3 MB.
 *    Upload Speed ..... 921600 (drop to 115200 if uploads fail)
 *
 *  WS2812 wiring:
 *    DIN -> LED_DATA_PIN via a 330–470 Ohm series resistor.
 *    GND -> common ground shared by the ESP32 AND the strip's 5V PSU.
 *    +5V -> external 5V PSU (never the board 3V3). 1000 uF cap across 5V/GND.
 *    Level-shift DIN to 5V (e.g. 74AHCT125) for long/bright strips. Short test
 *    strips usually latch fine straight off 3.3V.
 * ========================================================================== */

#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

/* ------------------------------- CONFIG ---------------------------------- */
// Strip -------------------------------------------------------------------
constexpr uint8_t  LED_DATA_PIN = 4;     // literal GPIO4 (safe on S3R8; avoid
                                         // 0/3/19/20/26-37/45/46 on this chip)
constexpr uint16_t LED_COUNT    = 60;    // <<< SET to your real strip length
constexpr neoPixelType LED_TYPE = NEO_GRB + NEO_KHZ800;

// BLE identity — MUST match the Flutter app's serviceUuid / charUuid ------
constexpr char DEV_NAME[] = "LED-Ctrl";
#define SVC_UUID "0000ABCD-0000-1000-8000-00805F9B34FB"
#define CHR_UUID "0000ABCE-0000-1000-8000-00805F9B34FB"

// Optional status LED (plain on/off pin). Disabled by default because the
// onboard-LED pin differs per board and isn't always a simple GPIO. To use,
// uncomment and set a free pin: HIGH = central connected, blink = advertising.
// #define STATUS_LED_PIN 2

// NVS persistence guard ---------------------------------------------------
constexpr uint8_t  ST_MAGIC   = 0xA7;    // bump if the struct layout changes
constexpr char     NVS_NS[]   = "ledctrl";
constexpr char     NVS_KEY[]  = "st";

/* ------------------------------- STATE ----------------------------------- */
struct __attribute__((packed)) LedState {
  uint8_t magic;                 // validity marker
  bool    on;
  uint8_t r, g, b, bright;
};

static LedState g_state = { ST_MAGIC, false, 255, 255, 255, 200 };  // shared
static volatile bool g_dirty     = false;   // new state waiting to render
static volatile bool g_connected = false;
static portMUX_TYPE  g_mux = portMUX_INITIALIZER_UNLOCKED;          // guards g_state

Adafruit_NeoPixel strip(LED_COUNT, LED_DATA_PIN, LED_TYPE);
Preferences       prefs;

// Frame assembler (lives across BLE writes; single central only) ----------
static char    rxBuf[64];
static uint8_t rxLen   = 0;
static bool    framing = false;

/* ------------------------------ HELPERS ---------------------------------- */
static void logState(const char* tag, const LedState& s) {
  Serial.printf("[STATE] %-8s on=%d rgb=%3u,%3u,%3u br=%3u\n",
                tag, s.on, s.r, s.g, s.b, s.bright);
}

static void applyLeds(const LedState& s) {
  strip.setBrightness(s.on ? s.bright : 0);
  uint32_t c = s.on ? strip.Color(s.r, s.g, s.b) : 0;
  for (uint16_t i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, c);
  strip.show();
}

static void saveState(const LedState& s) {
  prefs.putBytes(NVS_KEY, &s, sizeof(s));
}

static void loadState() {
  LedState tmp;
  if (prefs.getBytesLength(NVS_KEY) == sizeof(tmp) &&
      prefs.getBytes(NVS_KEY, &tmp, sizeof(tmp)) == sizeof(tmp) &&
      tmp.magic == ST_MAGIC) {
    g_state = tmp;
    Serial.println("[INIT] restored LED state from NVS");
  } else {
    Serial.println("[INIT] no valid saved state -> defaults");
  }
}

#ifdef STATUS_LED_PIN
static void statusLed(bool connected) { digitalWrite(STATUS_LED_PIN, connected ? HIGH : LOW); }
#else
static void statusLed(bool) {}
#endif

/* --------------------------- FRAME PARSER -------------------------------- */
// Parse the body between '#' and '*'. Returns true and fills `out` on success.
static bool parseFrame(const char* body, LedState& out) {
  if (strncmp(body, "LD:", 3) != 0) {
    Serial.printf("[WARN] unknown cmd: %.16s\n", body);
    return false;
  }
  int s, r, g, b, br;
  if (sscanf(body, "LD:%d:%d:%d:%d:%d", &s, &r, &g, &b, &br) != 5) {
    Serial.printf("[WARN] malformed LD frame: %.32s\n", body);
    return false;
  }
  out.magic  = ST_MAGIC;
  out.on     = (s != 0);
  out.r      = (uint8_t)constrain(r, 0, 255);
  out.g      = (uint8_t)constrain(g, 0, 255);
  out.b      = (uint8_t)constrain(b, 0, 255);
  out.bright = (uint8_t)constrain(br, 0, 255);
  return true;
}

// Feed one received byte through the framing state machine. On a complete,
// valid frame, publish the new state to loop() under a critical section.
static void feed(uint8_t ch) {
  if (ch == '#') { framing = true; rxLen = 0; return; }   // (re)start a frame
  if (!framing)  return;                                   // ignore noise
  if (ch == '\r' || ch == '\n') return;                    // tolerate line ends
  if (ch == '*') {                                         // end of frame
    rxBuf[rxLen] = '\0';
    framing = false;
    LedState ns;
    if (parseFrame(rxBuf, ns)) {
      portENTER_CRITICAL(&g_mux);
      g_state = ns;
      g_dirty = true;
      portEXIT_CRITICAL(&g_mux);
    }
    return;
  }
  if (rxLen < sizeof(rxBuf) - 1) {
    rxBuf[rxLen++] = (char)ch;
  } else {                                                 // overflow -> drop
    framing = false;
    Serial.println("[WARN] frame overflow, dropped");
  }
}

/* --------------------------- BLE CALLBACKS (2.x) ------------------------- */
class CmdCallbacks : public NimBLECharacteristicCallbacks {
  // 2.x signature: extra required NimBLEConnInfo& parameter.
  void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo& /*info*/) override {
    NimBLEAttValue v = chr->getValue();
    const uint8_t* p = v.data();
    for (size_t i = 0; i < v.length(); i++) feed(p[i]);
  }
};

class SrvCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* /*s*/, NimBLEConnInfo& info) override {
    g_connected = true;
    statusLed(true);
    Serial.printf("[BLE] central connected: %s\n", info.getAddress().toString().c_str());
  }
  // 2.x signature: NimBLEConnInfo& + int reason.
  void onDisconnect(NimBLEServer* /*s*/, NimBLEConnInfo& /*info*/, int reason) override {
    g_connected = false;
    statusLed(false);
    Serial.printf("[BLE] central disconnected (reason=%d), re-advertising\n", reason);
    NimBLEDevice::startAdvertising();          // let the app auto-reconnect
  }
  void onMTUChange(uint16_t mtu, NimBLEConnInfo& /*info*/) override {
    Serial.printf("[BLE] MTU = %u\n", mtu);
  }
};

/* ------------------------------- SETUP ----------------------------------- */
void setup() {
  Serial.begin(115200);
  delay(200);                                  // let native-USB CDC enumerate
  Serial.println("\n[BOOT] LED-Ctrl v1.0  (NimBLE 2.x)");

#ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
#endif

  prefs.begin(NVS_NS, false);
  strip.begin();
  strip.show();                                // all off
  loadState();
  applyLeds(g_state);                          // restore last look immediately
  logState("restored", g_state);

  // ---- BLE bring-up (NimBLE 2.x) ----
  NimBLEDevice::init(DEV_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new SrvCallbacks());

  NimBLEService* svc = server->createService(SVC_UUID);
  NimBLECharacteristic* chr = svc->createCharacteristic(
      CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  chr->setCallbacks(new CmdCallbacks());
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(DEV_NAME);            // REQUIRED in 2.x: name no longer auto-advertised
  adv->addServiceUUID(SVC_UUID);
  adv->enableScanResponse(true);     // REQUIRED in 2.x: scan response off by default
  adv->start();

  Serial.printf("[INIT] advertising as \"%s\"  svc=%s\n", DEV_NAME, SVC_UUID);
}

/* -------------------------------- LOOP ----------------------------------- */
void loop() {
  // Render + persist OFF the BLE task to avoid stalling the stack during
  // strip.show() / NVS writes.
  bool      dirty = false;
  LedState  local;
  portENTER_CRITICAL(&g_mux);
  if (g_dirty) { local = g_state; g_dirty = false; dirty = true; }
  portEXIT_CRITICAL(&g_mux);

  if (dirty) {
    applyLeds(local);
    saveState(local);
    logState("applied", local);
  }

#ifdef STATUS_LED_PIN
  // Slow blink while advertising (no central), solid while connected.
  static uint32_t t = 0;
  if (!g_connected && millis() - t > 500) { t = millis();
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN)); }
#endif

  // Heartbeat diagnostic every 10 s.
  static uint32_t hb = 0;
  if (millis() - hb > 10000) { hb = millis();
    Serial.printf("[HB] up=%lus heap=%u conn=%d\n",
                  millis() / 1000, (unsigned)ESP.getFreeHeap(), g_connected); }

  delay(5);   // event-driven; keep the loop cheap and non-blocking
}
