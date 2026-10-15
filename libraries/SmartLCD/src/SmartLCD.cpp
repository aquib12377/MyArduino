/*
 * SmartLCD.cpp — Implementation
 */

#include "SmartLCD.h"
#include <stdarg.h>

// ═══════════════════════════════════════════════════════════════════
//  Constructors
// ═══════════════════════════════════════════════════════════════════

SmartLCD::SmartLCD(uint8_t i2cAddr, uint8_t cols, uint8_t rows, TwoWire &wire)
    : _mode(MODE_I2C), _cols(cols), _rows(rows), _curCol(0), _curRow(0),
      _autoWrap(true), _i2cAddr(i2cAddr), _wire(&wire),
      _backlightFlag(LCD_BACKLIGHT), _rsPin(0), _enPin(0),
      _displayCtrl(LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF),
      _blTimeout(0), _lastActivity(0), _scrollActive(false),
      _scrollText(nullptr), _scrollRow(0), _scrollSpeed(300),
      _scrollLoops(0), _scrollPos(0), _scrollLen(0), _scrollLastStep(0)
{
    memset(_dataPins, 0, sizeof(_dataPins));
    _setRowOffsets();
}

SmartLCD::SmartLCD(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5,
                   uint8_t d6, uint8_t d7, uint8_t cols, uint8_t rows)
    : _mode(MODE_PARALLEL), _cols(cols), _rows(rows), _curCol(0), _curRow(0),
      _autoWrap(true), _i2cAddr(0), _wire(nullptr),
      _backlightFlag(LCD_BACKLIGHT), _rsPin(rs), _enPin(en),
      _displayCtrl(LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF),
      _blTimeout(0), _lastActivity(0), _scrollActive(false),
      _scrollText(nullptr), _scrollRow(0), _scrollSpeed(300),
      _scrollLoops(0), _scrollPos(0), _scrollLen(0), _scrollLastStep(0)
{
    _dataPins[0] = d4;
    _dataPins[1] = d5;
    _dataPins[2] = d6;
    _dataPins[3] = d7;
    _setRowOffsets();
}

// ═══════════════════════════════════════════════════════════════════
//  Row offset helper
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::_setRowOffsets() {
    _rowOffsets[0] = 0x00;
    _rowOffsets[1] = 0x40;
    _rowOffsets[2] = _cols;          // 0x14 for 20-col
    _rowOffsets[3] = 0x40 + _cols;   // 0x54 for 20-col
}

// ═══════════════════════════════════════════════════════════════════
//  begin
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::begin() {
    if (_mode == MODE_I2C) {
        _wire->begin();
        delay(50);
        // Put into 4-bit mode (HD44780 init sequence)
        _i2cWriteNibble(0x03 << 4); delay(5);
        _i2cWriteNibble(0x03 << 4); delay(5);
        _i2cWriteNibble(0x03 << 4); delay(1);
        _i2cWriteNibble(0x02 << 4); // switch to 4-bit
    } else {
        pinMode(_rsPin, OUTPUT);
        pinMode(_enPin, OUTPUT);
        for (uint8_t i = 0; i < 4; i++) pinMode(_dataPins[i], OUTPUT);
        delay(50);
        // HD44780 4-bit init
        _parWrite4(0x03, false); delay(5);
        _parWrite4(0x03, false); delay(5);
        _parWrite4(0x03, false); delay(1);
        _parWrite4(0x02, false); // 4-bit mode
    }

    uint8_t funcSet = LCD_4BITMODE | LCD_5x8DOTS;
    funcSet |= (_rows > 1) ? LCD_2LINE : LCD_1LINE;
    _command(LCD_FUNCTIONSET | funcSet);

    _command(LCD_DISPLAYCONTROL | _displayCtrl);
    clear();
    _command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDEC);
    _lastActivity = millis();
}

// ═══════════════════════════════════════════════════════════════════
//  Core commands
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::clear() {
    _command(LCD_CLEARDISPLAY);
    delay(2);
    _curCol = 0;
    _curRow = 0;
}

void SmartLCD::home() {
    _command(LCD_RETURNHOME);
    delay(2);
    _curCol = 0;
    _curRow = 0;
}

void SmartLCD::setCursor(uint8_t col, uint8_t row) {
    if (row >= _rows) row = _rows - 1;
    _curCol = col;
    _curRow = row;
    _command(LCD_SETDDRAMADDR | (col + _rowOffsets[row]));
}

