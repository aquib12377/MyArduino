#include "BluetoothSerial.h"  // Include BluetoothSerial library

// Create Bluetooth Serial object
BluetoothSerial SerialBT;

// MQ135 sensor pin
const int mq135Pin = 34;  // Analog pin 34

void setup() {
  // Start serial communication for debugging
  Serial.begin(115200);
  
  // Start Bluetooth Serial communication with a device name
  if (!SerialBT.begin("ESP32_GAS")) {  // You can set any name you like for the ESP32 Bluetooth
    Serial.println("An error occurred initializing Bluetooth");
  } else {
    Serial.println("Bluetooth started. Device name: ESP32_MQ135");
  }
  
  // Optionally, you can check the sensor calibration or startup here
}

void loop() {
  // Read analog value from the MQ135 sensor
  int mq135Value = analogRead(mq135Pin);

  // Convert analog value to voltage for easier interpretation (if needed)
  float voltage = mq135Value * (3.3 / 4095.0);  // ESP32 ADC has 12-bit resolution
  
  mq135Value = map(mq135Value,100,4095,0,100);
  // Send sensor value over Bluetooth
  SerialBT.print("Gas Value: ");
  SerialBT.print(mq135Value);
  SerialBT.println("%");
  // Print the same data to the Serial monitor for debugging
  Serial.print("MQ135 Gas Value: ");
  Serial.println(mq135Value);
  
  // Optionally print the voltage value for debugging
  Serial.print("Voltage: ");
  Serial.println(voltage);
  
  // Delay for a bit before reading again
  delay(1000);  // Send data every second
}
