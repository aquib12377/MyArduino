#include <Wire.h>                      // Built-in Arduino I2C library
#include <LiquidCrystal_I2C.h>         // Library for I2C LCD

// Change this to match your LCD’s I2C address and dimensions
LiquidCrystal_I2C lcd(0x27, 16, 2);    

// ADC and reference parameters
const float VREF    = 5.0;   // Assuming a 5 V reference on the Arduino
const int   ADC_MAX = 1023;  // 10-bit ADC → values 0..1023

void setup() {
  // Initialize serial (optional for debug)
  Serial.begin(9600);

  // Initialize the LCD
  Wire.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  
  // Print a startup message
  lcd.setCursor(0, 0);
  lcd.print("Wheatstone Demo");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();
}

void loop() {
  // 1) Read Node A on A0
  int rawA = analogRead(A0);
  // 2) Read Node B on A1
  int rawB = analogRead(A1);

  // 3) Convert to voltage
  float voltageA = (float)rawA * (VREF / ADC_MAX);
  float voltageB = (float)rawB * (VREF / ADC_MAX);

  // 4) Compute difference
  float diff = voltageA - voltageB;

  // 5) Print to Serial (optional)
  Serial.print("VA=");
  Serial.print(voltageA, 3);
  Serial.print(" V, VB=");
  Serial.print(voltageB, 3);
  Serial.print(" V, Diff=");
  Serial.print(diff, 3);
  Serial.println(" V");

  // 6) Display on LCD
  lcd.clear();
  
  // First row: Show Node A and Node B
  // Format: "A=1.23 B=1.20"
  lcd.setCursor(0, 0);
  lcd.print("A=");
  lcd.print(voltageA, 2);
  lcd.print(" B=");
  lcd.print(voltageB, 2);

  // Second row: Show difference
  // Format: "Diff=0.03 V"
  lcd.setCursor(0, 1);
  lcd.print("Diff=");
  lcd.print(diff, 3);
  lcd.print("V");

  // Small delay between readings
  delay(1000);
}
