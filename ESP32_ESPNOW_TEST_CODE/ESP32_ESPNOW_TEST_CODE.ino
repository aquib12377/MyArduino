/*
   ESP-NOW sniffer – prints every packet it hears (channel-locked)
   Works on both Arduino-ESP32 2.x (IDF-v4) and 3.x (IDF-v5).

   ─── set this to the SAME channel your sensor uses ───
*/
#define WIFI_CH  6

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

/* ---------- helpers ---------- */
static void printMac(const uint8_t *mac)
{
  for (int i = 0; i < 6; ++i) {
    if (i) Serial.write(':');
    Serial.printf("%02X", mac[i]);
  }
}

/* ---------- receive callback (v4 & v5 compatible) ---------- */
#if ESP_IDF_VERSION_MAJOR >= 5          // Arduino-ESP32 ≥ 3.0
void onRx(const esp_now_recv_info_t *info,
          const uint8_t             *data,
          int                        len)
{
  Serial.printf("[RX] %d B  ", len);
  printMac(info->src_addr);
  Serial.printf("  \"%.*s\"\n", len, (const char *)data);
}
#else                                    // Arduino-ESP32 1.x / 2.x
void onRx(const uint8_t *mac,
          const uint8_t *data,
          int            len)
{
  Serial.printf("[RX] %d B  ", len);
  printMac(mac);
  Serial.printf("  \"%.*s\"\n", len, (const char *)data);
}
#endif

/* ---------- setup ---------- */
void setup()
{
  Serial.begin(115200);

  /* lock the radio on channel WIFI_CH and keep it there */
  WiFi.mode(WIFI_AP);                       // AP-only → fixed channel
  WiFi.softAP("espnow_sniffer", nullptr, WIFI_CH);
  Serial.printf("[INFO] Soft-AP up, CH-%d\n", WIFI_CH);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[FATAL] esp_now_init() failed");
    for (;;) delay(1000);
  }
  esp_now_register_recv_cb(onRx);
  Serial.println("[READY] waiting for ESP-NOW frames…");
}

void loop() { delay(100); }
