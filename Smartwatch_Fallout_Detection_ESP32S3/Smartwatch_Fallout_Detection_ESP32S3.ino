  /**************************************************************************
    Example: ESP32-S3 Fall Detection with QMI8658 (FastIMU) and LEDC Buzzer
    ------------------------------------------------------------------------
    - Uses threshold-based free-fall detection
    - Buzzer on GPIO 33 with LEDC PWM (for a passive buzzer)

    Requirements:
    1) Arduino-ESP32 core v3.0.0 or newer (select ESP32 board in Arduino IDE).
    2) FastIMU library installed (for QMI8658).
    3) A passive buzzer (needs a PWM tone). If you have an active buzzer,
      you can just do digitalWrite() instead of LEDC.

    Wiring (example):
    - QMI8658 is onboard or I2C: SDA=GPIO 11, SCL=GPIO 10. Adjust as needed.
    - Buzzer on GPIO 33 to buzzer +, buzzer - to GND.

    Tuning:
    - FREE_FALL_THRESHOLD (g), IMPACT_THRESHOLD (g), and FREE_FALL_TIME_MS
      will need real-world testing to get reliable detection.

  **************************************************************************/

  #include <Arduino.h>
  #include "FastIMU.h"
  #include <Wire.h>

  // -------------------- IMU Setup --------------------
  #define IMU_ADDRESS 0x6B  // QMI8658 I2C address (per your code)
  QMI8658 IMU;              // FastIMU object

  calData calib = {0};
  AccelData accelData;
  GyroData gyroData;
  MagData  magData;

  // -------------------- I2C Pins (Adjust as needed) --------------------
  #define SDA_PIN 11
  #define SCL_PIN 10

  // -------------------- Buzzer Setup (Passive) --------------------
  #define BUZZER_PIN 42
  // We'll generate a 1 kHz tone, 8-bit resolution (duty range 0..255)
  #define BUZZER_FREQ  1000
  #define BUZZER_RES   8  

  // -------------------- Fall Detection Parameters --------------------
  // You can experiment with these values
  const float FREE_FALL_THRESHOLD    = 0.3; // G  (acc < 0.3 => free fall)
  const unsigned long FREE_FALL_TIME_MS = 200; 
  const float IMPACT_THRESHOLD       = 2.0; // G  (acc > 2.0 => impact)

  bool freeFallDetected     = false;
  unsigned long freeFallStartTime = 0;

  // -------------------------------------------------------------------
  void setup() {
    Serial.begin(115200);
    while (!Serial) {;}

    // 1) Initialize I2C for the IMU
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000); // 400kHz

    // 2) Initialize IMU
    int err = IMU.init(calib, IMU_ADDRESS);
    if (err != 0) {
      Serial.print("Error initializing IMU: ");
      Serial.println(err);
      while (true) { delay(10); }
    }

    // 3) (Optional) set range for accel/gyro if desired
    // err = IMU.setGyroRange(500); // ±500 dps
    // err = IMU.setAccelRange(2);  // ±2g
    if (err != 0) {
      Serial.print("Error setting range: ");
      Serial.println(err);
      while (true) { delay(10); }
    }

    // 4) Setup LEDC for the buzzer (new 3.x API)
    // This "attaches" a PWM channel automatically to BUZZER_PIN
    ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RES);

    // Initially set duty to 0 (off)
    ledcWrite(BUZZER_PIN, 0);

    Serial.println("Setup complete. Starting fall detection + buzzer test...");
  }

  void loop() {
    // 1) Update and read current sensor data
    IMU.update();
    IMU.getAccel(&accelData);
    IMU.getGyro(&gyroData);
    if (IMU.hasMagnetometer()) {
      IMU.getMag(&magData);
    }

    // 2) Compute overall acceleration magnitude (assuming FastIMU returns G)
    float ax = accelData.accelX;
    float ay = accelData.accelY;
    float az = accelData.accelZ;
    float magnitude = sqrt(ax*ax + ay*ay + az*az);
    Serial.print(magnitude);
    Serial.println(" ");
    // 3) Check free-fall
    if (magnitude < FREE_FALL_THRESHOLD) {
      if (!freeFallDetected) {
        freeFallDetected = true;
        freeFallStartTime = millis();
      }
    }

    // 4) If we've been in free-fall for some time, check impact
    if (freeFallDetected && (millis() - freeFallStartTime > FREE_FALL_TIME_MS)) {
      if (magnitude > IMPACT_THRESHOLD) {
        Serial.println("FALL DETECTED! Activating buzzer...");
        // Generate a tone for ~1 second
        beepBuzzer(1000); // beep for 1 second

        // Reset fall detection
        freeFallDetected = false;
      }
      // If no spike and acceleration is back above freeFallThreshold, reset
      else if (magnitude > FREE_FALL_THRESHOLD) {
        freeFallDetected = false;
      }
    }

    delay(50);
  }

  // -------------------- Helper Function: beepBuzzer() --------------------
  // beepDurationMs: how long (ms) to keep the tone playing
  void beepBuzzer(unsigned long beepDurationMs) {
    // For an 8-bit resolution, duty goes 0..255. 
    // e.g., 128 is ~50% duty => fairly loud
    ledcWrite(BUZZER_PIN, 128);   // start tone
    delay(beepDurationMs); 
    ledcWrite(BUZZER_PIN, 0);     // stop tone
  }
