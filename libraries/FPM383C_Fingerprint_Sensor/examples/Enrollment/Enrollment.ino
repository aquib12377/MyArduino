/*
  FPM383C Detailed Enrollment Example
  
  This example demonstrates detailed fingerprint enrollment process
  with step-by-step guidance and error handling.
  
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
  Serial.println("FPM383C Enrollment Example");
  Serial.println("==========================");
  
  // Initialize sensor
  if (fingerprint.begin()) {
    Serial.println("Sensor initialized successfully!");
    
    // Get module information
    String moduleId = fingerprint.getModuleId();
    Serial.println("Module ID: " + moduleId);
    
    uint16_t templateCount = fingerprint.getTemplateCount();
    Serial.println("Current templates stored: " + String(templateCount) + "/60");
    
    // Set LED to indicate ready
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
    
    Serial.println("\nReady for enrollment!");
  } else {
    Serial.println("Failed to initialize sensor!");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    while(1);
  }
}

void loop() {
  Serial.println("\n=== FINGERPRINT ENROLLMENT MENU ===");
  Serial.println("1 - Manual enrollment (step-by-step)");
  Serial.println("2 - Auto enrollment");
  Serial.println("3 - Show enrollment statistics");
  Serial.println("4 - Delete fingerprint by ID");
  Serial.println("5 - List all fingerprints");
  Serial.println("Enter your choice (1-5):");
  
  while (!Serial.available());
  char choice = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  switch (choice) {
    case '1':
      manualEnrollment();
      break;
    case '2':
      autoEnrollment();
      break;
    case '3':
      showEnrollmentStats();
      break;
    case '4':
      deleteFingerprint();
      break;
    case '5':
      listFingerprints();
      break;
    default:
      Serial.println("Invalid choice. Please select 1-5.");
  }
  
  delay(2000); // Brief pause before showing menu again
}

void manualEnrollment() {
  Serial.println("\n=== MANUAL ENROLLMENT ===");
  
  // Get available ID
  uint16_t fingerprintId = getNextAvailableId();
  if (fingerprintId == 0xFFFF) {
    Serial.println("Error: No available fingerprint slots!");
    return;
  }
  
  Serial.println("Enrolling fingerprint with ID: " + String(fingerprintId));
  Serial.println("This process requires 6 finger presses.");
  Serial.println("Place your finger on the sensor when prompted...\n");
  
  // Set LED to enrollment mode
  fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED_GREEN, 5, 5, 0);
  
  // Manual enrollment process
  for (int step = 1; step <= 6; step++) {
    Serial.println("=== Step " + String(step) + " of 6 ===");
    Serial.println("Place finger on sensor (you have 10 seconds)...");
    
    // Wait for finger
    if (!fingerprint.waitForFinger(10000)) {
      Serial.println("Timeout! No finger detected.");
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 3);
      return;
    }
    
    // Visual feedback
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
    Serial.println("Finger detected! Processing...");
    
    // Start enrollment step
    if (!fingerprint.startEnrollment(step)) {
      Serial.println("Failed to start enrollment step " + String(step));
      Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      
      // Blink red on error
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 3);
      step--; // Retry current step
      continue;
    }
    
    // Wait for processing
    delay(500);
    
    // Query enrollment result
    FingerprintEnrollResult result = fingerprint.queryEnrollmentResult();
    
    if (fingerprint.getLastError() != FP_ERROR_SUCCESS) {
      Serial.println("Enrollment step failed!");
      Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      
      if (fingerprint.getLastError() == 0x0000000E) {
        Serial.println("Tip: Image quality poor. Try cleaning your finger and the sensor.");
      } else if (fingerprint.getLastError() == 0x00000010) {
        Serial.println("Tip: Contact area too small. Press finger more firmly.");
      }
      
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 2);
      step--; // Retry current step
      continue;
    }
    
    Serial.println("Step completed! Progress: " + String(result.progress) + "%");
    
    // Success feedback
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 2);
    
    if (result.completed && result.progress >= 100) {
      Serial.println("Enrollment data collection complete!");
      break;
    }
    
    // Wait for finger removal before next step
    Serial.println("Please remove finger...");
    if (!fingerprint.waitForFingerRemoval(5000)) {
      Serial.println("Warning: Finger still detected, continuing anyway.");
    }
    
    delay(500); // Brief pause between steps
    Serial.println("");
  }
  
  // Save the template
  Serial.println("\nSaving fingerprint template...");
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_BLUE, 100, 0, 50);
  
  if (fingerprint.saveTemplate(fingerprintId)) {
    delay(1000);
    if (fingerprint.querySaveResult()) {
      Serial.println("SUCCESS: Fingerprint enrolled with ID " + String(fingerprintId) + "!");
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 5);
      
      // Update feature for better matching (self-learning)
      fingerprint.updateFeature(fingerprintId);
    } else {
      Serial.println("Failed to save fingerprint template.");
      Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
    }
  } else {
    Serial.println("Failed to save fingerprint template.");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  // Return to ready state
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void autoEnrollment() {
  Serial.println("\n=== AUTO ENROLLMENT ===");
  
  // Get available ID
  uint16_t fingerprintId = getNextAvailableId();
  if (fingerprintId == 0xFFFF) {
    Serial.println("Error: No available fingerprint slots!");
    return;
  }
  
  Serial.println("Auto-enrolling fingerprint with ID: " + String(fingerprintId));
  Serial.println("Follow the LED indicators and place/remove finger as needed.");
  Serial.println("The process will guide you automatically...\n");
  
  // Set breathing light during auto enrollment
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_RED_GREEN, 100, 0, 30);
  
  // Auto enrollment (6 captures, wait for finger lift)
  if (fingerprint.autoEnroll(fingerprintId, 6, true)) {
    Serial.println("SUCCESS: Auto enrollment completed for ID " + String(fingerprintId) + "!");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 5);
  } else {
    Serial.println("Auto enrollment failed!");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    
    // Provide specific error guidance
    if (fingerprint.getLastError() == 0x00000008) {
      Serial.println("Tip: No finger detected within timeout. Try again.");
    } else if (fingerprint.getLastError() == 0x0000000E) {
      Serial.println("Tip: Poor image quality. Clean finger and sensor, try again.");
    } else if (fingerprint.getLastError() == 0x0000000F) {
      Serial.println("Tip: Duplicate fingerprint detected.");
    }
    
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  // Return to ready state
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void showEnrollmentStats() {
  Serial.println("\n=== ENROLLMENT STATISTICS ===");
  
  uint16_t totalTemplates = fingerprint.getTemplateCount();
  Serial.println("Templates stored: " + String(totalTemplates) + "/60");
  Serial.println("Available slots: " + String(60 - totalTemplates));
  
  if (totalTemplates > 0) {
    Serial.println("Storage usage: " + String((totalTemplates * 100) / 60) + "%");
    
    // Get storage distribution
    FingerprintStorageInfo storageInfo = fingerprint.getStorageInfo();
    if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
      Serial.println("Total count from storage map: " + String(storageInfo.totalCount));
      
      Serial.print("Occupied slots: ");
      bool first = true;
      for (int i = 0; i < 60; i++) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        if (storageInfo.storageMap[byteIndex] & (1 << bitIndex)) {
          if (!first) Serial.print(", ");
          Serial.print(String(i));
          first = false;
        }
      }
      Serial.println();
    }
  } else {
    Serial.println("No fingerprints enrolled yet.");
  }
  
  // Module information
  String moduleId = fingerprint.getModuleId();
  Serial.println("Module ID: " + moduleId);
}

void deleteFingerprint() {
  Serial.println("\n=== DELETE FINGERPRINT ===");
  Serial.println("Enter fingerprint ID to delete (0-59): ");
  
  while (!Serial.available());
  uint16_t fingerprintId = Serial.parseInt();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  if (fingerprintId > 59) {
    Serial.println("Invalid ID! Must be 0-59");
    return;
  }
  
  // Check if ID exists
  if (!fingerprint.checkFingerprintExists(fingerprintId)) {
    Serial.println("Fingerprint ID " + String(fingerprintId) + " does not exist.");
    return;
  }
  
  Serial.println("Are you sure you want to delete fingerprint ID " + String(fingerprintId) + "? (y/N)");
  while (!Serial.available());
  char confirm = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  if (confirm == 'y' || confirm == 'Y') {
    Serial.println("Deleting fingerprint ID " + String(fingerprintId) + "...");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 3);
    
    if (fingerprint.deleteFingerprint(fingerprintId)) {
      delay(500);
      if (fingerprint.queryDeleteResult()) {
        Serial.println("Fingerprint deleted successfully!");
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
      } else {
        Serial.println("Failed to delete fingerprint.");
        Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      }
    } else {
      Serial.println("Failed to delete fingerprint.");
      Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    }
  } else {
    Serial.println("Operation cancelled.");
  }
  
  // Return to ready state
  delay(1000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void listFingerprints() {
  Serial.println("\n=== FINGERPRINT LIST ===");
  
  FingerprintStorageInfo storageInfo = fingerprint.getStorageInfo();
  if (fingerprint.getLastError() != FP_ERROR_SUCCESS) {
    Serial.println("Failed to get storage information.");
    return;
  }
  
  Serial.println("Enrolled fingerprint IDs:");
  
  bool found = false;
  for (int i = 0; i < 60; i++) {
    int byteIndex = i / 8;
    int bitIndex = i % 8;
    if (storageInfo.storageMap[byteIndex] & (1 << bitIndex)) {
      Serial.println("  ID " + String(i) + " - Enrolled");
      found = true;
    }
  }
  
  if (!found) {
    Serial.println("  No fingerprints enrolled.");
  }
  
  Serial.println("Total: " + String(storageInfo.totalCount) + " fingerprints");
}

uint16_t getNextAvailableId() {
  for (uint16_t i = 0; i < 60; i++) {
    if (!fingerprint.checkFingerprintExists(i)) {
      return i;
    }
  }
  return 0xFFFF; // No available slots
}
