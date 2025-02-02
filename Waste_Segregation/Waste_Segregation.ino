#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Initialize LCD I2C address (usually 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Initialize Servo
Servo wasteServo;

// Pin definitions
const int metalDetectorPin = 12;
const int soilMoisturePin = A3;
const int servoPin = 9;
const int trigPin = 11;  // Ultrasonic trigger pin
const int echoPin = 10;  // Ultrasonic echo pin

// Waste segregation positions for servo
const int defaultPosition = 90;   // Default position of the servo (center)
const int metalPosition = 0;      // Position for metal waste
const int organicPosition = 180;  // Position for organic waste
const int dryWastePosition = 0;  // Position for dry waste

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Waste Segregation");

  // Initialize Servo
  wasteServo.attach(servoPin);
  wasteServo.write(defaultPosition);  // Set servo to default position

  // Initialize pins
  pinMode(metalDetectorPin, INPUT);
  pinMode(soilMoisturePin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initial setup messages
  Serial.println("Waste Segregation System Started");
  delay(2000);  // Display the welcome message for 2 seconds
  lcd.clear();
}

long readUltrasonicDistance() {
  // Send a 10us pulse to trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the pulse duration from echo pin
  long duration = pulseIn(echoPin, HIGH);
  // Calculate the distance in cm (Speed of sound is 340m/s or 29.1 µs per cm)
  long distance = duration * 0.034 / 2;
  return distance;
}

// Function to move the servo gradually
void moveServoSlowly(int startPos, int endPos, int delayTime = 20) {
  if (startPos < endPos) {
    for (int pos = startPos; pos <= endPos; pos++) {
      wasteServo.write(pos);
      delay(delayTime);  // Delay between each step
    }
  } else {
    for (int pos = startPos; pos >= endPos; pos--) {
      wasteServo.write(pos);
      delay(delayTime);  // Delay between each step
    }
  }
}

void loop() {
  long distance = readUltrasonicDistance();
  int metalDetected = digitalRead(metalDetectorPin);
  int moistureDetected = digitalRead(soilMoisturePin);

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Check if an object is within 15 cm (assuming the garbage passes through this range)
  if (distance < 15) {
    Serial.println("Garbage detected by Ultrasonic");

    Serial.print("Metal Detector: ");
    Serial.println(metalDetected);
    Serial.print("Moisture Detector: ");
    Serial.println(moistureDetected);

    int currentPos = wasteServo.read(); // Get the current servo position

    if (moistureDetected == HIGH && metalDetected == 0) {
      lcd.print("Dry Waste");
      Serial.println("Dry waste detected");
      moveServoSlowly(currentPos, dryWastePosition);  // Slowly move to dry waste position
    } 
    else if (moistureDetected == LOW && metalDetected == 0) {
      lcd.print("Wet Waste");
      Serial.println("Wet waste detected");
      moveServoSlowly(currentPos, organicPosition);  // Slowly move to wet waste position
    } 
    else if (metalDetected == 1) {
      lcd.print("Metal Waste");
      Serial.println("Metal waste detected");
      moveServoSlowly(currentPos, metalPosition);  // Slowly move to metal waste position
    } 
    else {
      lcd.print("No Waste");
      Serial.println("No waste detected");
      moveServoSlowly(currentPos, defaultPosition);  // Reset servo to default position
    }

    delay(2000);  // Delay to hold the position and display the waste type
    moveServoSlowly(wasteServo.read(), defaultPosition);  // Slowly reset servo to default position
    Serial.println("Servo reset to default position");
    lcd.clear();  // Clear the LCD
  } 
  else {
    Serial.println("No garbage detected by Ultrasonic");
    lcd.print("No Garbage");
    delay(1000);
    lcd.clear();  // Clear the LCD if no garbage is detected
    delay(1000);  // Wait before the next loop
  }
}
