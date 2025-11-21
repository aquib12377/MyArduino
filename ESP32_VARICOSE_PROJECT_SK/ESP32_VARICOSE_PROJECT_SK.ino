/*
  STM32 Nucleo + 3x LM35 + MAX30100 + ADXL345 + Relay (Arduino IDE / STM32Duino)

  - LM35 on A0, A1, A2 (10 mV/°C), ADC is 12-bit with Vref=3.3V (VDDA)
  - MAX30100 via I2C: heart rate + SpO2 (non-blocking with pox.update())
  - ADXL345 via I2C: acceleration (m/s^2)
  - Relay: turns ON if ANY LM35 > 35°C (with hysteresis & min-switch interval)
*/

#ifndef ARDUINO_ARCH_STM32
#warning "This sketch targets STM32 (STM32Duino core)."
#endif

#include <Wire.h>

// ---- ADXL345 (Adafruit) ----
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// ---- MAX30100 (OXullo lib) ----
#include "MAX30100_PulseOximeter.h"
PulseOximeter pox;

#define REPORTING_PERIOD_MS 1000

// Beat callback (optional)
void onBeatDetected() {
  // Serial.println("Beat!");  // uncomment if you want beat notifications
}

// ---- LM35 config ----
const uint8_t LM_PINS[] = { A0, A1, A2 };
const size_t  LM_COUNT  = sizeof(LM_PINS) / sizeof(LM_PINS[0]);

const float    ADC_REF_V = 3.3f;    // STM32 ADC reference (VDDA), NOT sensor supply
const uint16_t ADC_MAX   = 4095;    // 12-bit
const uint8_t  SAMPLES   = 8;       // per-channel averaging (kept modest to keep pox.update() frequent)
const uint16_t SAMPLE_DELAY_MS = 2; // delay between LM35 samples

// Optional per-sensor °C offsets for calibration
float CAL_OFFSET_C[3] = { 0.0f, 0.0f, 0.0f };

// ---- Relay control (ACTIVE relay module) ----
const uint8_t RELAY_PIN = D7;        // change to your pin
const bool    RELAY_ACTIVE_HIGH = true; // true: HIGH=ON; false: LOW=ON
const float   TEMP_THRESHOLD_C = 35.0f; // turn ON above this
const float   TEMP_HYSTERESIS_C = 1.5f; // turn OFF only after dropping below (35 - 1.5)
const uint32_t MIN_SWITCH_INTERVAL_MS = 2000; // debounce rapid toggles

// ---- Schedulers ----
const uint32_t PRINT_PERIOD_MS = 500;
const uint32_t LM35_PERIOD_MS  = 250;
const uint32_t ADXL_PERIOD_MS  = 200;

uint32_t lastPrintMs = 0;
uint32_t lastLmMs    = 0;
uint32_t lastAdxlMs  = 0;
uint32_t tsLastHR    = 0;

// Latest readings
float lmC[3] = {NAN, NAN, NAN};
float adxlX = NAN, adxlY = NAN, adxlZ = NAN;
float hrBpm = NAN, spo2Pct = NAN;

// Relay state tracking
bool relayOn = false;
uint32_t lastRelayChangeMs = 0;

// ---------- Helpers ----------
void displaySensorDetails(void) {
  sensor_t sensor;
  accel.getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.print  (F("Sensor:       ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:   ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:    ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:    ")); Serial.print(sensor.max_value); Serial.println(F(" m/s^2"));
  Serial.print  (F("Min Value:    ")); Serial.print(sensor.min_value); Serial.println(F(" m/s^2"));
  Serial.print  (F("Resolution:   ")); Serial.print(sensor.resolution); Serial.println(F(" m/s^2"));
  Serial.println(F("------------------------------------\n"));
  delay(250);
}

