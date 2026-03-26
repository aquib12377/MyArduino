// ============================================================
//  Solar_Tracker.ino  — Memory-Optimised Build
//  Dual-LDR Solar Tracker  +  Temperature  +  Voltage Monitor
//
//  Wiring:
//    Voltage Sensor (0–25 V)  →  A2
//    LDR Left  (Digital)      →  A0  (LOW = light, HIGH = dark)
//    LDR Right (Digital)      →  A1  (LOW = light, HIGH = dark)
//    DHT11 Data               →  D2  (no external pull-up needed)
//    Servo Signal             →  D9
//    LCD I2C SDA              →  A4
//    LCD I2C SCL              →  A5
//
//  Memory optimisations applied:
//    • ALL string literals moved to Flash via F() macro
//    • String class removed — replaced with enum + const char* PROGMEM
//    • lcd.clear() replaced with targeted overwrites (no flicker either)
//    • Voltage averaging over 4 samples (costs 0 extra RAM)
//
//  Libraries required:
//    • DHT sensor library    (Adafruit)
//    • Servo                 (built-in)
//    • LiquidCrystal_I2C     (Frank de Brabander)
// ============================================================

#include <Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/pgmspace.h>       // PROGMEM / pgm_read_byte

// ── Pin Definitions ──────────────────────────────────────────
#define PIN_VOLT_SENSOR   A2
#define PIN_LDR_LEFT      A0
#define PIN_LDR_RIGHT     A1
#define PIN_DHT11          2
#define DHT_TYPE           DHT11
#define PIN_SERVO          9

// ── LCD ──────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);  // change to 0x3F if blank

// ── Tuning ───────────────────────────────────────────────────
#define SERVO_MIN          30
#define SERVO_MAX         150
#define SERVO_START        90
#define SERVO_STEP          3
#define LOOP_DELAY        150

#define VDIV_FACTOR        5.0f
#define VREF               5.0f
#define ADC_RESOLUTION  1023.0f

#define REPORT_INTERVAL  1000
#define LCD_INTERVAL     1000

// ── Direction — enum instead of String (saves ~50 bytes heap) ─
enum Direction : uint8_t {
  DIR_ALIGNED = 0,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_NOSUN
};

// Labels stored in Flash, not RAM
const char _dAligned[] PROGMEM = "Aligned    ";
const char _dLeft[]    PROGMEM = "Moving LEFT";
const char _dRight[]   PROGMEM = "Moving RIGHT";
const char _dNoSun[]   PROGMEM = "No Sun     ";

const char* const DIR_LABELS[] PROGMEM = {
  _dAligned, _dLeft, _dRight, _dNoSun
};

// Helper: print a PROGMEM direction label to Serial
void printDirLabel(Direction d) {
  PGM_P p = (PGM_P)pgm_read_word(&DIR_LABELS[d]);
  char c;
  while ((c = pgm_read_byte(p++))) Serial.print(c);
}

// ─────────────────────────────────────────────────────────────
Servo trackServo;
DHT dht(PIN_DHT11, DHT_TYPE);

int           servoAngle = SERVO_START;
unsigned long lastReport = 0;
unsigned long lastLCD    = 0;

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  pinMode(PIN_LDR_LEFT,  INPUT);
  pinMode(PIN_LDR_RIGHT, INPUT);

  trackServo.attach(PIN_SERVO);
  trackServo.write(servoAngle);
  delay(500);

  dht.begin();

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print(F("  Solar Tracker "));
  lcd.setCursor(0, 1); lcd.print(F("  Initialising.."));
  delay(1500);
  lcd.clear();

  // All string literals in Flash via F() — zero RAM used
  Serial.println(F("╔══════════════════════════════════════════════════╗"));
  Serial.println(F("║         Solar Tracker — Ready                    ║"));
  Serial.println(F("╠══════════════════════════════════════════════════╣"));
  Serial.println(F("║  LDR logic : LOW = light  |  HIGH = dark         ║"));
  Serial.println(F("╚══════════════════════════════════════════════════╝"));
  Serial.println();
  Serial.println(F("  Angle | LDR_L | LDR_R | Direction    | Volt(V) | Temp(C) | Hum(%)"));
  Serial.println(F("  ------+-------+-------+--------------+---------+---------+-------"));
}

// ─────────────────────────────────────────────────────────────
void loop() {
  bool ldrLeft  = digitalRead(PIN_LDR_LEFT);
  bool ldrRight = digitalRead(PIN_LDR_RIGHT);

  Direction dir = trackSun(ldrLeft, ldrRight);

  float voltage = readVoltage();
  float tempC   = readTemperature();

  unsigned long now = millis();

  if (now - lastReport >= REPORT_INTERVAL) {
    lastReport = now;
    printReport(ldrLeft, ldrRight, dir, voltage, tempC);
  }

  if (now - lastLCD >= LCD_INTERVAL) {
    lastLCD = now;
    updateLCD(ldrLeft, ldrRight, voltage, tempC, dir);
  }

  delay(LOOP_DELAY);
}

