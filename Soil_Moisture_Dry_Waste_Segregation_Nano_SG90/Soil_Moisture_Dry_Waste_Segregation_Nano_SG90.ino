#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Initialize the I2C LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 to your LCD's I2C address if needed

// Define pins
#define SOIL_SENSOR_PIN A0 // Soil Moisture Sensor connected to analog pin A0
#define SERVO_PIN 9       // Servo motor connected to digital pin 9
#define IR_SENSOR_PIN 2   // IR Sensor connected to digital pin 2 (Active LOW)

// Define thresholds
#define DRY_THRESHOLD 500 // Adjust this value based on your sensor's readings

// Create servo object
Servo myServo;

void setup() {
  // Initialize LCD
  lcd.begin();
  lcd.backlight();

  // Initialize pins
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  // Initialize servo
  myServo.attach(SERVO_PIN);
  myServo.write(90); // Set servo to initial position

  // Initialize serial monitor for debugging
  Serial.begin(9600);
}

void loop() {
  // Read IR sensor value
  int irValue = digitalRead(IR_SENSOR_PIN);

  if (irValue == LOW) { // Object detected
    // Read soil moisture sensor value
    int moistureValue = digitalRead(SOIL_SENSOR_PIN);

    // Display soil moisture value on the LCD
    lcd.setCursor(0, 0);
    lcd.print("Soil Moisture:   ");
    lcd.setCursor(0, 1);
    lcd.print(moistureValue);

    // Check if soil is wet or dry
    if (moistureValue == HIGH) {
      // Soil is dry
      lcd.setCursor(1, 1);
      lcd.print("Dry ");
      myServo.write(180); // Rotate servo to 180 degrees
    delay(5000);
    } else {
      // Soil is wet
      lcd.setCursor(1, 1);
      lcd.print("Wet ");
      myServo.write(0); // Rotate servo back to 90 degrees
      delay(5000);
    }

    // Debugging output to serial monitor
    Serial.print("Moisture Value: ");
    Serial.println(moistureValue);
  } else {
    // No object detected
    lcd.setCursor(0, 0);
    lcd.print("No object       ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    myServo.write(90);
  }

  // Delay before next reading
  delay(1000);
}
