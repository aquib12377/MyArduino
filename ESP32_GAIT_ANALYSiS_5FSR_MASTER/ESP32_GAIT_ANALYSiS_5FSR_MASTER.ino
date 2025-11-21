/*
    ESP-NOW Broadcast FSR Master
    - Reads 5x FSRs on ADC pins
    - Sends FsrPacket via ESP32_NOW broadcast
    - Compatible with the SD logger slave you just made

    FSR pins (adjust to your wiring):
      FSR0 -> GPIO 32
      FSR1 -> GPIO 33
      FSR2 -> GPIO 34
      FSR3 -> GPIO 35
      FSR4 -> GPIO 36
*/

#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>  // For MAC2STR and MACSTR

/* Definitions */

#define ESPNOW_WIFI_CHANNEL 6

// Use ADC1 pins only
static const uint8_t FSR_PINS[5] = {32, 33, 34, 35, 36};

/* FSR Packet – must match slave */

#pragma pack(push, 1)
struct FsrPacket {
  uint8_t  magic[4];     // 'F','S','R','1'
  uint32_t seq;
  uint32_t msec;
  uint16_t raw[5];
  float    volt[5];
};
#pragma pack(pop)

/* Broadcast Peer Class */

class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk)
    : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}

  ~ESP_NOW_Broadcast_Peer() {
    remove();
  }

  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      log_e("Failed to initialize ESP-NOW or register the broadcast peer");
      return false;
    }
    return true;
  }

  bool send_packet(const uint8_t *data, size_t len) {
    if (!send(data, len)) {
      log_e("Failed to broadcast FSR packet");
      return false;
    }
    return true;
  }
};

/* Globals */

ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);
uint32_t g_seq = 0;

/* Helpers */

void fillFsrPacket(FsrPacket &pkt) {
  // Magic
  pkt.magic[0] = 'F';
  pkt.magic[1] = 'S';
  pkt.magic[2] = 'R';
  pkt.magic[3] = '1';

  pkt.seq  = g_seq++;
  pkt.msec = millis();

  for (int i = 0; i < 5; ++i) {
    uint16_t r = analogRead(FSR_PINS[i]);
    pkt.raw[i]  = r;
    pkt.volt[i] = (float)r * (3.3f / 4095.0f);  // adjust if your Vref is different
  }
}

void printPacketDebug(const FsrPacket &pkt) {
  Serial.print(F("[SEND] seq="));
  Serial.print(pkt.seq);
  Serial.print(F(" ms="));
  Serial.print(pkt.msec);
  Serial.print(F(" RAW="));
  for (int i = 0; i < 5; ++i) {
    Serial.print(pkt.raw[i]);
    if (i < 4) Serial.print(',');
  }
  Serial.print(F("  V="));
  for (int i = 0; i < 5; ++i) {
    Serial.print(pkt.volt[i], 3);
    if (i < 4) Serial.print(',');
  }
  Serial.println();
}

/* Main */

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("ESP-NOW FSR Example - Broadcast Master");
  Serial.println("Wi-Fi parameters:");

  // Wi-Fi init
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // FSR pins as inputs
  for (int i = 0; i < 5; ++i) {
    pinMode(FSR_PINS[i], INPUT);
  }
  analogReadResolution(12);  // 0..4095

  // Register broadcast peer
  if (!broadcast_peer.begin()) {
    Serial.println("Failed to initialize broadcast peer");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Setup complete. Broadcasting FSR packets...");
}

void loop() {
  static uint32_t lastSend = 0;
  const uint32_t SEND_INTERVAL_MS = 200;   // 5 Hz

  uint32_t now = millis();
  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;

    FsrPacket pkt;
    fillFsrPacket(pkt);

    // Debug print
    printPacketDebug(pkt);

    // Broadcast as raw bytes
    if (!broadcast_peer.send_packet((uint8_t*)&pkt, sizeof(pkt))) {
      Serial.println("Failed to broadcast FSR packet");
    }
  }
}
