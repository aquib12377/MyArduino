/*****************************************************************
  TRACK-MONITOR – broadcasts 5-sensor bitmap via ESP-NOW
*****************************************************************/
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

/* ---------- user settings ---------------------------------- */
constexpr uint8_t IR_PINS[5] = {13,12,14,27,26};   // active-LOW
//00:4B:12:2F:AD:24
uint8_t  BOT_MAC[6] = {0x00, 0x4B, 0x12, 0x2F, 0xAD, 0x24}; // <-- fill in!
constexpr uint8_t RF_CH = 11;            // hotspot’s 2.4 GHz channel

/* ---------- wire format ------------------------------------ */
struct __attribute__((packed)) SensorFrame {
  uint8_t  bitmap;        // bit0=S1 … bit4=S5  (1 = object)
  uint32_t ts;            // local millis()
};
static_assert(sizeof(SensorFrame) == 5, "Struct must stay 5 bytes");

/* ---------- globals ---------------------------------------- */
SensorFrame pkt{0, 0};

/* ---------- send callback ---------------------------------- */
void onSend(const uint8_t* mac, esp_now_send_status_t status)
{
  Serial.printf("[SEND] to %02X%02X%02X … %s\n",
                mac[3], mac[4], mac[5],
                status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Track-Monitor boot ===");

  /* Wi-Fi & ESP-NOW  --------------------------------------- */
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);                     // no power save
  ESP_ERROR_CHECK(esp_wifi_set_channel(RF_CH, WIFI_SECOND_CHAN_NONE));

  ESP_ERROR_CHECK(esp_now_init());
  esp_now_register_send_cb(onSend);

  /* Add peers:  (1) unicast to bot  (2) broadcast  --------- */
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, BOT_MAC, 6);
  p.channel = RF_CH;  p.encrypt = false;
  ESP_ERROR_CHECK(esp_now_add_peer(&p));

  memset(p.peer_addr, 0xFF, 6);                      // broadcast
  ESP_ERROR_CHECK(esp_now_add_peer(&p));

  /* IR inputs ---------------------------------------------- */
  for (uint8_t pin : IR_PINS) pinMode(pin, INPUT_PULLUP);

  Serial.printf("[INFO] MAC  : %s\n", WiFi.macAddress().c_str());
  Serial.printf("[INFO] CH   : %u\n", RF_CH);
  Serial.println("[READY] sending every 50 ms");
}

void loop()
{
  /* build bitmap */
  pkt.bitmap = 0;
  for (uint8_t i = 0; i < 5; ++i)
    pkt.bitmap |= (!digitalRead(IR_PINS[i])) << i;
  pkt.ts = millis();

  /* debug print */
  Serial.printf("[LOOP] t=%lu  bits=%02X\n", pkt.ts, pkt.bitmap);

  /* send  (nullptr = broadcast first, then ACKed unicast) */
  esp_now_send(nullptr, reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
  esp_now_send(BOT_MAC, reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));

  delay(50);                 // 20 Hz
}
