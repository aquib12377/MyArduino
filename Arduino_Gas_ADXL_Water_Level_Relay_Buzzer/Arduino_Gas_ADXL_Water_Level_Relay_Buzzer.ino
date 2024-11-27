#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// Pins
#define MQ6_PIN 2           // Digital output of MQ6
#define RELAY_PIN 3         // Relay (Active LOW)
#define TRIG_PIN 6          // Ultrasonic Trig pin
#define ECHO_PIN 7          // Ultrasonic Echo pin
#define BUZZER_PIN 5        // Buzzer pin

// Threshold for angle change
#define ANGLE_THRESHOLD 15.0
#define WATER_LEVEL_THRESHOLD 10 // Threshold in cm for high water level

// ADXL345 Object
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

void setup() {
  pinMode(MQ6_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Turn off Relay initially
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF (Active LOW)

  // Initialize Serial Monitor
  Serial.begin(9600);
  Serial.println("System Initialized!");

  // Initialize ADXL345
  if (!accel.begin()) {
    Serial.println("ADXL345 not detected. Check connections.");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_2_G);
}

void loop() {
  // Relay Control based on MQ6
  int mq6Value = digitalRead(MQ6_PIN);
  if (mq6Value == LOW) {
    digitalWrite(RELAY_PIN, LOW); // Relay ON
    Serial.println("MQ6 Detected Gas! Relay ON.");
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF
    Serial.println("MQ6 Normal. Relay OFF.");
  }

  // Water Level Detection
  float waterLevel = getUltrasonicDistance();
  Serial.print("Water Level (cm): ");
  Serial.println(waterLevel);

  if (waterLevel <= WATER_LEVEL_THRESHOLD) {
    Serial.println("High Water Level Detected!");
    beepBuzzer(1); // Beep for high water level
  }

  // Accelerometer Angle Detection
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate tilt angle (simplified)
  float angleX = atan2(event.acceleration.y, event.acceleration.z) * 180 / PI;
  float angleY = atan2(event.acceleration.x, event.acceleration.z) * 180 / PI;

  if (abs(angleX) > ANGLE_THRESHOLD || abs(angleY) > ANGLE_THRESHOLD) {
    Serial.print("Angle Change Detected! X: ");
    Serial.print(angleX);
    Serial.print(", Y: ");
    Serial.println(angleY);
    beepBuzzer(2); // Beep for angle change
  }

  delay(500); // Adjust delay as needed
}

// Function to measure distance using the ultrasonic sensor
float getUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2; // Convert to cm
  return distance;
}

// Function to control buzzer beeps
void beepBuzzer(int beepType) {
  if (beepType == 1) { // Beep Type 1 for water level
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
  } else if (beepType == 2) { // Beep Type 2 for angle change
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
}
