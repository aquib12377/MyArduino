/*
  FPM383C Fingerprint Matching Example
  
  This example demonstrates fingerprint verification and matching
  with detailed feedback and statistics.
  
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

// Statistics tracking
struct MatchingStats {
  uint32_t totalAttempts;
  uint32_t successfulMatches;
  uint32_t failedMatches;
  uint32_t qualityErrors;
  uint32_t timeouts;
  uint16_t lastMatchedId;
  uint16_t lastMatchScore;
};

MatchingStats stats = {0, 0, 0, 0, 0, 0, 0};

void setup() {
  Serial.begin(115200);
  Serial.println("FPM383C Matching Example");
  Serial.println("========================");
  
  // Initialize sensor
  if (fingerprint.begin()) {
    Serial.println("Sensor initialized successfully!");
    
    // Get module information
    String moduleId = fingerprint.getModuleId();
    Serial.println("Module ID: " + moduleId);
    
    uint16_t templateCount = fingerprint.getTemplateCount();
    Serial.println("Enrolled fingerprints: " + String(templateCount) + "/60");
    
    if (templateCount == 0) {
      Serial.println("\nWARNING: No fingerprints enrolled!");
      Serial.println("Please enroll fingerprints first before testing matching.");
    }
    
    // Set LED to indicate ready
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
    
    Serial.println("\nReady for matching!");
  } else {
    Serial.println("Failed to initialize sensor!");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    while(1);
  }
}

void loop() {
  Serial.println("\n=== FINGERPRINT MATCHING MENU ===");
  Serial.println("1 - Single match test");
  Serial.println("2 - Continuous matching mode");
  Serial.println("3 - Matching with self-learning");
  Serial.println("4 - Touch detection test");
  Serial.println("5 - Show matching statistics");
  Serial.println("6 - Reset statistics");
  Serial.println("7 - Speed benchmark test");
  Serial.println("Enter your choice (1-7):");
  
  while (!Serial.available());
  char choice = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  switch (choice) {
    case '1':
      singleMatchTest();
      break;
    case '2':
      continuousMatchingMode();
      break;
    case '3':
      matchingWithLearning();
      break;
    case '4':
      touchDetectionTest();
      break;
    case '5':
      showStatistics();
      break;
    case '6':
      resetStatistics();
      break;
    case '7':
      speedBenchmark();
      break;
    default:
      Serial.println("Invalid choice. Please select 1-7.");
  }
}

void singleMatchTest() {
  Serial.println("\n=== SINGLE MATCH TEST ===");
  Serial.println("Place your finger on the sensor...");
  
  // Set LED to matching mode
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
  
  // Record start time for performance measurement
  unsigned long startTime = millis();
  
  // Perform synchronous matching
  FingerprintMatchResult result = fingerprint.matchSync();
  
  unsigned long matchTime = millis() - startTime;
  
  // Update statistics
  stats.totalAttempts++;
  
  if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
    if (result.matched) {
      stats.successfulMatches++;
      stats.lastMatchedId = result.fingerprintId;
      stats.lastMatchScore = result.matchScore;
      
      Serial.println("✓ MATCH FOUND!");
      Serial.println("  Fingerprint ID: " + String(result.fingerprintId));
      Serial.println("  Match Score: " + String(result.matchScore));
      Serial.println("  Match Time: " + String(matchTime) + "ms");
      
      // Green LED for success
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
      
      // Provide match quality feedback
      if (result.matchScore > 80) {
        Serial.println("  Quality: Excellent match");
      } else if (result.matchScore > 60) {
        Serial.println("  Quality: Good match");
      } else {
        Serial.println("  Quality: Fair match");
      }
      
    } else {
      stats.failedMatches++;
      Serial.println("✗ NO MATCH FOUND");
      Serial.println("  No enrolled fingerprint matches the scanned finger.");
      Serial.println("  Scan Time: " + String(matchTime) + "ms");
      
      // Red LED for no match
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 3);
    }
  } else {
    // Handle errors
    stats.failedMatches++;
    Serial.println("✗ MATCHING ERROR");
    Serial.println("  Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    
    // Categorize error types
    uint32_t error = fingerprint.getLastError();
    if (error == 0x00000008) {
      stats.timeouts++;
      Serial.println("  Tip: No finger detected within timeout period.");
    } else if (error == 0x0000000E || error == 0x00000010) {
      stats.qualityErrors++;
      if (error == 0x0000000E) {
        Serial.println("  Tip: Image quality poor. Clean finger and sensor.");
      } else {
        Serial.println("  Tip: Contact area too small. Press more firmly.");
      }
    } else if (error == 0x0000000A) {
      Serial.println("  Tip: No fingerprints enrolled in database.");
    }
    
    // Red LED for errors
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  // Return to ready state
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
}

void continuousMatchingMode() {
  Serial.println("\n=== CONTINUOUS MATCHING MODE ===");
  Serial.println("Continuous matching mode activated.");
  Serial.println("Place fingers on sensor. Press any key to exit.");
  
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_BLUE, 100, 20, 30);
  
  while (!Serial.available()) {
    // Check for finger presence
    if (fingerprint.isFingerPresent()) {
      Serial.println("\nFinger detected! Matching...");
      
      FingerprintMatchResult result = fingerprint.matchSync();
      
      stats.totalAttempts++;
      
      if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
        if (result.matched) {
          stats.successfulMatches++;
          Serial.println("✓ Match: ID " + String(result.fingerprintId) + 
                        ", Score: " + String(result.matchScore));
          fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
        } else {
          stats.failedMatches++;
          Serial.println("✗ No match found");
          fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED);
        }
      } else {
        stats.failedMatches++;
        Serial.println("✗ Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 2);
      }
      
      // Wait for finger removal
      Serial.println("Remove finger...");
      fingerprint.waitForFingerRemoval(5000);
      delay(1000);
      fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_BLUE, 100, 20, 30);
    }
    
    delay(100); // Small delay to prevent excessive polling
  }
  
  // Clear input buffer
  while (Serial.available()) Serial.read();
  
  Serial.println("Exiting continuous mode.");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
}

void matchingWithLearning() {
  Serial.println("\n=== MATCHING WITH SELF-LEARNING ===");
  Serial.println("This mode updates fingerprint templates after successful matches");
  Serial.println("to improve future recognition accuracy.");
  Serial.println("Place your finger on the sensor...");
  
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
  
  FingerprintMatchResult result = fingerprint.matchSync();
  
  stats.totalAttempts++;
  
  if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
    if (result.matched) {
      stats.successfulMatches++;
      stats.lastMatchedId = result.fingerprintId;
      stats.lastMatchScore = result.matchScore;
      
      Serial.println("✓ MATCH FOUND!");
      Serial.println("  Fingerprint ID: " + String(result.fingerprintId));
      Serial.println("  Match Score: " + String(result.matchScore));
      
      // Green LED for success
      fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
      
      // Perform self-learning update
      Serial.println("  Updating template for improved recognition...");
      fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_GREEN, 100, 0, 50);
      
      if (fingerprint.updateFeature(result.fingerprintId)) {
        delay(500);
        if (fingerprint.queryUpdateResult()) {
          Serial.println("  ✓ Template updated successfully!");
          fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
        } else {
          Serial.println("  ⚠ Template update not needed or failed.");
          fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN_BLUE, 10, 10, 2);
        }
      } else {
        Serial.println("  ✗ Failed to update template.");
        Serial.println("  Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 2);
      }
      
    } else {
      stats.failedMatches++;
      Serial.println("✗ NO MATCH FOUND");
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 3);
    }
  } else {
    stats.failedMatches++;
    Serial.println("✗ MATCHING ERROR");
    Serial.println("  Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
}

void touchDetectionTest() {
  Serial.println("\n=== TOUCH DETECTION TEST ===");
  Serial.println("Testing finger presence detection.");
  Serial.println("Touch and remove finger from sensor. Press any key to exit.");
  
  fingerprint.setLED(FP_LED_MODE_OFF, 0);
  
  bool lastFingerState = false;
  
  while (!Serial.available()) {
    bool currentFingerState = fingerprint.isFingerPresent();
    
    if (currentFingerState != lastFingerState) {
      if (currentFingerState) {
        Serial.println("✓ Finger detected!");
        fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
      } else {
        Serial.println("✗ Finger removed!");
        fingerprint.setLED(FP_LED_MODE_OFF, 0);
      }
      lastFingerState = currentFingerState;
    }
    
    delay(50); // Small delay for responsiveness
  }
  
  // Clear input buffer
  while (Serial.available()) Serial.read();
  
  Serial.println("Exiting touch detection test.");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
}

void showStatistics() {
  Serial.println("\n=== MATCHING STATISTICS ===");
  Serial.println("Total Attempts: " + String(stats.totalAttempts));
  Serial.println("Successful Matches: " + String(stats.successfulMatches));
  Serial.println("Failed Matches: " + String(stats.failedMatches));
  Serial.println("Quality Errors: " + String(stats.qualityErrors));
  Serial.println("Timeouts: " + String(stats.timeouts));
  
  if (stats.totalAttempts > 0) {
    float successRate = (float)stats.successfulMatches / stats.totalAttempts * 100;
    Serial.println("Success Rate: " + String(successRate, 1) + "%");
  }
  
  if (stats.successfulMatches > 0) {
    Serial.println("Last Matched ID: " + String(stats.lastMatchedId));
    Serial.println("Last Match Score: " + String(stats.lastMatchScore));
  }
  
  // Current system status
  uint16_t templateCount = fingerprint.getTemplateCount();
  Serial.println("\nCurrent System Status:");
  Serial.println("Enrolled Templates: " + String(templateCount) + "/60");
  
  String moduleId = fingerprint.getModuleId();
  Serial.println("Module ID: " + moduleId);
}

void resetStatistics() {
  Serial.println("\n=== RESET STATISTICS ===");
  Serial.println("Are you sure you want to reset all statistics? (y/N)");
  
  while (!Serial.available());
  char confirm = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  if (confirm == 'y' || confirm == 'Y') {
    memset(&stats, 0, sizeof(stats));
    Serial.println("Statistics reset successfully!");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 2);
  } else {
    Serial.println("Operation cancelled.");
  }
  
  delay(1000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
}

void speedBenchmark() {
  Serial.println("\n=== SPEED BENCHMARK TEST ===");
  Serial.println("This test measures matching speed performance.");
  Serial.println("Place an enrolled finger on the sensor when prompted.");
  
  const int testRuns = 5;
  unsigned long totalTime = 0;
  int successfulRuns = 0;
  
  for (int i = 1; i <= testRuns; i++) {
    Serial.println("\nRun " + String(i) + "/" + String(testRuns) + ":");
    Serial.println("Place finger on sensor...");
    
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
    
    unsigned long startTime = millis();
    FingerprintMatchResult result = fingerprint.matchSync();
    unsigned long matchTime = millis() - startTime;
    
    if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
      if (result.matched) {
        totalTime += matchTime;
        successfulRuns++;
        Serial.println("✓ Match found in " + String(matchTime) + "ms");
        Serial.println("  ID: " + String(result.fingerprintId) + 
                      ", Score: " + String(result.matchScore));
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 1);
      } else {
        Serial.println("✗ No match found (" + String(matchTime) + "ms)");
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 1);
      }
    } else {
      Serial.println("✗ Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 2);
    }
    
    if (i < testRuns) {
      Serial.println("Remove finger for next test...");
      fingerprint.waitForFingerRemoval(5000);
      delay(1000);
    }
  }
  
  // Calculate and display results
  Serial.println("\n=== BENCHMARK RESULTS ===");
  Serial.println("Test Runs: " + String(testRuns));
  Serial.println("Successful Runs: " + String(successfulRuns));
  
  if (successfulRuns > 0) {
    float averageTime = (float)totalTime / successfulRuns;
    Serial.println("Average Match Time: " + String(averageTime, 1) + "ms");
    Serial.println("Fastest Match: " + String(totalTime > 0 ? totalTime / successfulRuns : 0) + "ms");
    
    if (averageTime < 200) {
      Serial.println("Performance: Excellent (< 200ms)");
    } else if (averageTime < 500) {
      Serial.println("Performance: Good (< 500ms)");
    } else {
      Serial.println("Performance: Average (≥ 500ms)");
    }
  }
  
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
}
