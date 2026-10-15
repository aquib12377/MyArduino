/*
 * ============================================================
 *  CAR BRAKE TEMPERATURE MONITOR
 *  Hardware : Arduino Nano
 *  Sensors  : 4x MLX90614 (I2C, unique addresses)
 *  Display  : 16x2 LCD via PCF8574 I2C backpack
 *  Output   : Buzzer (high-pitch), Active-LOW Relay
 * ============================================================
 *
 *  MLX90614 I2C ADDRESSES  (must be pre-programmed):
 *    FL (Front-Left)  => 0x5A  (factory default)
 *    FR (Front-Right) => 0x5B
 *    RL (Rear-Left)   => 0x5C
 *    RR (Rear-Right)  => 0x5D
 *
 *  WIRING SUMMARY:
 *  ┌─────────────────────────────────────────────────────┐
 *  │  All MLX90614 + LCD  → SDA=A4, SCL=A5, 3.3 V / 5 V│
 *  │  Buzzer (+)          → D9  (PWM for pitch control)  │
 *  │  Relay IN            → D8  (active LOW)             │
 *  └─────────────────────────────────────────────────────┘
 *
 *  LIBRARIES NEEDED (install via Library Manager):
 *    • Adafruit MLX90614  (Adafruit)
 *    • LiquidCrystal_I2C  (Frank de Brabander / johnrickman)
 *    • Wire               (built-in)
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

// ── Pin Definitions ──────────────────────────────────────────
#define BUZZER_PIN    3    // PWM pin for tone()
#define RELAY_PIN     2    // Active-LOW relay

// ── Thresholds & Timing ──────────────────────────────────────
#define TEMP_THRESHOLD    75.0   // °C  — alarm trigger
#define RELAY_ON_TIME  60000UL   // 60 s in ms
#define BEEP_FREQ_WARN  3500     // Hz  — warning tone
#define BEEP_FREQ_OK     800     // Hz  — brief "all clear" tone
#define BEEP_OK_DUR      120     // ms  — "all clear" duration
#define SCAN_INTERVAL   1000UL   // ms  — sensor poll rate

// ── Sensor I2C Addresses ─────────────────────────────────────
const uint8_t MLX_ADDR[4] = { 0x5A, 0x5B, 0x5C, 0x5D };
const char*   WHEEL_LBL[4] = { "FL", "FR", "RL", "RR" };

// ── Objects ──────────────────────────────────────────────────
Adafruit_MLX90614 mlx[4];
LiquidCrystal_I2C lcd(0x27, 16, 2);  // change 0x27 → 0x3F if needed

// ── State Variables ───────────────────────────────────────────
float      tempC[4]       = {0};
bool       sensorOK[4]    = {false};
bool       alarmActive    = false;
bool       relayOn        = false;
unsigned long relayStartMs = 0;
unsigned long lastScanMs   = 0;
bool       prevAlarm      = false;  // edge-detect for "all clear" beep

// ── Custom LCD Characters ─────────────────────────────────────
// Degree symbol  (°)
byte degChar[8] = {
  0b01110, 0b01010, 0b01110,
  0b00000, 0b00000, 0b00000, 0b00000, 0b00000
};
// Flame / warning (▲)
byte warnChar[8] = {
  0b00100, 0b01110, 0b01110,
  0b11111, 0b11111, 0b01110, 0b00100, 0b00000
};

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  // GPIO
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // relay OFF (active LOW)
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, degChar);
  lcd.createChar(1, warnChar);

  splashScreen();

  // Initialise sensors
  for (uint8_t i = 0; i < 4; i++) {
    sensorOK[i] = mlx[i].begin(MLX_ADDR[i]);
    if (!sensorOK[i]) {
      Serial.print(F("MLX90614 "));
      Serial.print(WHEEL_LBL[i]);
      Serial.println(F(" NOT FOUND — check address/wiring"));
    }
  }

  // Brief sensor check display
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("Sensor Check:"));
  lcd.setCursor(0, 1);
  for (uint8_t i = 0; i < 4; i++) {
    lcd.print(WHEEL_LBL[i]);
    lcd.print(sensorOK[i] ? F("\x01 ") : F("X "));
  }
  delay(2000);
  lcd.clear();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── 1. Sensor Poll (every SCAN_INTERVAL ms) ──────────────
  if (now - lastScanMs >= SCAN_INTERVAL) {
    lastScanMs = now;
    readSensors();
    evaluateAlarm();
    updateLCD();
    printSerial();
  }

  // ── 2. Buzzer management ─────────────────────────────────
  if (alarmActive) {
    // Continuous high-pitch tone while alarm is active
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    noTone(BUZZER_PIN);
    // Play a brief "all clear" beep once when alarm clears
    if (prevAlarm && !alarmActive) {
      //tone(BUZZER_PIN, BEEP_FREQ_OK, BEEP_OK_DUR);
    }
  }
  prevAlarm = alarmActive;

  // ── 3. Relay auto-off after 60 s ─────────────────────────
  if (relayOn && (now - relayStartMs >= RELAY_ON_TIME)) {
    deactivateRelay();
  }
}

// ─────────────────────────────────────────────────────────────
void readSensors() {
  for (uint8_t i = 0; i < 4; i++) {
    if (sensorOK[i]) {
      tempC[i] = mlx[i].readObjectTempC();
      // Sanity-check: MLX returns ~-273 on read failure
      if (tempC[i] < -50 || tempC[i] > 300) {
        tempC[i]  = -99;
        sensorOK[i] = false;    // flag fault
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────
void evaluateAlarm() {
  alarmActive = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (sensorOK[i] && tempC[i] >= TEMP_THRESHOLD) {
      alarmActive = true;
      break;
    }
  }

  if (alarmActive && !relayOn) {
    activateRelay();
  }
}

// ─────────────────────────────────────────────────────────────
void activateRelay() {
  digitalWrite(RELAY_PIN, LOW);   // active LOW → relay ON
  relayOn      = true;
  relayStartMs = millis();
  Serial.println(F(">>> RELAY ON — cooling activated <<<"));
}

void deactivateRelay() {
  digitalWrite(RELAY_PIN, HIGH);  // relay OFF
  relayOn = false;
  Serial.println(F(">>> RELAY OFF — 60 s elapsed <<<"));
}

// ─────────────────────────────────────────────────────────────
/*  LCD LAYOUT  (16 columns, 2 rows)
 *
 *  Normal (no alarm):
 *  ┌────────────────┐
 *  │FL:025° FR:027° │
 *  │RL:022° RR:024° │
 *  └────────────────┘
 *
 *  Alarm (≥30°C):
 *  ┌────────────────┐
 *  │▲FL:031° FR:035°│   ← ▲ marks the hot wheel(s)
 *  │RL:022° RR:024° │
 *  └────────────────┘
 */
