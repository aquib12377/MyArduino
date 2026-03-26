/*
 * ================================================================
 *  FINGERPRINT ENROLMENT UTILITY  (run once per finger)
 * ================================================================
 *  Upload this to the ESP32-CAM temporarily to register new
 *  fingerprints.  Open Serial Monitor at 115200, follow prompts.
 *  After enrolling, re-upload the main ESP32CAM_FaceFinger.ino.
 *
 *  Wiring: same as main sketch (R307 on GPIO 14/15)
 * ================================================================
 */

#include <Adafruit_Fingerprint.h>

#define FP_RX_PIN 14
#define FP_TX_PIN 15

HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);

uint8_t nextID = 1;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Fingerprint Enrolment ===");

  fpSerial.begin(57600, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  finger.begin(57600);

  if (!finger.verifyPassword()) {
    Serial.println("Sensor not found! Check wiring.");
    while (true) delay(1000);
  }

  finger.getParameters();
  Serial.printf("Sensor capacity: %d\n", finger.capacity);
  Serial.printf("Already enrolled: %d\n", finger.templateCount);

  nextID = finger.templateCount + 1;
  Serial.printf("\nReady to enroll as ID #%d\n", nextID);
  Serial.println("Place your finger on the sensor...\n");
}

uint8_t enrollFinger(uint8_t id) {
  int p = -1;

  // Step 1: first scan
  Serial.printf("Waiting for finger for ID #%d ...\n", id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) { delay(100); continue; }
    if (p != FINGERPRINT_OK) { Serial.println("Image error"); return p; }
  }
  Serial.println("Image taken");

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) { Serial.println("image2Tz(1) failed"); return p; }

  Serial.println("Remove finger...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(100);

  // Step 2: second scan
  Serial.println("Place the SAME finger again...");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) { delay(100); continue; }
    if (p != FINGERPRINT_OK) { Serial.println("Image error"); return p; }
  }
  Serial.println("Image taken");

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) { Serial.println("image2Tz(2) failed"); return p; }

  // Step 3: create model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Prints did not match, try again.");
    return p;
  }

  // Step 4: store
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.printf("Enrolled successfully as ID #%d!\n\n", id);
  } else {
    Serial.println("Store failed.");
  }
  return p;
}

void loop() {
  if (enrollFinger(nextID) == FINGERPRINT_OK) {
    nextID++;
    Serial.printf("Next enrolment will be ID #%d\n", nextID);
    Serial.println("Place a new finger, or reset to stop.\n");
  } else {
    Serial.println("Try again...\n");
  }
  delay(1000);
}
