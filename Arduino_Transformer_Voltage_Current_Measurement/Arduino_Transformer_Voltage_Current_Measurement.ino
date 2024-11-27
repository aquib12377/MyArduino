#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ZMPT101B.h>
#include "ACS712.h"

// Constants
#define SENSITIVITY 648.5f
#define VOLTAGE_FREQUENCY 50.0

// Pin Definitions
#define VOLTAGE_SENSOR_PIN_1 A0
#define VOLTAGE_SENSOR_PIN_2 A1
#define CURRENT_SENSOR_PIN A2

#define PWM_OUTPUT_PIN_1 5  // PWM output for Voltage 1
#define PWM_OUTPUT_PIN_2 6  // PWM output for Voltage 2
#define PWM_OUTPUT_PIN_3 9  // PWM output for Current

// LCD I2C address and dimensions
#define LCD_ADDRESS 0x27  // Adjust if necessary
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// Objects
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
ZMPT101B voltageSensor1(VOLTAGE_SENSOR_PIN_1, VOLTAGE_FREQUENCY);
ZMPT101B voltageSensor2(VOLTAGE_SENSOR_PIN_2, VOLTAGE_FREQUENCY);
ACS712 ACS(CURRENT_SENSOR_PIN, 5.0, 1023, 100);

// Variables
float voltage1 = 0;
float voltage2 = 0;
float current = 0;

void setup() {
  Serial.begin(115200);

  // Initialize the LCD
  lcd.begin();
  lcd.backlight();

  // Initialize sensors
  voltageSensor1.setSensitivity(SENSITIVITY);
  voltageSensor2.setSensitivity(SENSITIVITY);
  ACS.setADC(signal, 5.0, 1024);
  ACS.autoMidPoint();

  Serial.print("MidPoint: ");
  Serial.print(ACS.getMidPoint());
  Serial.print(". Noise mV: ");
  Serial.println(ACS.getNoisemV());

  // Set up PWM output pins
  pinMode(PWM_OUTPUT_PIN_1, OUTPUT);
  pinMode(PWM_OUTPUT_PIN_2, OUTPUT);
  pinMode(PWM_OUTPUT_PIN_3, OUTPUT);
}

void loop() {
  // Read voltage sensors
  voltage1 = voltageSensor1.getRmsVoltage();
  voltage2 = voltageSensor2.getRmsVoltage();

  // Read current sensor
  int adc = analogRead(CURRENT_SENSOR_PIN);
  float voltage = adc * (5.0 / 1023.0);
  current = (voltage - 2.5) / 0.185;  // Adjust the formula based on your sensor
  current = constrain(current, 0, 20.0);  // Assuming maximum current is 20A

  // Print values to Serial Monitor
  Serial.print("Voltage 1: ");
  Serial.println(voltage1);
  Serial.print("Voltage 2: ");
  Serial.println(voltage2);
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" A");

  // Display values on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V1:");
  lcd.print(voltage1, 2);
  

  lcd.setCursor(0, 1);
  lcd.print("V2:");
  lcd.print(voltage2, 2);
  
  lcd.print(" I:");
  lcd.print(current, 2);
  lcd.print("A");

  // Output measurements via PWM
  outputPWM(PWM_OUTPUT_PIN_1, voltage1, 0.0, 250.0);  // Adjust max voltage if necessary
  outputPWM(PWM_OUTPUT_PIN_2, voltage2, 0.0, 250.0);
  outputPWM(PWM_OUTPUT_PIN_3, current, 0.0, 20.0);    // Adjust max current if necessary

  delay(100);
}

// Function to output measurement as PWM
void outputPWM(int pin, float value, float minValue, float maxValue) {
  // Map the measurement value to PWM duty cycle (0-255)
  int pwmValue = mapFloat(value, minValue, maxValue, 0, 255);
  pwmValue = constrain(pwmValue, 0, 255);
  analogWrite(pin, pwmValue);
}

// Helper function to map float values
int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  return (int)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

// Simulated signal function (as per original code)
uint16_t signal(uint8_t p) {
  return 512 + 400 * sin((micros() % 1000000) * (TWO_PI * 50 / 1e6));
}
