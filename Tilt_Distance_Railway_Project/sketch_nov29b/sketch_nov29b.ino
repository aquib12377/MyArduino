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

// Global variables for tilt angle and distance
float tiltAngle = 0;
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

// Function to calculate tilt angle from accelerometer data
float calculateTiltAngle() {
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate tilt angle in degrees
  float x = event.acceleration.x;
  float y = event.acceleration.y;
  float z = event.acceleration.z;
  Serial.println(accel.getX());
  Serial.println(accel.getY());
  Serial.println(accel.getZ());
  return atan2(sqrt(y * y + z * z), x) * 180.0 / PI;
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
  // Measure tilt angle
  tiltAngle = calculateTiltAngle();

  // Measure distance
  distance = measureDistance();

  // Display data on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tilt: ");
  lcd.print(tiltAngle, 1);
  lcd.print(" deg");

  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distance, 2);
  lcd.print(" m");

  // Check conditions
  if (tiltAngle >= 2 && tiltAngle <= 5 && distance > 1) {
    Serial.println("Condition Met: Tilt 2-5 deg, Dist > 1m");
  } else {
    Serial.println("Condition Not Met");
  }

  // Wait 500ms before the next reading
  delay(500);
}
