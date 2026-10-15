/*  SLAVE  -  ESP32 30-pin (WROOM)  [Adafruit NeoPixel build]
 *  Two FSRs (left & right foot). Alternating L→R or R→L pair advances LEDs.
 *  Same foot pressed repeatedly is ignored.
 *  When strip is full -> notify master over ESP-NOW.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_NeoPixel.h>
#define EVT_STEP     0   // sent on every valid alternating press
#define EVT_COMPLETE 1   // sent when strip is full
// ----------------------- CONFIG (edit per build) -----------------------
#define SLAVE_ID         4
uint8_t masterAddress[] = {0x20,0x9B,0xA9,0x6F,0x57,0xA4};

#define LED_PIN          13
#define NUM_LEDS         300
#define BRIGHTNESS       80
#define LEDS_PER_PRESS   1          // LEDs advanced per valid alternating pair

#define FSR_PIN_L        34         // Left  foot — ADC1 only: 32,33,34,35,36,39
#define FSR_PIN_R        35         // Right foot — ADC1 only
#define PRESS_THRESH     1500
#define RELEASE_THRESH   800
#define DEBOUNCE_MS      40

#define COMPLETE_HOLD_MS 2000
#define COMPLETE_RETRIES 3
// -----------------------------------------------------------------------

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
uint32_t FILL_COLOR;

int      litCount   = 0;
bool     completed  = false;
uint32_t completeAt = 0;

// Per-FSR debounce state
bool     armed_L = true,  armed_R = true;
uint32_t lastEdge_L = 0,  lastEdge_R = 0;

// Alternation tracker: 0 = nothing yet, 1 = left last, 2 = right last
uint8_t  lastFoot = 0;

typedef struct { uint8_t slaveId; uint8_t event; } msg_t;
msg_t msg;
esp_now_peer_info_t peerInfo;

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.printf("[INFO] send: %s\n", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

void lightRange(int from, int to) {
  if (from < 0) from = 0;
  if (to > NUM_LEDS) to = NUM_LEDS;
  for (int i = from; i < to; i++) strip.setPixelColor(i, FILL_COLOR);
  strip.show();
}

void clearAll() { strip.clear(); strip.show(); }

void sendStep() {
  msg.slaveId = SLAVE_ID;
  msg.event   = EVT_STEP;
  esp_now_send(masterAddress, (uint8_t*)&msg, sizeof(msg));
  Serial.println("[INFO] step event sent to master");
}

void sendComplete() {
  msg.slaveId = SLAVE_ID;
  msg.event   = EVT_COMPLETE;
  for (int i = 0; i < COMPLETE_RETRIES; i++) {
    esp_now_send(masterAddress, (uint8_t*)&msg, sizeof(msg));
    delay(30);
  }
}

// Called only when a valid L→R or R→L alternating step is confirmed
// ── replace advanceLED() ────────────────────────────────────────────────
void advanceLED() {
  int prev = litCount;
  litCount += LEDS_PER_PRESS;
  if (litCount > NUM_LEDS) litCount = NUM_LEDS;
  lightRange(prev, litCount);

  Serial.printf("[INFO] LED advance -> %d / %d\n", litCount, NUM_LEDS);

  if (litCount >= NUM_LEDS) {
    sendComplete();           // retried 3× — strip full
    completed  = true;
    completeAt = millis();
  } else {
    sendStep();               // single send — each valid alternating step
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(FSR_PIN_L, INPUT);
  pinMode(FSR_PIN_R, INPUT);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  FILL_COLOR = strip.Color(220, 20, 60);   // Crimson
  clearAll();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) { Serial.println("[ERROR] espnow init fail"); return; }
  esp_now_register_send_cb(onSent);

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) Serial.println("[ERROR] add peer fail");

  Serial.printf("[BOOT] Slave %d ready. NUM_LEDS=%d, dual-FSR alternating mode\n",
                SLAVE_ID, NUM_LEDS);
}

void loop() {
  // Hold completed state, then auto-reset for next round
  if (completed) {
    if (millis() - completeAt >= COMPLETE_HOLD_MS) {
      completed = false;
      litCount  = 0;
      lastFoot  = 0;        // reset alternation for next round
      armed_L   = true;
      armed_R   = true;
      clearAll();
    }
    return;
  }

  uint32_t now = millis();
  int vL = analogRead(FSR_PIN_L);
  int vR = analogRead(FSR_PIN_R);
  // Serial.printf("L=%d  R=%d\n", vL, vR);  // uncomment to calibrate thresholds

  // ── Left foot ──────────────────────────────────────────────────────────
  if (armed_L && vL > PRESS_THRESH && (now - lastEdge_L) > DEBOUNCE_MS) {
    armed_L    = false;
    lastEdge_L = now;
    Serial.println("[DBG] Left press");

    if (lastFoot == 2) advanceLED();  // right was last → valid alternating pair
    lastFoot = 1;                     // record left as the most recent foot
  }
  if (!armed_L && vL < RELEASE_THRESH && (now - lastEdge_L) > DEBOUNCE_MS) {
    armed_L    = true;
    lastEdge_L = now;
  }

  // ── Right foot ─────────────────────────────────────────────────────────
  if (armed_R && vR > PRESS_THRESH && (now - lastEdge_R) > DEBOUNCE_MS) {
    armed_R    = false;
    lastEdge_R = now;
    Serial.println("[DBG] Right press");

    if (lastFoot == 1) advanceLED();  // left was last → valid alternating pair
    lastFoot = 2;                     // record right as the most recent foot
  }
  if (!armed_R && vR < RELEASE_THRESH && (now - lastEdge_R) > DEBOUNCE_MS) {
    armed_R    = true;
    lastEdge_R = now;
  }
}