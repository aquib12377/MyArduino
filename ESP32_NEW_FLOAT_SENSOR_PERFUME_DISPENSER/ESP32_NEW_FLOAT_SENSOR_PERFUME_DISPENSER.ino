/*************************************************************
   ESP32  – PWM-controlled dispenser by pulse count
   ----------------------------------------------------------
   • Flow sensor  : GPIO-13  (FALLING edge = 1 pulse)
   • Pump MOSFET  : GPIO-25  (PWM - ledc CH 0, 8-bit, 5 kHz)
   • Valve relay  : GPIO-26  (ACTIVE-LOW)
   • Serial (115 200 baud):
         – “pulses  [power%]”  → start dispense
         – live read-out every 250 ms
*************************************************************/
#include <Arduino.h>

/*************** user configuration ***************************/
constexpr uint8_t  FLOW_PIN         = 13;
constexpr uint8_t  PUMP_PWM_PIN     = 25;     // must support ledc
constexpr uint8_t  VALVE_RELAY_PIN  = 26;     // LOW = OPEN
constexpr uint16_t PULSES_PER_LITRE = 1750;
constexpr uint32_t PRINT_EVERY_MS   = 250;

constexpr uint8_t  PWM_CHANNEL      = 0;
constexpr uint32_t PWM_FREQUENCY    = 5000;   // Hz
constexpr uint8_t  PWM_RESOLUTION   = 8;      // bits  → 0-255
/**************************************************************/

volatile uint32_t pulseCount = 0;             // ISR
uint32_t targetPulses        = 0;
bool      dispensing         = false;

uint8_t   pumpPowerPercent   = 100;           // default
uint8_t   dutyFromPercent(uint8_t pc) { return map(pc, 0, 100, 0, 255); }

/* ---------- ISR ------------------------------------------- */
void IRAM_ATTR onPulse() { pulseCount++; }

/* ---------- helpers --------------------------------------- */
void pumpDrive(uint8_t percent) {   analogWrite(PUMP_PWM_PIN, map(percent, 0, 100, 0, 255));
 }
void valveOpen()  { digitalWrite(VALVE_RELAY_PIN, LOW);  }
void valveClose() { digitalWrite(VALVE_RELAY_PIN, HIGH); }

void startDispense(uint32_t pulses, uint8_t powerPc)
{
  if (pulses == 0) return;
  targetPulses      = pulses;
  pumpPowerPercent  = powerPc;
  noInterrupts();   pulseCount = 0;   interrupts();

  valveOpen();
  pumpDrive(pumpPowerPercent);

  dispensing = true;

  Serial.printf("\n▶ Target: %lu pulses  | Power: %u %% (≈ %.3f L)\n",
                pulses, pumpPowerPercent,
                pulses / (float)PULSES_PER_LITRE);
}

void stopDispense()
{
  pumpDrive(0);
  valveClose();
  dispensing = false;

  float litres = pulseCount / (float)PULSES_PER_LITRE;
  Serial.printf("✔ Finished: %lu pulses  (%.3f L)\n\n",
                pulseCount, litres);
}

/* ---------- Arduino setup --------------------------------- */
void setup()
{
  Serial.begin(115200);
  delay(200);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onPulse, FALLING);

  /* PWM for MOSFET */
  // analogWriteResolution(PUMP_PWM_PIN,PWM_RESOLUTION);      // core ≥ 2.0.2
  // analogWriteFrequency(PUMP_PWM_PIN, PWM_FREQUENCY);
  analogWrite(PUMP_PWM_PIN, 0);    

  pinMode(VALVE_RELAY_PIN, OUTPUT);
  valveClose();

  Serial.println("PWM-driven dispenser ready.");
  Serial.println("Enter:  pulses  [power%]   e.g.  3500 40");
}

/* ---------- main loop ------------------------------------- */
void loop()
{
  /* 1. parse user command when idle ------------------------ */
  if (!dispensing && Serial.available())
  {
    uint32_t pulses = Serial.parseInt();          // first number
    uint32_t power  = Serial.parseInt();          // second (may be 0)

    /* clear buffer */
    while (Serial.available()) Serial.read();

    /* keep last power if none typed */
    uint8_t powerPc = power ? constrain(power, 1, 100) : pumpPowerPercent;
    startDispense(pulses, powerPc);
  }

  /* 2. progress / finish ----------------------------------- */
  static uint32_t nextPrintMs = 0;
  uint32_t now = millis();

  if (dispensing)
  {
    if (pulseCount >= targetPulses) stopDispense();

    if (now - nextPrintMs >= PRINT_EVERY_MS)
    {
      nextPrintMs = now;
      Serial.printf("… %lu / %lu pulses  (%.3f L)\n",
                    pulseCount, targetPulses,
                    pulseCount / (float)PULSES_PER_LITRE);
    }
  }
}
