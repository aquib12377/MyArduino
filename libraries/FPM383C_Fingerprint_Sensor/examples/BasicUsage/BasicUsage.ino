/*
  FPM383C Basic Usage Example
  
  This example demonstrates basic fingerprint enrollment and matching
  using the FPM383C fingerprint sensor.
  
  Hardware Connections:
  - V_TOUCH: 3.3V
  - TOUCHOUT: Digital Pin 3 (optional)
  - VCC: 3.3V
  - TX: Digital Pin 2
  - RX: Digital Pin 4 (with 10kΩ pull-up)
  - GND: GND
*/

#include <FPM383C.h>

// Initialize sensor (RX pin 2, TX pin 4, Touch interrupt pin 3)
FPM383C fingerprint(2, 4, 3);

void setup() {
  Serial.begin(115200);
  Serial.println("FPM383C Basic Usage Example");
  
  // Enable debug output
  fingerprint.enableDebug(true);
  
  // Initialize sensor
  if (fingerprint.begin()) {
    Serial.println("Sensor initialized successfully!");
    
    // Get module information
    String moduleId = fingerprint.getModuleId();
    Serial.println("Module ID: " + moduleId);
    
    uint16_t templateCount = fingerprint.getTemplateCount();
    Serial.println("Stored templates: " + String(templateCount));
    
    // Set LED to green to indicate ready
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
  } else {
    Serial.println("Failed to initialize sensor!");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    while(1); // Stop execution
  }
}

void loop() {
  Serial.println("\nSelect an option:");
  Serial.println("1 - Auto enroll fingerprint");
  Serial.println("2 - Match fingerprint");
  Serial.println("3 - Delete all fingerprints");
  Serial.println("4 - Get template count");
  
  while (!Serial.available());
  char choice = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  switch (choice) {
    case '1':
      autoEnrollExample();
      break;
    case '2':
      matchExample();
      break;
    case '3':
      deleteAllExample();
      break;
    case '4':
      getCountExample();
      break;
    default:
      Serial.println("Invalid choice");
  }
}

void autoEnrollExample() {
  Serial.println("\n--- Auto Fingerprint Enrollment ---");
  Serial.println("Place finger on sensor and follow LED indicators...");
  
  fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED_GREEN, 10, 10, 0);
  
  // Auto-assign ID (65535 = auto-assign)
  if (fingerprint.autoEnroll(65535, 6, true)) {
    Serial.println("Auto enrollment successful!");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
  } else {
    Serial.println("Auto enrollment failed");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void matchExample() {
  Serial.println("\n--- Fingerprint Matching ---");
  Serial.println("Place finger on sensor...");
  
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
  
  FingerprintMatchResult result = fingerprint.matchSync();
  
  if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
    if (result.matched) {
      Serial.println("Match found!");
      Serial.println("Fingerprint ID: " + String(result.fingerprintId));
      Serial.println("Match Score: " + String(result.matchScore));
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
    } else {
      Serial.println("No match found");
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 3);
    }
  } else {
    Serial.println("Matching failed");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void deleteAllExample() {
  Serial.println("\n--- Delete All Fingerprints ---");
  Serial.println("Are you sure? Send 'Y' to confirm...");
  
  while (!Serial.available());
  char confirm = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  if (confirm == 'Y' || confirm == 'y') {
    Serial.println("Deleting all fingerprints...");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 10);
    
    if (fingerprint.deleteAllFingerprints()) {
      Serial.println("All fingerprints deleted successfully!");
      fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
    } else {
      Serial.println("Failed to delete fingerprints");
      Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
    }
  } else {
    Serial.println("Operation cancelled");
  }
  
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void getCountExample() {
  Serial.println("\n--- Template Count ---");
  
  uint16_t count = fingerprint.getTemplateCount();
  Serial.println("Stored fingerprint templates: " + String(count) + "/60");
  
  if (fingerprint.getLastError() != FP_ERROR_SUCCESS) {
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
  }
}
