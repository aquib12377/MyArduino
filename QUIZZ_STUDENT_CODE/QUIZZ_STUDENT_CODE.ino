/*
  STUDENT – 4-Button Clicker (ESP-NOW Broadcast)
  ----------------------------------------------
  - 4 buttons to GND with INPUT_PULLUP
  - Broadcasts compact AnswerMsg (8 bytes) to master on the same Wi-Fi channel
*/

#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>

// -------- Pins (adjust if needed) --------
#define BTN1 14
#define BTN2 27
#define BTN3 26
#define BTN4 25

// -------- Identity / radio --------
static const uint16_t DEVICE_ID = 1;     // <--- set 1..40 per unit
#define ESPNOW_WIFI_CHANNEL 11            // <--- set to your router's 2.4 GHz channel (e.g., 1/6/11)

// -------- Message --------
struct __attribute__((packed)) AnswerMsg {
  uint16_t id;
  uint8_t  q;     // not used; master tags question from Firebase
  uint8_t  ans;   // 1..4
  uint32_t seq;
};

class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk)
  : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}
  ~ESP_NOW_Broadcast_Peer(){ remove(); }
  bool begin(){ return ESP_NOW.begin() && add(); }
  bool send_message(const uint8_t *data, size_t len){ return send(data, len); }
};

ESP_NOW_Broadcast_Peer bpeer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);
static uint32_t seq = 0;

inline bool pressed(int pin){ return digitalRead(pin) == LOW; }

void sendAns(uint8_t ans){
  AnswerMsg m{ DEVICE_ID, 0, ans, ++seq };
  bool ok = bpeer.send_message((uint8_t*)&m, sizeof(m));
  Serial.printf("[TX] len=%u id=%u ans=%u seq=%lu -> %s\n",
                (unsigned)sizeof(m), DEVICE_ID, ans, (unsigned long)seq, ok?"OK":"FAIL");
}

void setup(){
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== STUDENT – Clicker ===");

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) delay(50);

  Serial.println("Wi-Fi:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC : " + WiFi.macAddress());
  Serial.printf("  CH  : %d\n", ESPNOW_WIFI_CHANNEL);

  if (!bpeer.begin()) {
    Serial.println("ESP-NOW init/register failed. Rebooting...");
    delay(2000); ESP.restart();
  }
  Serial.println("Ready. Press any button 1..4.");
}

void loop(){
  static uint32_t last=0;
  uint32_t now = millis();
  if (now - last > 200) { // debounce window
    if      (pressed(BTN1)) { sendAns(1); last = now; }
    else if (pressed(BTN2)) { sendAns(2); last = now; }
    else if (pressed(BTN3)) { sendAns(3); last = now; }
    else if (pressed(BTN4)) { sendAns(4); last = now; }
  }
}
