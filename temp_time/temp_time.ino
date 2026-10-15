#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include "time.h"

// WiFi
const char* ssid = "AAA";
const char* password = "Acube123";

// Time
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);

// DHT
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

bool systemReady = false;

void setup() {
  Serial.begin(115200);

  lcd.begin();          // ✅ better than begin()
  lcd.backlight();
  dht.begin();

  // ===== WIFI CONNECT SCREEN =====
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("Please wait...   ");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");

  // ===== TIME SYNC =====
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  lcd.setCursor(0, 1);
  lcd.print("Syncing time...");

  while (!getLocalTime(&timeinfo)) {
    delay(1000);
  }

  lcd.clear();
  systemReady = true;
}

void loop() {

  if (!systemReady) return;

  // Read sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Get time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) {
    lcd.setCursor(0, 1);
    lcd.print("Time Error       ");
    return;
  }

  char timeStr[20];
  char dateStr[20];

  // 12-hour format
  strftime(timeStr, sizeof(timeStr), "%l:%M:%S %p", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%d/%m/%y", &timeinfo);

  // ===== DISPLAY (FINAL FORMAT) =====

  // Row 0 → Date
  lcd.setCursor(0, 0);
  lcd.print("Date: ");
  lcd.print(dateStr);
  lcd.print("      ");

  // Row 1 → Time
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(timeStr);
  lcd.print("   ");

  // Row 2 → Temperature
  lcd.setCursor(0, 2);
  lcd.print("Temp: ");
  lcd.print(temp, 1);
  lcd.print((char)223);
  lcd.print("C     ");

  // Row 3 → Humidity
  lcd.setCursor(0, 3);
  lcd.print("Hum : ");
  lcd.print(hum, 0);
  lcd.print("%     ");

  delay(2000);
}