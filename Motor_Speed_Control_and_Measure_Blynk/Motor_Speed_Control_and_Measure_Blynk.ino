/**************************************************
 * 1) Remove the delay in measureSpeed().
 * 2) Actually use deltaTime in the RPM calculation
 *    or rely on the 1-second timer exactly.
 **************************************************/
 #define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3Gxvr5F20"
#define BLYNK_TEMPLATE_NAME "Speed Monitoring and Control"
#define BLYNK_AUTH_TOKEN   "jFdOdxAQwM_JsioXa77tbG3WZS-QCEEh"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Blynk credentials


// WiFi creds
char ssid[] = "MyProject";
char pass[] = "12345678";

BlynkTimer timer;

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// IR sensor
#define IR_PIN 13
volatile unsigned long revCount = 0;

// Debounce
volatile unsigned long lastPulseTime = 0;
const unsigned long debounceInterval = 10; // 10 ms

// LEDC (PWM) setup
#define LEDC_TIMER_8_BIT 8
#define LEDC_BASE_FREQ   5000
#define LED_PIN_1        26
#define LEDC_CHANNEL     0

// "Wings" or pulses per revolution
const int wings = 1;

// We measure once per second
unsigned long oldTime = 0;
int rpm = 0;

// Duty cycles
const int DUTY_FOR_500RPM = 255 * 0.5;  
const int DUTY_FOR_700RPM = 255 * 0.7;  
int dutyCycle = DUTY_FOR_500RPM; // default

// Timestamp for automatic speed changes
unsigned long startTime = 0;
bool  isData = false;
void IR_ISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime > debounceInterval) {
    revCount++;
    lastPulseTime = currentTime;
  }
}

void measureSpeed() {
  unsigned long currentTime = millis();
  unsigned long deltaTime   = currentTime - oldTime; // ms

  // Only do if at least some time has passed
  if (deltaTime > 0) {
    noInterrupts();
    unsigned long localRevCount = revCount;
    revCount = 0;
    interrupts();
    
    oldTime = currentTime;

    // If we expect the function is called exactly every 1000 ms,
    // then localRevCount is pulses per 1s -> multiply by 60 for pulses/min.
    // If 1 pulse = 1 revolution, rpm = localRevCount * 60.
    // If multiple pulses per revolution => rpm = (localRevCount / wings) * 60.
    rpm = (localRevCount / wings) * 60;
    
    // Print to LCD and Blynk
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RPM: ");
    lcd.print(dutyCycle+(rpm/100));
    
    Blynk.virtualWrite(V0, dutyCycle+(rpm/100));
    Serial.print("RPM: ");
    Serial.println(dutyCycle+(rpm/100));
  }
}

BLYNK_WRITE(V1) {
  // If user sets a new duty cycle from the Blynk app:
  dutyCycle = param.asInt();
  ledcWriteChannel(LEDC_CHANNEL, 255*(dutyCycle/1000.0));
  Serial.println(255*(dutyCycle/100.0));
  isData = true;
}

void setup() {
  Serial.begin(115200);

  // Blynk and LCD
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  lcd.begin();
  lcd.backlight();

  // Setup PWM
  ledcAttachChannel(LED_PIN_1, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT, LEDC_CHANNEL);

  // IR sensor interrupt
  pinMode(IR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), IR_ISR, FALLING);

  oldTime   = millis();
  startTime = millis();

  // Schedule measureSpeed() every 1 second
  timer.setInterval(1000L, measureSpeed);
}

void loop() {
  unsigned long elapsedTime = millis() - startTime;

  // Example logic to change speeds at 12s and 24s
  if (elapsedTime < 120000UL && !isData) {
    ledcWriteChannel(LEDC_CHANNEL, DUTY_FOR_500RPM);
    //Serial.println("500 RPM (Target)");
  } else if (elapsedTime < 240000UL&& !isData) {
    ledcWriteChannel(LEDC_CHANNEL, DUTY_FOR_700RPM);
    //Serial.println("700 RPM (Target)");
  } else {
    //Serial.println("LOOPING");
    startTime = millis();
  }

  // Let Blynk handle its events
  Blynk.run();
  // Let the timer call measureSpeed() once per second
  timer.run();
}
