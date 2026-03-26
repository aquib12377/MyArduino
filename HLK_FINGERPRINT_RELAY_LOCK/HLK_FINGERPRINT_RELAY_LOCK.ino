#include <FPM383C.h>

#define RELAY_PIN 7

FPM383C fingerprint(2, 11, 3);

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  fingerprint.begin();
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
  Serial.println("Ready - place finger on sensor");
}

void loop() {
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);

  FingerprintMatchResult result = fingerprint.matchSync();
  uint8_t err = fingerprint.getLastError();

  if (err == FP_ERROR_SUCCESS) {
    Serial.print(">> MATCHED! ID=");
    Serial.print(result.fingerprintId);
    Serial.print(" Score=");
    Serial.println(result.matchScore);

    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
    digitalWrite(RELAY_PIN, LOW);
    Serial.println(">> RELAY ON");
    delay(10000);
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println(">> RELAY OFF");
  } else {
    Serial.println(">> No match");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 10, 10, 2);
    delay(1000);
  }

  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}