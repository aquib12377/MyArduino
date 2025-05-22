/*
 *  ESP32/ESP8266   DS1307 → TM1637 4‑digit clock
 *  ------------------------------------------------
 *  displays HH:MM, colon blinks every second
 *  (based on the example from ArduinoGetStarted.com)
 */

#include <TM1637Display.h>
#include <RTClib.h>

// ── wiring -------------------------------------------------------
#define CLK 19           // TM1637 CLK → GPIO19
#define DIO 18           // TM1637 DIO → GPIO18
// ----------------------------------------------------------------

TM1637Display display(CLK, DIO);
RTC_DS1307 rtc;          // use RTC_DS3231 rtc; if you have a DS3231

uint8_t lastSecond = 60; // force immediate update on power‑up

void setup() {
  Serial.begin(9600);
  display.clear();
  display.setBrightness(7);               // 0 = dim, 7 = bright

  // ---- RTC initialisation --------------------------------------
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (true) delay(1);
  }

  // comment‑out after the first successful flash if your RTC is already set
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
  DateTime now = rtc.now();

  // update once per second (needed for colon blink)
  if (now.second() != lastSecond) {

    // pack HH and MM into a single integer HHMM
    uint16_t hhmm = now.hour() * 100 + now.minute();

    // blink colon: bit 0b0100 0000 (0x40) lights the centre colon segments
    uint8_t colonMask = (now.second() % 2) ? 0x40 : 0x00;

    /*                value    colon  leadZero length pos */
    display.showNumberDecEx(hhmm, colonMask, true,    4,   0);

    Serial.printf("%02d:%02d\n", now.hour(), now.minute());
    lastSecond = now.second();
  }
}
