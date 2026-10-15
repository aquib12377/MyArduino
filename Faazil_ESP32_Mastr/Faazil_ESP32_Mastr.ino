/*  MASTER  -  ESP32 30-pin (WROOM)
 *  Receives "strip full" from any slave over ESP-NOW, then emits a keypress.
 *  One master for the whole system. Flash once.
 *
 *  KEY OUTPUT (pick with USE_BLE_KEYBOARD below):
 *    1 -> Bluetooth keyboard   (lib: ESP32-BLE-Keyboard by T-vK)
 *    0 -> print KEY:x on USB serial; run key_bridge.py on the PC
 *
 *  Target: ESP32 Arduino core 3.x
 *  FIRST RUN: open Serial Monitor, copy the printed "Master MAC" into every slave.
 *
 *  AUTO-DETECT SUPPORT:
 *    The host bridge (key_bridge.py) can auto-find this board. On boot, and whenever
 *    it receives a single '?' over USB serial, the master prints an identity line:
 *        MASTER:ESPNOWPAD MAC:AA:BB:CC:DD:EE:FF
 *    The bridge scans serial ports, sends '?', and locks onto the one that replies.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_mac.h"               // esp_read_mac() - reliable MAC, no WiFi-timing race

#define USE_BLE_KEYBOARD  0        // 1 = BLE keyboard, 0 = serial bridge

#if USE_BLE_KEYBOARD
#include <BleKeyboard.h>
BleKeyboard bleKeyboard("ESP-NOW Pad", "DIY", 100);
#endif
#define EVT_STEP       0
#define EVT_COMPLETE   1
// ----------------------- CONFIG -----------------------
#define MAX_SLAVES 8

#define STEP_GUARD_MS    100       // just enough to suppress RF duplicate delivery

volatile bool    gotStep  = false;
volatile uint8_t stepSlave = 0;
uint32_t lastStep[MAX_SLAVES + 1] = {0};  // per-slave debounce for steps

// slaveId (1-based) -> key to send. Index 0 unused.
// Use same char for all if every station should press the same key.
// slaveId (1-based) -> the TWO keys to send for that slave. Index 0 unused.
// Each event fires both keys in order (col 0 then col 1).
const char slaveKey[MAX_SLAVES + 1][2] = {
  { 0 ,  0 },   // [0] unused
  {'1', '2'},   // slave 1
  {'3', '4'},   // slave 2
  {'5', '6'},   // slave 3
  {'7', '8'},   // slave 4
  {'9', '0'},   // slave 5  (digits run out here - edit as you like)
  {'a', 'b'},   // slave 6
  {'c', 'd'},   // slave 7
  {'e', 'f'},   // slave 8
};

#define KEY_GAP_MS 15   // small gap between the two presses so the host registers both
#define REPEAT_GUARD_MS   3000     // ignore repeats from same slave within this window
#define IDENTITY_TOKEN    "MASTER:ESPNOWPAD"   // must match key_bridge.py IDENTITY_TOKEN1
// ------------------------------------------------------

typedef struct {
  uint8_t slaveId;
  uint8_t event;
} msg_t;
msg_t in;

volatile bool    gotEvent = false;
volatile uint8_t gotSlave = 0;
uint32_t lastFire[MAX_SLAVES + 1] = {0};

// core 3.x receive callback. Keep it SHORT (runs in WiFi task): just set a flag.
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < (int)sizeof(msg_t)) return;
  memcpy(&in, data, sizeof(in));
  if (in.event == EVT_COMPLETE) { gotSlave  = in.slaveId; gotEvent = true; }
  if (in.event == EVT_STEP)     { stepSlave = in.slaveId; gotStep  = true; }
}

// Reads the STA-interface MAC straight from eFuse. Works even before/independent of
// the WiFi driver being fully up, so it never returns 00:00:00:00:00:00.
void readStaMac(uint8_t mac[6]) {
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

// Identity line for the host bridge to recognise this board.
void printIdentity() {
  uint8_t mac[6];
  readStaMac(mac);
  Serial.printf("%s MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
                IDENTITY_TOKEN, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Answer the bridge's '?' probe at any time (non-blocking).
void handleHostQuery() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '?') printIdentity();
  }
}

void pressKey(uint8_t slaveId) {
  if (slaveId < 1 || slaveId > MAX_SLAVES) return;
  for (uint8_t i = 0; i < 2; i++) {
    char k = slaveKey[slaveId][i];
    if (!k) continue;
#if USE_BLE_KEYBOARD
    if (bleKeyboard.isConnected()) bleKeyboard.write(k);
    else Serial.println("[WARN] BLE not connected; key dropped");
#else
    Serial.printf("KEY:%c\n", k);     // host bridge turns this into a real keypress
#endif
    if (i == 0) delay(KEY_GAP_MS);    // gap between the two presses
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);                       // let USB-CDC/host settle before first prints

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Human-readable line for the first-run "copy MAC into slaves" step.
  uint8_t mac[6];
  readStaMac(mac);
  Serial.printf("Master MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Machine-readable identity line for key_bridge.py auto-detect.
  printIdentity();

  if (esp_now_init() != ESP_OK) { Serial.println("[ERROR] espnow init fail"); return; }
  esp_now_register_recv_cb(onRecv);

#if USE_BLE_KEYBOARD
  bleKeyboard.begin();
#endif
  Serial.println("[BOOT] Master ready.");
}

void loop() {
  handleHostQuery();                // respond to bridge auto-detect probes
if (gotStep) {
    gotStep = false;
    uint8_t s   = stepSlave;
    uint32_t now = millis();
    if (s >= 1 && s <= MAX_SLAVES && (now - lastStep[s] > STEP_GUARD_MS)) {
      lastStep[s] = now;
      Serial.printf("slave %u step -> key '%c'\n", s, slaveKey[s]);
      pressKey(s);
    }
  }

  // ── Complete event (strip full) ──────────────────────────────────────
  if (gotEvent) {
    gotEvent = false;
    uint8_t s   = gotSlave;
    uint32_t now = millis();
    if (s >= 1 && s <= MAX_SLAVES && (now - lastFire[s] > REPEAT_GUARD_MS)) {
      lastFire[s] = now;
      Serial.printf("slave %u complete -> key '%c'\n", s, slaveKey[s]);
      pressKey(s);
    }
  }
}
