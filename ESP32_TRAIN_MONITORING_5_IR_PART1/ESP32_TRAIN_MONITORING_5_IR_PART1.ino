/*****************************************************************
  TRACK-MONITOR – broadcasts 5-sensor bitmap via ESP-NOW
*****************************************************************/
#include <esp_now.h>
#include <WiFi.h>

/* ------------ pinout (active-LOW IR sensors) ---------------- */
constexpr uint8_t IR_PINS[5] = {32, 33, 34, 35, 36};

/* ------------ payload structure ----------------------------- */
struct SensorFrame {
  uint8_t bitmap;          // bit0 = S1 … bit4 = S5  (1 = object)
  uint32_t millisUTC;      // sender time stamp (optional)
} __attribute__((packed));

static_assert(sizeof(SensorFrame) == 5, "Packing failed");

esp_now_peer_info_t peer{};   // filled at runtime

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);                     // mandatory for ESP-NOW
  esp_now_init();

  // Broadcast peer (FF:FF:FF:FF:FF:FF)
  memset(&peer, 0xFF, 6);
  peer.channel   = 6;                      // pick any valid Wi-Fi CH
  peer.encrypt   = false;
  ESP_ERROR_CHECK(esp_now_add_peer(&peer));

  for (uint8_t pin : IR_PINS) pinMode(pin, INPUT_PULLUP);

  Serial.println(F("[TRACK] ready"));
}

void loop() {
  SensorFrame pkt{};
  for (uint8_t i = 0; i < 5; ++i)
    pkt.bitmap |= (!digitalRead(IR_PINS[i])) << i;   // active-LOW → 1
  pkt.millisUTC = millis();

  esp_now_send(nullptr, (uint8_t*)&pkt, sizeof(pkt));   // broadcast
  delay(50);                                            // 20 Hz
}