void updateLCD() {
  // Row 0: FL  FR
  lcd.setCursor(0, 0);
  printWheelTemp(0);   // FL — occupies cols 0-6
  lcd.setCursor(8, 0);
  printWheelTemp(1);   // FR — occupies cols 8-15

  // Row 1: RL  RR
  lcd.setCursor(0, 1);
  printWheelTemp(2);   // RL
  lcd.setCursor(8, 1);
  printWheelTemp(3);   // RR
}

/*  Prints one wheel cell: "FL:025°" or "▲FL:031°"
 *  Always exactly 8 characters wide to keep columns clean.
 */
void printWheelTemp(uint8_t idx) {
  bool hot = sensorOK[idx] && tempC[idx] >= TEMP_THRESHOLD;

  // Warning flag (1 char)
  if (hot) lcd.write(byte(1));   // flame char
  else     lcd.print(' ');

  // Label (2 chars)
  lcd.print(WHEEL_LBL[idx]);
  lcd.print(':');

  // Temperature (3 chars + degree symbol)
  if (!sensorOK[idx]) {
    lcd.print(F("ERR "));
  } else {
    int t = (int)tempC[idx];
    // Clamp to 3-digit display
    if (t < 0)   { t = 0; }
    if (t > 999) { t = 999; }

    if (t < 10)  lcd.print(' ');
    if (t < 100) lcd.print(' ');
    lcd.print(t);
    lcd.write(byte(0));   // degree char
  }
  // 8th char = space separator / padding
  lcd.print(' ');
}

// ─────────────────────────────────────────────────────────────
void printSerial() {
  Serial.println(F("=============================="));
  Serial.println(F("  BRAKE TEMPERATURE MONITOR"));
  Serial.println(F("------------------------------"));
  for (uint8_t i = 0; i < 4; i++) {
    Serial.print(F("  "));
    Serial.print(WHEEL_LBL[i]);
    Serial.print(F(" : "));
    if (sensorOK[i]) {
      Serial.print(tempC[i], 1);
      Serial.print(F(" °C"));
      if (tempC[i] >= TEMP_THRESHOLD) Serial.print(F("  ⚠ ALARM"));
    } else {
      Serial.print(F("SENSOR ERROR"));
    }
    Serial.println();
  }
  Serial.print(F("  Alarm : ")); Serial.println(alarmActive ? F("ACTIVE") : F("OK"));
  Serial.print(F("  Relay : ")); Serial.println(relayOn     ? F("ON")     : F("OFF"));
  if (relayOn) {
    unsigned long elapsed = millis() - relayStartMs;
    Serial.print(F("  Relay time left : "));
    Serial.print((RELAY_ON_TIME - elapsed) / 1000);
    Serial.println(F(" s"));
  }
  Serial.println(F("==============================\n"));
}

// ─────────────────────────────────────────────────────────────
void splashScreen() {
  lcd.clear();
  lcd.setCursor(1, 0); lcd.print(F("BRAKE  TEMP  MON"));
  lcd.setCursor(3, 1); lcd.print(F("Initializing..."));
  delay(2000);
  lcd.clear();
}
