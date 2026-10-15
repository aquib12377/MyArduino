/*
 * TDS + DS18B20 Monitor on SSD1306 OLED
 * Board  : Arduino Nano
 * OLED   : SSD1306 128x64 I2C  → SDA=A4, SCL=A5
 * DS18B20: OneWire              → D4 (with 4.7kΩ pull-up to 5V)
 * TDS    : Analog sensor        → A0
 *
 * Libraries (install via Library Manager):
 *   - Adafruit SSD1306
 *   - Adafruit GFX
 *   - OneWire
 *   - DallasTemperature
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ─── Pin Definitions ──────────────────────────────────────────────
#define TDS_PIN        A0
#define ONE_WIRE_PIN   2

// ─── OLED Config ──────────────────────────────────────────────────
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1        // Not used; share Arduino reset
#define OLED_ADDRESS   0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Sensor Objects ───────────────────────────────────────────────
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature tempSensor(&oneWire);

// ─── TDS Constants ────────────────────────────────────────────────
#define VREF           5.0       // ADC reference voltage
#define SCOUNT         30        // Median filter sample count
#define TDS_FACTOR     0.5       // ppm = EC(µS/cm) * 0.5  (standard approx.)

int analogBuffer[SCOUNT];
int analogBufferIndex = 0;

// ─── Helpers ──────────────────────────────────────────────────────

// Median filter to reject noise spikes
int getMedianValue(int* buf, int size) {
  int sorted[SCOUNT];
  memcpy(sorted, buf, size * sizeof(int));
  // Bubble sort (small array, fine for Nano)
  for (int i = 0; i < size - 1; i++)
    for (int j = 0; j < size - 1 - i; j++)
      if (sorted[j] > sorted[j + 1]) {
        int tmp = sorted[j]; sorted[j] = sorted[j + 1]; sorted[j + 1] = tmp;
      }
  return sorted[size / 2];
}

// Temperature-compensated TDS (reference temp = 25 °C)
float calcTDS(int rawADC, float tempC) {
  float voltage    = rawADC * VREF / 1024.0;
  float compCoeff  = 1.0 + 0.02 * (tempC - 25.0);   // 2 %/°C compensation
  float compVolt   = voltage / compCoeff;
  // Quadratic formula from DFRobot TDS probe calibration
  float tdsValue   = (133.42 * pow(compVolt, 3)
                    - 255.86 * pow(compVolt, 2)
                    + 857.39 * compVolt) * TDS_FACTOR;
  return tdsValue;
}

// Water quality label based on TDS ppm
const char* qualityLabel(float tds) {
  if (tds < 50)   return "Excellent";
  if (tds < 150)  return "Good";
  if (tds < 300)  return "Fair";
  if (tds < 500)  return "Poor";
  return "Bad";
}

// ─── Setup ────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 init failed!"));
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Splash screen
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println(F("TDS + Temp Monitor"));
  display.setCursor(38, 38);
  display.println(F("Starting..."));
  display.display();
  delay(1500);

  tempSensor.begin();
  Serial.println(F("Sensor ready."));
}

// ─── Loop ─────────────────────────────────────────────────────────

unsigned long lastSampleTime  = 0;
unsigned long lastDisplayTime = 0;

void loop() {
  unsigned long now = millis();

  // Fill sample buffer at ~40 ms intervals
  if (now - lastSampleTime >= 40UL) {
    lastSampleTime = now;
    analogBuffer[analogBufferIndex] = analogRead(TDS_PIN);
    analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;
  }

  // Update display every 1 second
  if (now - lastDisplayTime >= 1000UL) {
    lastDisplayTime = now;

    // — Read temperature —
    tempSensor.requestTemperatures();
    float tempC = tempSensor.getTempCByIndex(0);
    bool  tempOK = (tempC != DEVICE_DISCONNECTED_C);

    // — Read TDS (use 25 °C fallback if sensor absent) —
    int   medianADC = getMedianValue(analogBuffer, SCOUNT);
    float usedTemp  = tempOK ? tempC : 25.0;
    float tds       = calcTDS(medianADC, usedTemp);

    // Serial log
    Serial.print(F("Temp: "));   Serial.print(tempC, 1);  Serial.print(F(" C  |  "));
    Serial.print(F("TDS: "));    Serial.print(tds,  1);   Serial.println(F(" ppm"));

    // — Draw OLED ——————————————————————————————————————
    display.clearDisplay();

    // ── Header bar ──
    display.fillRect(0, 0, 128, 13, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(22, 3);
    display.print(F("WATER  MONITOR"));

    display.setTextColor(SSD1306_WHITE);

    // ── Temperature row ──
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print(F("TEMP"));

    display.setTextSize(2);
    display.setCursor(40, 15);
    if (tempOK) {
      display.print(tempC, 1);
      display.setTextSize(1);
      display.setCursor(104, 15);
      display.print(F("\xF8""C"));      // degree symbol
    } else {
      display.print(F("--.-"));
      display.setTextSize(1);
      display.setCursor(104, 15);
      display.print(F("\xF8""C"));
    }

    // Divider
    display.drawFastHLine(0, 34, 128, SSD1306_WHITE);

    // ── TDS row ──
    display.setTextSize(1);
    display.setCursor(0, 38);
    display.print(F("TDS"));

    display.setTextSize(2);
    display.setCursor(32, 36);
    display.print((int)tds);
    display.setTextSize(1);
    display.setCursor(96, 36);
    display.print(F("ppm"));

    // Divider
    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);

    // ── Quality label ──
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(F("Quality: "));
    display.print(qualityLabel(tds));

    display.display();
  }
}