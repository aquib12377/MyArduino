#include <LiquidCrystal_I2C.h>
#include "MAX30100_PulseOximeter.h"
#include <DHT.h>

// 1) Adjust these to match your wiring and preferred sensor:
#define DHTPIN 2              // DHT data pin connected to digital pin 4
#define DHTTYPE DHT11         // or DHT22, etc.
DHT dht(DHTPIN, DHTTYPE);

// 2) LCD and MAX30100 Setup
LiquidCrystal_I2C lcd (0x27, 16, 2);  // I2C LCD at address 0x27, 16 columns x 2 rows

#define REPORTING_PERIOD_MS 1000
PulseOximeter pox;
uint32_t tsLastReport = 0;

// Callback when a pulse is detected
void onBeatDetected() {
  Serial.println("Beat!");
}

void setup() {
  Serial.begin(115200);

  // -- Initialize LCD --
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  // -- Initialize DHT sensor --
  dht.begin();

  // -- Initialize the PulseOximeter (MAX30100) --
  Serial.print("Initializing pulse oximeter..");
  if (!pox.begin()) {
    Serial.println("FAILED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MAX30100 Err!");
    while (true) { /* Halt */ }
  } else {
    Serial.println("SUCCESS");
  }

  // Set IR LED current (optional; default is 50mA)
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

  // Register a callback for the beat detection
  pox.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
  // Continuously update the MAX30100 readings
  pox.update();

  // Report every 1 second
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    tsLastReport = millis();

    // 1) Read the DHT sensor
    float temperature = dht.readTemperature();  // °C
    float humidity    = dht.readHumidity();     // %
    
    // 2) Get the heart rate / SpO2 from MAX30100
    float heartRate   = pox.getHeartRate();     // bpm
    float spo2        = pox.getSpO2();          // %

    // Print to Serial (optional)
    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print("C, Hum: "); Serial.print(humidity);
    Serial.print("% | HR: "); Serial.print(heartRate);
    Serial.print(" bpm, SpO2: "); Serial.print(spo2);
    Serial.println("%");

    // 3) Update the LCD
    // Clear each time so old data doesn't remain
    lcd.clear();

    // Line 1 (Example layout): BPM and Temperature
    lcd.setCursor(0, 0);
    lcd.print("BPM:");
    lcd.print((int)heartRate);    // As integer
    lcd.print(" T:");
    lcd.print(temperature, 1);    // One decimal place
    lcd.print("C");

    // Line 2 (Example layout): SpO2 and Humidity
    lcd.setCursor(0, 1);
    lcd.print("SpO2:");
    lcd.print((int)spo2);         // As integer
    lcd.print("% H:");
    lcd.print((int)humidity);     // As integer
    lcd.print("%");
  }
}
