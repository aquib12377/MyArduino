/*
 * SmartLCD — Custom Characters, Alignment & Effects
 */

#include <SmartLCD.h>

SmartLCD lcd(0x27, 16, 2);

// Heart glyph
const uint8_t heart[8] = {
    0b00000, 0b01010, 0b11111, 0b11111,
    0b01110, 0b00100, 0b00000, 0b00000
};

// Smiley glyph
const uint8_t smiley[8] = {
    0b00000, 0b00000, 0b01010, 0b00000,
    0b10001, 0b01110, 0b00000, 0b00000
};

void setup() {
    lcd.begin();

    // Register custom chars in slots 0 and 1
    lcd.createChar(0, heart);
    lcd.createChar(1, smiley);

    // Use them
    lcd.print("I ");
    lcd.printCustom(0);  // heart
    lcd.print(" Arduino ");
    lcd.printCustom(1);  // smiley

    delay(3000);
    lcd.clear();

    // ── Alignment demo ──
    lcd.printCentered("CENTERED", 0);
    lcd.printRight("RIGHT", 1);

    delay(3000);
    lcd.clear();

    // ── Transition effects ──
    lcd.printEffect("Typewriter!", 0, EFFECT_TYPEWRITER, 120);
    delay(2000);
    lcd.printEffect("Blink Fade!", 1, EFFECT_BLINK_FADE, 150);

    delay(3000);
    lcd.clear();

    // ── Backlight timeout: turns off after 10 seconds of no writes ──
    lcd.print("Idle timeout 10s");
    lcd.setBacklightTimeout(10000);
}

void loop() {
    lcd.tickBacklight();
}
