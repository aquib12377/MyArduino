/*
  Smart Street Light (5) with Fault Detection + I2C LCD (16x2)
  - DAY:    all lights OFF
  - NIGHT:  default 50% brightness
  - CAR:    IR (active-LOW) near a pole -> that pole 100%
  - FAULT:  commanded ON but feedback says OFF for a sustained time -> latch fault
  - LCD:    rotating pages (IR view / Fault+Power view)

  BOARD: Arduino UNO/Nano
  NOTE:
    * LCD uses I2C on A4(SDA)/A5(SCL).
    * We use D0, D1 as regular inputs for feedback. DO NOT use Serial.
      (Unplug D0/D1 while uploading if needed.)
    * Feedback is treated as DIGITAL ACTIVE-HIGH by default (HIGH == LED actually ON).
      If your feedback is analog, see FB_IS_ANALOG switch below.

  ------------------- WIRING -------------------
  I2C LCD (16x2): SDA -> A4, SCL -> A5, Vcc, GND

  LDR module (analog): OUT -> A2   (moved off A4/A5)
  LED PWM (5x):        D3, D5, D6, D9, D10
  IR inputs (5x):      D2, D4, D7, D8, D12  (ACTIVE-LOW; INPUT_PULLUP)
  Feedback inputs (5x, DIGITAL by default, ACTIVE-HIGH):
                       D0, D1, D11, D13, A0 (as digital input)
  ---------------------------------------------
*/

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/********** USER SWITCHES **********/
#define USE_SERIAL     0      // <== keep 0 because D0/D1 are used as inputs
#define FB_IS_ANALOG   0      // 0=digital feedback, 1=analog feedback
#define FB_ACTIVE_HIGH 1      // only used when FB_IS_ANALOG==0 (digital). 1=HIGH means LED really ON
/***********************************/

// ---------------- PIN MAP ----------------
static const uint8_t N_LIGHTS = 5;

// LEDs: keep on PWM-capable pins
const uint8_t LED_PWM_PINS[N_LIGHTS] = {3, 5, 6, 9, 10};

// IR sensors (ACTIVE-LOW) with INPUT_PULLUP
const uint8_t IR_PINS[N_LIGHTS]      = {2, 4, 7, 8, 12};

// Feedback: DIGITAL by default, includes D0,D1
#if FB_IS_ANALOG
// If you prefer analog sense, give 5 analog-capable pins here (Nano: A0..A3,A6/A7; UNO has only A0..A5)
const uint8_t FB_ANALOG_PINS[N_LIGHTS] = {A0, A1, A2, A3, A0}; // <== adjust if you have Nano A6/A7
#else
const uint8_t FB_DIGITAL_PINS[N_LIGHTS] = {0, 1, 11, 13, A0};  // A0 used as digital input
#endif

// LDR (moved away from A4/A5 used by I2C)
const uint8_t LDR_PIN = A2;

// Night/day thresholds (tune from Serial/LCD)
const bool    LDR_HIGH_IS_DARK = true;
const int     LDR_DAY   = 600;
const int     LDR_NIGHT = 700;

// Brightness (0..255)
const uint8_t BR_NIGHT_DEFAULT = 128;
const uint8_t BR_CAR_PRESENT   = 255;

// Fault detection
const uint32_t FAULT_HOLD_MS   = 800;  // how long sense must disagree while commanded ON
const uint8_t  MIN_CMD_ON      = 10;   // consider "commanded ON" only above this PWM

#if FB_IS_ANALOG
const int FB_ANALOG_THRESHOLD  = 150;  // ADC threshold for "LED really ON"
#endif

// Loop/LCD timing
const uint32_t LOOP_MS         = 100;
const uint32_t PRINT_EVERY_MS  = 1000;
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
const uint32_t LCD_PAGE_MS     = 1500;

// --------------- STATE ---------------
bool     isNight = false;
uint8_t  commandedPWM[N_LIGHTS] = {0};
bool     faultLatched[N_LIGHTS] = {false};
uint32_t faultStartMs[N_LIGHTS] = {0};
uint32_t lastPrint = 0;

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
uint32_t lastLcdFlip = 0;
uint8_t  lcdPage     = 0;  // 0=IR view, 1=Fault/Power

// ------------- HELPERS --------------
#if USE_SERIAL
  #define SPRINT(x)   Serial.print(x)
  #define SPRINTLN(x) Serial.println(x)
#else
  #define SPRINT(x)
  #define SPRINTLN(x)
#endif

int readLDR() { return analogRead(LDR_PIN); }

bool computeIsNight(int ldr) {
  if (LDR_HIGH_IS_DARK) {
    if (!isNight && ldr >= LDR_NIGHT) return true;
    if (isNight  && ldr <= LDR_DAY)   return false;
  } else {
    if (!isNight && ldr <= LDR_NIGHT) return true;
    if (isNight  && ldr >= LDR_DAY)   return false;
  }
  return isNight;
}

bool carPresent(uint8_t i) {
  return digitalRead(IR_PINS[i]) == LOW; // ACTIVE-LOW
}

