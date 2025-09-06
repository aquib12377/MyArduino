/*****************************************************************************************
  Functionality : 5-Channel DISPENSER – **raw pulse commands**
                  • Command over Serial:  <channel 1-5> <pulse-count>  + Enter
                  • Each channel has its own mL↔pulse calibration (for on-screen L display only)
                  • Dispenses all channels concurrently, LOW-active pump / valve
  Date          : 19-Jul-2025
  Project       : Multi-Pump Dispenser – ESP32 (pulse-based)
  Author        : Mohammed Aquib Ansari
******************************************************************************************
  -- Pin map (edit if needed) --
      Pumps   : 12 14 27 26 25   (MOSFET gate, LOW = ON)
      Valves  : 13 15 32 33  4   (Relay,      LOW = OPEN)
      Sensors : 34 35 39 36 18   (FALLING, input-only pins 34-39)

  -- Serial -- 115 200 baud
******************************************************************************************/

#include <Arduino.h>
#include <pgmspace.h>

/* ────────── CONFIG ────────── */
constexpr uint8_t  NUM_CH    = 5;     // channels
constexpr uint32_t PRINT_MS  = 250;   // progress interval (ms)

/* GPIO lists (index = ch-1) */
const uint8_t pumpPin [NUM_CH] = {4,5,18,21,23};
const uint8_t valvePin[NUM_CH] = {16,17,19,22,15};
const uint8_t flowPin [NUM_CH] = {13,14,27,26,25};

/* Calibration tables (mL ↔ pulses) – one row per channel
   -- Used ONLY to print an approximate litre value while dispensing -- */
constexpr uint8_t N_PT = 19;
const uint16_t mlPt [NUM_CH][N_PT] PROGMEM = {
  {100,150,200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000},
  {100,150,200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000},
  {100,150,200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000},
  {100,150,200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000},
  {100,150,200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000}
};
const uint16_t pulPt[NUM_CH][N_PT] PROGMEM = {
  {130,200,270,335,405,470,540,600,670,740,810,880,950,1020,1090,1160,1230,1290,1365},
  {128,197,265,330,400,464,535,595,663,733,802,871,940,1010,1080,1150,1220,1288,1360},
  {135,205,275,340,410,478,548,610,680,748,820,890,960,1030,1100,1170,1240,1310,1380},
  {132,202,272,338,408,474,544,606,676,744,814,884,954,1024,1094,1164,1234,1304,1374},
  {129,199,269,334,404,469,539,601,670,739,809,879,949,1019,1089,1159,1229,1299,1369}
};

/* ────────── STATE ────────── */
volatile uint32_t pulseCount[NUM_CH] = {0};
uint32_t  targetPulses[NUM_CH]       = {0};
bool      dispensing  [NUM_CH]       = {false};
uint32_t  nextPrintMS [NUM_CH]       = {0};

/* ────────── ISR GENERATION (one per channel) ────────── */
#define MAKE_ISR(i) void IRAM_ATTR flowISR##i(){ ++pulseCount[i]; }
MAKE_ISR(0) MAKE_ISR(1) MAKE_ISR(2) MAKE_ISR(3) MAKE_ISR(4)
void (*isrTab[NUM_CH])() = {flowISR0, flowISR1, flowISR2, flowISR3, flowISR4};

/* ────────── GPIO helpers ────────── */
inline void pumpOn   (uint8_t ch){ digitalWrite(pumpPin [ch], LOW ); }
inline void pumpOff  (uint8_t ch){ digitalWrite(pumpPin [ch], HIGH); }
inline void valveOpen(uint8_t ch){ digitalWrite(valvePin[ch], LOW ); }
inline void valveClose(uint8_t ch){digitalWrite(valvePin[ch], HIGH);}

/* ────────── Helper: pulses → litres (rough) ────────── */
float pulsesToLitres(uint8_t ch, uint32_t pulses)
{
  /* use ratio of last calibration point for speed */
  return pulses * pgm_read_word(&mlPt [ch][N_PT-1]) /
                (1000.0f * pgm_read_word(&pulPt[ch][N_PT-1]));
}

/* ────────── START & STOP ────────── */
void startDispense(uint8_t ch, uint32_t pulses)
{
  if (ch>=NUM_CH || pulses==0 || dispensing[ch]) return;

  targetPulses[ch] = pulses;
  pulseCount [ch]  = 0;            // atomic on Xtensa (32-bit)
  valveOpen(ch);  pumpOn(ch);
  dispensing[ch]  = true;
  nextPrintMS[ch] = millis();

  Serial.printf("\n▶ CH%u  target %lu pulses  (≈ %.3f L)\n",
                ch+1, pulses, pulsesToLitres(ch, pulses));
}

void stopDispense(uint8_t ch)
{
  pumpOff(ch); valveClose(ch); dispensing[ch]=false;
  uint32_t pc = pulseCount[ch];
  Serial.printf("✔ CH%u  %lu pulses  (≈ %.3f L)\n",
                ch+1, pc, pulsesToLitres(ch, pc));
}

/* ────────── Serial parser  <ch> <pulses> ────────── */
void pollSerial()
{
  static String line;
  while (Serial.available()){
    char c = Serial.read();
    if (c=='\r') continue;
    if (c=='\n'){
      uint8_t ch; uint32_t pulses;
      if (sscanf(line.c_str(), "%hhu %lu", &ch, &pulses)==2 && pulses>0){
        startDispense(ch-1, pulses);
      } else {
        Serial.println(F("Err → format:  <ch 1-5> <pulse-count>"));
      }
      line.clear();
    }else line += c;
  }
}

/* ────────── SETUP ────────── */
void setup()
{
  Serial.begin(115200);
  Serial.println(F("\n=== 5-Channel Dispenser – pulse mode ==="));
  Serial.println(F("Type:  <channel 1-5> <pulses>   e.g.  2 2600"));

  for (uint8_t i=0;i<NUM_CH;++i){
    pinMode(pumpPin [i], OUTPUT); pumpOff (i);
    pinMode(valvePin[i], OUTPUT); valveClose(i);
    pinMode(flowPin [i], INPUT );                     // ext. pull-up recommended
    attachInterrupt(digitalPinToInterrupt(flowPin[i]), isrTab[i], FALLING);
  }
}

/* ────────── LOOP ────────── */
void loop()
{
  pollSerial();
  uint32_t now = millis();

  for (uint8_t ch=0; ch<NUM_CH; ++ch){
    if (!dispensing[ch]) continue;

    uint32_t pc = pulseCount[ch];
    if (pc >= targetPulses[ch]) { stopDispense(ch); continue; }

    if (now - nextPrintMS[ch] >= PRINT_MS){
      nextPrintMS[ch] = now;
      Serial.printf("… CH%u  %lu / %lu\n", ch+1, pc, targetPulses[ch]);
    }
  }
}