// ─────────────────────────────────────────────────────────────
Direction trackSun(bool ldrLeft, bool ldrRight) {

  if (!ldrLeft && !ldrRight) {
    Serial.println(F("  [TRACK] Both lit — aligned, no move."));
    return DIR_ALIGNED;
  }

  if (!ldrLeft && ldrRight) {
    servoAngle = constrain(servoAngle - SERVO_STEP, SERVO_MIN, SERVO_MAX);
    trackServo.write(servoAngle);
    Serial.print(F("  [TRACK] Light LEFT  → Angle: "));
    Serial.println(servoAngle);
    return DIR_LEFT;
  }

  if (ldrLeft && !ldrRight) {
    servoAngle = constrain(servoAngle + SERVO_STEP, SERVO_MIN, SERVO_MAX);
    trackServo.write(servoAngle);
    Serial.print(F("  [TRACK] Light RIGHT → Angle: "));
    Serial.println(servoAngle);
    return DIR_RIGHT;
  }

  Serial.println(F("  [TRACK] Both dark — holding position."));
  return DIR_NOSUN;
}

// ─────────────────────────────────────────────────────────────
// Average 4 ADC readings to reduce noise (no extra RAM cost)
// ─────────────────────────────────────────────────────────────
float readVoltage() {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 4; i++) sum += analogRead(PIN_VOLT_SENSOR);
  float vOut = ((sum >> 2) / ADC_RESOLUTION) * VREF;  // >>2 = /4
  return vOut * VDIV_FACTOR;
}

// ─────────────────────────────────────────────────────────────
// readTemperature() — DHT11 (also reads humidity, stored globally)
// DHT11 needs ~1 s between reads; call no faster than REPORT_INTERVAL
// ─────────────────────────────────────────────────────────────
float g_humidity = 0.0f;   // stored so LCD can show it too

float readTemperature() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();   // Celsius
  if (isnan(t) || isnan(h)) {
    Serial.println(F("  [WARN] DHT11 read failed!"));
    return -999.0f;
  }
  g_humidity = h;
  return t;
}

// ─────────────────────────────────────────────────────────────
// printReport() — no String objects, all literals in Flash
// ─────────────────────────────────────────────────────────────
void printReport(bool ldrL, bool ldrR, Direction dir,
                 float voltage, float tempC) {
  Serial.print(F("  "));
  if (servoAngle < 100) Serial.print(F("0"));
  if (servoAngle <  10) Serial.print(F("0"));
  Serial.print(servoAngle);
  Serial.print(F("°  | "));
  Serial.print(ldrL ? F("DARK ") : F("LIGHT"));
  Serial.print(F(" | "));
  Serial.print(ldrR ? F("DARK ") : F("LIGHT"));
  Serial.print(F(" | "));
  printDirLabel(dir);
  Serial.print(F(" | "));
  Serial.print(voltage, 2);
  Serial.print(F(" V  | "));
  if (tempC == -999.0f) {
    Serial.println(F("ERR C  | ERR %"));
  } else {
    Serial.print(tempC, 1);
    Serial.print(F(" C  | "));
    Serial.print(g_humidity, 1);
    Serial.println(F(" %"));
  }
}

// ─────────────────────────────────────────────────────────────
// updateLCD() — no lcd.clear() to avoid flicker + saves cycles
//
//   Row 0:  A:090°  12.34V
//   Row 1:  L:LIT R:DRK 28.5
// ─────────────────────────────────────────────────────────────
void updateLCD(bool ldrL, bool ldrR, float voltage,
               float tempC, Direction dir) {
  // ── Row 0: Angle + Voltage ─────────────────────────────────
  lcd.setCursor(0, 0);
  lcd.print(F("A:"));
  if (servoAngle < 100) lcd.print(F("0"));
  if (servoAngle <  10) lcd.print(F("0"));
  lcd.print(servoAngle);
  lcd.print((char)223);   // °
  lcd.print(F(" "));

  // Voltage padded to 6 chars to overwrite any leftover digits
  if (voltage < 10.0f) lcd.print(F(" "));
  lcd.print(voltage, 2);
  lcd.print(F("V "));

  // ── Row 1: Temp + Humidity ─────────────────────────────────
  // Format: T:28.5C  H:65%
  lcd.setCursor(0, 1);
  lcd.print(F("T:"));
  if (tempC == -999.0f) {
    lcd.print(F("ERR     "));
  } else {
    if (tempC < 10.0f) lcd.print(F(" "));
    lcd.print(tempC, 1);
    lcd.print((char)223);
    lcd.print(F("C "));
    lcd.print(F("H:"));
    if (g_humidity < 10.0f) lcd.print(F(" "));
    lcd.print((uint8_t)g_humidity);
    lcd.print(F("% "));
  }
}
