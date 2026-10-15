/*
 * SmartLCD — Parallel 4-bit Mode Example
 *
 * Wiring:
 *   RS → pin 12,  EN → pin 11
 *   D4 → pin 5,   D5 → pin 4,   D6 → pin 3,   D7 → pin 2
 *   VSS → GND,    VDD → 5V,     V0 → potentiometer wiper
 */

#include <SmartLCD.h>

//                 RS  EN  D4  D5  D6  D7  cols rows
SmartLCD lcd(12, 11,  5,  4,  3,  2,  16,  2);

void setup() {
    lcd.begin();

    // printf-style formatted output
    float temp = 24.5;
    int hum = 62;
    lcd.printf("Temp: %.1fC", temp);
    lcd.setCursor(0, 1);
    lcd.printf("Humidity: %d%%", hum);
}

void loop() {}