// ═══════════════════════════════════════════════════════════════════
//  Display / cursor / blink
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::displayOn()  { _displayCtrl |= LCD_DISPLAYON;  _command(LCD_DISPLAYCONTROL | _displayCtrl); }
void SmartLCD::displayOff() { _displayCtrl &= ~LCD_DISPLAYON; _command(LCD_DISPLAYCONTROL | _displayCtrl); }
void SmartLCD::cursorOn()   { _displayCtrl |= LCD_CURSORON;   _command(LCD_DISPLAYCONTROL | _displayCtrl); }
void SmartLCD::cursorOff()  { _displayCtrl &= ~LCD_CURSORON;  _command(LCD_DISPLAYCONTROL | _displayCtrl); }
void SmartLCD::blinkOn()    { _displayCtrl |= LCD_BLINKON;    _command(LCD_DISPLAYCONTROL | _displayCtrl); }
void SmartLCD::blinkOff()   { _displayCtrl &= ~LCD_BLINKON;   _command(LCD_DISPLAYCONTROL | _displayCtrl); }

// ═══════════════════════════════════════════════════════════════════
//  Backlight
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::backlightOn() {
    _backlightFlag = LCD_BACKLIGHT;
    if (_mode == MODE_I2C) _i2cWrite(_backlightFlag);
}

void SmartLCD::backlightOff() {
    _backlightFlag = LCD_NOBACKLIGHT;
    if (_mode == MODE_I2C) _i2cWrite(_backlightFlag);
}

void SmartLCD::setBacklightTimeout(unsigned long ms) {
    _blTimeout = ms;
    _lastActivity = millis();
    backlightOn();
}

void SmartLCD::tickBacklight() {
    if (_blTimeout == 0) return;
    if (_backlightFlag && (millis() - _lastActivity >= _blTimeout)) {
        backlightOff();
    }
}

// ═══════════════════════════════════════════════════════════════════
//  write() — the Print interface entry point + auto-wrap
// ═══════════════════════════════════════════════════════════════════

size_t SmartLCD::write(uint8_t ch) {
    // Wake backlight on activity
    _lastActivity = millis();
    if (!_backlightFlag) backlightOn();

    if (ch == '\n') {
        // Move to next line
        _curCol = 0;
        _curRow++;
        if (_curRow >= _rows) _curRow = 0;
        setCursor(_curCol, _curRow);
        return 1;
    }

    if (ch == '\r') return 1; // ignore CR

    // Auto-wrap: if we've reached end of current row, go to next
    if (_autoWrap && _curCol >= _cols) {
        _curCol = 0;
        _curRow++;
        if (_curRow >= _rows) _curRow = 0;
        setCursor(_curCol, _curRow);
    }

    _sendData(ch);
    _curCol++;
    return 1;
}

void SmartLCD::setAutoWrap(bool on) { _autoWrap = on; }

// ═══════════════════════════════════════════════════════════════════
//  printf
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::printf(const char *fmt, ...) {
    char buf[81];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    print(buf);  // uses Print::print → write()
}

// ═══════════════════════════════════════════════════════════════════
//  Alignment helpers
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::printAligned(const char *text, uint8_t row, LcdAlign align) {
    uint8_t len = strlen(text);
    if (len > _cols) len = _cols;
    uint8_t col = 0;
    switch (align) {
        case ALIGN_CENTER: col = (_cols - len) / 2; break;
        case ALIGN_RIGHT:  col = _cols - len;       break;
        default:           col = 0;                  break;
    }
    // Clear the row first
    setCursor(0, row);
    for (uint8_t i = 0; i < _cols; i++) _sendData(' ');
    // Print aligned text
    setCursor(col, row);
    for (uint8_t i = 0; i < len; i++) _sendData(text[i]);
    _curCol = col + len;
    _curRow = row;
}

void SmartLCD::printCentered(const char *text, uint8_t row) {
    printAligned(text, row, ALIGN_CENTER);
}

void SmartLCD::printRight(const char *text, uint8_t row) {
    printAligned(text, row, ALIGN_RIGHT);
}

