#include <Wire.h>
#include "ACS712.h"
#include <ZMPT101B.h>
#include <LiquidCrystal_I2C.h>

// ───── PIN & HARDWARE SETTINGS ──────────────────────────────────────────
// Analog inputs
#define CURRENT_SENSOR_PIN    A1    // ACS712 output
#define VOLTAGE_SENSOR_PIN    A0    // ZMPT101B output

// Relay driver (active LOW)
#define RELAY_PIN             7

// ───── SENSOR CALIBRATION ────────────────────────────────────────────────
// ACS712 sensitivity (mV per A)
#define ACS_SENSITIVITY_MV_PER_A   66.0   // 20 A module = 100 mV/A

// ZMPT101B line frequency & sensitivity
#define LINE_FREQUENCY_HZ          50.0
#define ZMPT101B_SENSITIVITY       551.0f   // from your calibrate sketch

// ───── SYSTEM SETTINGS ─────────────────────────────────────────────────
#define SAMPLE_COUNT               50       // samples per reading
#define POWER_THRESHOLD_W          0.4     // watts; adjust to your normal max

// ───── OBJECTS ───────────────────────────────────────────────────────────
ACS712      acs(CURRENT_SENSOR_PIN, 5.0, 1023, ACS_SENSITIVITY_MV_PER_A);
ZMPT101B    voltSensor(VOLTAGE_SENSOR_PIN, LINE_FREQUENCY_HZ);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I²C addr 0x27, 16 cols × 2 rows

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // ACS712
  acs.autoMidPoint();
  Serial.print("ACS712 midPoint: "); Serial.println(acs.getMidPoint());
  Serial.print("ACS712 noise mV: ");   Serial.println(acs.getNoisemV());

  // ZMPT101B
  voltSensor.setSensitivity(ZMPT101B_SENSITIVITY);

  // Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // relay off (active LOW)

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Power Monitor");
  delay(1000);
}

void loop() {
  float sumI = 0, sumV = 0;

  // Take SAMPLE_COUNT readings
  for (uint16_t i = 0; i < SAMPLE_COUNT; i++) {
    sumI += acs.mA_AC();
    sumV += voltSensor.getRmsVoltage();
    delay(10);
  }

  float Irms   = (sumI / SAMPLE_COUNT) / 1000.0;  // A
  float Vrms   =  sumV / SAMPLE_COUNT;            // V
  float powerW =  Vrms * Irms;                    // W

  // Serial log
  Serial.print("Irms: ");   Serial.print(Irms,3);   Serial.print(" A, ");
  Serial.print("Vrms: ");   Serial.print(Vrms,1);   Serial.print(" V, ");
  Serial.print("Power: ");  Serial.print(powerW,1); Serial.println(" W");

  bool theft = (powerW > POWER_THRESHOLD_W);
  if (theft) {
    digitalWrite(RELAY_PIN, HIGH);   // activate relay
    Serial.println(">>> THEFT DETECTED! Relay ACTIVE");
  } else {
    digitalWrite(RELAY_PIN, LOW);  // deactivate relay
    Serial.println("Normal load – relay OFF.");
  }
  Serial.println();

  // Display on I2C LCD
  lcd.clear();
  // Line 0: power & current
  lcd.setCursor(0,0);
  lcd.print("P:");
  lcd.print(powerW,1);
  lcd.print("W I:");
  lcd.print(Irms,);
  lcd.print("A");

  // Line 1: voltage & status
  lcd.setCursor(0,1);
  lcd.print("V:");
  lcd.print(Vrms,1);
  lcd.print("V ");
  lcd.print(theft ? "ALARM" : "OK");

  delay(1000);
}
