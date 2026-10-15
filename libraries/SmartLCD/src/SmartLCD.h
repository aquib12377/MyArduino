/*
 * SmartLCD.h - Smart LCD Library for Arduino / ESP32
 * Supports both I2C (PCF8574 backpack) and parallel (4-bit) modes.
 *
 * Features:
 *   - Auto line-wrap: text flows to next line automatically
 *   - Scrolling text with configurable speed
 *   - printf-style formatted printing
 *   - Text alignment (left / center / right)
 *   - Custom character helpers
 *   - Backlight auto-off after idle timeout
 *   - Screen transition effects (typewriter, wipe, fade-blink)
 *
 * MIT License
 */

#ifndef SMART_LCD_H
#define SMART_LCD_H

#include <Arduino.h>
#include <Print.h>
#include <Wire.h>

// ── HD44780 commands ────────────────────────────────────────────────
#define LCD_CLEARDISPLAY   0x01
#define LCD_RETURNHOME     0x02
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT    0x10
#define LCD_FUNCTIONSET    0x20
#define LCD_SETCGRAMADDR   0x40
#define LCD_SETDDRAMADDR   0x80

// Flags
#define LCD_ENTRYLEFT          0x02
#define LCD_ENTRYSHIFTDEC      0x00
#define LCD_DISPLAYON          0x04
#define LCD_CURSORON           0x02
#define LCD_CURSOROFF          0x00
#define LCD_BLINKON            0x01
#define LCD_BLINKOFF           0x00
#define LCD_8BITMODE           0x10
#define LCD_4BITMODE           0x00
#define LCD_2LINE              0x08
#define LCD_1LINE              0x00
#define LCD_5x10DOTS           0x04
#define LCD_5x8DOTS            0x00

// PCF8574 backpack bit positions
#define LCD_BACKLIGHT  0x08
#define LCD_NOBACKLIGHT 0x00
#define EN_BIT         0x04  // Enable
#define RW_BIT         0x02  // Read/Write
#define RS_BIT         0x01  // Register Select

// ── Text alignment ─────────────────────────────────────────────────
enum LcdAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

// ── Connection mode ─────────────────────────────────────────────────
enum LcdMode  { MODE_I2C, MODE_PARALLEL };

// ── Transition effects ──────────────────────────────────────────────
enum LcdEffect { EFFECT_NONE, EFFECT_TYPEWRITER, EFFECT_WIPE, EFFECT_BLINK_FADE };

class SmartLCD : public Print {
public:
    // ── Constructors ────────────────────────────────────────────────

    /// I2C constructor (address, cols, rows, Wire instance)
    SmartLCD(uint8_t i2cAddr, uint8_t cols = 16, uint8_t rows = 2, TwoWire &wire = Wire);

    /// Parallel 4-bit constructor (rs, en, d4, d5, d6, d7, cols, rows)
    SmartLCD(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5,
             uint8_t d6, uint8_t d7, uint8_t cols = 16, uint8_t rows = 2);

    // ── Core ────────────────────────────────────────────────────────
    void begin();
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);

    // ── Display control ─────────────────────────────────────────────
    void displayOn();
    void displayOff();
    void cursorOn();
    void cursorOff();
    void blinkOn();
    void blinkOff();

    // ── Backlight ───────────────────────────────────────────────────
    void backlightOn();
    void backlightOff();
    void setBacklightTimeout(unsigned long ms);  // 0 = always on
    void tickBacklight();                         // call in loop()

    // ── Printing (overrides Print) ──────────────────────────────────
    virtual size_t write(uint8_t ch) override;
    using Print::write;  // pull in write(const uint8_t*, size_t)

    /// printf-style convenience (max 80 chars)
    void printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

    // ── Auto-wrap control ───────────────────────────────────────────
    void setAutoWrap(bool on);   // default: ON

    // ── Alignment helpers ───────────────────────────────────────────
    void printAligned(const char *text, uint8_t row, LcdAlign align = ALIGN_LEFT);
    void printCentered(const char *text, uint8_t row);
    void printRight(const char *text, uint8_t row);

    // ── Scrolling ───────────────────────────────────────────────────
    /// One-shot scroll a long string across a row.  speed = ms per step.
    void scrollText(const char *text, uint8_t row = 0, unsigned int speed = 300,
                    int loops = 1);

    /// Non-blocking scroller — call tickScroll() in loop().
    void startScroll(const char *text, uint8_t row = 0, unsigned int speed = 300,
                     int loops = -1);  // -1 = infinite
    void stopScroll();
    bool tickScroll();  // returns true while scrolling

    // ── Custom characters ───────────────────────────────────────────
    /// Define a custom glyph (slot 0-7).
    void createChar(uint8_t slot, const uint8_t bitmap[8]);

    /// Convenience: pass a progmem array.
    void createChar_P(uint8_t slot, const uint8_t *bitmap);

    /// Print a previously defined custom char.
    void printCustom(uint8_t slot);

    // ── Transition effects ──────────────────────────────────────────
    void printEffect(const char *text, uint8_t row, LcdEffect effect,
                     unsigned int speed = 80);

    // ── Utility ─────────────────────────────────────────────────────
    uint8_t getCols() const { return _cols; }
    uint8_t getRows() const { return _rows; }
    LcdMode getMode() const { return _mode; }

private:
    // ── Low-level HD44780 interface ─────────────────────────────────
    void _command(uint8_t value);
    void _sendData(uint8_t value);
    void _send(uint8_t value, uint8_t rsFlag);

    // I2C helpers
    void _i2cWrite(uint8_t data);
    void _i2cWriteNibble(uint8_t nibble);

    // Parallel helpers
    void _parWrite4(uint8_t value, bool isData);
    void _pulseEnable();

    void _setRowOffsets();

    // ── State ───────────────────────────────────────────────────────
    LcdMode   _mode;
    uint8_t   _cols, _rows;
    uint8_t   _curCol, _curRow;
    bool      _autoWrap;

    // I2C
    uint8_t   _i2cAddr;
    TwoWire  *_wire;
    uint8_t   _backlightFlag;

    // Parallel
    uint8_t   _rsPin, _enPin, _dataPins[4];

    // Display control register cache
    uint8_t   _displayCtrl;

    // Row DDRAM offsets
    uint8_t   _rowOffsets[4];

    // Backlight timeout
    unsigned long _blTimeout;
    unsigned long _lastActivity;

    // Non-blocking scroll state
    bool          _scrollActive;
    const char   *_scrollText;
    uint8_t       _scrollRow;
    unsigned int  _scrollSpeed;
    int           _scrollLoops;
    int           _scrollPos;
    int           _scrollLen;
    unsigned long _scrollLastStep;
};

#endif // SMART_LCD_H
