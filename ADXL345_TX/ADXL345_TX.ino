#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <SoftwareSerial.h>

// Create ADXL345 instance
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Create a software serial port for HC-12
SoftwareSerial hc12(5,4); // RX, TX

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Initialize HC-12
  hc12.begin(9600);

  // Initialize ADXL345
  if (!accel.begin()) {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
    while (1);
  }

  // Set ADXL345 range (optional)
  accel.setRange(ADXL345_RANGE_16_G);
}

void loop() {
  // Get acceleration data
  sensors_event_t event;
  accel.getEvent(&event);

  // Determine direction based on acceleration values
  String command = "S"; // Default to Stop

  if (event.acceleration.x > 3) {
    command = "R"; // Right
  } else if (event.acceleration.x < -3) {
    command = "L"; // Left
  } else if (event.acceleration.y > 3) {
    command = "F"; // Forward
  } else if (event.acceleration.y < -3) {
    command = "B"; // Backward
  }

  // Send command via HC-12
  hc12.println(command);

  // Print command to Serial Monitor (optional)
  Serial.println(command);

  // Delay for stability
  delay(500);
}
