#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define LDR_PIN_1 A0
#define LDR_PIN_2 A1
#define LIMIT_SWITCH_PIN_1 A2
#define LIMIT_SWITCH_PIN_2 A3
#define TEMP_SENSOR_PIN 2
#define RELAY_PIN 5
#define MF 3
#define MB 4

LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  pinMode(LIMIT_SWITCH_PIN_1, INPUT_PULLUP);
  pinMode(LIMIT_SWITCH_PIN_2, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MF, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Turn off the relay

  pinMode(MB, OUTPUT);
  digitalWrite(MF, HIGH);
  digitalWrite(MB, HIGH);
  lcd.begin();
  lcd.backlight();


  sensors.begin();

  // Start Serial communication for debugging
  Serial.begin(9600);
  while (!Serial) continue;
  Serial.println("Serial communication started.");
}

void loop() {
  int ldrValue1 = digitalRead(LDR_PIN_1);
  int ldrValue2 = digitalRead(LDR_PIN_2);

  Serial.println("LDR1 Value: " + String(ldrValue1));
  Serial.println("LDR2 Value: " + String(ldrValue2));

  boolean limitSwitch1Pressed = digitalRead(LIMIT_SWITCH_PIN_1) == LOW;
  boolean limitSwitch2Pressed = digitalRead(LIMIT_SWITCH_PIN_2) == LOW;

  // Control motor based on LDR values and limit switches
  if ((ldrValue1 ==1 && ldrValue2 == 1) || (ldrValue1 ==0 && ldrValue2 == 0)) {
    digitalWrite(MF, HIGH);
    digitalWrite(MB, HIGH);
    Serial.println("Even light stop motor");
  }
  else if (ldrValue1 ==0 && ldrValue2 ==1 && !limitSwitch1Pressed) {
    digitalWrite(MF, LOW);
    digitalWrite(MB, HIGH);
    Serial.println("Moving motor clockwise.");
  } else if (ldrValue1 ==1 && ldrValue2 ==0 && !limitSwitch2Pressed) {
    digitalWrite(MF, HIGH);
    digitalWrite(MB, LOW);
    Serial.println("Moving motor anti-clockwise.");
  }
  else {
    digitalWrite(MF, HIGH);
    digitalWrite(MB, HIGH);
    Serial.println("StopMotor no signal");

  }

  // Read temperature from DS18B20 sensor
  sensors.requestTemperatures();
  float temperatureCelsius = sensors.getTempCByIndex(0);
  lcd.print("Temp: ---.- C");
  // Display temperature on LCD
  lcd.setCursor(6, 0);
  lcd.print(temperatureCelsius, 1);
  lcd.setCursor(0, 1);
  lcd.print(ldrValue1);
  lcd.setCursor(8, 1);
  lcd.print(ldrValue2);

  Serial.println(temperatureCelsius);

  // Control relay based on temperature
  if (temperatureCelsius > 35) {
    digitalWrite(RELAY_PIN, LOW); // Turn on the relay
    Serial.println("Relay turned ON.");
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Turn off the relay
    Serial.println("Relay turned OFF.");
  }

  // Add some delay to prevent fast updates
  delay(500);
  lcd.clear();
}