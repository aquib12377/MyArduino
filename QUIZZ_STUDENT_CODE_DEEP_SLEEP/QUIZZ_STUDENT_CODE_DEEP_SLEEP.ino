/*
  STUDENT – 4-Button Clicker (ESP-NOW Broadcast) + Deep Sleep (Active HIGH buttons)
  - Buttons pulled down, go HIGH when pressed
  - Wake on ANY button press (EXT1 ANY_HIGH)
*/

#include "ESP32_NOW.h"
#include "WiFi.h"
#include "esp_sleep.h"

// -------- Pins (RTC capable) --------
#define BTN1 32
#define BTN2 33
#define BTN3 25
#define BTN4 26

static const uint16_t DEVICE_ID = 4;
#define ESPNOW_WIFI_CHANNEL 11

struct __attribute__((packed)) AnswerMsg {
  uint16_t id;
  uint8_t  q;
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
RTC_DATA_ATTR static uint32_t seq = 0;

// ACTIVE HIGH now
inline bool pressed(int pin){ return digitalRead(pin) == HIGH; }

void printWakeReason() {
  esp_sleep_wakeup_cause_t c = esp_sleep_get_wakeup_cause();
  Serial.print("Wake cause: ");
  switch (c) {
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("EXT1"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("TIMER"); break;
    case ESP_SLEEP_WAKEUP_UNDEFINED: Serial.println("UNDEFINED/POWERON"); break;
    default: Serial.println((int)c); break;
  }
}

// Stable button detect (debounce + noise filter)
uint8_t detectPressedButton() {
  delay(15); // settle after wake

  int c1=0,c2=0,c3=0,c4=0;
  for (int i=0; i<12; i++) {
    c1 += pressed(BTN1);
    c2 += pressed(BTN2);
    c3 += pressed(BTN3);
    c4 += pressed(BTN4);
    delay(2);
  }

  if (c1 >= 7) return 1;
  if (c2 >= 7) return 2;
  if (c3 >= 7) return 3;
  if (c4 >= 7) return 4;
  return 0;
}

void setupWake() {
  uint64_t mask =
    (1ULL << BTN1) |
    (1ULL << BTN2) |
    (1ULL << BTN3) |
    (1ULL << BTN4);

  // Buttons are pulled DOWN; press makes pin HIGH -> wake on ANY_HIGH
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
}

void goSleep() {
  Serial.println("Going to deep sleep (wake on any button HIGH)...");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

void sendAns(uint8_t ans) {
  AnswerMsg m{ DEVICE_ID, 0, ans, ++seq };

  for (int i=0; i<3; i++) {
    bool ok = bpeer.send_message((uint8_t*)&m, sizeof(m));
    Serial.printf("[TX] try=%d id=%u ans=%u seq=%lu -> %s\n",
                  i+1, DEVICE_ID, ans, (unsigned long)seq, ok?"OK":"FAIL");
    delay(25);
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // ACTIVE HIGH -> use pulldown
  pinMode(BTN1, INPUT_PULLDOWN);
  pinMode(BTN2, INPUT_PULLDOWN);
  pinMode(BTN3, INPUT_PULLDOWN);
  pinMode(BTN4, INPUT_PULLDOWN);

  setupWake();
  printWakeReason();

  uint8_t ans = detectPressedButton();
  if (ans == 0) {
    Serial.println("No stable button detected. Sleeping again.");
    goSleep();
  }

  // Start radio only after valid press
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  uint32_t t0 = millis();
  while (!WiFi.STA.started() && (millis()-t0) < 1000) delay(10);

  if (!bpeer.begin()) {
    Serial.println("ESP-NOW init failed. Sleeping.");
    goSleep();
  }

  Serial.printf("Button %u pressed -> sending\n", ans);
  sendAns(ans);

  delay(80); // let last TX finish
  goSleep();
}

void loop() {}
