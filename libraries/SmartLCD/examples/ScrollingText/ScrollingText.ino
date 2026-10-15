/*
 * SmartLCD — Scrolling Text (blocking + non-blocking)
 */

#include <SmartLCD.h>

SmartLCD lcd(0x27, 16, 2);

void setup() {
    lcd.begin();

    // ── Blocking scroll: scrolls once and returns ──
    lcd.scrollText("This is a long scrolling message!", 0, 250, 1);

    delay(1000);
    lcd.clear();

    // ── Non-blocking scroll: keeps scrolling in loop() ──
    lcd.printAligned("Status:", 0, ALIGN_LEFT);
    lcd.startScroll("Non-blocking scroll running smoothly...", 1, 200, -1);
}

void loop() {
    lcd.tickScroll();     // advances scroll one step when due
    lcd.tickBacklight();  // manages backlight timeout

    // You can do other work here — the scroll doesn't block!
}
