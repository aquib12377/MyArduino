#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(123);

void setup() {
  Serial.begin(9600); // HC-12 communication (connected to pins 0 and 1)
  Wire.begin();

  if (!accel.begin()) {
    Serial.println("No ADXL345 detected.");
    while (1);
  }

  accel.setRange(ADXL345_RANGE_2_G); // Sensitivity range
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);

  float x = event.acceleration.x;
  float y = event.acceleration.y;

  String command = "S";

  if (y > 3) command = "F";
  else if (y < -3) command = "B";
  else if (x > 3) command = "R";
  else if (x < -3) command = "L";

  Serial.println(command);
  delay(500);
}