void displayDataRate(void) {
  Serial.print(F("Data Rate:    "));
  switch (accel.getDataRate()) {
    case ADXL345_DATARATE_3200_HZ: Serial.print("3200 "); break;
    case ADXL345_DATARATE_1600_HZ: Serial.print("1600 "); break;
    case ADXL345_DATARATE_800_HZ:  Serial.print("800 ");  break;
    case ADXL345_DATARATE_400_HZ:  Serial.print("400 ");  break;
    case ADXL345_DATARATE_200_HZ:  Serial.print("200 ");  break;
    case ADXL345_DATARATE_100_HZ:  Serial.print("100 ");  break;
    case ADXL345_DATARATE_50_HZ:   Serial.print("50 ");   break;
    case ADXL345_DATARATE_25_HZ:   Serial.print("25 ");   break;
    case ADXL345_DATARATE_12_5_HZ: Serial.print("12.5 "); break;
    case ADXL345_DATARATE_6_25HZ:  Serial.print("6.25 "); break;
    case ADXL345_DATARATE_3_13_HZ: Serial.print("3.13 "); break;
    case ADXL345_DATARATE_1_56_HZ: Serial.print("1.56 "); break;
    case ADXL345_DATARATE_0_78_HZ: Serial.print("0.78 "); break;
    case ADXL345_DATARATE_0_39_HZ: Serial.print("0.39 "); break;
    case ADXL345_DATARATE_0_20_HZ: Serial.print("0.20 "); break;
    case ADXL345_DATARATE_0_10_HZ: Serial.print("0.10 "); break;
    default:                       Serial.print("???? "); break;
  }
  Serial.println(F("Hz"));
}

void displayRange(void) {
  Serial.print(F("Range:         +/- "));
  switch (accel.getRange()) {
    case ADXL345_RANGE_16_G: Serial.print("16 "); break;
    case ADXL345_RANGE_8_G:  Serial.print("8 ");  break;
    case ADXL345_RANGE_4_G:  Serial.print("4 ");  break;
    case ADXL345_RANGE_2_G:  Serial.print("2 ");  break;
    default:                 Serial.print("?? "); break;
  }
  Serial.println(F("g\n"));
}

float readLM35Celsius(uint8_t pin, float calOffsetC) {
  // Throwaway read to settle sampling cap after channel switch
  (void)analogRead(pin);
  delayMicroseconds(80);

  uint32_t sum = 0;
  for (uint8_t i = 0; i < SAMPLES; i++) {
    sum += analogRead(pin);
    delay(SAMPLE_DELAY_MS);
  }
  const float adc = sum / float(SAMPLES);
  const float v_out = (adc * ADC_REF_V) / float(ADC_MAX);
  return (v_out / 0.010f) + calOffsetC;  // 10 mV/°C
}

// Decide whether relay should be ON given current temps (with hysteresis)
bool computeRelayTarget(float t0, float t1, float t2, bool currentlyOn) {
  const float maxT = max(t0, max(t1, t2));

  if (currentlyOn) {
    // Turn OFF only after dropping below threshold - hysteresis
    if (maxT <= (TEMP_THRESHOLD_C - TEMP_HYSTERESIS_C)) {
      return false;
    }
    return true;  // stay ON
  } else {
    // Turn ON when exceeding threshold
    if (maxT > TEMP_THRESHOLD_C) {
      return true;
    }
    return false; // stay OFF
  }
}

void applyRelay(bool on) {
  const bool level = RELAY_ACTIVE_HIGH ? on : !on;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
}

