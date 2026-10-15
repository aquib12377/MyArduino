/*
 * SmartLCD — Basic I2C Example
 *
 * Wiring (PCF8574 backpack):
 *   SDA → A4 (or ESP32 GPIO 21)
 *   SCL → A5 (or ESP32 GPIO 22)
 *   VCC → 5V,  GND → GND
 *
 * Default I2C address is 0x27 (try 0x3F if it doesn't work).
 */

#include <SmartLCD.h>

SmartLCD lcd(0x27, 16, 2);  // addr, cols, rows

void setup() {
    lcd.begin();
    lcd.backlightOn();

    // Auto-wrap demo: this 25-char string overflows line 1
    // and automatically continues on line 2.
    lcd.print("Hello World! Auto wrap works great!");
}

void loop() {
    lcd.tickBacklight();  // handles auto-off if timeout is set
}
