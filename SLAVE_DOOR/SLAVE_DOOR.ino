/*
   ESP-NOW Door Sensor – self-identifying version
   • Uses GPIO-15 (active-LOW) as the reed contact
   • Sends  "DOOR|<id>|CLOSED#<n>" on every close event
   • <id> = last-3-byte MAC (hex, upper-case) so it is unique
*/

#include "ESP32_NOW.h"
#include <WiFi.h>
#include <esp_mac.h>

/* ───────── user config ───────── */
#define WIFI_CH        6
#define DOOR_PIN       15           // active-LOW switch
#define DEBOUNCE_MS    50
#define PAYLOAD_MAX    48
/* ─────────────────────────────── */

class BroadcastPeer : public ESP_NOW_Peer {
public:
  BroadcastPeer(uint8_t ch) : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, ch, WIFI_IF_STA) {}
  bool begin()          { return ESP_NOW.begin() && add(); }
  bool sendMsg(const void *d, size_t l) { return send((uint8_t*)d, l); }
} bPeer(WIFI_CH);

uint8_t mac[6];
char    idStr[7];                    // 6 hex chars + NUL
uint32_t counter = 0;
bool    lastState = HIGH;
uint32_t lastMs   = 0;

/* ---------- send-status log ---------- */
void onSend(const uint8_t *mac, esp_now_send_status_t st) {
  Serial.printf("[SEND] → " MACSTR "  %s\n",
                MAC2STR(mac),
                st == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

/* ---------- setup ---------- */
void setup() {
  Serial.begin(115200);

  pinMode(DOOR_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(WIFI_CH);
  while (!WiFi.STA.started()) delay(50);

  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  sprintf(idStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);   // A945C8 etc.

  Serial.printf("\n[SENSOR] MAC=%s  ID=%s  CH=%d\n",
                WiFi.macAddress().c_str(), idStr, WIFI_CH);

  if (!bPeer.begin()) {
    Serial.println("[FATAL] ESP-NOW init failed – rebooting");
    delay(3000); ESP.restart();
  }
  esp_now_register_send_cb(onSend);
  Serial.println("[READY] waiting for door events");
}

/* ---------- loop ---------- */
void loop() {
  bool state = digitalRead(DOOR_PIN);           // HIGH=open, LOW=closed
  uint32_t now = millis();

  if (state != lastState && now - lastMs > DEBOUNCE_MS) {
    lastMs = now;
    lastState = state;

    if (state == HIGH) {                         // door just closed
      char payload[PAYLOAD_MAX];
      snprintf(payload, sizeof(payload),
               "DOOR|%s|CLOSED#%lu", idStr, ++counter);

      Serial.printf("[EVNT] %s – sending…\n", payload);
      if (!bPeer.sendMsg(payload, strlen(payload) + 1))
        Serial.println("[ERR ] esp_now_send immediate fail");
    }
  }
  delay(5);
}
