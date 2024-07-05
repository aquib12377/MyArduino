#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Initialize the LCD with the I2C address 0x27. Adjust if your module has a different address.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Sensor pins
const int mq2Pin = A0;
const int mq6Pin = A1;
const int flameSensorPin = 2;
const int soilMoisturePin = A2;
const int pirSensorPin = 3;

void setup() {
  // Initialize serial communication for debugging purposes
  Serial.begin(9600);

  // Initialize the LCD
  lcd.begin();
  lcd.backlight();

  // Set the flame sensor and PIR sensor pins as input
  pinMode(flameSensorPin, INPUT);
  pinMode(pirSensorPin, INPUT);

  // Display initial message
  lcd.setCursor(0, 0);
  lcd.print("Sensor Reading");
  delay(2000);
  lcd.clear();
}

void loop() {
  lcd.clear();
  // Read sensor values
  int mq2Value = analogRead(mq2Pin);
  int mq6Value = analogRead(mq6Pin);
  int flameValue = digitalRead(flameSensorPin);
  int soilMoistureValue = analogRead(soilMoisturePin);
  int pirValue = digitalRead(pirSensorPin);

  // Print sensor values to the serial monitor
  Serial.print("MQ2: ");
  Serial.print(mq2Value);
  Serial.print("\tMQ6: ");
  Serial.print(mq6Value);
  Serial.print("\tFlame: ");
  Serial.print(flameValue);
  Serial.print("\tSoil Moisture: ");
  Serial.print(soilMoistureValue);
  Serial.print("\tPIR: ");
  Serial.println(pirValue);

  // Display sensor values on the LCD
  lcd.setCursor(0, 0);
  lcd.print("MQ2:");
  lcd.print(mq2Value);
  lcd.setCursor(8, 0);
  lcd.print("MQ6:");
  lcd.print(mq6Value);

  lcd.setCursor(0, 1);
  lcd.print("F:");
  lcd.print(!flameValue ? "YES" : "NO");
  lcd.setCursor(6, 1);
  lcd.print("Soil:");
  lcd.print(map(soilMoistureValue,0,1024,100,0));

  // Update PIR status
  lcd.setCursor(14, 1);
  lcd.print(pirValue ? "M" : "S");

  // Delay for readability
  delay(1000);
}
