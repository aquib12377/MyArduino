#include <Arduino.h>

// Classic ESP32: GPIO34
// ESP32-S3 N16R8: GPIO4
const uint8_t TDS_PIN = 34;

const int SAMPLE_COUNT = 31;

// Manually entered temperature; no temperature sensor connected.
const float WATER_TEMP_C = 25.0f;

// Adjust only after checking against a known TDS calibration solution.
const float CALIBRATION_FACTOR = 1.0f;

unsigned long readingNumber = 0;

void setup() {
  Serial.begin(115200);
  pinMode(TDS_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(TDS_PIN, ADC_11db);

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("       ESP32 DETAILED TDS TEST");
  Serial.println("========================================");
  Serial.print("Sensor GPIO        : ");
  Serial.println(TDS_PIN);
  Serial.println("ADC resolution     : 12 bits (0-4095)");
  Serial.println("Temperature source : Manual setting");
  Serial.println("Voltage source     : analogReadMilliVolts()");
  Serial.println("Raw ADC and millivolts use separate ADC reads.");
  Serial.println("========================================");
}

void loop() {
  uint16_t rawSamples[SAMPLE_COUNT];
  uint32_t mvSamples[SAMPLE_COUNT];

  uint32_t rawTotal = 0;
  uint32_t mvTotal = 0;

  uint16_t rawMin = 4095;
  uint16_t rawMax = 0;

  uint32_t mvMin = UINT32_MAX;
  uint32_t mvMax = 0;

  // Collect raw ADC and calibrated millivolt readings.
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    rawSamples[i] = analogRead(TDS_PIN);
    mvSamples[i] = analogReadMilliVolts(TDS_PIN);

    rawTotal += rawSamples[i];
    mvTotal += mvSamples[i];

    if (rawSamples[i] < rawMin) rawMin = rawSamples[i];
    if (rawSamples[i] > rawMax) rawMax = rawSamples[i];

    if (mvSamples[i] < mvMin) mvMin = mvSamples[i];
    if (mvSamples[i] > mvMax) mvMax = mvSamples[i];

    delay(10);
  }

  // Sort a copy of voltage samples for median filtering.
  uint32_t sortedMv[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sortedMv[i] = mvSamples[i];
  }

  for (int i = 1; i < SAMPLE_COUNT; i++) {
    uint32_t value = sortedMv[i];
    int j = i - 1;

    while (j >= 0 && sortedMv[j] > value) {
      sortedMv[j + 1] = sortedMv[j];
      j--;
    }

    sortedMv[j + 1] = value;
  }

  float averageRaw = rawTotal / (float)SAMPLE_COUNT;
  float averageMv = mvTotal / (float)SAMPLE_COUNT;
  uint32_t medianMv = sortedMv[SAMPLE_COUNT / 2];

  float voltage = medianMv / 1000.0f;

  float compensationCoefficient =
      1.0f + 0.02f * (WATER_TEMP_C - 25.0f);

  float compensatedVoltage =
      voltage / compensationCoefficient;

  float tdsBeforeCalibration =
      (133.42f * compensatedVoltage *
                    compensatedVoltage *
                    compensatedVoltage
       - 255.86f * compensatedVoltage *
                     compensatedVoltage
       + 857.39f * compensatedVoltage) * 0.5f;

  if (tdsBeforeCalibration < 0.0f) {
    tdsBeforeCalibration = 0.0f;
  }

  float finalTds =
      tdsBeforeCalibration * CALIBRATION_FACTOR;

  readingNumber++;

  Serial.println();
  Serial.println("========================================");
  Serial.print("READING #");
  Serial.println(readingNumber);

  Serial.print("Uptime              : ");
  Serial.print(millis() / 1000.0f, 1);
  Serial.println(" seconds");

  Serial.print("Sample count        : ");
  Serial.println(SAMPLE_COUNT);

  Serial.println();
  Serial.println("--- Individual readings ---");
  Serial.println("Sample\tRaw ADC\tVoltage (mV)");

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    Serial.print(i + 1);
    Serial.print('\t');
    Serial.print(rawSamples[i]);
    Serial.print('\t');
    Serial.println(mvSamples[i]);
  }

  Serial.println();
  Serial.println("--- Raw ADC statistics ---");

  Serial.print("Minimum ADC         : ");
  Serial.println(rawMin);

  Serial.print("Maximum ADC         : ");
  Serial.println(rawMax);

  Serial.print("Average ADC         : ");
  Serial.println(averageRaw, 2);

  Serial.print("ADC spread          : ");
  Serial.println(rawMax - rawMin);

  Serial.println();
  Serial.println("--- Sensor voltage ---");

  Serial.print("Minimum voltage     : ");
  Serial.print(mvMin);
  Serial.println(" mV");

  Serial.print("Maximum voltage     : ");
  Serial.print(mvMax);
  Serial.println(" mV");

  Serial.print("Average voltage     : ");
  Serial.print(averageMv, 2);
  Serial.println(" mV");

  Serial.print("Median voltage      : ");
  Serial.print(medianMv);
  Serial.println(" mV");

  Serial.print("Voltage spread      : ");
  Serial.print(mvMax - mvMin);
  Serial.println(" mV");

  Serial.print("Voltage used        : ");
  Serial.print(voltage, 4);
  Serial.println(" V");

  Serial.println();
  Serial.println("--- Temperature compensation ---");

  Serial.print("Assumed water temp  : ");
  Serial.print(WATER_TEMP_C, 2);
  Serial.println(" C");

  Serial.print("Compensation factor : ");
  Serial.println(compensationCoefficient, 4);

  Serial.print("Compensated voltage : ");
  Serial.print(compensatedVoltage, 4);
  Serial.println(" V");

  Serial.println();
  Serial.println("--- TDS result ---");

  Serial.print("Before calibration  : ");
  Serial.print(tdsBeforeCalibration, 2);
  Serial.println(" ppm");

  Serial.print("Calibration factor  : ");
  Serial.println(CALIBRATION_FACTOR, 4);

  Serial.print("FINAL TDS           : ");
  Serial.print(finalTds, 2);
  Serial.println(" ppm");

  if (rawMax >= 4090) {
    Serial.println("CHECK: ADC is near its maximum; check signal voltage.");
  }

  if (medianMv < 10) {
    Serial.println("CHECK: Very low signal; check probe and wiring.");
  }

  if (finalTds > 1000.0f) {
    Serial.println("CHECK: Above SEN0244 nominal 0-1000 ppm range.");
  }

  Serial.println("========================================");

  delay(1700);
}