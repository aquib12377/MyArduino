/***********************************************************************
   Mega 2560  –  25 Push-Buttons → 25 LEDs  +  I²C Command (debounced)
   --------------------------------------------------------------------
   • Each push-button (N) wired to the pin in buttonPins[] pulls it LOW.
   • On the *press* edge (LOW after 15 ms stability) the matching LED
     (ledPins[N]) turns ON for 5 s **or** until another button is pressed.
   • Sends the button index (0-24) to a Pro Micro at I²C address 0x08.
   • Verbose diagnostics on Serial @ 115200.
 ***********************************************************************/

#include <Wire.h>
#include <avr/pgmspace.h>

/* ----------- user constants ----------- */
const uint8_t  PRO_MICRO_ADDR  = 0x08;
const uint32_t LED_ON_TIME_MS  = 5000UL;
const uint32_t DEBOUNCE_MS     = 15UL;

/* 25 momentary buttons (push to GND) */
const uint8_t buttonPins[] PROGMEM = {
  10, 9, 8, 6, 5, 4, 3, 2,          // 0-8
  14,15,16,17,18,19,                   // 9-14
  52,50,48,46,44,42,40,38, 36,34      // 15-24
};

/* 25 LEDs */
const uint8_t ledPins[] PROGMEM = {
  39,41,43,47,49,51,53,
  A15, A14, A13, A12, A11,37, A10, A9, A8, A7, A6, A5, A4, A3, A2, A1, A0  
};

const size_t NUM_KEYS = sizeof(buttonPins) / sizeof(buttonPins[0]);  // 25

/* --------- runtime state --------- */
bool          rawState   [NUM_KEYS];      // immediate pin level (bounces)
bool          stableState[NUM_KEYS];      // last debounced level
unsigned long lastChange [NUM_KEYS];      // millis of last raw change

int           currentLed = -1;            // index of LED currently ON
unsigned long ledStartMs = 0;             // millis when LED was lit

/* ---------------------------- setup ---------------------------- */
void setup()
{
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("\n=== Mega 25-Key Debounced Tester ==="));

  /* configure LED outputs */
  for (size_t i = 0; i < NUM_KEYS; ++i) {
    uint8_t pin = pgm_read_byte(&ledPins[i]);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  /* configure buttons + initialise debounce arrays */
  unsigned long now = millis();
  for (size_t i = 0; i < NUM_KEYS; ++i) {
    uint8_t pin = pgm_read_byte(&buttonPins[i]);
    pinMode(pin, INPUT_PULLUP);

    rawState[i]    = HIGH;
    stableState[i] = HIGH;               // released
    lastChange[i]  = now;
  }

  Wire.begin();                          // master (SDA 20, SCL 21)
  Serial.println(F("Initialisation complete.\n"));
}

/* ---------------------------- main loop ---------------------------- */
void loop()
{
  unsigned long now = millis();

  /* ----- scan every button with edge + debounce ----- */
  for (size_t idx = 0; idx < NUM_KEYS; ++idx) {
    uint8_t pin   = pgm_read_byte(&buttonPins[idx]);
    bool    level = digitalRead(pin);            // current raw level

    if (level != rawState[idx]) {                // raw level toggled
      rawState[idx]  = level;
      lastChange[idx] = now;                     // restart debounce timer
    }

    /* Stable long enough?  */
    if ((now - lastChange[idx]) >= DEBOUNCE_MS &&
        level != stableState[idx])               // NEW debounced edge
    {
      stableState[idx] = level;                  // commit new state
      if (level == LOW) handlePress(idx, now);   // LOW edge = press
    }
  }

  /* ----- auto-off after 5 s ----- */
  if (currentLed >= 0 && (now - ledStartMs) >= LED_ON_TIME_MS) {
    uint8_t pin = pgm_read_byte(&ledPins[currentLed]);
    digitalWrite(pin, LOW);
    Serial.print(F("Timeout – LED OFF (pin ")); Serial.print(pin); Serial.println(')');

    currentLed = -1;
  }
}

/* ----------------- handle a *debounced* button press ---------------- */
void handlePress(int idx, unsigned long nowMs)
{
  /* turn OFF previous LED if any */
  if (currentLed >= 0) {
    uint8_t oldPin = pgm_read_byte(&ledPins[currentLed]);
    digitalWrite(oldPin, LOW);
    Serial.print(F("Cancelled LED pin ")); Serial.println(oldPin);
  }

  /* turn ON new LED */
  currentLed = idx;
  uint8_t ledPin = pgm_read_byte(&ledPins[idx]);
  digitalWrite(ledPin, HIGH);
  ledStartMs = nowMs;

  /* send I²C command */
  Wire.beginTransmission(PRO_MICRO_ADDR);
  Wire.write((uint8_t)idx);                 // 0-24
  uint8_t err = Wire.endTransmission();

  /* verbose report */
  Serial.println(F("--------------------------------------------------"));
  Serial.print(F("Button index      : ")); Serial.println(idx);
  Serial.print(F("Button pin        : ")); Serial.println(
                 pgm_read_byte(&buttonPins[idx]));
  Serial.print(F("LED pin ON        : ")); Serial.println(ledPin);
  Serial.print(F("Millis timestamp  : ")); Serial.println(ledStartMs);
  Serial.print(F("I2C status        : ")); Serial.println(err);
  Serial.println(F("--------------------------------------------------"));
}
