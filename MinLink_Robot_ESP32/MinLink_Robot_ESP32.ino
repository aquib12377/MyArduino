/*
  ESP32 ↔ FT&S MINDLINK (NeuroSky TGAM1)
  Dual-core version:
    - Core 0: Bluetooth ThinkGear parser
    - Core 1: Motor controller (L298N, EN jumpers tied HIGH)

  New logic:
    - If Attention >= 50 => keep doing one cycle (Forward PHASE_MS, Stop PHASE_MS_OFF)
      until Attention falls below 50, then stop.
*/

#include <Arduino.h>
#include "BluetoothSerial.h"
BluetoothSerial BT;

// ================== BT TARGET =========================
const char* TARGET_NAME = "Mind Link";   // or "MindWave Mobile", "BrainLink", "MindLink"
//uint8_t TARGET_MAC[6] = {0x0D,0x00,0x18,0x11,0x30,0xFA}; // your MAC
uint8_t TARGET_MAC[6] = {0x0D,0x00,0x18,0x11,0x3E,0xF0}; // 2nd MindBlink

// ================== ThinkGear =========================
static const uint8_t SYNC = 0xAA;
enum {
  CODE_POOR_SIGNAL = 0x02,
  CODE_ATTENTION   = 0x04,
  CODE_MEDITATION  = 0x05,
  CODE_BLINK       = 0x16,
  CODE_RAW_WAVE    = 0x80, // 2 bytes (big-endian signed)
  CODE_EEG_POWER   = 0x83  // 24 bytes (8 × 3-byte big-endian: delta..gammaH)
};

// ================== State / Smoothing =================
volatile uint8_t gQual=200, gAttn=0, gMed=0, gBlink=0;
volatile int16_t gRaw=0;
float emaAttn=0, emaMed=0; const float ALPHA=0.2f;

// RAW RMS over sliding window (for your plotter/HUD)
const int RAW_WIN = 256;
int16_t rawBuf[RAW_WIN]; int rawIdx=0; long rawSumSq=0;
uint16_t gRMS=0;

// EEG band powers (scaled integers)
uint32_t band[8]; // Δ Θ αL αH βL βH γL γH

// ================== Motor control (L298N, EN jumpers high) ========
const int M1_IN1 = 26;  // L298N IN1
const int M1_IN2 = 27;  // L298N IN2
const int M2_IN3 = 14;  // L298N IN3
const int M2_IN4 = 12;  // L298N IN4

inline void motorsForward() {
  digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN3, HIGH); digitalWrite(M2_IN4, LOW);
}
inline void motorsStop() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN3, LOW); digitalWrite(M2_IN4, LOW);
}

// ======== Parameters ========
const int ATTENTION_ON_TH   = 50;     // start/run while >= 50
const int ATTENTION_OFF_TH  = 49;     // stop when < 50 (tiny hysteresis)
const uint32_t PHASE_MS     = 100;    // your current ON time (e.g., 200)
const uint32_t PHASE_MS_OFF = 2000;   // your current OFF time (e.g., 200)

// ======== Inter-task signaling ========
TaskHandle_t taskParser = nullptr;
TaskHandle_t taskMotor  = nullptr;
volatile bool runningCycle = false;   // true while the motor task is cycling

// ================== Utils =============================
static inline void pushRaw(int16_t v){
  int16_t old = rawBuf[rawIdx];
  rawSumSq -= (long)old * (long)old;
  rawBuf[rawIdx] = v;
  rawSumSq += (long)v * (long)v;
  rawIdx = (rawIdx+1) % RAW_WIN;
  gRMS = (uint16_t)sqrtf((float)rawSumSq / RAW_WIN);
}

static inline void printBars(){
  auto bar = [](int v){
    char s[22]; int n = (v*18)/100; for(int i=0;i<18;i++) s[i] = i<n ? 0xDB : '-'; s[18]=0;
    return String(s);
  };
  static uint32_t last=0; uint32_t now=millis(); if(now-last<200) return; last=now;
  Serial.printf("ATTN [%s] %3d | MED [%s] %3d | QUAL %3d | BLINK %3d | RMS %3d\n",
    bar((int)emaAttn).c_str(), (int)emaAttn,
    bar((int)emaMed).c_str(),  (int)emaMed,
    (int)gQual, (int)gBlink, (int)gRMS);
}

static inline void plotter(){
  static uint32_t last=0; uint32_t now=millis(); if(now-last<80) return; last=now;
  Serial.printf("ATTN:%d MED:%d QUAL:%d RMS:%d\n",
    (int)emaAttn, (int)emaMed, (int)gQual, (int)gRMS);
}

