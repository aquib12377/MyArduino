/*
 * Door node – ESP32-WROOM-32  (verbose debug)
 * Works with Arduino-ESP32 core ≥ 3.2.0
 */

#include "door_common.h"

/* ------------ customise per board ------------ */
#define DOOR_ID          0          // 0‥3
#define REED_PIN         15         // pick a safe GPIO (4/13/14/27/32/33)
/* --------------------------------------------- */

#define FORCED_CHANNEL   11          // 0 = don’t force; else 1-13

volatile esp_err_t lastQueueErr = ESP_OK;
volatile uint8_t   lastState    = CMD_CLOSE;

/* ------------ helpers ------------ */
const char* espErrToStr(esp_err_t e);   // forward-decl

void onSend(const uint8_t *mac, esp_now_send_status_t status)
{
  Serial.printf("[CB] to %02X:%02X:%02X:%02X:%02X:%02X  %s  (queue=%s)\n",
                mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],
                status == ESP_NOW_SEND_SUCCESS ? "✓ ACKED" : "✗ FAILED",
                espErrToStr(lastQueueErr));
}

const char* espErrToStr(esp_err_t e)
{
  switch (e) {
    case ESP_OK:                  return "ESP_OK";
    case ESP_ERR_ESPNOW_NOT_INIT: return "ESPNOW not init";
    case ESP_ERR_ESPNOW_ARG:      return "Bad arg";
    case ESP_ERR_ESPNOW_INTERNAL: return "Internal";
    case ESP_ERR_ESPNOW_NO_MEM:   return "No mem";
    case ESP_ERR_ESPNOW_NOT_FOUND:return "Peer not found";
    case ESP_ERR_ESPNOW_IF:       return "Wi-Fi IF err";
    default:                      return "Unknown";
  }
}

void IRAM_ATTR reedISR()
{
  uint8_t state = digitalRead(REED_PIN) ? CMD_CLOSE : CMD_OPEN;
  if (state == lastState) return;

  DoorMsg msg{ DOOR_ID, state, millis() };
  lastQueueErr = esp_now_send(MASTER_MAC, (uint8_t*)&msg, sizeof(msg));

  Serial.printf("[ISR] Door %u  %s  – send() %s\n",
                DOOR_ID+1,
                state == CMD_OPEN ? "OPEN" : "CLOSE",
                espErrToStr(lastQueueErr));

  lastState = state;
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  pinMode(REED_PIN, INPUT_PULLUP);
  attachInterrupt(REED_PIN, reedISR, CHANGE);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                       // disable modem-sleep
  esp_wifi_set_ps(WIFI_PS_NONE);              // same at low level

  if (FORCED_CHANNEL) {
    esp_wifi_set_channel(FORCED_CHANNEL, WIFI_SECOND_CHAN_NONE);
    Serial.printf("[BOOT] Forced channel %u\n", FORCED_CHANNEL);
  }

  uint8_t ch; wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);
  Serial.printf("[BOOT] Wi-Fi ch %u\n", ch);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[BOOT] ESP-NOW init FAIL"); while (true);
  }
  Serial.println("[BOOT] ESP-NOW init OK");

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, MASTER_MAC, 6);
  peer.channel = ch;      // must match master
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.printf("[BOOT] Peer added %02X:%02X:%02X:%02X:%02X:%02X\n",
                  MASTER_MAC[0],MASTER_MAC[1],MASTER_MAC[2],
                  MASTER_MAC[3],MASTER_MAC[4],MASTER_MAC[5]);
  else
    Serial.println("[BOOT] Peer add FAILED");

  esp_now_register_send_cb(onSend);

  reedISR();   // push initial state
}

void loop()
{
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 30000) {
    lastBeat = millis();
    Serial.printf("[HB] Alive | RSSI=%d dBm\n", WiFi.RSSI());
  }
  delay(10);
}
