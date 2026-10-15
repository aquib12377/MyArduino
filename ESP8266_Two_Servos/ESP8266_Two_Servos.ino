#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pca(0x40);

// ESP8266 NodeMCU / Wemos D1 mini
const uint8_t SDA_PIN = 4;  // D2
const uint8_t SCL_PIN = 5;  // D1

const uint8_t SERVO_1_CHANNEL = 0;
const uint8_t SERVO_2_CHANNEL = 1;

const uint16_t SERVO_FREQUENCY = 50;

// Starting pulse limits: adjust for your servo model.
// These represent nominal 0 and 180 degrees.
const uint16_t SERVO_MIN_US = 1000;
const uint16_t SERVO_MAX_US = 2000;

const int MIN_ANGLE = 0;
const int MAX_ANGLE = 180;

// Larger value = slower movement
const uint16_t STEP_DELAY_MS = 2;

void moveBothServos(int angle) {
  angle = constrain(angle, MIN_ANGLE, MAX_ANGLE);

  // Map against 180 degrees so 120 stays at nominal 120.
  long pulseUs = map(
    angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US
  );

  uint16_t ticks =
    (uint16_t)(pulseUs * SERVO_FREQUENCY * 4096UL / 1000000UL);

  pca.setPWM(SERVO_1_CHANNEL, 0, ticks);
  pca.setPWM(SERVO_2_CHANNEL, 0, ticks);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!pca.begin()) {
    Serial.println("PCA9685 not found. Check wiring and address.");
    while (true) {
      delay(1000);
    }
  }

  // Disable both servo signals during the startup wait.
  pca.setPWM(SERVO_1_CHANNEL, 0, 4096);
  pca.setPWM(SERVO_2_CHANNEL, 0, 4096);

  pca.setPWMFreq(SERVO_FREQUENCY);

  Serial.println("Waiting 5 seconds...");
  delay(5000);

  Serial.println("Starting continuous movement.");
}

void loop() {
  // Both servos: 0 -> 120 degrees
  for (int angle = MIN_ANGLE; angle <= MAX_ANGLE; angle++) {
    moveBothServos(angle);
    delay(STEP_DELAY_MS);
  }

  // Both servos: 120 -> 0 degrees
  for (int angle = MAX_ANGLE - 1; angle > MIN_ANGLE; angle--) {
    moveBothServos(angle);
    delay(STEP_DELAY_MS);
  }
}