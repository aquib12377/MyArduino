# SmartLCD

A feature-rich LCD library for **Arduino** and **ESP32** with both I2C and parallel mode support.

## Features

| Feature | Description |
|---|---|
| **Dual mode** | I2C (PCF8574 backpack) and parallel 4-bit GPIO |
| **Auto line-wrap** | Text flows to next line automatically — no manual cursor management |
| **Scrolling text** | Blocking and non-blocking modes with configurable speed and loop count |
| **printf()** | Formatted printing: `lcd.printf("Temp: %d°C", val)` |
| **Text alignment** | Left, center, and right alignment helpers |
| **Custom characters** | Easy API to define and display custom 5×8 glyphs |
| **Backlight timeout** | Auto-off after idle; wakes on new content |
| **Transition effects** | Typewriter, wipe, and blink-fade animations |
| **Any LCD size** | 16×2, 20×4, or any HD44780 compatible |

## Installation

1. Download or clone this repository.
2. Copy the `SmartLCD` folder into your Arduino `libraries/` directory.
3. Restart the Arduino IDE → it appears under **Sketch → Include Library**.

## Quick Start

### I2C mode (most common)

```cpp
#include <SmartLCD.h>

SmartLCD lcd(0x27, 16, 2);  // I2C address, cols, rows

void setup() {
    lcd.begin();
    lcd.print("Hello World!");  // auto-wraps to line 2 if needed
}

void loop() {
    lcd.tickBacklight();
}
```

### Parallel 4-bit mode

```cpp
#include <SmartLCD.h>

SmartLCD lcd(12, 11, 5, 4, 3, 2, 16, 2);  // RS, EN, D4-D7, cols, rows

void setup() {
    lcd.begin();
    lcd.printf("Temp: %.1fC", 23.7);
}

void loop() {}
```

## API Reference

### Constructors
- `SmartLCD(addr, cols, rows)` — I2C
- `SmartLCD(rs, en, d4, d5, d6, d7, cols, rows)` — Parallel

### Display
- `begin()`, `clear()`, `home()`, `setCursor(col, row)`
- `displayOn/Off()`, `cursorOn/Off()`, `blinkOn/Off()`

### Printing
- `print()` / `println()` — inherited from Arduino Print
- `printf(fmt, ...)` — formatted output
- `setAutoWrap(bool)` — toggle auto line-wrap (default: on)

### Alignment
- `printAligned(text, row, ALIGN_LEFT|CENTER|RIGHT)`
- `printCentered(text, row)` / `printRight(text, row)`

### Scrolling
- `scrollText(text, row, speed, loops)` — blocking
- `startScroll(text, row, speed, loops)` + `tickScroll()` — non-blocking

### Custom Characters
- `createChar(slot, bitmap[8])` — define glyph (slots 0–7)
- `createChar_P(slot, progmem_bitmap)` — from PROGMEM
- `printCustom(slot)` — display it

### Backlight
- `backlightOn/Off()`
- `setBacklightTimeout(ms)` + `tickBacklight()` — auto-off

### Effects
- `printEffect(text, row, EFFECT_TYPEWRITER|WIPE|BLINK_FADE, speed)`

## Examples

See the `examples/` folder for complete sketches.

## License

MIT
