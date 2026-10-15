#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD I2C address: usually 0x27 or 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Define analog input
#define ANALOG_IN_PIN A1

// Relay pin
#define RELAY_PIN A2

// Floats for ADC voltage & Input voltage
float adc_voltage = 0.0;
float in_voltage = 0.0;

// Floats for resistor values in divider (in ohms)
float R1 = 30000.0;
float R2 = 7500.0;

// Float for Reference Voltage
float ref_voltage = 5.0;

// Integer for ADC value
int adc_value = 0;

void setup()
{
  pinMode(RELAY_PIN, OUTPUT);

  // Relay ON
  // If your relay is active LOW, change HIGH to LOW
  digitalWrite(RELAY_PIN, LOW);

  Serial.begin(9600);
  Serial.println("DC Voltage Test");

  lcd.begin();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Battery Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  lcd.clear();
}

void loop()
{
  // Read the Analog Input
  adc_value = analogRead(ANALOG_IN_PIN);

  // Determine voltage at ADC input
  adc_voltage = (adc_value * ref_voltage) / 1024.0;

  // Calculate voltage at divider input
  in_voltage = adc_voltage / (R2 / (R1 + R2));

  // Print results to Serial Monitor
  Serial.print("Input Voltage = ");
  Serial.println(in_voltage, 2);

  // Show voltage on LCD
  lcd.setCursor(0, 0);
  lcd.print("Voltage: ");
  lcd.print(in_voltage, 2);
  lcd.print("V   ");

  // Show relay status
  lcd.setCursor(0, 1);
  lcd.print("Relay: ON       ");

  delay(500);
}