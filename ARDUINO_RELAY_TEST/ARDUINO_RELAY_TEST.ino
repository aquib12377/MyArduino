/****************************************************************
  Arduino UNO – simple 2-relay control via Serial Monitor
  --------------------------------------------------------------
  • Relay-1 IN → D8
  • Relay-2 IN → D9
  • Open Serial Monitor at 115 200 baud and send: 1 0 3 2 …
****************************************************************/
#include <Arduino.h>

/* ── pin assignments ───────────────────────────────────────── */
const uint8_t RELAY1_PIN = 7;
const uint8_t RELAY2_PIN = 8;

/* ── logic level (change if your relays are active-LOW) ────── */
const uint8_t RELAY_ON  = HIGH;   // use LOW if active-LOW
const uint8_t RELAY_OFF = LOW;    // use HIGH if active-LOW
/* ──────────────────────────────────────────────────────────── */

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);

  Serial.println(F("\n2-Relay controller ready"));
  Serial.println(F("Commands: 1=ON1  0=OFF1  3=ON2  2=OFF2"));
}

void loop()
{
  if (Serial.available())
  {
    String c = Serial.readStringUntil('\n');
    int cmd = c.toInt();
    switch (cmd)
    {
      case 1:  digitalWrite(RELAY1_PIN, RELAY_ON ); break;
      case 2:  digitalWrite(RELAY1_PIN, RELAY_OFF); break;
      case 3:  digitalWrite(RELAY2_PIN, RELAY_ON ); break;
      case 4:  digitalWrite(RELAY2_PIN, RELAY_OFF); break;
      default:   Serial.println(F("Unknown command"));        break;
    }
  }
}
