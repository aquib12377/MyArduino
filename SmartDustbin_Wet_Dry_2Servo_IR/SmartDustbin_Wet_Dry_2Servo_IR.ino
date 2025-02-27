#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Create servo objects for each dustbin lid
Servo servoDry;  // Servo for Dry Dustbin Lid
Servo servoWet;  // Servo for Wet Dustbin Lid

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change the address if needed

// Pin definitions
const int irPin = 3;         // IR sensor digital input pin
const int moisturePin = 2;   // Soil moisture sensor analog input pin
const int servoDryPin = 9;   // Dry dustbin servo control pin
const int servoWetPin = 10;  // Wet dustbin servo control pin

// Threshold for moisture sensor reading (adjust based on your calibration)
const int moistureThreshold = 500;

// Servo movement parameters
const int closedAngle = 90;  // Angle when lid is closed
const int openAngle = 0;     // Angle when lid is open (adjust as needed)
const int stepDelay = 15;    // Delay (ms) between incremental servo moves

void setup() {
  Serial.begin(9600);

  // Initialize IR sensor pin as input
  pinMode(irPin, INPUT_PULLUP);

  // Attach servos to their respective pins
  servoDry.attach(servoDryPin);
  servoWet.attach(servoWetPin);

  // Set initial positions (closed lids)
  servoDry.write(closedAngle);
  servoWet.write(closedAngle);

  Serial.println("System initialized");
  
  // Initialize LCD I2C
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System initialized");
}

void loop() {
  lcd.clear();
  // Check if garbage is detected via the IR sensor.
  // (Assuming HIGH means detection; adjust if your sensor is inverted)
  int irState = digitalRead(irPin);
  if (irState == LOW) {
    Serial.println("Garbage detected!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Garbage detected!");

    // Read the soil moisture sensor value
    int moistureValue = analogRead(moisturePin);
    Serial.print("Moisture sensor value: ");
    Serial.println(moistureValue);
    lcd.setCursor(0, 1);
    lcd.print("Moisture: ");
    lcd.print(moistureValue);

    // Decide which dustbin to open based on moisture level:
    if (digitalRead(moisturePin) == HIGH) {
      Serial.println("Dry waste detected. Opening dry dustbin lid.");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Dry waste");
      for (int pos = 90; pos >= 0; pos--) {
        servoDry.write(pos);
        delay(stepDelay);
      }
      delay(5000);
      for (int pos = 0; pos <= 90; pos++) {
        servoDry.write(pos);
        delay(stepDelay);
      }

    } else {
      Serial.println("Wet waste detected. Opening wet dustbin lid.");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wet waste");
      for (int pos = 90; pos >= 0; pos--) {
        servoWet.write(pos);
        delay(stepDelay);
      }
      delay(5000);
      for (int pos = 0; pos <= 90; pos++) {
        servoWet.write(pos);
        delay(stepDelay);
      }
    }

    // Wait until the IR sensor no longer detects garbage
    while (digitalRead(irPin) == HIGH) {
      delay(100);
    }
  }

  delay(100);  // Small delay to avoid rapid sensor polling
}

// Function to gradually open the lid, keep it open, and then close it slowly
void openLidGradually(Servo &servo) {
  // Gradually open the lid
  for (int pos = closedAngle; pos <= openAngle; pos++) {
    servo.write(pos);
    delay(stepDelay);
  }

  // Keep the lid open for a set time (e.g., 2 seconds)
  delay(2000);

  // Gradually close the lid
  for (int pos = openAngle; pos >= closedAngle; pos--) {
    servo.write(pos);
    delay(stepDelay);
  }

  // Optional: small delay before allowing the next operation
  delay(1000);
}
