/*************************************************************
   Arduino UNO – dispenser by pulse count (fixed-speed pump)
   ----------------------------------------------------------
   • Flow sensor  : D2   (FALLING edge = 1 pulse, INT0)
   • Pump MOSFET  : D9   (ON = HIGH, OFF = LOW)*
   • Valve relay  : D8   (ACTIVE-LOW)
   • Serial (115 200 baud):
         – type “pulses”  → start dispense
         – progress every 250 ms
   *If your MOSFET needs LOW-to-turn-ON just swap HIGH/LOW in pumpON/OFF.
*************************************************************/
#include <Arduino.h>

/* ---------- user configuration ---------------------------- */
const uint8_t FLOW_PIN        = 2;      // INT0
const uint8_t PUMP_PIN        = 8;      // MOSFET gate (digital)
const uint8_t VALVE_PIN       = 7;      // LOW = open
const uint16_t PULSES_PER_LITRE = 1750; // sensor calibration
const uint32_t PRINT_MS       = 250;    // console update rate
/* ---------------------------------------------------------- */

volatile uint32_t pulseCount = 0;       // ISR increments
uint32_t targetPulses        = 0;
bool      dispensing         = false;

/* ---------- helpers --------------------------------------- */
inline void pumpON () { digitalWrite(PUMP_PIN,  LOW); }
inline void pumpOFF() { digitalWrite(PUMP_PIN,  LOW ); }

inline void valveOpen()  { digitalWrite(VALVE_PIN, LOW ); }
inline void valveClose() { digitalWrite(VALVE_PIN, HIGH); }

/* ---------- interrupt routine ----------------------------- */
void ISR_flow() { ++pulseCount; }

/* ---------- dispense logic -------------------------------- */
void startDispense(uint32_t pulses)
{
  if (pulses == 0 || dispensing) return;

  targetPulses = pulses;
  noInterrupts(); pulseCount = 0; interrupts();

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

  Serial.print(F("✔ Finished: "));
  Serial.print(pulseCount);
  Serial.print(F(" pulses  ("));
  Serial.print(pulseCount / (float)PULSES_PER_LITRE, 3);
  Serial.println(F(" L)\n"));
}

/* ---------- setup ----------------------------------------- */
void setup()
{
  Serial.begin(115200);
  while (!Serial);                          // wait for PC (optional)

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(0, ISR_flow, FALLING);    // INT0 = pin 2

  pinMode(PUMP_PIN,   OUTPUT);
  pumpOFF();

  pinMode(VALVE_PIN,  OUTPUT);
  valveClose();

  Serial.println(F("UNO dispenser ready."));
  Serial.println(F("Enter pulse target (e.g. 3500) then press Enter."));
}

/* ---------- main loop ------------------------------------- */
void loop()
{
  /* 1 – accept new command when idle */
  if (!dispensing && Serial.available())
  {
    uint32_t pulses = Serial.parseInt();    // single integer
    while (Serial.available()) Serial.read();
    startDispense(pulses);
  }

  /* 2 – progress & completion */
  static uint32_t nextPrint = 0;
  uint32_t now = millis();

  if (dispensing)
  {
    if (pulseCount >= targetPulses) stopDispense();

    if (now - nextPrint >= PRINT_MS)
    {
      nextPrint = now;
      noInterrupts();
      uint32_t pc = pulseCount;
      interrupts();

      Serial.print(F("… "));
      Serial.print(pc);
      Serial.print(F(" / "));
      Serial.print(targetPulses);
      Serial.print(F(" pulses  ("));
      Serial.print(pc / (float)PULSES_PER_LITRE, 3);
      Serial.println(F(" L)"));
    }
  }
}
