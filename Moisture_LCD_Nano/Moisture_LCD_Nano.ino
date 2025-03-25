#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Adjust this address to match your I2C LCD
#define I2C_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS    2

LiquidCrystal_I2C lcd(I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// Soil Moisture Input Pin
const int moisturePin = A0;

// Calibration values (adjust based on your sensor)
const int AIR_VALUE = 1023; // Soil sensor reading in air (dry)
const int WATER_VALUE = 200; // Soil sensor reading in water (fully wet)

void setup() {
  // Initialize the LCD
  lcd.begin();
  lcd.backlight();

  // Initialize the serial (optional for debugging)
  Serial.begin(9600);

  // Print a welcome message on LCD
  lcd.setCursor(0, 0);
  lcd.print("Soil Moisture");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Read the raw value from the soil moisture sensor
  int sensorValue = analogRead(moisturePin);

  // Map the sensor reading to percentage
  // For example, 0% = AIR_VALUE, 100% = WATER_VALUE
  // If your sensor is reversed, swap AIR_VALUE and WATER_VALUE
  int moisturePercent = map(sensorValue, AIR_VALUE, WATER_VALUE, 0, 100);

  // Constrain the values to avoid going below 0% or above 100%
  if (moisturePercent < 0) moisturePercent = 0;
  if (moisturePercent > 100) moisturePercent = 100;

  // Print to Serial (optional)
  Serial.print("Soil Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moisture Level:");
  lcd.setCursor(0, 1);
  lcd.print(moisturePercent);
  lcd.print(" %");

  delay(1000); // update every 1 second
}
