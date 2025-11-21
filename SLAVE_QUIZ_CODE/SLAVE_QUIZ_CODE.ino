// STUDENT — ESP-NOW Broadcast (ESP32_NOW wrapper)
// Sends AnswerMsg via broadcast on button press.

#include <WiFi.h>
#include <esp_mac.h>
#include "ESP32_NOW.h"

#define BTN1 14
#define BTN2 27
#define BTN3 26
#define BTN4 25

static const uint16_t DEVICE_ID = 1;        // <- change per device: 1..6
static const uint8_t  ESPNOW_WIFI_CHANNEL = 6;

struct __attribute__((packed)) AnswerMsg {
  uint16_t id;
  uint8_t  q;      // keep 0; master uses its own currentQ
  uint8_t  ans;    // 1..4
  uint32_t seq;
};

uint32_t seq = 0;

// --- Broadcast peer class (like your example) ---
class BroadcastPeer : public ESP_NOW_Peer {
public:
  BroadcastPeer(uint8_t ch, wifi_interface_t iface, const uint8_t* lmk)
  : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, ch, iface, lmk) {}

  bool begin() { return ESP_NOW.begin() && add(); }
  bool sendMsg(const void* data, size_t len) { return send((const uint8_t*)data, len); }
};

BroadcastPeer bpeer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, nullptr);

inline bool pressed(int pin) { return digitalRead(pin) == LOW; }

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("=== STUDENT (ESP32_NOW broadcast) ==="));

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) delay(50);

  Serial.printf("STA MAC: %s  CH=%d\n", WiFi.macAddress().c_str(), ESPNOW_WIFI_CHANNEL);

  if (!bpeer.begin()) {
    Serial.println(F("ESP_NOW begin/add broadcast peer FAILED; rebooting..."));
    delay(2000); ESP.restart();
  }
  Serial.println(F("ESP_NOW ready. Press a button (1..4)."));
}

static void sendAns(uint8_t ans) {
  AnswerMsg m{DEVICE_ID, 0, ans, ++seq};
  bool ok = bpeer.sendMsg(&m, sizeof(m));
  Serial.printf("[TX] id=%u ans=%u seq=%lu -> %s\n", DEVICE_ID, ans, (unsigned long)seq, ok?"OK":"FAIL");
}

void loop() {
  static uint32_t last=0, now;
  now = millis();
  if (now - last > 200) {            // simple debounce
    if      (pressed(BTN1)) { sendAns(1); last = now; }
    else if (pressed(BTN2)) { sendAns(2); last = now; }
    else if (pressed(BTN3)) { sendAns(3); last = now; }
    else if (pressed(BTN4)) { sendAns(4); last = now; }
  }
}
