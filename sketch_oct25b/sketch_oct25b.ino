/****************************************************************
  UNO SLAVE – I2C-controlled dispenser with ACK handshake
  -------------------------------------------------------------
  I2C    : SDA=A4, SCL=A5, Address = I2C_ADDR
  Flow   : D2 (INT0, FALLING)
  Pump   : D8 (LOW = ON)
  Valve  : D7 (LOW = OPEN)
  Serial : 115200 (debug only)
****************************************************************/
#include <Arduino.h>
#include <Wire.h>
#include <util/atomic.h>

// ======== SET UNIQUE ADDRESS PER UNIT ========
#define I2C_ADDR  0x24   // UNO#1: 0x20, UNO#2: 0x21, … UNO#5: 0x24

// ---- Protocol ----
enum : uint8_t {
  CMD_START  = 0x01,
  CMD_QUERY  = 0x02,
  R_ACK      = 0x00,   // accepted & started
  R_BUSY     = 0x01,
  R_ERR      = 0xFF,
  R_INPROG   = 0x10,
  R_DONE     = 0x20
};

// ---- IO macros ----
#define PUMP_DDR   DDRB
#define PUMP_PORT  PORTB
#define PUMP_BIT   0          // PB0 = D8

#define VALVE_DDR  DDRD
#define VALVE_PORT PORTD
#define VALVE_BIT  7          // PD7 = D7

#define pumpON()     (PUMP_PORT  &= ~_BV(PUMP_BIT))   // LOW active
#define pumpOFF()    (PUMP_PORT  |=  _BV(PUMP_BIT))
#define valveOpen()  (VALVE_PORT &= ~_BV(VALVE_BIT))
#define valveClose() (VALVE_PORT |=  _BV(VALVE_BIT))

// ---- calibration table (mL ↔ pulses) ----
const uint16_t mlPt[]  PROGMEM = {
 100,150,200,250,300,350,400,450,500,
 550,600,650,700,750,800,850,900,950,1000};
const uint16_t pulPt[] PROGMEM = {
 148,223,298,373,448,523,608,673,758,
 828,942,1018,1093,1178,1243,1318,1393,1468,1543};
const uint8_t  N_PT = sizeof(mlPt)/sizeof(mlPt[0]);

uint32_t pulsesForMilliLitres(uint32_t mL)
{
  if (mL <= pgm_read_word(&mlPt[0])) {
    return (uint32_t)mL * pgm_read_word(&pulPt[0]) / pgm_read_word(&mlPt[0]);
  }
  for (uint8_t i = 1; i < N_PT; ++i) {
    uint16_t ml1 = pgm_read_word(&mlPt[i]);
    if (mL <= ml1) {
      uint16_t ml0 = pgm_read_word(&mlPt[i-1]);
      uint16_t p0  = pgm_read_word(&pulPt[i-1]);
      uint16_t p1  = pgm_read_word(&pulPt[i]);
      return p0 + (uint32_t)(mL-ml0) * (p1-p0) / (ml1-ml0);
    }
  }
  return (uint32_t)mL * pgm_read_word(&pulPt[N_PT-1]) /
                       pgm_read_word(&mlPt[N_PT-1]);
}

// ---- globals ----
volatile uint32_t pulseCount = 0;
uint32_t targetPulses        = 0;
bool      dispensing         = false;

// reply byte for next onRequest (set by onReceive/loop)
volatile uint8_t g_nextReply = R_INPROG;

// ---- ISR ----
void isr_flow(){ ++pulseCount; }

// ---- control ----
void startDispense(uint32_t pulses){
  if (pulses==0 || dispensing) return;
  targetPulses = pulses;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE){ pulseCount = 0; }
  valveOpen(); pumpON(); dispensing = true;
}

void stopDispense(){
  pumpOFF(); valveClose(); dispensing = false;
}

// ---- I2C callbacks ----
void onReceiveHandler(int len){
  if (len <= 0) return;

  uint8_t cmd = Wire.read();
  len--;

  if (cmd == CMD_START) {
    // expects 4 bytes mL little-endian
    if (len < 4) { g_nextReply = R_ERR; return; }
    uint32_t mL = 0;
    uint8_t b0 = Wire.read();
    uint8_t b1 = Wire.read();
    uint8_t b2 = Wire.read();
    uint8_t b3 = Wire.read();
    mL = (uint32_t)b0 | ((uint32_t)b1<<8) | ((uint32_t)b2<<16) | ((uint32_t)b3<<24);

    if (dispensing) { g_nextReply = R_BUSY; return; }
    if (mL == 0)    { g_nextReply = R_ERR;  return; }

    uint32_t pulses = pulsesForMilliLitres(mL);
    startDispense(pulses);
    g_nextReply = R_ACK;  // immediate ACK for master
  }
  else if (cmd == CMD_QUERY) {
    if (dispensing) g_nextReply = R_INPROG;
    else            g_nextReply = R_DONE;
  }
  else {
    g_nextReply = R_ERR;
  }
}

void onRequestHandler(){
  Wire.write(g_nextReply);
  // After reporting DONE once, default to IN_PROGRESS/DONE based on state next time
  if (g_nextReply == R_DONE) {
    // keep it as DONE; master may poll again
  }
}

// ---- setup/loop ----
void setup(){
  Serial.begin(115200);

  PUMP_DDR  |= _BV(PUMP_BIT);  pumpOFF();
  VALVE_DDR |= _BV(VALVE_BIT); valveClose();

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), isr_flow, FALLING);
  interrupts();

  Wire.begin(I2C_ADDR);
  Wire.onReceive(onReceiveHandler);
  Wire.onRequest(onRequestHandler);

  Serial.print(F("UNO I2C Slave @0x"));
  Serial.println(I2C_ADDR, HEX);
}

void loop(){
  if (dispensing){
    uint32_t pc; ATOMIC_BLOCK(ATOMIC_RESTORESTATE){ pc = pulseCount; }
    if (pc >= targetPulses){
      stopDispense();
      g_nextReply = R_DONE; // next master query will see DONE
      Serial.println(F("Done."));
    }
  }
}
