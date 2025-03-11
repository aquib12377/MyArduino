#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3r7MfIrP2"
#define BLYNK_TEMPLATE_NAME "Gesture Words"
#define BLYNK_AUTH_TOKEN "nJUDmg27ZC1hwVn4f0Smximblo7modNA"

#include <Arduino.h>
#include <Wire.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDRESS 0x27
#define BUZZER_PIN  13

char ssid[] = "MyProject";
char pass[] = "12345678";

Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!adxl.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("ADXL345 error!");
    while (1);
  }

  adxl.setRange(ADXL345_RANGE_2_G);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setup done.");
  delay(1000);
}

void loop() {
  Blynk.run();
  sensors_event_t event;
  adxl.getEvent(&event);

  float x = event.acceleration.x;
  float y = event.acceleration.y;
  float z = event.acceleration.z;

  String message;

  if (fabs(x) < 3 && fabs(y) < 3 && z > 8) {
    message = "Food";
  } else if (x > 8) {
    message = "Water";
  } else if (x < -8) {
    message = "Help";
  } else if (fabs(x) < 3 && y > 8) {
    message = "Potty";
  } else {
    message = "";
  }

  static String prevMessage = "";
  if (message != prevMessage && message.length() > 0) {
    prevMessage = message;
    Serial.println("Detected: " + message);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gesture: ");
    lcd.setCursor(0, 1);
    lcd.print(message);

    Blynk.virtualWrite(V0, message);

    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
  delay(200);
}
