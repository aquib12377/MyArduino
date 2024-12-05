#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Pin Definitions
#define FENCE_SENSOR_PIN A1
#define BUZZER_PIN 2
#define LDR_PIN 13
#define SOIL_MOISTURE_PIN A0
#define RAIN_MOISTURE_PIN A3
#define PUMP_PIN1 4
#define PUMP_PIN2 5
#define FAN_PIN1 6
#define FAN_PIN2 7
#define SERVO_PIN 9
#define SERVO_PIN1 10
#define DHT_PIN 3
#define DHT_TYPE DHT11

// Constants
const int fenceThreshold = 50;
const int soilDryThreshold = 600;
const int tempHighThreshold = 35; // Temperature threshold for Fan ON
const int servoStepDelay = 50;    // Delay between servo steps (milliseconds)
const int servoMin = 0;           // Minimum servo angle
const int servoMax = 180;         // Maximum servo angle

// Variables for millis-based timing
unsigned long previousMillisDHT = 0;
unsigned long previousMillisPump = 0;
unsigned long previousMillisFan = 0;
unsigned long previousMillisServo = 0; // Timing for servo
const long intervalDHT = 2000;
const long intervalPump = 1000;
const long intervalFan = 1000;

// Servo rotation variables
int servoPosition = 0; // Current position of the servo
int servoDirection = 1; // 1 for increasing angle, -1 for decreasing

// Objects
Servo scarecrowServo;
Servo seedServo;
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD object
DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(9600);

  // Initialize pins
  pinMode(FENCE_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(FAN_PIN1, OUTPUT);
  pinMode(FAN_PIN2, OUTPUT);
  pinMode(PUMP_PIN1, OUTPUT);
  pinMode(PUMP_PIN2, OUTPUT);
  
  scarecrowServo.attach(SERVO_PIN);
  seedServo.attach(SERVO_PIN1);

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Agri System");
  delay(2000);
  lcd.clear();

  dht.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Read DHT11 Temperature and Humidity
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
      Serial.print("Temp: ");
      Serial.print(temperature);
      Serial.print(" C, Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(temperature);
      lcd.print("C   "); // Ensure clearing of leftover characters
      lcd.setCursor(0, 1);
      lcd.print("Humid: ");
      lcd.print(humidity);
      lcd.print("%   "); // Ensure clearing of leftover characters
      delay(1000);
      // Fan control based on temperature
      if (currentMillis - previousMillisFan >= intervalFan) {
        if (temperature > tempHighThreshold) {
          Serial.println("Temperature high. Turning on fan...");
          digitalWrite(FAN_PIN1, HIGH);
          digitalWrite(FAN_PIN2, LOW);
        } else {
          Serial.println("Temperature normal. Turning off fan...");
          digitalWrite(FAN_PIN1, LOW);
          digitalWrite(FAN_PIN2, LOW);
        }
        previousMillisFan = currentMillis;
      }
    } else {
      Serial.println("Failed to read from DHT sensor!");
      lcd.setCursor(0, 0);
      lcd.print("DHT11 Error!     ");
    }
  }

  // 2. Soil Moisture and Pump Control
  if (currentMillis - previousMillisPump >= intervalPump) {
    previousMillisPump = currentMillis;

    int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
    Serial.print("Soil Moisture Value: ");
    Serial.println(soilMoistureValue);

    lcd.setCursor(0, 0);
    lcd.print("Soil: ");
    lcd.print(soilMoistureValue);
    lcd.print("     "); // Clear leftover characters

    if (soilMoistureValue > soilDryThreshold) {
      Serial.println("Soil is dry. Turning on pump...");
      lcd.setCursor(0, 1);
      lcd.print("Pump: ON   ");
      digitalWrite(PUMP_PIN1, HIGH);
      digitalWrite(PUMP_PIN2, LOW);
    } else {
      Serial.println("Soil is moist. Turning off pump...");
      lcd.setCursor(0, 1);
      lcd.print("Pump: OFF  ");
      digitalWrite(PUMP_PIN1, LOW);
      digitalWrite(PUMP_PIN2, LOW);
    }
  }

  // 3. Continuous Slow Servo Rotation for Scarecrow
  if (currentMillis - previousMillisServo >= servoStepDelay) {
    previousMillisServo = currentMillis;

    scarecrowServo.write(servoPosition);
    servoPosition += servoDirection;

    // Reverse direction at limits
    if (servoPosition >= servoMax || servoPosition <= servoMin) {
      servoDirection = -servoDirection;
    }
  }

  // 4. Fence Security
  int fenceValue = analogRead(FENCE_SENSOR_PIN);
  Serial.println("Fence Value: "+String(fenceValue));
  if (fenceValue > fenceThreshold) {
    Serial.println("Fence touched! Activating buzzer...");
    lcd.setCursor(0, 1);
    lcd.print("Fence Alert!  ");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Fence Secure  ");
  }

  // 5. Rain Sensor and Seed Dispensing
  int rain = analogRead(RAIN_MOISTURE_PIN);
  if (rain < 300) {
    seedServo.write(90);
  } else {
    seedServo.write(0);
  }
}
