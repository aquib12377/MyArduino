#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ====== Pin Definitions ======
#define PH_PIN A0
#define SOIL_PIN A1
#define MQ135_PIN A2
#define DHT_PIN 2

#define RELAY_PUMP 7    // Active LOW
#define RELAY_FAN 8     // Active LOW

#define DHTTYPE DHT11

// ====== Objects ======
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHTTYPE);

// ====== Thresholds ======
int soilThreshold = 600;      // Adjust after calibration
float tempThreshold = 30.0;   // Fan ON above 30°C

void setup()
{
  Serial.begin(9600);

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);

  digitalWrite(RELAY_PUMP, HIGH);  // OFF (Active LOW)
  digitalWrite(RELAY_FAN, HIGH);   // OFF (Active LOW)

  lcd.begin();
  lcd.backlight();

  dht.begin();

  lcd.setCursor(0,0);
  lcd.print("GreenHouse Init");
  delay(2000);
  lcd.clear();
}

void loop()
{
  // ===== Read Sensors =====
  int phRaw = analogRead(PH_PIN);
  int soilValue = analogRead(SOIL_PIN);
  int mq135Value = analogRead(MQ135_PIN);

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // ===== Convert pH (Simple Approximation) =====
  float voltage = phRaw * (5.0 / 1023.0);
  float phValue = 3.5 * voltage;  // Adjust after calibration

  // ===== Control Pump =====
  if (soilValue > soilThreshold)  // Dry soil
  {
    digitalWrite(RELAY_PUMP, LOW);   // ON
  }
  else
  {
    digitalWrite(RELAY_PUMP, HIGH);  // OFF
  }

  // ===== Control Fan =====
  if (temperature > tempThreshold)
  {
    digitalWrite(RELAY_FAN, LOW);   // ON
  }
  else
  {
    digitalWrite(RELAY_FAN, HIGH);  // OFF
  }

  // ===== Display Screen 1 =====
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature);
  lcd.print("C H:");
  lcd.print(humidity);
  lcd.print("% ");

  lcd.setCursor(0, 1);
  lcd.print("Soil:");
  lcd.print(soilValue);
  lcd.print("    ");

  delay(3000);
  lcd.clear();

  // ===== Display Screen 2 =====
  lcd.setCursor(0, 0);
  lcd.print("pH:");
  lcd.print(phValue);
  lcd.print("  ");

  lcd.setCursor(0, 1);
  lcd.print("Air:");
  lcd.print(mq135Value);
  lcd.print("    ");

  delay(3000);
  lcd.clear();
}