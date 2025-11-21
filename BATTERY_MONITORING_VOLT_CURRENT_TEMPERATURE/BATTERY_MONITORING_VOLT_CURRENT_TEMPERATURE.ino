/*
  Battery Monitor – Nano + DHT11 + ACS712-05 + 25V sensor + 16x2 I2C LCD
  - No startup wait; shows values immediately
  - Row1: V:12.56  I:1.23
  - Row2: T:27.3  H:55%
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ------------------ USER CALIBRATION ------------------
static const float VREF               = 5.00;     // Measure Nano 5V with DMM
static const float R1                 = 30000.0;  // Divider upper resistor
static const float R2                 = 7500.0;   // Divider lower resistor
static const float ACS_SENS           = 0.185;    // V/A for ACS712-05B
static const uint16_t VOLT_SAMPLES    = 100;
static const uint16_t CURR_SAMPLES    = 300;
static const float CURRENT_ZERO_NOISE = 0.03;     // set 0.0 to disable
static const uint32_t LCD_UPDATE_MS   = 500;
static const uint32_t DHT_UPDATE_MS   = 2000;
// ------------------------------------------------------

const uint8_t PIN_VOLT = A0;
const uint8_t PIN_ACS  = A1;
const uint8_t DHTPIN   = 7;

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);  // change to 0x3F if needed

static const float ADC_LSB = VREF / 1023.0;

float acsZeroVoltage = VREF / 2.0; // mid-supply (no wait)
uint32_t lastLcdMs = 0, lastDhtMs = 0;
float lastTempC = NAN, lastHum = NAN;

float analogReadAverageVoltage(uint8_t pin, uint16_t samples) {
  uint32_t sum = 0;
  for (uint16_t i = 0; i < samples; i++) sum += analogRead(pin);
  return (sum / (float)samples) * ADC_LSB;
}

float readBatteryVoltage() {
  float vOut = analogReadAverageVoltage(PIN_VOLT, VOLT_SAMPLES);
  float scale = (R1 + R2) / R2;
  return vOut * scale;
}

float readCurrentDC() {
  float vAcs = analogReadAverageVoltage(PIN_ACS, CURR_SAMPLES);
  float i = (vAcs - acsZeroVoltage) / ACS_SENS;
  if (i > -CURRENT_ZERO_NOISE && i < CURRENT_ZERO_NOISE) i = 0.0f;
  return i;
}

inline void clearRow(uint8_t row) {
  lcd.setCursor(0, row);
  lcd.print("                "); // 16 spaces
}

void printRow1(float vbat, float i) {
  clearRow(0);
  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(vbat, 2);    // <-- no printf; lcd.print handles floats
  lcd.print("  I:");
  lcd.print(i, 2);
}

void printRow2(float tC, float rh) {
  clearRow(1);
  lcd.setCursor(0, 1);
  if (isnan(tC) || isnan(rh)) {
    lcd.print("T:--.-  H:--%");
  } else {
    lcd.print("T:");
    lcd.print(tC, 1);
    lcd.print("  H:");
    lcd.print(rh, 0);
    lcd.print("%");
  }
}

void setup() {
  analogReference(DEFAULT);
  Serial.begin(115200);
  Wire.begin();

  lcd.begin();      // use lcd.init() if your lib requires it
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Battery Monitor");
  lcd.setCursor(0, 1); lcd.print("Booting...");
  delay(200);

  dht.begin();

  // Immediate DHT read so something shows right away
  lastTempC = dht.readTemperature();
  lastHum   = dht.readHumidity();
  lastDhtMs = millis();

  lcd.clear();
}

void loop() {
  uint32_t now = millis();

  float vbat = readBatteryVoltage();
  float curr = readCurrentDC();

  if (now - lastDhtMs >= DHT_UPDATE_MS) {
    lastDhtMs = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) lastTempC = t;
    if (!isnan(h)) lastHum   = h;
  }

  if (now - lastLcdMs >= LCD_UPDATE_MS) {
    lastLcdMs = now;
    printRow1(vbat, curr);
    printRow2(lastTempC, lastHum);
  }

  static uint32_t lastLog = 0;
  if (now - lastLog >= 1000) {
    lastLog = now;
    Serial.print(F("Vbat=")); Serial.print(vbat, 3); Serial.print(F(" V, "));
    Serial.print(F("I="));    Serial.print(curr, 3); Serial.print(F(" A, "));
    Serial.print(F("T="));    Serial.print(lastTempC, 1); Serial.print(F(" C, "));
    Serial.print(F("H="));    Serial.print(lastHum, 0);   Serial.println(F(" %"));
  }
}
