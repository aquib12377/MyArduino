/********************  Blynk IoT (ESP32)  ********************/
#define BLYNK_TEMPLATE_ID   "TMPL3VY_TpKLl"
#define BLYNK_TEMPLATE_NAME "Speed Monitoring and Control"
#define BLYNK_AUTH_TOKEN    "iF2YoDvaFvwvBHwWZZ_3ciwpK6ZD3Gu5"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/********************  Wi-Fi creds  ********************/
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";

/********************  Pins & Hardware  ********************/
#define PIN_PWM_EN     13      // Motor EN (PWM via analogWrite)
#define PIN_IR_SPEED   14      // IR tacho input (active-LOW)

// I2C LCD (16x2)
#define LCD_ADDR       0x27
#define LCD_COLS       16
#define LCD_ROWS       2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

/********************  Tacho / RPM  ********************/
volatile uint32_t pulseCount = 0;      // pulses within measurement window
volatile uint32_t lastPulseUs = 0;

const uint8_t  SLOTS_PER_REV = 1;      // adjust per encoder/disc
const uint32_t MIN_PULSE_US  = 2000;   // simple debounce (2 ms)
const uint32_t WINDOW_MS     = 1000;   // compute RPM every 1s

// Telemetry
int   duty_0_255 = 0;
float rpm        = 0.0;

/********************  Blynk  ********************/
BlynkTimer timer;

// V1: speed slider 0..255
BLYNK_WRITE(V1) {
  int v = param.asInt();
  duty_0_255 = constrain(v, 0, 255);
  analogWrite(PIN_PWM_EN, duty_0_255);

  // Optional: duty % on V3
  int dutyPct = (int)lroundf((duty_0_255 / 255.0f) * 100.0f);
  Blynk.virtualWrite(V3, dutyPct);
}

void IRAM_ATTR isrSpeed() {
  // Active-LOW sensor: trigger on FALLING edge
  uint32_t now = micros();
  if (now - lastPulseUs >= MIN_PULSE_US) {  // crude debounce
    pulseCount++;
    lastPulseUs = now;
  }
}

void computeAndPublishRPM() {
  static uint32_t lastMillis = 0;

  uint32_t now = millis();
  uint32_t elapsed = (lastMillis == 0) ? WINDOW_MS : (now - lastMillis);
  if (elapsed < WINDOW_MS) return;
  lastMillis = now;

  noInterrupts();
  uint32_t count = pulseCount;
  pulseCount = 0;     // reset window
  interrupts();

  // RPM = (pulses / slots_per_rev) * (60 / window_sec)
  float windowSec = elapsed / 1000.0f;
  if (SLOTS_PER_REV > 0 && windowSec > 0) {
    rpm = (count / (float)SLOTS_PER_REV) * (60.0f / windowSec);
  } else {
    rpm = 0;
  }

  // Update LCD
  lcd.setCursor(0, 0);
  lcd.print("RPM:");
  lcd.setCursor(4, 0); lcd.print("        ");
  lcd.setCursor(4, 0); lcd.print((int)lroundf(rpm));

  lcd.setCursor(0, 1);
  lcd.print("Duty:");
  int dutyPct = (int)lroundf((duty_0_255 / 255.0f) * 100.0f);
  lcd.setCursor(5, 1); lcd.print("    ");
  lcd.setCursor(5, 1); lcd.print(dutyPct);
  lcd.print("%   ");

  // Push to Blynk
  Blynk.virtualWrite(V2, (int)lroundf(rpm)); // measured RPM
  Blynk.virtualWrite(V3, dutyPct);           // duty %
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // LCD
  Wire.begin();            // SDA=21, SCL=22 by default on ESP32
  lcd.begin();              // LiquidCrystal_I2C init
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Motor Controller");
  lcd.setCursor(0, 1); lcd.print("Init...");

  // PWM output
  pinMode(PIN_PWM_EN, OUTPUT);
  analogWrite(PIN_PWM_EN, 0);  // start stopped

  // IR input (active-LOW). Use internal pull-up to keep stable.
  pinMode(PIN_IR_SPEED, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_IR_SPEED), isrSpeed, FALLING);

  // Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  // Timers
  timer.setInterval(WINDOW_MS, computeAndPublishRPM);

  // UI ready
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("RPM: 0");
  lcd.setCursor(0, 1); lcd.print("Duty: 0%");
}

void loop() {
  Blynk.run();
  timer.run();
}
