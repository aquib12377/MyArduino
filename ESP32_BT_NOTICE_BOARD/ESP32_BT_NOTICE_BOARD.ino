/* -------- ESP32: Bluetooth Classic (SPP) -> 16x2 I2C LCD --------
   - Displays any incoming BT text like a tiny terminal
   - Auto-wrap at 16 chars, scrolls up after 2 lines
   - Newline '\n' forces a new line
   - Pair with PIN 1234, device name "ESP32-BT-LCD"
------------------------------------------------------------------ */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "BluetoothSerial.h"

// ---------- Build guard (BT must be enabled in ESP32 core) ----------
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth is not enabled! Enable Bluetooth in menuconfig or use an ESP32 core with BT enabled."
#endif

/* -------- USER CONFIG -------- */
#define LCD_ADDR 0x27     // change to 0x3F if your module differs
#define LCD_COLS 16
#define LCD_ROWS 2

// ESP32 default I2C pins (DevKit): SDA=21, SCL=22
#define LCD_SDA 21
#define LCD_SCL 22

// Bluetooth device name & PIN
static const char* BT_NAME = "ESP32-BT-LCD";
static const char* BT_PIN  = "1234";    // optional; many hosts accept without PIN

/* ----------------------------- */
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
BluetoothSerial   SerialBT;

String line1, line2; // current LCD lines
String cur;          // building the active line

void lcdRefresh() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.length() > LCD_COLS ? line1.substring(0, LCD_COLS) : line1);

  lcd.setCursor(0, 1);
  lcd.print(line2.length() > LCD_COLS ? line2.substring(0, LCD_COLS) : line2);
}

void pushLine(const String& s) {
  // Scroll: line1 <- line2, line2 <- s
  line1 = line2;
  line2 = s;
  lcdRefresh();
}

void flushCurToScreen() {
  if (cur.length() == 0) return;

  // Place cur respecting width on line2, else scroll
  if ((int)line2.length() < LCD_COLS) {
    line2 += cur;
  } else {
    pushLine(cur);
  }
  cur.remove(0);
  lcdRefresh();
}

void appendChar(char c) {
  if (c == '\r') return; // ignore CR
  if (c == '\n') {
    if (cur.length() == 0) {
      pushLine("");      // blank line -> just scroll
    } else {
      pushLine(cur);     // commit current buffer as a new line
      cur.remove(0);
    }
    return;
  }

  // printable range
  if (c < 32 || c > 126) return;

  cur += c;

  // If cur exceeds row width, push slices
  while ((int)cur.length() >= LCD_COLS) {
    String slice = cur.substring(0, LCD_COLS);
    pushLine(slice);
    cur = cur.substring(LCD_COLS);
  }

  // Live preview on LCD: line2 + as much of cur as fits
  String preview2 = line2;
  if ((int)preview2.length() < LCD_COLS) {
    int space = LCD_COLS - preview2.length();
    int take  = (int)cur.length() < space ? (int)cur.length() : space;
    preview2 += cur.substring(0, take);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.length() > LCD_COLS ? line1.substring(0, LCD_COLS) : line1);
  lcd.setCursor(0, 1);
  lcd.print(preview2.length() > LCD_COLS ? preview2.substring(0, LCD_COLS) : preview2);
}

void setup() {
  Serial.begin(115200);

  // I2C + LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("BT->LCD Ready");
  lcd.setCursor(0, 1); lcd.print("Waiting...");
  // Bluetooth Classic SPP server
  //SerialBT.setPin(BT_PIN);              // optional, must be before begin() on most cores
  bool ok = SerialBT.begin("ESP32-BT-LCD");    // starts SPP, device visible as "ESP32-BT-LCD"
  if (!ok) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("BT start FAIL");
    lcd.setCursor(0,1); lcd.print("Check core/BT");
  } else {
    Serial.println("Bluetooth SPP started, device name: " + String(BT_NAME));
  }
}

void loop() {
  // Read any incoming Bluetooth bytes
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    appendChar(c);
  }

  // If input pauses for 2s, flush partial line
  static unsigned long lastRx = millis();
  static size_t lastLen = 0;

  if (SerialBT.available()) {
    lastRx = millis();
  }

  if (cur.length() != lastLen) {
    lastLen = cur.length();
    lastRx  = millis();
  }

  if (millis() - lastRx > 2000 && cur.length() > 0) {
    flushCurToScreen();
  }

  // Optional: show connect status briefly (non-blocking)
  // if (SerialBT.hasClient()) { /* you could display "Connected" */ }
}
