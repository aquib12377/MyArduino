#include <EEPROM.h>
#include "GravityTDS.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library

// TDS Sensor Configuration
#define TdsSensorPin A1
GravityTDS gravityTds;

// Turbidity Sensor Configuration
int turbiditySensorPin = A2;

// LCD Configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address

// Global Variables
float temperature = 25;      // Default temperature for TDS compensation
float tdsValue = 0;          // TDS value in ppm
float voltage = 0;           // Voltage for pH calculation
float pHValue = 0;           // Calculated pH value
float turbidityVolt = 0;     // Voltage for turbidity calculation
float turbidityNTU = 0;      // Turbidity in NTU
int sensorValue = 0;         // Raw sensor reading from analog pins

// pH Sensor Calibration Constants
const float voltageOffset = 0.0;         // Offset for voltage calibration
const float pH7Voltage = 2.5;            // Voltage at pH 7 (neutral pH)
const float slope = -5.0 / (7.0 - 4.0);  // Slope based on pH sensor's datasheet

void setup() {
  Serial.begin(9600);              // Initialize serial communication
  pinMode(A0, INPUT);              // pH Sensor pin
  pinMode(TdsSensorPin, INPUT);    // TDS Sensor pin
  pinMode(turbiditySensorPin, INPUT);  // Turbidity sensor pin
  
  // TDS Sensor Configuration
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);       // Reference voltage on ADC (5.0V on Arduino UNO)
  gravityTds.setAdcRange(1024);  // 1024 for 10-bit ADC
  gravityTds.begin();            // Initialize TDS sensor
  
  // LCD Initialization
  lcd.begin();
  lcd.backlight();
}

void loop() {
  // --- pH Sensor Reading ---
  voltage = 0;
  for (int i = 0; i < 100; i++) {
    sensorValue = analogRead(A0);             // Read raw value from the sensor
    voltage += sensorValue * (5.0 / 1023.0);  // Convert to voltage
  }
  voltage = voltage / 100.0;  // Calculate average voltage
  voltage += voltageOffset;   // Apply voltage offset
  pHValue = 7.0 + (voltage - pH7Voltage) * slope;  // Calculate pH

  // --- TDS Sensor Reading ---
  gravityTds.setTemperature(temperature);  // Set temperature for compensation
  gravityTds.update();                     // Sample and calculate
  tdsValue = gravityTds.getTdsValue();     // Get TDS value in ppm

  // --- Turbidity Sensor Reading ---
  turbidityVolt = 0;
  for (int i = 0; i < 800; i++) {
    turbidityVolt += ((float)analogRead(turbiditySensorPin) / 1023) * 5;
  }
  turbidityVolt = turbidityVolt / 800;  // Average voltage
  turbidityVolt = round_to_dp(turbidityVolt, 2);
  if (turbidityVolt < 2.5) {
    turbidityNTU = 3000;  // High turbidity range
  } else {
    turbidityNTU = -1120.4 * sq(turbidityVolt) + 5742.3 * turbidityVolt - 4353.8;  // Calibration formula
  }

  // --- Display Results on LCD ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("pH: ");
  lcd.print(pHValue, 2);  // Display pH value
  
  lcd.setCursor(0, 1);
  lcd.print("TDS: ");
  lcd.print(tdsValue, 0); // Display TDS in ppm
  lcd.print("ppm");

  delay(2000);  // Pause for 2 seconds

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Turb: ");
  lcd.print(turbidityNTU, 2);  // Display turbidity in NTU
  lcd.print(" NTU");

  delay(2000);  // Pause for 2 seconds

  // --- Print Results to Serial Monitor ---
  Serial.print("pH: ");
  Serial.print(pHValue, 2);
  Serial.print(" | TDS: ");
  Serial.print(tdsValue, 0);
  Serial.print("ppm | Turbidity: ");
  Serial.print(turbidityNTU, 2);
  Serial.println(" NTU");

  delay(1000);  // Wait for 1 second before the next reading
}

// --- Function to Round Floating-Point Numbers ---
float round_to_dp(float in_value, int decimal_place) {
  float multiplier = pow(10.0f, decimal_place);
  in_value = round(in_value * multiplier) / multiplier;
  return in_value;
}
