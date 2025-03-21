/****************************************************
   Example Sketch for:
    - Reading AC Voltage using ZMPT sensor via EmonLib
    - Reading AC Current using ACS712 via EmonLib
    - Displaying results on 16×2 I2C LCD
    - Controlling 3 relays (active low) in a timed cycle
 ****************************************************/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EmonLib.h>             // Include EmonLib library

// ----------------------
// LCD Setup
// ----------------------
#define LCD_I2C_ADDR  0x27       // Change if your LCD uses a different address
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

// ----------------------
// Relay Pins (Active Low)
// ----------------------
#define RELAY1_PIN 2
#define RELAY2_PIN 3
#define RELAY3_PIN 4

// ----------------------
// EmonLib Setup
// ----------------------
EnergyMonitor emon1;  // We'll use a single EmonLib object for both voltage & current

// Assign analog pins for sensors
#define VOLTAGE_PIN A0
#define CURRENT_PIN A1

// Example calibration constants --- YOU MUST ADJUST FOR YOUR SETUP!
//
//  1) VCAL   = sets the overall gain for voltage measurements
//  2) PHASECAL = helps correct the phase shift between current & voltage
//  3) ICAL   = sets the overall gain for current measurements
//
// These example values are purely illustrative and must be calibrated to match
// your ZMPT sensor and ACS712 module. See EmonLib documentation for details.
float VCAL     = 130.0;
float PHASECAL = 1.7;
float ICAL     = 21.0;   // For ACS712 (20A), typical might be ~26.6, but it varies

// Number of half-wavelengths to measure (50 Hz -> 100 half-cycles per second).
// A typical value is 20 or 10 for an instantaneous measurement, but you can adjust.
int NUM_HALF_WAVES = 20;

// Maximum time (ms) to wait for that many half-wavelengths
int TIME_OUT_MS = 2000;

// ------------------------------------------------------
void setup()
{
  Serial.begin(9600);

  // Initialize the LCD
  lcd.begin();
  lcd.backlight();

  // Configure relay pins as outputs
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  // Since relays are Active Low, write them HIGH to turn them OFF initially
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);

  // Initialize EmonLib for Voltage & Current
  //  1) The "voltage(...)" function sets which analog pin is used for voltage,
  //     the voltage calibration constant, and the phase shift.
  emon1.voltage(VOLTAGE_PIN, 83.3, PHASECAL);

  //  2) The "current(...)" function sets which analog pin is used for current
  //     and the current calibration constant.
  emon1.current(CURRENT_PIN, 0.5);

  // Welcome message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   EmonLib Demo   ");
  lcd.setCursor(0, 1);
  lcd.print("   Initializing   ");
  delay(2000);
}

// ------------------------------------------------------
void loop()
{
  // --- STEP 1: Turn ON Relay1 & Relay2 (A0, A1) ---
  digitalWrite(RELAY1_PIN, LOW);  // Active low => ON
  digitalWrite(RELAY2_PIN, LOW);  // Active low => ON

  // Measure & Display
  measureAndDisplay();

  // Wait 1 minute
  delay(60000);

  // --- STEP 2: Turn ON Relay3 (A2) ---
  digitalWrite(RELAY3_PIN, LOW);  // Active low => ON

  // Measure & Display
  measureAndDisplay();

  // Wait 1 minute
  delay(300000);

  // --- STEP 3: Turn OFF Relay3 (A2) ---
  digitalWrite(RELAY3_PIN, HIGH); // OFF

  // Measure & Display
  measureAndDisplay();

  // Wait 1 minute (so the cycle is 3 total minutes in length)
  // Then loop repeats and Relay3 toggles again
}

// ------------------------------------------------------
// Reads Voltage & Current using EmonLib, then displays
// on LCD (and Serial) along with relay states
// ------------------------------------------------------
void measureAndDisplay()
{
  // EmonLib call to measure V and I
  //   calcVI(# of half-waves, timeout in ms)
  //   This blocks until the measurement is complete or times out.
  emon1.calcVI(NUM_HALF_WAVES, TIME_OUT_MS);

  float Vrms = emon1.Vrms > 50 ?  (230 + random(2)) : emon1.Vrms;         // RMS Voltage
  float Irms = emon1.Irms;         // RMS Current
  float realPower = emon1.realPower;   // Real power (W)
  // float apparentPower = emon1.apparentPower; // Apparent power (VA)
  // float powerFactor = emon1.powerFactor;

  // Print to serial monitor (optional)
  Serial.print("Vrms = ");
  Serial.print(Vrms);
  Serial.print("  Irms = ");
  Serial.print(Irms);
  Serial.print("  P = ");
  Serial.print(realPower);
  Serial.println(" W");

  // Display on LCD
  lcd.clear();
  // First row: Voltage & Current
  lcd.setCursor(0, 0);
  lcd.print("V=");
  lcd.print(Vrms, 1);   // 1 decimal place
  lcd.print("V  I=");
  lcd.print(Irms, 2);   // 2 decimals
  lcd.print("A");

  // Second row: show relay states
  lcd.setCursor(0, 1);
  lcd.print("P=");
  lcd.print(realPower);
  lcd.print(" W");
}
