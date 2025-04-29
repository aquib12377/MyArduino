#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MLX90614.h>
#include <SoftwareSerial.h>

#define REPORTING_PERIOD_MS 1000

// Create a PulseOximeter object
PulseOximeter pox;

// Create LCD object (address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Create MLX90614 object
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Define analog pin for MIC sensor
const int micPin = A0;

// Create SoftwareSerial object for HC-05 (HC-05 TX -> Arduino RX on pin 10, HC-05 RX -> Arduino TX on pin 11)
SoftwareSerial BTSerial(10, 11); // RX, TX

// Time at which the last report was made
uint32_t tsLastReport = 0;

// Callback routine is executed when a pulse is detected
void onBeatDetected() {
    Serial.println("Beat!");
}

void setup() {
    Serial.begin(9600);
    // Initialize Bluetooth serial at 9600 baud (HC-05 default)
    BTSerial.begin(9600);

    Wire.begin();

    // Initialize LCD
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    
    Serial.print("Initializing pulse oximeter..");

    // Initialize pulse oximeter sensor
    if (!pox.begin()) {
        Serial.println("FAILED");
        // Optionally, add code to handle sensor failure
    } else {
        Serial.println("SUCCESS");
    }
    
    // Configure pulse oximeter sensor to use 7.6mA for LED drive
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);

    // Initialize MLX90614 sensor
    if (!mlx.begin()) {
        Serial.println("Error connecting to MLX90614 sensor. Check wiring!");
    } else {
        Serial.println("MLX90614 sensor initialized.");
    }
}

void loop() {
    // Update pulse oximeter sensor
    pox.update();

    // Read MLX90614 temperatures
    double ambientTemp = mlx.readAmbientTempC();
    double objectTemp = mlx.readObjectTempC();

    // Read MIC sensor value (analog reading)
    int micValue = analogRead(micPin);

    // Report sensor readings every REPORTING_PERIOD_MS
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        // Prepare formatted data string
        String dataString = "";
        dataString += "HR:" + String(pox.getHeartRate()) + "bpm, ";
        dataString += "SpO2:" + String(pox.getSpO2()) + "%, ";
        dataString += "ObjT:" + String(objectTemp, 1) + "C, ";
        dataString += "MIC:" + String(micValue);

        // Print formatted data string to Serial Monitor
        Serial.println(dataString);
        
        // Send formatted data string over HC-05
        BTSerial.println(dataString);

        // Display on LCD
        lcd.clear();

        // First row: display heart rate and SpO2
        lcd.setCursor(0, 0);
        lcd.print("HR:");
        lcd.print(pox.getHeartRate());
        lcd.print(" SpO2:");
        lcd.print(pox.getSpO2());

        // Second row: display object temperature and MIC sensor reading
        lcd.setCursor(0, 1);
        lcd.print("ObjT:");
        lcd.print(objectTemp, 1); // 1 decimal place
        lcd.print(" MIC:");
        lcd.print(micValue);

        tsLastReport = millis();  
    }
}
