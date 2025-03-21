#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "BluetoothSerial.h"

// Bluetooth Setup
BluetoothSerial SerialBT;

// Define I2C LCD (Address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Define Relay Pins
const int relayPins[4] = {13, 12, 14, 27};
bool relayStates[4] = {true, true, true, true};  // true = OFF, false = ON

// DHT11 Sensor Setup
#define DHTPIN 4     // DHT11 connected to GPIO4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Home Automation"); // Set Bluetooth device name
  Serial.println("Bluetooth Ready. Waiting for commands...");

  // Initialize I2C LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 Relay Ctrl");

  // Initialize DHT11 Sensor
  dht.begin();

  // Initialize Relay Pins
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Relays are ACTIVE LOW, so start HIGH (OFF)
  }
}

void loop() {
  // Read DHT11 Temperature & Humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Update LCD with Temperature & Humidity
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperature);
  lcd.print("C H:");
  lcd.print(humidity);
  lcd.print("%  ");

  // Check for Bluetooth Commands
  if (SerialBT.available()) {
    char command = SerialBT.read(); // Read received character
    Serial.print("Received: ");
    Serial.println(command);

    if (command >= '1' && command <= '4') { 
      int relayIndex = command - '1';  // Convert char to index (e.g., '1' -> 0)

      relayStates[relayIndex] = !relayStates[relayIndex]; // Toggle state
      digitalWrite(relayPins[relayIndex], relayStates[relayIndex] ? HIGH : LOW); // Set relay state

      Serial.print("Relay ");
      Serial.print(command);
      Serial.println(relayStates[relayIndex] ? " OFF" : " ON");
      SerialBT.print("Relay ");
      SerialBT.print(command);
      SerialBT.println(relayStates[relayIndex] ? " OFF" : " ON");

      // Update LCD with Relay Status
      lcd.setCursor(0, relayIndex);  // Line 0-3 for relays
      lcd.print("Relay ");
      lcd.print(command);
      lcd.print(": ");
      lcd.print(relayStates[relayIndex] ? "OFF " : "ON  ");
    }
  }

  delay(1000); // Update every second
}
