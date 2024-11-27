/*
 * Battery Monitoring System with Temperature Monitoring
 * Features:
 * - Voltage Measurement (Up to 25V DC)
 * - Current Measurement (ACS712 30A)
 * - Temperature Monitoring (DHT11)
 * - Relay Control to Cut-Off Load
 * - Data Display on I2C LCD
 */

// =====================
// Include Libraries
// =====================
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ACS712.h>
#include <DHT.h>

// =====================
// Pin Definitions
// =====================
const int VOLTAGE_PIN = PA0;    // Analog Pin A0 for Voltage Sensor
const int CURRENT_PIN = PA1;    // Analog Pin A1 for Current Sensor
const int RELAY_PIN = PA7;       // Digital Pin 3 for Relay Control
const int DHT_PIN = PA6;         // Digital Pin 4 for DHT11 Data

// =====================
// Constants
// =====================

// Voltage Divider Constants
const float R1 = 30000.0;      // 100kΩ
const float R2 = 7500.0;       // 12kΩ
const float VOLTAGE_SCALE = (R1 + R2) / R2; // ≈9.333

// ADC Reference Voltage
const float VREF = 3.3;
const int ADC_RESOLUTION = 4095;

// Thresholds
const float MAX_VOLTAGE = 25.0; // 25V
const float MAX_CURRENT = 30.0; // 30A
const float MAX_TEMPERATURE = 35.0; // 60°C (example threshold)

// =====================
// LCD Configuration
// =====================
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change address if needed

// =====================
// ACS712 Configuration
// =====================
//ACS712 sensor(ACS712_30A, 5.0, CURRENT_PIN); // ACS712-30A, 5V Vcc, connected to A1
ACS712  sensor(CURRENT_PIN, 3.3, 4095, 66);

// =====================
// DHT11 Configuration
// =====================
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// =====================
// Function Prototypes
// =====================
float readVoltage();
float readCurrent();
float readTemperature();
void controlRelay(bool state);
void displayData(float voltage, float current, float temperature);
void checkThresholds(float voltage, float current, float temperature);

// =====================
// Setup Function
// =====================
void setup() {
  // Initialize Serial Monitor (optional for debugging)
  Serial.begin(115200);
  
  // Initialize Relay Pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Assume HIGH = Relay ON
  delay(1000);
  digitalWrite(RELAY_PIN, HIGH);
  delay(1000);
  digitalWrite(RELAY_PIN, LOW); // Assume HIGH = Relay ON
  delay(1000);
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Battery Monitor");
  lcd.setCursor(0,1);
  lcd.print("Initializing...");
  delay(2000);

  // Initialize ACS712
  sensor.autoMidPoint(); // Calibrate the ACS712 sensor (ensure no current flows during calibration)

  // Initialize DHT11
  dht.begin();
}

// =====================
// Main Loop
// =====================
void loop() {
  // Read Voltage, Current, and Temperature
  float voltage = readVoltage();
  float current = readCurrent();
  float temperature = readTemperature();
  
  // Display Data on LCD
  displayData(voltage, current, temperature);
  
  // Check Thresholds and Control Relay
  checkThresholds(voltage, current, temperature);
  
  // Optional: Print to Serial Monitor
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.print(" V, Current: ");
  Serial.print(current);
  Serial.print(" A, Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");
  if(temperature > 35)
  {
    digitalWrite(RELAY_PIN, HIGH);
  }
  else{
    digitalWrite(RELAY_PIN, LOW);
  }
  delay(1000); // Update every 1 second
}

// =====================
// Function Definitions
// =====================

// Function to Read Voltage
float readVoltage() {
  int sensorValue = 0;
  // Take multiple readings for stability
  for(int i = 0; i < 10; i++) {
    sensorValue += analogRead(VOLTAGE_PIN);
    delay(10);
  }
  sensorValue /= 10;
  float voltage = (sensorValue / (float)ADC_RESOLUTION) * VREF; // Convert ADC value to voltage
  voltage *= VOLTAGE_SCALE; // Apply voltage divider scale
  return voltage;
}

// Function to Read Current
float readCurrent() {
  return sensor.mA_DC(); // Read DC current using ACS712 library
}

// Function to Read Temperature
float readTemperature() {
  float temp = dht.readTemperature(); // Read temperature in Celsius
  if (isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
    return -1.0; // Return an invalid temperature
  }
  return temp;
}

// Function to Control Relay
// state = true -> Relay ON
// state = false -> Relay OFF
void controlRelay(bool state) {
  if(state) {
    //digitalWrite(RELAY_PIN, LOW); // Turn Relay ON
  }
  else {
    //digitalWrite(RELAY_PIN, HIGH);  // Turn Relay OFF
  }
}

// Function to Display Data on LCD
void displayData(float voltage, float current, float temperature) {
  lcd.clear();
  
  // First Line: Voltage and Current
  lcd.setCursor(0,0);
  lcd.print("V:");
  lcd.print(voltage, 1);
  lcd.print("V I:");
  lcd.print(current, 1);
  lcd.print("A");
  
  // Second Line: Temperature
  lcd.setCursor(0,1);
  if(temperature != -1.0) { // Valid temperature
    lcd.print("Temp:");
    lcd.print(temperature, 1);
    lcd.print((char)223); // Degree symbol
    lcd.print("C");
  }
  else { // Invalid reading
    lcd.print("Temp: Err");
  }
}

// Function to Check Thresholds and Control Relay
void checkThresholds(float voltage, float current, float temperature) {
  bool alert = false;
  String alertMessage = "";

  if(voltage > MAX_VOLTAGE) {
    alert = true;
    alertMessage += "V High! ";
  }

  if(current > MAX_CURRENT) {
    alert = true;
    alertMessage += "I High! ";
  }

  if(temperature > MAX_TEMPERATURE) {
    digitalWrite(RELAY_PIN, HIGH);
    alert = true;
    alertMessage += "Temp High!";
  }

  if(alert) {
    //controlRelay(false); // Cut-Off Load
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ALERT!            ");
    lcd.setCursor(0,1);
    lcd.print(alertMessage);
  }
  else {
    //controlRelay(true); // Ensure Relay is ON
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Battery Monitor  ");
    lcd.setCursor(0,1);
    lcd.print("All OK           ");
  }
}