// ---------- Setup ----------
void setup() {
  // Serial
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println(F("\nSTM32 + 3x LM35 + MAX30100 + ADXL345 + Relay"));
  Serial.println(F("LM35 on A0/A1/A2, ADC 12-bit, Vref=3.3V"));
  Serial.println(F("I2C: MAX30100 (3.3V only), ADXL345"));

  // ADC (LM35)
  analogReadResolution(12);
#ifdef INPUT_ANALOG
  for (size_t i = 0; i < LM_COUNT; i++) pinMode(LM_PINS[i], INPUT_ANALOG);
#endif

  // Relay pin
  pinMode(RELAY_PIN, OUTPUT);
  applyRelay(false); // start OFF

  // I2C
  Wire.begin();                 // Uses default SDA/SCL for your Nucleo
  // Wire.setClock(100000);     // (optional) slow bus if you have long wires

 

  // ADXL345
  Serial.print(F("Initializing ADXL345... "));
  if (!accel.begin()) {
    Serial.println(F("FAILED (check wiring / address 0x53)"));
  } else {
    Serial.println(F("OK"));
    accel.setRange(ADXL345_RANGE_2_G);          // best resolution for human motion
    accel.setDataRate(ADXL345_DATARATE_100_HZ); // responsive and smooth
    // Show details once
    displaySensorDetails();
    displayDataRate();
    displayRange();
  }
   // MAX30100
  Serial.print(F("Initializing MAX30100... "));
  if (!pox.begin()) {
    Serial.println(F("FAILED (check 3.3V power & I2C wiring)"));
  } else {
    Serial.println(F("OK"));
    pox.setIRLedCurrent(MAX30100_LED_CURR_27_1MA); // adjust if needed
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }
}

// ---------- Loop ----------
void loop() {
  const uint32_t now = millis();

  // MAX30100 must be called very frequently
  pox.update();

  // Update LM35s on schedule
  if (now - lastLmMs >= LM35_PERIOD_MS) {
    for (size_t i = 0; i < LM_COUNT; i++) {
      const float cal = (i < (sizeof(CAL_OFFSET_C) / sizeof(CAL_OFFSET_C[0]))) ? CAL_OFFSET_C[i] : 0.0f;
      lmC[i] = readLM35Celsius(LM_PINS[i], cal);
    }
    lastLmMs = now;

    // ----- Relay decision (right after fresh temperatures) -----
    const bool targetOn =  computeRelayTarget(lmC[0], lmC[1], lmC[2], relayOn);
    if (targetOn != relayOn && (now - lastRelayChangeMs) >= MIN_SWITCH_INTERVAL_MS) {
      relayOn = targetOn;
      applyRelay(relayOn);
      lastRelayChangeMs = now;

      Serial.print(F("Relay -> "));
      Serial.println(relayOn ? F("ON (temp high)") : F("OFF (temp normal)"));
    }
  }

  // Update ADXL345 on schedule
  if (now - lastAdxlMs >= ADXL_PERIOD_MS) {
    sensors_event_t aevent;
    if (accel.getEvent(&aevent)) {
      adxlX = aevent.acceleration.x;
      adxlY = aevent.acceleration.y;
      adxlZ = aevent.acceleration.z;
    }
    lastAdxlMs = now;
  }

  // Grab HR/SpO2 snapshot about once per second
  if (now - tsLastHR >= REPORTING_PERIOD_MS) {
    hrBpm   = pox.getHeartRate();  // NaN/0 until a valid lock
    spo2Pct = pox.getSpO2();
    tsLastHR = now;
  }

  // Print combined line
  if (now - lastPrintMs >= PRINT_PERIOD_MS) {
    Serial.print(F("LM35C: "));
    Serial.print(lmC[0], 2); Serial.print(F(", "));
    Serial.print(lmC[1], 2); Serial.print(F(", "));
    Serial.print(lmC[2], 2);

    Serial.print(F("  |  HR: "));
    if (isnan(hrBpm) || hrBpm <= 0) Serial.print(F("--"));
    else                            Serial.print(hrBpm, 1);

    Serial.print(F(" bpm  |  SpO2: "));
    if (isnan(spo2Pct) || spo2Pct <= 0) Serial.print(F("--"));
    else                                Serial.print(spo2Pct, 1);
    Serial.print(F(" %"));

    Serial.print(F("  |  Accel (m/s^2): X="));
    if (isnan(adxlX)) Serial.print(F("--")); else Serial.print(adxlX, 2);
    Serial.print(F(" Y="));
    if (isnan(adxlY)) Serial.print(F("--")); else Serial.print(adxlY, 2);
    Serial.print(F(" Z="));
    if (isnan(adxlZ)) Serial.print(F("--")); else Serial.print(adxlZ, 2);

    Serial.print(F("  |  Relay: "));
    Serial.println(relayOn ? F("ON") : F("OFF"));

    lastPrintMs = now;
  }
}
