#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Pin Configuration ---
#define DHTPIN 2          // DHT11 data pin connected to digital pin 2
#define DHTTYPE DHT11     // Define sensor type
#define RELAY_PIN 7       // Relay control pin

// --- Objects ---
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16x2 LCD

// --- Variables ---
float temperature;
float humidity;
const float TEMP_THRESHOLD = 15.0;  // Temperature in °C to trigger relay

void setup() {
  Serial.begin(9600);
  Serial.println("DHT11 Temperature & Relay Control with LCD Initialized...");

  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Start with relay OFF

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(2000); // Allow sensor to stabilize
  lcd.clear();
}

void loop() {
  // Read sensor values
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    delay(2000);
    return;
  }

  // Print readings to Serial Monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C  |  Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Control relay based on temperature threshold
  if (temperature <= TEMP_THRESHOLD) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Relay ON - Temperature below threshold!");
  } else {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Relay OFF - Temperature above threshold.");
  }

  // --- Update LCD Display ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print((char)223); // Degree symbol
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  if (digitalRead(RELAY_PIN) == LOW) {
    lcd.print("Peltier: ON ");
  } else {
    lcd.print("Peltier: OFF");
  }

  Serial.println("-------------------------------------");
  delay(2000); // Delay between readings
}
