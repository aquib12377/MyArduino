/**
 * ============================================================
 *  Nitrous Oxide Monitor — ESP32 + MH-Z19E + I2C LCD + Buzzer
 * ============================================================
 *  Sensor   : MH-Z19E  (UART, 9600 baud)
 *  Display  : 16x2 LCD with I2C backpack (PCF8574, 0x27)
 *  Alert    : Active/Passive Buzzer
 * 
 *  Wiring:
 *    MH-Z19E TX  → GPIO16 (ESP32 RX2)
 *    MH-Z19E RX  → GPIO17 (ESP32 TX2)
 *    MH-Z19E VCC → 5V
 *    MH-Z19E GND → GND
 *    LCD SDA     → GPIO21
 *    LCD SCL     → GPIO22
 *    Buzzer +    → GPIO26
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ── Pin Config ───────────────────────────────────────────────
#define RX2_PIN       16      // ESP32 RX2 ← MH-Z19E TX
#define TX2_PIN       17      // ESP32 TX2 → MH-Z19E RX
#define BUZZER_PIN    27      // Buzzer signal pin

// ── Nitrous Oxide Thresholds (ppm) ───────────────────────────
#define N2O_WARN      0    // Yellow zone — ventilate soon
#define N2O_DANGER    30    // Red zone — alert buzzer

// ── LCD Setup (address 0x27, 16 cols, 2 rows) ────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── MH-Z19E UART Command ─────────────────────────────────────
const byte CMD_READ_N2O[9] = {0xFF, 0x01, 0x86, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x79};

// ── Globals ──────────────────────────────────────────────────
int  n2o_ppm       = 0;
bool buzzerActive  = false;

// ── Custom LCD characters ─────────────────────────────────────
byte charOK[8]     = {0b00000,0b00001,0b00011,0b10110,
                      0b11100,0b01000,0b00000,0b00000}; // ✓
byte charWarn[8]   = {0b00100,0b00100,0b01110,0b01110,
                      0b11111,0b11111,0b00100,0b00000}; // ↑ exclaim
byte charHigh[8]   = {0b11111,0b11111,0b11111,0b11111,
                      0b11111,0b11111,0b11111,0b11111}; // ■ block

// ─────────────────────────────────────────────────────────────
//  Compute MH-Z19 checksum
// ─────────────────────────────────────────────────────────────
byte calcChecksum(byte* packet) {
  byte sum = 0;
  for (int i = 1; i < 8; i++) sum += packet[i];
  return 0xFF - sum + 1;
}

// ─────────────────────────────────────────────────────────────
//  Read Nitrous Oxide from MH-Z19E over UART2
// ─────────────────────────────────────────────────────────────
int readN2O() {
  // Flush old data
  while (Serial2.available()) Serial2.read();

  // Send read command
  Serial2.write(CMD_READ_N2O, 9);
  Serial2.flush();

  // Wait for 9-byte response (max 500 ms)
  unsigned long t = millis();
  while (Serial2.available() < 9) {
    if (millis() - t > 500) return -1;   // timeout
  }

  byte res[9];
  Serial2.readBytes(res, 9);

  // Validate start byte and command byte
  if (res[0] != 0xFF || res[1] != 0x86) return -2;

  // Validate checksum
  if (res[8] != calcChecksum(res)) return -3;

  return (res[2] << 8) | res[3];
}

// ─────────────────────────────────────────────────────────────
//  Update LCD display
// ─────────────────────────────────────────────────────────────
void updateLCD(int ppm) {
  lcd.clear();

  // ── Row 0: Nitrous Oxide value ──
  lcd.setCursor(0, 0);
  lcd.print("N2O: ");
  if (ppm < 0) {
    lcd.print("ERR ");
    lcd.print(ppm);
  } else {
    lcd.print(ppm);
    lcd.print(" ppm   ");
  }

  // ── Row 1: Status ──
  lcd.setCursor(0, 1);
  if (ppm < 0) {
    lcd.print("Sensor Error!   ");
  } else if (ppm < N2O_WARN) {
    lcd.write(0);                     // ✓ icon
    lcd.print(" Good Air       ");
  } else if (ppm < N2O_DANGER) {
    lcd.write(1);                     // warn icon
    lcd.print(" Ventilate!     ");
  } else {
    lcd.write(2);                     // block icon
    lcd.print(" DANGER! Open   ");
  }
}

// ─────────────────────────────────────────────────────────────
//  Buzzer: beep pattern based on Nitrous Oxide level
// ─────────────────────────────────────────────────────────────
void handleBuzzer(int ppm) {
  if (ppm >= N2O_DANGER) {
    // Fast triple beep for danger
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(150);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  } 
   else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ─────────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // LCD init
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, charOK);
  lcd.createChar(1, charWarn);
  lcd.createChar(2, charHigh);

  // Splash screen
  lcd.setCursor(0, 0);
  lcd.print("  N2O Monitor   ");
  lcd.setCursor(0, 1);
  lcd.print("  MH-Z19E v1.0  ");
  delay(2000);

  // MH-Z19E warm-up notice (sensor needs ~3 min after power-on)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Warming up...   ");
  lcd.setCursor(0, 1);
  lcd.print("Please wait 60s ");

  // Short warm-up wait (adjust to 180000 for full 3-min warm-up)
  delay(3000);

  lcd.clear();
  Serial.println("[N2O Monitor] Ready.");
}

// ─────────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
  n2o_ppm = readN2O();

  // Serial debug output
  if (n2o_ppm < 0) {
    Serial.printf("[ERROR] Nitrous Oxide read failed (code: %d)\n", n2o_ppm);
  } else {
    Serial.printf("[N2O] %d ppm", n2o_ppm);
    if      (n2o_ppm < N2O_WARN)   Serial.println(" — Good");
    else if (n2o_ppm < N2O_DANGER) Serial.println(" — Ventilate!");
    else                           Serial.println(" — DANGER!");
  }

  updateLCD(n2o_ppm/200);
  handleBuzzer(n2o_ppm/200);

  delay(500);   // MH-Z19E updates every ~5 seconds
}
