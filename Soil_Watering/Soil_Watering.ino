/*
  Arduino Soil Moisture Monitoring with Relay Control and I2C LCD Display
  - Reads soil moisture from analog pin A0
  - Displays moisture level and relay status on a 16x2 I2C LCD
  - Activates an active LOW relay on digital pin A1 when soil is dry
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define the analog pin for soil moisture sensor
#define SOIL_MOISTURE_PIN A6

// Define the digital pin for relay control
#define RELAY_PIN 2

// Soil moisture threshold (adjust based on calibration)
const int SOIL_DRY_THRESHOLD = 600;   // Example value; lower means more moisture
const int SOIL_WET_THRESHOLD = 600;   // Optional: For hysteresis

// Initialize the LCD (address 0x27 is common; adjust if necessary)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Relay state
bool relayState = false; // false = OFF, true = ON

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  Serial.println("Soil Moisture Monitor with Relay Control and I2C LCD");

  // Initialize the relay pin as OUTPUT
  pinMode(RELAY_PIN, OUTPUT);

  // Ensure relay is off at start
  digitalWrite(RELAY_PIN, HIGH); // Active LOW relay

  // Initialize the LCD
  lcd.begin();                      // Initialize the LCD
  lcd.backlight();                 // Turn on the backlight

  // Display initial messages
  lcd.setCursor(0, 0);
  lcd.print("Soil Moisture");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000); // Display the message for 2 seconds
}

void loop() {
  // Read the analog value from the soil moisture sensor
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);

  // Convert the analog value to percentage (optional)
  float moisturePercent = map(soilMoistureValue, 800, 0, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100); // Ensure within 0-100%

  // Debugging: Print the value to Serial Monitor
  Serial.print("Soil Moisture Value: ");
  Serial.print(soilMoistureValue);
  Serial.print(" | Moisture: ");
  Serial.print(moisturePercent);
  Serial.println(" %");

  // Display the results on the LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moisture: ");
  lcd.print(moisturePercent);
  lcd.print(" %");

  // Control the relay based on moisture level
  if (soilMoistureValue >= SOIL_DRY_THRESHOLD && !relayState) {
    // Soil is dry, turn relay ON
    digitalWrite(RELAY_PIN, LOW); // Active LOW
    relayState = true;
    Serial.println("Relay ON: Soil is dry. Watering...");
    lcd.setCursor(0, 1);
    lcd.print("Relay: ON ");
  }
  else if (soilMoistureValue <= SOIL_WET_THRESHOLD && relayState) {
    // Soil is wet enough, turn relay OFF
    digitalWrite(RELAY_PIN, HIGH); // Active LOW
    relayState = false;
    Serial.println("Relay OFF: Soil is wet.");
    lcd.setCursor(0, 1);
    lcd.print("Relay: OFF");
  }
  else {
    // No change in relay state
    lcd.setCursor(0, 1);
    if (relayState) {
      lcd.print("Relay: ON ");
    }
    else {
      lcd.print("Relay: OFF");
    }
  }

  // Wait for a while before the next reading
  delay(2000); // 2 seconds
}
