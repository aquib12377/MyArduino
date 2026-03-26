/*
 * BLDC Motor Speed, Current, and Torque Measurement System
 * 
 * Hardware:
 * - Arduino Nano
 * - IR Speed Sensor Module (Active LOW)
 * - ACS712 30A Current Sensor
 * - LCD I2C Display (16x2 or 20x4)
 * - BLDC Motor 1000KV with 30A ESC
 * 
 * Connections:
 * - IR Sensor OUT -> Pin 2 (Interrupt)
 * - ACS712 OUT -> A0
 * - LCD SDA -> A4
 * - LCD SCL -> A5
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin Definitions
#define IR_SENSOR_PIN 2        // IR sensor connected to interrupt pin
#define CURRENT_SENSOR_PIN A0  // ACS712 analog pin

// LCD Configuration (adjust address if needed: 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change to (0x27, 20, 4) for 20x4 LCD

// ACS712 30A Sensor Configuration
#define ACS712_SENSITIVITY 0.066  // 66mV/A for 30A module
#define ACS712_VREF 2.5           // Zero current voltage at 5V supply (Vcc/2)
#define ADC_RESOLUTION 1024.0
#define VCC 5.0

// Motor Configuration
#define MOTOR_KV 1000              // Motor KV rating
#define MOTOR_POLES 14             // Typical BLDC poles (adjust for your motor)
#define POLE_PAIRS (MOTOR_POLES / 2)

// Speed Measurement Variables
volatile unsigned long pulseCount = 0;
unsigned long lastSpeedCalcTime = 0;
unsigned long speedCalcInterval = 1000;  // Calculate speed every 1 second
float currentRPM = 0;

// Current Measurement Variables
float current = 0;
const int numReadings = 50;  // Number of samples for averaging

// Torque Calculation Variables
float torque = 0;  // in N⋅m
float power = 0;   // in Watts

void setup() {
  Serial.begin(9600);
  
  // Initialize IR sensor pin
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_PIN), pulseISR, FALLING);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BLDC Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();
  
  Serial.println("BLDC Speed, Current & Torque Monitor");
  Serial.println("=====================================");
}

void loop() {
  // Calculate speed every interval
  if (millis() - lastSpeedCalcTime >= speedCalcInterval) {
    calculateSpeed();
    lastSpeedCalcTime = millis();
  }
  
  // Measure current
  current = measureCurrent();
  
  // Calculate power and torque
  calculatePowerAndTorque();
  
  // Display on LCD
  displayOnLCD();
  
  // Display on Serial Monitor for debugging
  displayOnSerial();
  
  delay(100);  // Update display every 100ms
}

// Interrupt Service Routine for IR sensor
void pulseISR() {
  pulseCount++;
}

// Calculate RPM from pulse count
void calculateSpeed() {
  noInterrupts();
  unsigned long pulses = pulseCount;
  pulseCount = 0;
  interrupts();
  
  // Calculate RPM
  // RPM = (pulses per second * 60) / (pole pairs)
  // For electrical RPM: pulses directly represent electrical cycles
  // For mechanical RPM: divide by pole pairs
  
  float pulsesPerSecond = (float)pulses / (speedCalcInterval / 1000.0);
  
  // Mechanical RPM calculation
  currentRPM = (pulsesPerSecond * 60.0) / POLE_PAIRS;
  
  // If using single magnet/reflector on shaft, uncomment below:
  // currentRPM = pulsesPerSecond * 60.0;
}

// Measure current using ACS712
float measureCurrent() {
  long sum = 0;
  
  // Take multiple readings for better accuracy
  for (int i = 0; i < numReadings; i++) {
    sum += analogRead(CURRENT_SENSOR_PIN);
    delayMicroseconds(100);
  }
  
  float avgReading = (float)sum / numReadings;
  
  // Convert ADC reading to voltage
  float voltage = (avgReading * VCC) / ADC_RESOLUTION;
  
  // Calculate current
  // Current = (Measured Voltage - Zero Current Voltage) / Sensitivity
  float calculatedCurrent = (voltage - ACS712_VREF) / ACS712_SENSITIVITY;
  
  // Return absolute value (for DC current measurement)
  return abs(calculatedCurrent);
}

// Calculate electrical power and mechanical torque
void calculatePowerAndTorque() {
  // Assuming battery voltage (adjust based on your setup)
  // For accurate measurement, add a voltage sensor
  float batteryVoltage = 11.1;  // 3S LiPo nominal voltage
  
  // Electrical Power (Watts) = Voltage × Current
  power = batteryVoltage * current;
  
  // Theoretical Mechanical Torque calculation
  // Torque (N⋅m) = (Power in Watts) / (Angular velocity in rad/s)
  // Angular velocity (rad/s) = (RPM × 2π) / 60
  
  if (currentRPM > 0) {
    float angularVelocity = (currentRPM * 2.0 * PI) / 60.0;
    
    // Assuming 75% efficiency for typical BLDC motor
    float efficiency = 0.75;
    float mechanicalPower = power * efficiency;
    
    torque = mechanicalPower / angularVelocity;
    
    // Convert to mN⋅m for better readability
    torque = torque * 1000.0;  // in mN⋅m
  } else {
    torque = 0;
  }
}

// Display data on LCD
void displayOnLCD() {
  lcd.clear();
  
  // Line 1: RPM and Current
  lcd.setCursor(0, 0);
  lcd.print("RPM:");
  lcd.print((int)currentRPM);
  lcd.print(" ");
  
  lcd.setCursor(10, 0);
  lcd.print("A:");
  lcd.print(current, 1);
  
  // Line 2: Power and Torque
  lcd.setCursor(0, 1);
  lcd.print("P:");
  lcd.print((int)power);
  lcd.print("W");
  
  lcd.setCursor(9, 1);
  lcd.print("T:");
  lcd.print((int)torque);
  lcd.print("mNm");
  
  // For 20x4 LCD, you can add more info:
  /*
  lcd.setCursor(0, 2);
  lcd.print("Efficiency: 75%");
  lcd.setCursor(0, 3);
  lcd.print("KV: 1000");
  */
}

// Display data on Serial Monitor
void displayOnSerial() {
  static unsigned long lastSerialPrint = 0;
  
  if (millis() - lastSerialPrint >= 500) {
    Serial.print("RPM: ");
    Serial.print(currentRPM, 0);
    Serial.print(" | Current: ");
    Serial.print(current, 2);
    Serial.print(" A | Power: ");
    Serial.print(power, 1);
    Serial.print(" W | Torque: ");
    Serial.print(torque, 1);
    Serial.println(" mN⋅m");
    
    lastSerialPrint = millis();
  }
}
