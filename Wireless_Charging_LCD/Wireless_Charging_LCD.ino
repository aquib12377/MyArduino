#include <LiquidCrystal_I2C.h>

// Define analog input pin for voltage sensor
#define ANALOG_IN_PIN A0

// Resistor values in voltage divider (in ohms)
float R1 = 30000.0;
float R2 = 7500.0;

// Reference Voltage for ADC
float ref_voltage = 5.0;

// Variables for ADC voltage and calculated input voltage
float adc_voltage = 0.0;
float in_voltage = 0.0;

// Variable for ADC value
int adc_value = 0;

// Set the threshold voltage for detecting charging (adjust as required)
float chargingThreshold = 3.0;

// Initialize the LCD with pin configuration: RS, E, D4, D5, D6, D7
LiquidCrystal_I2C lcd(0x27,16,2);

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
  
  // Initialize the LCD (16 columns and 2 rows)
  lcd.begin();
  lcd.backlight();
  // Display a startup message
  lcd.print("Wireless   ");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Read the analog value from the voltage sensor
  adc_value = analogRead(ANALOG_IN_PIN);
  
  // Convert the ADC value to the voltage at the ADC pin
  adc_voltage = (adc_value * ref_voltage) / 1024.0;
  
  // Calculate the actual input voltage using the voltage divider formula
  in_voltage = adc_voltage * ((R1 + R2) / R2);
  
  // Print the voltage value to the Serial Monitor for debugging
  Serial.print("Input Voltage = ");
  Serial.println(in_voltage, 2);
  
  // Clear the LCD for new information
  lcd.clear();
  
  // Check if the measured voltage is above the charging threshold
  if (in_voltage >= chargingThreshold) {
    lcd.setCursor(0, 0);
    lcd.print("Charging...");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Not Charging");
  }
  
  // Display the measured voltage on the second row of the LCD
  lcd.setCursor(0, 1);
  lcd.print("Volt: ");
  lcd.print(in_voltage, 2);
  
  // Short delay before the next reading
  delay(500);
}
