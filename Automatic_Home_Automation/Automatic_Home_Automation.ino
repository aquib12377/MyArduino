#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

// ——— Pin definitions ———
// IR modules (go LOW on object detection)
const int   IR_PINS[3]  = {A0, A1, A2};

// Gas sensor digital output (goes HIGH when gas threshold exceeded)
const int GAS_SENSOR_PIN   = A3;

const int RELAY_PINS[5]    = {4, 5, 6, 7, 8};

const int GAS_LED_PIN      = 9;

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Temperature thresholds to trigger
const float TEMP_THRESHOLD_OBJ = 40.0;  // °C for object‑temp relay
const float TEMP_THRESHOLD_AMB = 30.0;  // °C for ambient‑temp LED on pin 9

// 16×2 I²C LCD at address 0x27 (adjust if yours is 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Init MLX90614
  if (!mlx.begin()) {
    Serial.println("ERROR: MLX90614 not found");
    while (1) delay(10);
  }

  // Init LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Initializing...");
  delay(500);

  // Configure IR inputs
  for (int i = 0; i < 3; i++) {
    pinMode(IR_PINS[i], INPUT_PULLUP);
  }
  // Configure gas sensor input
  pinMode(GAS_SENSOR_PIN, INPUT);

  // Configure relays as outputs and turn them all OFF (HIGH = off)
  for (int i = 0; i < 5; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH);
  }
  // Gas LED/buzzer
  pinMode(GAS_LED_PIN, OUTPUT);
  digitalWrite(GAS_LED_PIN, LOW);

  delay(100);
}

void loop() {
  // 1) Read IR modules & drive Relays 1–3
  char irMask[4] = {'0','0','0','\0'};
  for (int i = 0; i < 3; i++) {
    bool detected = (digitalRead(IR_PINS[i]) == LOW);
    digitalWrite(RELAY_PINS[i], detected ? LOW : HIGH);
    irMask[i] = detected ? '1' : '0';
    Serial.print("IR"); Serial.print(i+1);
    Serial.print(detected ? ":DET  " : ":OK   ");
  }

  // 2) Read MLX90614 temps & drive Relay 4
  float ambTemp = mlx.readAmbientTempC();
  float objTemp = mlx.readObjectTempC();
  bool tooHot  = (objTemp >= TEMP_THRESHOLD_OBJ);
  digitalWrite(RELAY_PINS[3], tooHot ? LOW : HIGH);
    digitalWrite(RELAY_PINS[4], ambTemp >  TEMP_THRESHOLD_AMB ? LOW : HIGH);

  Serial.print(" | Amb:"); Serial.print(ambTemp,1);
  Serial.print("C Obj:"); Serial.print(objTemp,1);
  Serial.print("C → R4 "); Serial.print(tooHot ? "ON " : "OFF");

  // 3) Read gas sensor & drive Relay 5 + LED
  bool gasAlarm = (digitalRead(GAS_SENSOR_PIN) == LOW);
  digitalWrite(GAS_LED_PIN, gasAlarm ? HIGH : LOW);
  Serial.print(" | Gas:"); Serial.print(gasAlarm ? "ALARM" : "OK   ");
  Serial.println(" → R5");

  // 4) Update LCD (2 lines × 16 chars)
  lcd.clear();
  lcd.setCursor(0,0);
  // e.g. "I:101 G:OK T:A23/O45"
  lcd.print("I:"); lcd.print(irMask);
  lcd.print(" G:"); lcd.print(gasAlarm ? "AL" : "OK");
  lcd.setCursor(0,1);
  lcd.print("T:A"); lcd.print(ambTemp,2);
  lcd.print("C O"); lcd.print(objTemp,0);
  lcd.print("C");

  delay(1000);
}
