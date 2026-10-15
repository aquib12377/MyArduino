/*
 * SmartLCD — Auto Line-Wrap Detection
 *
 * Demonstrates that you DON'T need to manually position the cursor
 * on line 2. If the text overflows, SmartLCD wraps it automatically.
 * Works with any LCD size (16x2, 20x4, etc.).
 */

#include <SmartLCD.h>

SmartLCD lcd(0x27, 20, 4);  // 20x4 I2C LCD

void setup() {
    lcd.begin();

    // Just print — the library handles line breaks for you.
    // This 70-char string fills all 4 rows of a 20x4 display.
    lcd.print("Line1 auto-filled  "
              "Line2 auto-filled  "
              "Line3 auto-filled  "
              "Line4 auto-filled  ");

    delay(4000);
    lcd.clear();

    // You can also use '\n' for explicit line breaks
    lcd.print("Explicit\nline break");

    delay(4000);
    lcd.clear();

    // Disable auto-wrap if you want classic behaviour
    lcd.setAutoWrap(false);
    lcd.print("This text will NOT wrap to line 2, it just stops");
}

void loop() {}
