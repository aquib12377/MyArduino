/****************************************************************
  UNO  – dispenser by *any* litre quantity (fast, table based)
  -------------------------------------------------------------
  Flow  IN : D2  (INT0, FALLING)
  Pump  OUT: D8  (LOW = ON)
  Valve OUT: D7  (LOW = OPEN)
  Serial   : 115 200 baud
****************************************************************/
#include <Arduino.h>
#include <util/atomic.h>

/* ── direct port macros (fast) ─────────────────────────────── */
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

/* ── calibration table (mL ↔ pulses) ──────────────────────── */
const uint16_t mlPt[]  PROGMEM = {
 100,150,200,250,300,350,400,450,500,
 550,600,650,700,750,800,850,900,950,1000};
const uint16_t pulPt[] PROGMEM = {
 148,223,298,373,448,523,608,673,758,
 828,942.5,1017.5,1092.5,1177.5,1242.5,1317.5,1392.5,1467.5,1542.5};
const uint8_t  N_PT = sizeof(mlPt)/sizeof(mlPt[0]);

/* linear interpolation, with proportional extrapolation beyond table */
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
  /* above last point → scale linearly from last point */
  return (uint32_t)mL * pgm_read_word(&pulPt[N_PT-1]) /
                       pgm_read_word(&mlPt[N_PT-1]);
}

/* ── globals ──────────────────────────────────────────────── */
volatile uint32_t pulseCount = 0;
uint32_t targetPulses        = 0;
bool      dispensing         = false;

/* ── ISR ──────────────────────────────────────────────────── */

/* ── I/O helpers ──────────────────────────────────────────── */
void startDispense(uint32_t pulses, float litres)
{
  if (pulses == 0 || dispensing) return;

  targetPulses = pulses;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pulseCount = 0; }

  valveOpen();
  pumpON();
  dispensing = true;

  Serial.print(F("\n▶ Dispensing "));
  Serial.print(litres, 3);
  Serial.print(F(" L  ("));
  Serial.print(pulses);
  Serial.println(F(" pulses)"));
}

void stopDispense()
{
  pumpOFF(); valveClose(); dispensing = false;

  uint32_t pc; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pc = pulseCount; }

  Serial.print(F("✔ Reached "));
  Serial.print(pc);
  Serial.print(F(" pulses  (≈ "));
  Serial.print(pc / 1000.0 *
               pgm_read_word(&mlPt[N_PT-1]) /
               pgm_read_word(&pulPt[N_PT-1]) / 1000.0, 3);
  Serial.println(F(" L)\n"));
}

/* ── float parser (waits for NL/CR) ───────────────────────── */
float readFloatLine()
{
  char buf[16]; uint8_t idx = 0;
  while (true) {
    while (!Serial.available());           // busy wait
    char c = Serial.read();
    if (c=='\n'||c=='\r') { buf[idx]='\0'; return atof(buf); }
    if (idx < sizeof(buf)-1) buf[idx++] = c;
  }
}

/* ── setup ───────────────────────────────────────────────── */
void setup()
{
  Serial.begin(115200);

  PUMP_DDR  |= _BV(PUMP_BIT);  pumpOFF();
  VALVE_DDR |= _BV(VALVE_BIT); valveClose();

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), []{ ++pulseCount; }, FALLING); // use CHANGE to verify
  interrupts();  // ensure global interrupts are enabled
  Serial.println(F("Enter quantity in litres (e.g. 0.237 or 1.5) then <Enter>."));
}

/* ── loop ────────────────────────────────────────────────── */
void loop()
{
  /* 1: new command ------------------------------------------------ */
  if (!dispensing && Serial.available())
  {
    float litres = readFloatLine();
    if (litres <= 0) { Serial.println(F("✖ Quantity must be >0")); return; }

    uint32_t mL = (uint32_t)(litres*1000.0 + 0.5);     // round to mL
    uint32_t pulses = pulsesForMilliLitres(mL);
    startDispense(pulses, litres);
  }

  /* 2: progress / completion ------------------------------------- */
  static uint32_t nextPrint = 0;
  uint32_t now = millis();

  if (dispensing)
  {
    uint32_t pc; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pc = pulseCount; }
    if (pc >= targetPulses) { stopDispense(); return; }

    if (now - nextPrint >= 250) {          // 250 ms status
      nextPrint = now;
      Serial.print(F("… "));
      Serial.print(pc);
      Serial.print(F(" / "));
      Serial.println(targetPulses);
    }
  }
}