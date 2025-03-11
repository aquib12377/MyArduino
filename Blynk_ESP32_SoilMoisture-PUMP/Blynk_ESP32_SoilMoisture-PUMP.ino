#define BLYNK_TEMPLATE_ID "TMPL3qa7s9SDh"
#define BLYNK_TEMPLATE_NAME "Iot Irrigation"
#define BLYNK_AUTH_TOKEN "k3L3fG70ok5E71sZm7cFAKH_QdqIw-JN"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Blynk credentials
char ssid[] = "MyProject";
char pass[] = "12345678";

// Pin definitions
#define SOIL_SENSOR_PIN 34
#define PUMP_PIN 27

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
    Serial.begin(115200);
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    lcd.begin();
    lcd.backlight();
    pinMode(SOIL_SENSOR_PIN, INPUT_PULLUP);
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, HIGH);

    lcd.setCursor(0, 0);
    lcd.print("Irrigation Ready");
    Serial.println("Irrigation system ready");
    delay(3000);
    lcd.clear();
}

void loop() {
    Blynk.run();
    int moisture = analogRead(SOIL_SENSOR_PIN);
    int moisturePercent = map(moisture, 4095, 0, 0, 100);
    
    Serial.print("Soil Moisture: ");
    Serial.print(moisturePercent);
    Serial.println("%");

    lcd.setCursor(0, 0);
    lcd.print("Moisture: " + String(moisturePercent) + "%  ");

    Blynk.virtualWrite(V0, moisturePercent);
    delay(2000);
}

// Blynk function to control pump
BLYNK_WRITE(V1) {
    int pumpState = param.asInt();
    digitalWrite(PUMP_PIN, !pumpState);
    lcd.setCursor(0, 1);
    lcd.print(pumpState ? "Pump ON  " : "Pump OFF  ");
    Serial.println(pumpState ? "Pump ON" : "Pump OFF");
    delay(2000);
}
