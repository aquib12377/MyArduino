/*
   STM32 Dice Roll — Ultra Slim Edition
   =====================================
   Target : STM32F103C6 (32KB flash)
   OLED   : 0.96" SSD1306 128x64 I2C
   Lib    : SSD1306Ascii by Bill Greiman

   Wiring:
   -------
   Button    : PA0 → GND (INPUT_PULLUP)

   I2C Option A (I2C1 — recommended):
     SCL → PB6
     SDA → PB7

   I2C Option B (I2C2):
     SCL → PB10
     SDA → PB11
*/

#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// ─── Config ───────────────────────────────────────────────────────
#define FW_VERSION      "v1.2"
#define SERIAL_BAUD     115200
#define BTN_PIN         PA0
#define DEBOUNCE_MS     200
#define ROLL_STEPS      15
#define ROLL_DELAY_MS   80
#define OLED_ADDR       0x3C    // try 0x3D if blank screen

// ─── OLED ─────────────────────────────────────────────────────────
SSD1306AsciiWire oled;

unsigned long lastPressTime = 0;
uint8_t detectedAddr = 0;
unsigned long rollCount = 0;

// ─── Serial Logging Macros ────────────────────────────────────────
#define LOG_BOOT(msg)   Serial.println(F("[BOOT]  " msg))
#define LOG_INIT(msg)   Serial.println(F("[INIT]  " msg))
#define LOG_INFO(msg)   Serial.println(F("[INFO]  " msg))
#define LOG_WARN(msg)   Serial.println(F("[WARN]  " msg))
#define LOG_ERROR(msg)  Serial.println(F("[ERROR] " msg))
#define LOG_STATE(msg)  Serial.println(F("[STATE] " msg))

// ─── Screens ──────────────────────────────────────────────────────

void showIdle() {
  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.setCursor(20, 0);
  oled.print(F("[ STM32 DICE ]"));
  oled.setCursor(0, 2);
  oled.print(F("----------------"));
  oled.setCursor(24, 4);
  oled.print(F("Press Button"));
  oled.setCursor(44, 6);
  oled.print(F("[  *  ]"));

  LOG_STATE("Idle — waiting for button press");
}

void showRolling(int val) {
  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.setCursor(28, 0);
  oled.print(F("Rolling..."));
  oled.setCursor(0, 2);
  oled.print(F("----------------"));
  oled.set2X();
  oled.setCursor(52, 4);
  oled.print(val);
  oled.set1X();
}

void showResult(int dice) {
  const __FlashStringHelper* labels[] = {
    F(""),
    F("  One  :(  "),
    F("   Two     "),
    F("  Three    "),
    F("   Four    "),
    F("   Five    "),
    F(" SIX!  :D  ")
  };

  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.setCursor(20, 0);
  oled.print(F("  YOUR ROLL!  "));
  oled.setCursor(0, 2);
  oled.print(F("----------------"));
  oled.set2X();
  oled.setCursor(52, 3);
  oled.print(dice);
  oled.set1X();
  oled.setCursor(16, 6);
  oled.print(labels[dice]);

  // ── Serial result log ──
  Serial.print(F("[INFO]  Result: "));
  Serial.print(dice);
  Serial.print(F(" | Roll #"));
  Serial.println(rollCount);
}

void showError() {
  oled.clear();
  oled.setFont(Adafruit5x7);
  oled.setCursor(0, 0);
  oled.print(F("  OLED ERROR!   "));
  oled.setCursor(0, 2);
  oled.print(F(" Check wiring   "));
  oled.setCursor(0, 4);
  oled.print(F(" 0x3C or 0x3D?  "));
}

// ─── OLED Init ────────────────────────────────────────────────────

bool initOLED() {
  LOG_INIT("Scanning I2C for OLED...");

  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    detectedAddr = 0x3C;
    oled.begin(&Adafruit128x64, 0x3C);
    oled.setFont(Adafruit5x7);
    LOG_INIT("OLED found at 0x3C OK");
    return true;
  }

  Wire.beginTransmission(0x3D);
  if (Wire.endTransmission() == 0) {
    detectedAddr = 0x3D;
    oled.begin(&Adafruit128x64, 0x3D);
    oled.setFont(Adafruit5x7);
    LOG_INIT("OLED found at 0x3D OK");
    return true;
  }

  LOG_ERROR("OLED not found at 0x3C or 0x3D — check SDA/SCL wiring");
  return false;
}

// ─── Roll Logic ───────────────────────────────────────────────────

void rollDice() {
  rollCount++;

  Serial.print(F("[STATE] Roll started | Count: "));
  Serial.println(rollCount);

  // Animation — no serial spam inside loop
  for (int i = 0; i < ROLL_STEPS; i++) {
    showRolling(random(1, 7));
    delay(ROLL_DELAY_MS);
  }

  int result = random(1, 7);
  showResult(result);   // serial log happens inside showResult()
  delay(2000);
  // Wait for button release
  while (digitalRead(BTN_PIN) == LOW);
  delay(DEBOUNCE_MS);

  LOG_STATE("Button released — returning to idle");
}

// ─── Setup ────────────────────────────────────────────────────────

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  LOG_BOOT("================================");
  LOG_BOOT("STM32 DICE ROLL  " FW_VERSION);
  LOG_BOOT("Target: STM32F103C6 (32KB)");
  LOG_BOOT("================================");

  Serial.print(F("[BOOT]  Baud rate : "));
  Serial.println(SERIAL_BAUD);
  Serial.print(F("[BOOT]  Roll steps: "));
  Serial.println(ROLL_STEPS);
  Serial.print(F("[BOOT]  Roll delay: "));
  Serial.print(ROLL_DELAY_MS);
  Serial.println(F("ms"));

  // Button
  pinMode(BTN_PIN, INPUT_PULLUP);
  LOG_INIT("Button PA0 INPUT_PULLUP OK");

  // LED
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  LOG_INIT("PC13 LED init OK");

  // I2C
  Wire.begin();
  LOG_INIT("Wire (I2C1 PB6/PB7) started");

  // OLED
  if (!initOLED()) {
    LOG_ERROR("Halting — OLED not found");
    while (true) {
      digitalWrite(PC13, LOW);  delay(150);
      digitalWrite(PC13, HIGH); delay(150);
    }
  }

  Serial.print(F("[INIT]  OLED address : 0x"));
  Serial.println(detectedAddr, HEX);

  // Random seed
  uint16_t seed = analogRead(PA1);
  randomSeed(seed);
  Serial.print(F("[INIT]  Random seed  : "));
  Serial.println(seed);

  LOG_BOOT("Ready — press button to roll");

  showIdle();
}

// ─── Loop ─────────────────────────────────────────────────────────

void loop() {
  if (digitalRead(BTN_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastPressTime > DEBOUNCE_MS) {
      lastPressTime = now;
      LOG_INFO("Button pressed");
      rollDice();
      showIdle();
    }
  }
}