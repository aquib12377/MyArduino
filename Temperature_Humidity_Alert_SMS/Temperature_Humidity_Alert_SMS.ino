/*
  Arduino DHT11 Temperature and Humidity Monitoring with Relay Control and I2C LCD Display
  - Reads temperature and humidity from a DHT11 sensor
  - Displays readings on a 16x2 I2C LCD
  - Triggers a relay when temperature >= 30°C
  - Turns off the relay when temperature < 30°C
*/

// Include the necessary libraries
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
SoftwareSerial gsmSerial(4, 5);  // RX, TX for GSM

// Define the pin where the DHT11 is connected
#define DHTPIN 2          // Digital pin 2

// Define the type of DHT sensor
#define DHTTYPE DHT11     // DHT 11

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);
const String phoneNumber = "+919579890466";  // Replace ZZ with country code and xxxxxxxxxxx with phone number

// Define the relay pin
#define RELAY_PIN 8       // Digital pin 8

// Temperature threshold
const float TEMP_THRESHOLD = 30.0; // 30°C

// Initialize the LCD, set the LCD address to 0x27 for a 16 chars and 2 line display
// The address may vary. Common addresses are 0x27 or 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  Serial.println("DHT11 Temperature & Humidity with Relay Control and I2C LCD");
    gsmSerial.begin(9600);
  // Initialize the DHT sensor
  dht.begin();

  // Initialize the relay pin as OUTPUT
  pinMode(RELAY_PIN, OUTPUT);

  // Ensure relay is off at start
  digitalWrite(RELAY_PIN, HIGH); // Assuming relay is active LOW

  gsmSerial.listen();
  gsmSerial.println("AT");
  updateSerial();
  gsmSerial.println("AT+CMGF=1");  // Set SMS text mode
  updateSerial();
  sendSMS("Project Started...");

  // Initialize the LCD
  lcd.begin();                      // Initialize the LCD
  lcd.backlight();                 // Turn on the backlight

  // Display initial messages
  lcd.setCursor(0, 0);
  lcd.print("Temp & Humidity");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000); // Display the message for 2 seconds
}

void loop() {
  // Wait a few seconds between measurements
  delay(2000); // 2 seconds

  // Read humidity
  float humidity = dht.readHumidity();

  // Read temperature as Celsius
  float temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again)
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error");
    lcd.setCursor(0, 1);
    lcd.print("Check Wiring");
    return;
  }

  // Print the results to the Serial Monitor
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C");

  // Display the results on the LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print(" %");

  // Control the relay based on temperature
  if (temperature >= TEMP_THRESHOLD) {
    // Turn relay ON
    digitalWrite(RELAY_PIN, LOW); // Assuming relay is active LOW
    Serial.println("Relay ON: Temperature threshold reached.");
    sendSMS("Temperature: "+String(temperature)+" C.\nHumidity: "+String(humidity)+" %.");
    delay(500);
  }
  else {
    // Turn relay OFF
    digitalWrite(RELAY_PIN, HIGH); // Assuming relay is active LOW
    Serial.println("Relay OFF: Temperature below threshold.");
    
  }
}

void sendSMS(String message) {
  gsmSerial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  updateSerial();
  gsmSerial.println("AT+CMGS=\"" + phoneNumber + "\"");  // Send SMS to phone number
  delay(1000);
  updateSerial();
  gsmSerial.print(message);  // Message content
  delay(100);
  updateSerial();
  gsmSerial.write(26);  // Send Ctrl+Z to indicate the end of the message
  delay(1000);
  updateSerial();
}
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    gsmSerial.write(Serial.read());  // Forward what Serial received to SIM800L
  }
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());  // Forward what SIM800L received to Serial
  }
}