void setLamp(uint8_t i, uint8_t pwm) {
  commandedPWM[i] = pwm;
  analogWrite(LED_PWM_PINS[i], pwm);
}

#if FB_IS_ANALOG
  bool senseGood(uint8_t i) {
    int v = analogRead(FB_ANALOG_PINS[i]);
    return v >= FB_ANALOG_THRESHOLD;
  }
#else
  bool senseGood(uint8_t i) {
    int v = digitalRead(FB_DIGITAL_PINS[i]);
    return FB_ACTIVE_HIGH ? (v == HIGH) : (v == LOW);
  }
#endif

void updateFaultState(uint8_t i, uint32_t nowMs) {
  const bool commandedOn = commandedPWM[i] > MIN_CMD_ON;
  const bool ok          = senseGood(i);

  if (faultLatched[i]) return;

  if (commandedOn && !ok) {
    if (faultStartMs[i] == 0) faultStartMs[i] = nowMs;
    if ((nowMs - faultStartMs[i]) >= FAULT_HOLD_MS) {
      faultLatched[i] = true;
      SPRINT("[FAULT] Lamp "); SPRINT(i); SPRINTLN(" latched");
    }
  } else {
    faultStartMs[i] = 0;
  }
}

char compactPwmDigit(uint8_t pwm) {
  if (pwm <= MIN_CMD_ON) return '0';
  if (pwm >= 220)        return '9';
  return '5';
}

// ---------- LCD helpers ----------
void lcdShowPageIR(int ldr) {
  lcd.setCursor(0,0);
  lcd.print(isNight ? "N " : "D ");
  lcd.print("LDR:");
  char buf[6];
  snprintf(buf, sizeof(buf), "%4d", ldr);
  lcd.print(buf);
  lcd.print("      ");

  lcd.setCursor(0,1);
  lcd.print("IR:");
  for (uint8_t i=0;i<N_LIGHTS;i++) lcd.print(carPresent(i) ? 'C' : '-');
  for (uint8_t i=0;i<LCD_COLS-3-N_LIGHTS;i++) lcd.print(' ');
}

void lcdShowPageFaultPower() {
  lcd.setCursor(0,0);
  lcd.print("F:");
  for (uint8_t i=0;i<N_LIGHTS;i++) lcd.print(faultLatched[i] ? 'X' : '.');
  for (uint8_t i=0;i<LCD_COLS-2-N_LIGHTS;i++) lcd.print(' ');

  lcd.setCursor(0,1);
  lcd.print("P:");
  for (uint8_t i=0;i<N_LIGHTS;i++) lcd.print(compactPwmDigit(commandedPWM[i]));
  for (uint8_t i=0;i<LCD_COLS-2-N_LIGHTS;i++) lcd.print(' ');
}

void lcdUpdate(uint32_t nowMs, int ldr) {
  if (nowMs - lastLcdFlip >= LCD_PAGE_MS) {
    lastLcdFlip = nowMs;
    lcdPage ^= 1;
    lcd.clear();
  }
  if (lcdPage == 0) lcdShowPageIR(ldr);
  else              lcdShowPageFaultPower();
}

// ------------- SETUP / LOOP -------------
void setup() {
#if USE_SERIAL
  Serial.begin(115200);
  delay(200);
#endif

  // IO config
  for (uint8_t i=0;i<N_LIGHTS;i++) {
    pinMode(LED_PWM_PINS[i], OUTPUT);
    analogWrite(LED_PWM_PINS[i], 0);
    pinMode(IR_PINS[i], INPUT_PULLUP);
  }

#if FB_IS_ANALOG
  for (uint8_t i=0;i<N_LIGHTS;i++) pinMode(FB_ANALOG_PINS[i], INPUT);
#else
  for (uint8_t i=0;i<N_LIGHTS;i++) pinMode(FB_DIGITAL_PINS[i], INPUT); // or INPUT_PULLUP if open-collector
#endif

  // LCD
  Wire.begin();         // A4/A5
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Street Light Sys");
  lcd.setCursor(0,1); lcd.print("Booting...");

  // Start mode from LDR
  int ldr = readLDR();
  isNight = LDR_HIGH_IS_DARK ? (ldr >= LDR_NIGHT) : (ldr <= LDR_NIGHT);

  lcd.clear();
  lcdShowPageIR(ldr);
}

void loop() {
  static uint32_t lastLoop = 0;
  const uint32_t now = millis();
  if (now - lastLoop < LOOP_MS) return;
  lastLoop = now;

  // Day/Night hysteresis
  int ldr = readLDR();
  bool newNight = computeIsNight(ldr);
  if (newNight != isNight) {
    isNight = newNight;
  }

  // Set targets
  for (uint8_t i=0;i<N_LIGHTS;i++) {
    uint8_t target = isNight ? (carPresent(i) ? BR_CAR_PRESENT : BR_NIGHT_DEFAULT) : 0;
    // If you prefer to force off on fault, uncomment:
    // if (faultLatched[i]) target = 0;
    setLamp(i, target);
  }

  // Fault logic
  for (uint8_t i=0;i<N_LIGHTS;i++) updateFaultState(i, now);

  // LCD
  lcdUpdate(now, ldr);
}
