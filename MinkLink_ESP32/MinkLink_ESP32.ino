/*
  ESP32 ↔ FT&S MINDLINK (NeuroSky TGAM1) – Visual Dashboard
  - Classic BT SPP client @57600, ThinkGear packets
  - Serial Plotter: ATTN, MED, QUAL, RMS
  - Serial HUD (ASCII bars)
  - Optional OLED SSD1306 (USE_OLED)
  - Optional Web UI over Wi-Fi (USE_WEBUI) with JSON polling

  Core: Arduino-ESP32 2.x
*/

#include "BluetoothSerial.h"
BluetoothSerial BT;

// ================== BT TARGET =========================
const char* TARGET_NAME = "Mind Link";   // or "MindWave Mobile", "BrainLink", "MindLink"
uint8_t TARGET_MAC[6] = {0x0D,0x00,0x18,0x11,0x30,0xFA}; // your MAC

// ================== OLED (optional) ===================

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
float emaAttn=0, emaMed=0; const float ALPHA=0.2f; // EMA smoothing
// RAW RMS over sliding window
const int RAW_WIN = 256;
int16_t rawBuf[RAW_WIN]; int rawIdx=0; long rawSumSq=0;
uint16_t gRMS=0;

// EEG band powers (scaled integers)
uint32_t band[8]; // Δ Θ αL αH βL βH γL γH

// ================== Utils =============================
int readByte() {
  while (!BT.available()) {}
  return BT.read();
}
void skipToSync() {
  int b;
  do { b = readByte(); } while (b != SYNC);
  if (readByte() != SYNC) skipToSync();
}

void pushRaw(int16_t v){
  // update ring RMS: remove old^2, add new^2
  int16_t old = rawBuf[rawIdx];
  rawSumSq -= (long)old * (long)old;
  rawBuf[rawIdx] = v;
  rawSumSq += (long)v * (long)v;
  rawIdx = (rawIdx+1) % RAW_WIN;
  gRMS = (uint16_t)sqrtf((float)rawSumSq / RAW_WIN);
}

// ================== Pretty Console Bars ===============
void printBars(){
  auto bar = [](int v){
    char s[22]; int n = (v*18)/100; for(int i=0;i<18;i++) s[i] = i<n ? 0xDB : '-'; s[18]=0; // █
    return String(s);
  };
  static uint32_t last=0; uint32_t now=millis(); if(now-last<200) return; last=now;

  Serial.printf("ATTN [%s] %3d | MED [%s] %3d | QUAL %3d | BLINK %3d | RMS %3d\n",
    bar((int)emaAttn).c_str(), (int)emaAttn,
    bar((int)emaMed).c_str(),  (int)emaMed,
    (int)gQual, (int)gBlink, (int)gRMS);
}

// ================== Serial Plotter Lines ==============
void plotter(){
  // Arduino Serial Plotter accepts "name:value name:value ..."
  static uint32_t last=0; uint32_t now=millis(); if(now-last<80) return; last=now;
  Serial.printf("ATTN:%d MED:%d QUAL:%d RMS:%d\n",
    (int)emaAttn, (int)emaMed, (int)gQual, (int)gRMS);
}

// ================== Setup / Loop ======================
void setup() {
  Serial.begin(115200);
  delay(200);

  // Init ring buffer
  for (int i=0;i<RAW_WIN;i++) rawBuf[i]=0;

  // Bluetooth client
  if (!BT.begin("ESP32-Mindlink", true)) { Serial.println("BT init failed"); while(1){} }
  Serial.println("Connecting… (ensure headband is on and not paired elsewhere)");
  bool ok=false;
  // Try MAC first (more reliable)
  ok = BT.connect(TARGET_MAC);
  if (!ok) ok = BT.connect(TARGET_NAME);
  if (!ok){ Serial.println("Connect failed"); while(1){ delay(1000);} }
  Serial.println("Connected! Syncing…");

}

void loop() {
  // Parse a ThinkGear packet
  skipToSync();
  int payloadLen = readByte();
  if (payloadLen < 0 || payloadLen > 169) return;

  uint8_t payload[169];
  uint8_t chksum = 0;
  for (int i=0;i<payloadLen;i++){ payload[i]=(uint8_t)readByte(); chksum += payload[i]; }
  uint8_t recvChk = (uint8_t)readByte();
  if (recvChk != ((~chksum)&0xFF)) return;

  for (int i=0;i<payloadLen; ){
    uint8_t code = payload[i++];
    if (code==0x55) continue; // extended code not used here

    uint8_t vlen = 1;
    if (code >= 0x80) vlen = payload[i++];

    if (i+vlen > payloadLen) break;

    switch (code){
      case CODE_POOR_SIGNAL:{
        gQual = payload[i];
        break;
      }
      case CODE_ATTENTION:{
        gAttn = payload[i];
        emaAttn = (1-ALPHA)*emaAttn + ALPHA*(float)gAttn;
        break;
      }
      case CODE_MEDITATION:{
        gMed = payload[i];
        emaMed = (1-ALPHA)*emaMed + ALPHA*(float)gMed;
        break;
      }
      case CODE_BLINK:{
        gBlink = payload[i];
        break;
      }
      case CODE_RAW_WAVE:{
        if (vlen==2){
          gRaw = (int16_t)((payload[i]<<8) | payload[i+1]);
          pushRaw(gRaw);
        }
        break;
      }
      case CODE_EEG_POWER:{
        // 8 bands × 3 bytes big-endian
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

  // Visuals
  plotter();     // Serial Plotter lines
  printBars();   // ASCII HUD
  
}