// ═══════════════════════════════════════════════════════════════════
//  Scrolling — blocking version
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::scrollText(const char *text, uint8_t row, unsigned int speed,
                          int loops) {
    int len = strlen(text);
    if (len <= _cols) {
        // Fits on screen — just print it
        printAligned(text, row, ALIGN_LEFT);
        return;
    }

    // Pad with spaces so text scrolls completely off
    int padded = len + _cols;
    for (int loop = 0; loop < loops; loop++) {
        for (int pos = 0; pos < padded; pos++) {
            setCursor(0, row);
            for (uint8_t c = 0; c < _cols; c++) {
                int idx = pos + c;
                if (idx < len)
                    _sendData(text[idx]);
                else
                    _sendData(' ');
            }
            delay(speed);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Scrolling — non-blocking version
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::startScroll(const char *text, uint8_t row, unsigned int speed,
                           int loops) {
    _scrollText     = text;
    _scrollRow      = row;
    _scrollSpeed    = speed;
    _scrollLoops    = loops;
    _scrollLen      = strlen(text);
    _scrollPos      = 0;
    _scrollActive   = true;
    _scrollLastStep = millis();
}

void SmartLCD::stopScroll() {
    _scrollActive = false;
}

bool SmartLCD::tickScroll() {
    if (!_scrollActive) return false;
    if (millis() - _scrollLastStep < _scrollSpeed) return true;
    _scrollLastStep = millis();

    setCursor(0, _scrollRow);
    for (uint8_t c = 0; c < _cols; c++) {
        int idx = _scrollPos + c;
        if (idx < _scrollLen)
            _sendData(_scrollText[idx]);
        else
            _sendData(' ');
    }
    _scrollPos++;

    int padded = _scrollLen + _cols;
    if (_scrollPos >= padded) {
        _scrollPos = 0;
        if (_scrollLoops > 0) {
            _scrollLoops--;
            if (_scrollLoops == 0) {
                _scrollActive = false;
                return false;
            }
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  Custom characters
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::createChar(uint8_t slot, const uint8_t bitmap[8]) {
    slot &= 0x07;
    _command(LCD_SETCGRAMADDR | (slot << 3));
    for (uint8_t i = 0; i < 8; i++) _sendData(bitmap[i]);
    setCursor(_curCol, _curRow); // restore position
}

void SmartLCD::createChar_P(uint8_t slot, const uint8_t *bitmap) {
    uint8_t buf[8];
    for (uint8_t i = 0; i < 8; i++) buf[i] = pgm_read_byte(bitmap + i);
    createChar(slot, buf);
}

void SmartLCD::printCustom(uint8_t slot) {
    write(slot & 0x07);
}

// ═══════════════════════════════════════════════════════════════════
//  Transition effects
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::printEffect(const char *text, uint8_t row, LcdEffect effect,
                           unsigned int speed) {
    uint8_t len = strlen(text);
    if (len > _cols) len = _cols;

    switch (effect) {
        case EFFECT_TYPEWRITER:
            setCursor(0, row);
            for (uint8_t i = 0; i < _cols; i++) _sendData(' ');
            for (uint8_t i = 0; i < len; i++) {
                setCursor(i, row);
                _sendData(text[i]);
                delay(speed);
            }
            break;

        case EFFECT_WIPE:
            // Wipe from left: blank → character one column at a time
            for (uint8_t i = 0; i < _cols; i++) {
                setCursor(i, row);
                _sendData(i < len ? text[i] : ' ');
                delay(speed);
            }
            break;

        case EFFECT_BLINK_FADE:
            // Flash the whole text 3 times, then leave it on
            for (uint8_t blink = 0; blink < 3; blink++) {
                setCursor(0, row);
                for (uint8_t i = 0; i < _cols; i++)
                    _sendData(i < len ? text[i] : ' ');
                delay(speed * 2);
                setCursor(0, row);
                for (uint8_t i = 0; i < _cols; i++) _sendData(' ');
                delay(speed);
            }
            // Final display
            setCursor(0, row);
            for (uint8_t i = 0; i < _cols; i++)
                _sendData(i < len ? text[i] : ' ');
            break;

        default: // EFFECT_NONE
            printAligned(text, row, ALIGN_LEFT);
            break;
    }
    _curCol = len;
    _curRow = row;
}

// ═══════════════════════════════════════════════════════════════════
//  Low-level: command vs data routing
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::_command(uint8_t value) { _send(value, 0); }
void SmartLCD::_sendData(uint8_t value) { _send(value, RS_BIT); }

void SmartLCD::_send(uint8_t value, uint8_t rsFlag) {
    if (_mode == MODE_I2C) {
        uint8_t hi = (value & 0xF0) | rsFlag | _backlightFlag;
        uint8_t lo = ((value << 4) & 0xF0) | rsFlag | _backlightFlag;

        _i2cWrite(hi | EN_BIT);
        _i2cWrite(hi);
        _i2cWrite(lo | EN_BIT);
        _i2cWrite(lo);
    } else {
        bool isData = (rsFlag != 0);
        _parWrite4(value >> 4, isData);
        _parWrite4(value & 0x0F, isData);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  I2C low-level
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::_i2cWrite(uint8_t data) {
    _wire->beginTransmission(_i2cAddr);
    _wire->write(data);
    _wire->endTransmission();
}

void SmartLCD::_i2cWriteNibble(uint8_t nibble) {
    uint8_t data = nibble | _backlightFlag;
    _i2cWrite(data | EN_BIT);
    delayMicroseconds(1);
    _i2cWrite(data);
    delayMicroseconds(50);
}

// ═══════════════════════════════════════════════════════════════════
//  Parallel low-level  (4-bit)
// ═══════════════════════════════════════════════════════════════════

void SmartLCD::_parWrite4(uint8_t value, bool isData) {
    digitalWrite(_rsPin, isData ? HIGH : LOW);
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(_dataPins[i], (value >> i) & 0x01);
    }
    _pulseEnable();
}

void SmartLCD::_pulseEnable() {
    digitalWrite(_enPin, LOW);
    delayMicroseconds(1);
    digitalWrite(_enPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(_enPin, LOW);
    delayMicroseconds(100);
}