// ================== Core 0: Parser Task =================
void parserTask(void*){
  if (!BT.begin("ESP32-Mindlink", true)) { Serial.println("BT init failed"); vTaskDelete(NULL); }
  Serial.println("Connecting… (ensure headband is on and not paired elsewhere)");
  bool ok=false;
  ok = BT.connect(TARGET_MAC);
  if (!ok) ok = BT.connect(TARGET_NAME);
  if (!ok){ Serial.println("Connect failed"); vTaskDelete(NULL); }
  Serial.println("Connected! Syncing…");

  uint8_t payload[169];
  uint8_t lastAttnForTrigger = 0;

  for(;;){
    // ---- sync on 0xAA 0xAA ----
    int b=-1;
    do { while (!BT.available()) vTaskDelay(1); b = BT.read(); } while (b != SYNC);
    while (!BT.available()) vTaskDelay(1);
    if (BT.read() != SYNC) continue;

    // ---- length ----
    while (!BT.available()) vTaskDelay(1);
    int payloadLen = BT.read();
    if (payloadLen < 0 || payloadLen > 169) continue;

    // ---- payload + checksum ----
    uint8_t sum=0;
    for (int i=0;i<payloadLen;i++){
      while (!BT.available()) vTaskDelay(1);
      payload[i] = (uint8_t)BT.read();
      sum += payload[i];
    }
    while (!BT.available()) vTaskDelay(1);
    uint8_t recvChk = (uint8_t)BT.read();
    if (recvChk != ((~sum)&0xFF)) continue;

    // ---- parse TLV ----
    for (int i=0;i<payloadLen; ){
      uint8_t code = payload[i++];
      if (code==0x55) continue;
      uint8_t vlen = 1;
      if (code >= 0x80) vlen = payload[i++];
      if (i+vlen > payloadLen) break;

      switch (code){
        case CODE_POOR_SIGNAL: { gQual = payload[i]; break; }
        case CODE_ATTENTION: {
          gAttn = payload[i];
          emaAttn = (1-ALPHA)*emaAttn + ALPHA*(float)gAttn;

          // Start/ensure motor cycling if at or above threshold
          if (!runningCycle && ( (lastAttnForTrigger < ATTENTION_ON_TH && gAttn >= ATTENTION_ON_TH) || (gAttn >= ATTENTION_ON_TH) )) {
            if (taskMotor) xTaskNotifyGive(taskMotor);
          }
          lastAttnForTrigger = gAttn;
          break;
        }
        case CODE_MEDITATION: { gMed = payload[i]; emaMed = (1-ALPHA)*emaMed + ALPHA*(float)gMed; break; }
        case CODE_BLINK: { gBlink = payload[i]; break; }
        case CODE_RAW_WAVE: {
          if (vlen==2){ gRaw = (int16_t)((payload[i]<<8) | payload[i+1]); pushRaw(gRaw); }
          break;
        }
        case CODE_EEG_POWER: {
          if (vlen==24){
            for (int k=0;k<8;k++){
              uint32_t v = ((uint32_t)payload[i+3*k]<<16) | ((uint32_t)payload[i+3*k+1]<<8) | (uint32_t)payload[i+3*k+2];
              band[k]=v;
            }
          }
          break;
        }
        default: break;
      }
      i += vlen;
    }

    plotter();
    printBars();
  }
}

// ================== Core 1: Motor Task =================
void motorTask(void*){
  const TickType_t onTicks  = pdMS_TO_TICKS(PHASE_MS);
  const TickType_t offTicks = pdMS_TO_TICKS(PHASE_MS_OFF);

  for(;;){
    // Wait until asked to run
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (runningCycle) continue;      // already running, ignore extra triggers
    runningCycle = true;
    Serial.println(F("[MOTOR] Cycle START (run while Attention >= threshold)"));

    // Ensure pins are ready
    pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
    pinMode(M2_IN3, OUTPUT); pinMode(M2_IN4, OUTPUT);

    // Schedule with vTaskDelayUntil for stable timing
    TickType_t t0 = xTaskGetTickCount();

    // Loop ON/OFF as long as Attention is above (or equal to) the ON threshold
    while (gAttn >= ATTENTION_ON_TH) {
      // Forward phase
      motorsForward();
      vTaskDelayUntil(&t0, onTicks);

      // Check if we should stop before doing OFF phase
      if (gAttn < ATTENTION_OFF_TH) break;

      // Stop phase
      motorsStop();
      vTaskDelayUntil(&t0, offTicks);
    }

    motorsStop();
    runningCycle = false;
    Serial.println(F("[MOTOR] Cycle STOP (Attention fell below threshold)"));
  }
}

// ================== Arduino setup/loop =================
void setup() {
  Serial.begin(115200);
  delay(200);

  // init RMS buffer
  for (int i=0;i<RAW_WIN;i++) rawBuf[i]=0;

  // Motor pins safe state
  pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
  pinMode(M2_IN3, OUTPUT); pinMode(M2_IN4, OUTPUT);
  motorsStop();

  // Create tasks pinned to cores
  xTaskCreatePinnedToCore(parserTask, "parser", 6144, NULL, 2, &taskParser, 0); // Core 0
  xTaskCreatePinnedToCore(motorTask,  "motor",  4096, NULL, 3, &taskMotor,  1); // Core 1
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
