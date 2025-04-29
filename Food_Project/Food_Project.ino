#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MQ135.h>
#include <MQ137.h>
#include <DHT.h>

// --- Pin Definitions ---
#define PIN_MQ135 A0
#define PIN_MQ137 A1
#define DHT_PIN 2
#define DHT_TYPE DHT11

// --- Sensor Constants ---
const float MQ137_R0 = 26.21f;

// --- Objects ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
MQ135 mq135(PIN_MQ135);
MQ137 mq137(PIN_MQ137, MQ137_R0, true);
DHT dht(DHT_PIN, DHT_TYPE);

// --- Variables ---
float temperature = 0.0;
float humidity = 0.0;

void setup() {
  Serial.begin(9600);
  mq137.begin();
  dht.begin();
  lcd.begin();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Food Rot Monitor");
  delay(2000);
}

void loop() {
  // Read sensors
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  float mq135_ppm = mq135.getCorrectedPPM(temperature, humidity);
  float mq137_ppm = mq137.getPPM();

  // Prepare scroll text
  String dataLine = "T:" + String(temperature, 0) + "C "
                  + "H:" + String(humidity, 0) + "% "
                  + "135:" + String(mq135_ppm, 0) + " "
                  + "137:" + String(mq137_ppm, 0);

  // Determine rot status
  String rotStatus;
  if (mq137_ppm < 5 && mq135_ppm < 100) {
    rotStatus = "Fresh";
  } else if (mq137_ppm < 15 || mq135_ppm < 200) {
    rotStatus = "Slight Rot";
  } else if (mq137_ppm < 30 || mq135_ppm < 300) {
    rotStatus = "Mod Rot";
  } else {
    rotStatus = "Severe Rot";
  }

  // Serial Output
  Serial.print("Temp: "); Serial.print(temperature);
  Serial.print(" C | Hum: "); Serial.print(humidity);
  Serial.print(" % | MQ135: "); Serial.print(mq135_ppm);
  Serial.print(" | MQ137: "); Serial.print(mq137_ppm);
  Serial.print(" | Status: "); Serial.println(rotStatus);

  // --- LCD Scroll ---
  for (int i = 0; i < dataLine.length() - 15; i++) {
    lcd.setCursor(0, 0);
    lcd.print(dataLine.substring(i, i + 16));
    
    lcd.setCursor(0, 1);
    lcd.print("Status: ");
    lcd.print(rotStatus);
    lcd.print("    ");  // padding to clear longer text
    
    delay(400);  // scroll speed
  }

  delay(2000);  // short pause before next loop
}
