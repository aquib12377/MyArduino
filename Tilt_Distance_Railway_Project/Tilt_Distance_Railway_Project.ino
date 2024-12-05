#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <LiquidCrystal_I2C.h>

// ADXL345 Accelerometer
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(123);

// Ultrasonic Sensor Pins
#define TRIG_PIN 3
#define ECHO_PIN 2

// LCD I2C Address
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Global variables for X-axis rotation angle and distance
float xRotationAngle = 0;
float distance = 0;

// Function to initialize the accelerometer
void setupADXL345() {
  if (!accel.begin()) {
    Serial.println("ADXL345 not detected!");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_2_G); // Set range to ±2g
  Serial.println("ADXL345 initialized.");
}

// Function to calculate X-axis rotation angle
float calculateXRotationAngle() {
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate X rotation angle in degrees
  float x = event.acceleration.x;
  float z = event.acceleration.z;

  return atan2(x, z) * 180.0 / PI;
}

// Function to measure distance using ultrasonic sensor
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure the echo time
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Convert time to distance in meters
  return duration * 0.034 / 2 / 100; // Divide by 100 to convert to meters
}

void setup() {
  // Serial Monitor for debugging
  Serial.begin(9600);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();

  // Initialize Accelerometer
  setupADXL345();

  // Ultrasonic Sensor Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(2000);
}

void loop() {
  // Measure X-axis rotation angle
  xRotationAngle = calculateXRotationAngle();

  // Measure distance
  distance = measureDistance();

  // Display data on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("X Angle: ");
  lcd.print(xRotationAngle, 1);
  lcd.print(" deg");

  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distance, 2);
  lcd.print(" m");

  // Check conditions
  if (xRotationAngle >= 2 || distance > 1) {
    lcd.clear();
    lcd.print("Track Tilt");
    delay(3000);
    Serial.println("Condition Met: X Angle 2-5 deg, Dist > 1m");
  } else {
    Serial.println("Condition Not Met");
  }

  // Wait 500ms before the next reading
  delay(500);
}
