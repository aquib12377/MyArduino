#include "ESP32_NOW.h"
#include "WiFi.h"

#define ESPNOW_WIFI_CHANNEL 6

int EMGPin = 34;
int EMGVal = 0;

class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk)
    : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}

  ~ESP_NOW_Broadcast_Peer() { remove(); }

  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      log_e("Failed to initialize ESP-NOW or register the broadcast peer");
      return false;
    }
    return true;
  }

  bool send_message(const uint8_t *data, size_t len) {
    if (!send(data, len)) {
      log_e("Failed to broadcast message");
      return false;
    }
    return true;
  }
};

ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);

void setup() {
  Serial.begin(115200);
  pinMode(EMGPin, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) delay(100);

  Serial.println("ESP-NOW EMG Transmitter");
  Serial.println("MAC Address: " + WiFi.macAddress());

  if (!broadcast_peer.begin()) {
    Serial.println("Failed to initialize broadcast peer. Rebooting...");
    delay(5000);
    ESP.restart();
  }
}

void loop() {
  EMGVal = analogRead(EMGPin);
  Serial.println(EMGVal);
 if(EMGVal < 3000){
  char emgData[16];
 
  snprintf(emgData, sizeof(emgData), "%d", EMGVal);

  Serial.printf("Broadcasting EMG value: %s\n", emgData);
 
  if (!broadcast_peer.send_message((uint8_t *)emgData, strlen(emgData) + 1)) {
    Serial.println("Broadcast Failed");
  }

}
}
