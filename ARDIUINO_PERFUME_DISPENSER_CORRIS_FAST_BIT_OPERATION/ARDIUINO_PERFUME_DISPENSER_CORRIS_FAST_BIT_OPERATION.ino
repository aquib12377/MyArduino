/****************************************************************
  UNO  – high-speed dispenser  (pulse-count / fixed-speed pump)
  -------------------------------------------------------------
  Flow IN  : D2  (INT0, FALLING)
  Pump OUT : D8  (MOSFET gate)   LOW = ON
  ValveOUT : D7  (relay)         LOW = OPEN
  Serial   : 115 200 baud
****************************************************************/
#include <Arduino.h>
#include <util/atomic.h>

/* ─── pins → AVR ports ─────────────────────────────────────── */
#define PUMP_DDR   DDRB
#define PUMP_PORT  PORTB
#define PUMP_BIT   0          // PB0 → D8

#define VALVE_DDR  DDRD
#define VALVE_PORT PORTD
#define VALVE_BIT  7          // PD7 → D7

#define pumpON()   (PUMP_PORT &= ~_BV(PUMP_BIT))   // LOW-active
#define pumpOFF()  (PUMP_PORT |=  _BV(PUMP_BIT))
#define valveOpen()  (VALVE_PORT &= ~_BV(VALVE_BIT))
#define valveClose() (VALVE_PORT |=  _BV(VALVE_BIT))

/* ─── constants ────────────────────────────────────────────── */
const uint16_t PULSES_PER_LITRE = 1300;
const uint32_t PRINT_MS         = 250;

/* ─── globals ──────────────────────────────────────────────── */
volatile uint32_t pulseCount = 0;          // ISR increments
uint32_t targetPulses        = 0;
bool      dispensing         = false;

/* ─── ISR ──────────────────────────────────────────────────── */
ISR(INT0_vect) { ++pulseCount; }           // fastest possible

/* ─── tiny int-parser (reads until \n) ─────────────────────── */
uint32_t readUintFromSerial()
{
  uint32_t v = 0;
  while (true) {
    if (!Serial.available()) continue;
    char c = Serial.read();
    if (c == '\n' || c == '\r') return v;  // end of number
    if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
  }
}

/* ─── helpers ─────────────────────────────────────────────── */
void startDispense(uint32_t pulses)
{
  if (pulses == 0 || dispensing) return;
  targetPulses = pulses;
  targetPulses = targetPulses - ((targetPulses / 650) * 0);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pulseCount = 0; }

  valveOpen();
  pumpON();
  dispensing = true;

  Serial.print(F("\n▶ Target: "));
  Serial.print(pulses);
  Serial.print(F(" pulses  (≈ "));
  Serial.print(pulses / (float)PULSES_PER_LITRE, 3);
  Serial.println(F(" L)"));
}

void stopDispense()
{
  pumpOFF();
  valveClose();
  dispensing = false;

  uint32_t pc;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pc = pulseCount; }

  Serial.print(F("✔ Finished: "));
  Serial.print(pc);
  Serial.print(F(" pulses  ("));
  Serial.print(pc / (float)PULSES_PER_LITRE, 3);
  Serial.println(F(" L)\n"));
}

/* ─── setup ───────────────────────────────────────────────── */
void setup()
{
  Serial.begin(115200);

  /* pin directions */
  PUMP_DDR  |= _BV(PUMP_BIT);   pumpOFF();
  VALVE_DDR |= _BV(VALVE_BIT);  valveClose();

  pinMode(2, INPUT_PULLUP);            // flow sensor
  EICRA = _BV(ISC01);                  // falling edge on INT0
  EIFR  = _BV(INTF0);                  // clear flag
  EIMSK = _BV(INT0);                   // enable INT0

  Serial.println(F("Fast dispenser ready – enter pulse target and <Enter>."));
}

/* ─── main loop ───────────────────────────────────────────── */
void loop()
{
  /* 1 ─ wait for new command when idle */
  if (!dispensing && Serial.available())
    startDispense(readUintFromSerial());

  /* 2 ─ check progress every PRINT_MS */
  static uint32_t nextPrint = 0;
  uint32_t now = millis();

  if (dispensing)
  {
    uint32_t pc;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pc = pulseCount; }

    if (pc >= targetPulses) { stopDispense(); return; }

    if (now - nextPrint >= PRINT_MS) {
      nextPrint = now;
      Serial.print(F("… "));
      Serial.print(pc);
      Serial.print(F(" / "));
      Serial.print(targetPulses);
      Serial.print(F("  ("));
      /* integer → fixed-point mL for speed */
      uint32_t mL = (pc * 1000UL) / PULSES_PER_LITRE; // mL ×10-3
      Serial.print(mL / 1000);  Serial.print('.');    // litres
      uint16_t frac = mL % 1000;
      if (frac < 100) Serial.print('0');
      if (frac <  10) Serial.print('0');
      Serial.println(frac);
    }
  }
}
