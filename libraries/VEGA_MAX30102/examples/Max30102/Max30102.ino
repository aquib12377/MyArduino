/**
 @file Max30102.ino
 @brief Checks the Heart Rate and SpO2
 @detail Checks the Heart Rate and SpO2 using Max30102 sensor

 * Reference arduino code: https://github.com/bobdavis321/Arduino-MAX30100 , https://www.youtube.com/watch?v=mmXJ33Nx4Mc 
 * Refrence aries board: https://vegaprocessors.in/blog/interfacing-max30102-pulse-oximeter-to-thejas-soc/
 * 
 * MAX30100 sensor
 * Device address -0x57
 * Connections:
 * MAX30102     Aries Board
 * 3V3      -   3.3V
 * GND      -   GND
 * SDA      -   SDA0
 * SCL      -   SCL0
 * For connecting to port 1 (SCL 1 and SDA1) of aries board use the default variable TwoWire Wire(1) instead of TwoWire Wire(0);
 * .
 * Note:
 * Use 10K pull-up resistors while connecting SDA and SCL pins to MAX30102 sensor for accurate measurements.
 **/

#include <Wire.h>

TwoWire Wire(0);

#include "MAX30102_PulseOximeter.h"

#define REPORTING_PERIOD_MS     1000

// PulseOximeter is the higher level interface to the sensor
// it offers:
//  * beat detection reporting
//  * heart rate calculation
//  * SpO2 (oxidation level) calculation 
PulseOximeter pox;
uint32_t tsLastReport = 0;
// Callback (registered below) fired when a pulse is detected 
void onBeatDetected()
{
    Serial.println("Beat!");
}
 
void setup()
{
    Serial.begin(115200);
    Serial.print("Initializing..");
    delay(3000);
    // Initialize the PulseOximeter instance
    if (!pox.begin()) {
        Serial.println("FAILED");
        for(;;);
    } else {
        Serial.println("SUCCESS");
    }
    // The default current for the IR LED is 50mA and is changed below
    pox.setIRLedCurrent(MAX30102_LED_CURR_7_6MA);
    // Register a callback for the beat detection
    pox.setOnBeatDetectedCallback(onBeatDetected);
}
 
void loop()
{
    // Make sure to call update as fast as possible
    pox.update();
   // long irValue = pox.getHeartRate();
    // Asynchronously dump heart rate and oxidation levels to the serial
    // For both, a value of 0 means "invalid"
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        Serial.print("Heart rate:");
        Serial.print(pox.getHeartRate());
        Serial.print("bpm / SpO2:");
        Serial.print(pox.getSpO2());
        Serial.print("%");

        
 // if (irValue < 70)
   // Serial.print(",  No finger?");
    Serial.println();
         
        tsLastReport = millis();
    }
}
