#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Pin Definitions
#define FENCE_SENSOR_PIN A0 // Analog pin for fence security sensor
#define BUZZER_PIN 2        // Buzzer pin
#define LDR_PIN 3           // Digital pin for LDR module
#define SOIL_MOISTURE_PIN A1
#define RAIN_MOISTURE_PIN A2 // Analog pin for soil moisture sensor
#define RELAY_PIN 4         // Relay control pin for pump
#define SERVO_PIN 9
#define SERVO_PIN1 10         // Servo motor pin
#define DHT_PIN 5           // DHT11 sensor pin
#define DHT_TYPE DHT11      // Define the type of DHT sensor

// Constants
const int fenceThreshold = 50;   // Threshold for fence security sensor
const int soilDryThreshold = 600; // Threshold for soil moisture sensor (adjust based on calibration)
const int servoDelay = 100;      // Delay between servo movements (for scarecrow)

// Objects
Servo scarecrowServo;
Servo seedServo;
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD object (address 0x27, 16x2 display)
DHT dht(DHT_PIN, DHT_TYPE);         // Initialize DHT sensor

// Functions
void buzz(int delayTime, int repeatCount) {
  for (int i = 0; i < repeatCount; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(delayTime);
    digitalWrite(BUZZER_PIN, LOW);
    delay(delayTime);
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Initialize pins
  pinMode(FENCE_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  scarecrowServo.attach(SERVO_PIN);
  seedServo.attach(SERVO_PIN1);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Agri System");
  delay(2000);
  lcd.clear();

  // Initialize DHT sensor
  dht.begin();

  // Initialize states
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);

  Serial.println("Smart Agriculture System Initialized.");
}

void loop() {
  // 1. Read DHT11 Temperature and Humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    lcd.setCursor(0, 0);
    lcd.print("DHT11 Error!");
  } else {
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print(" C, Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Humid: ");
    lcd.print(humidity);
    lcd.print("%");
    delay(2000); // Display temp and humidity for 2 seconds
    lcd.clear();
  }

  int rain = analogRead(RAIN_MOISTURE_PIN);
  Serial.println("Rain Sensor: "+String(rain));
  if(rain < 300)
  {
    seedServo.write(90);
  }
  else{
    seedServo.write(0);
  }

  // 2. Fence Security
  int fenceValue = analogRead(FENCE_SENSOR_PIN);
  Serial.print("Fence Sensor Value: ");
  Serial.println(fenceValue);
  lcd.setCursor(0, 0);
  lcd.print("Fence: ");
  lcd.print(fenceValue);

  if (fenceValue > fenceThreshold) {
    Serial.println("Fence touched! Activating buzzer...");
    lcd.setCursor(0, 1);
    lcd.print("Fence: ALERT!   ");
    buzz(300, 5);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Fence: Secure   ");
  }

  delay(500);

  // 3. Scarecrow Operation
  int ldrState = !digitalRead(LDR_PIN);
  Serial.print("LDR State: ");
  Serial.println(ldrState ? "Day" : "Night");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LDR: ");
  lcd.print(ldrState ? "Daytime" : "Night");

  if (ldrState == HIGH) { // Daytime detected
    Serial.println("Daytime detected. Moving scarecrow...");
    lcd.setCursor(0, 1);
    lcd.print("Scarecrow: ON ");
    for (int pos = 0; pos <= 180; pos += 10) {
      scarecrowServo.write(pos);
      delay(servoDelay);
    }
    for (int pos = 180; pos >= 0; pos -= 10) {
      scarecrowServo.write(pos);
      delay(servoDelay);
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Scarecrow: OFF");
  }

  delay(500);

  // 4. Soil Moisture and Pump Control
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  Serial.print("Soil Moisture Value: ");
  Serial.println(soilMoistureValue);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Soil: ");
  lcd.print(soilMoistureValue);

  if (soilMoistureValue < soilDryThreshold) { // Soil is dry
    Serial.println("Soil is dry. Turning on pump...");
    lcd.setCursor(0, 1);
    lcd.print("Pump: ON  ");
    digitalWrite(RELAY_PIN, LOW); // Turn on pump
    buzz(500, 1);
  } else {
    Serial.println("Soil is moist. Turning off pump...");
    lcd.setCursor(0, 1);
    lcd.print("Pump: OFF ");
    digitalWrite(RELAY_PIN, HIGH); // Turn off pump
  }

  // Delay for stability
  delay(1000);
}
