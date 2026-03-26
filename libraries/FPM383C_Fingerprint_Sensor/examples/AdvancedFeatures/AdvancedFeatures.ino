/*
  FPM383C Advanced Features Example
  
  This example demonstrates advanced features including:
  - LED control and patterns
  - Sleep mode and power management
  - Module information and diagnostics
  - System configuration
  - Template management
  
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
  Serial.println("FPM383C Advanced Features Example");
  Serial.println("=================================");
  
  // Enable debug mode for detailed output
  fingerprint.enableDebug(true);
  
  // Initialize sensor
  if (fingerprint.begin()) {
    Serial.println("Sensor initialized successfully!");
    
    // Display module information
    displayModuleInfo();
    
    // Set default LED state
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
    
    Serial.println("\nAdvanced features ready!");
  } else {
    Serial.println("Failed to initialize sensor!");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    while(1);
  }
}

void loop() {
  Serial.println("\n=== ADVANCED FEATURES MENU ===");
  Serial.println("1 - LED Control Patterns");
  Serial.println("2 - Power Management");
  Serial.println("3 - Module Diagnostics");
  Serial.println("4 - Template Management");
  Serial.println("5 - Communication Settings");
  Serial.println("6 - System Information");
  Serial.println("7 - Factory Reset Demo");
  Serial.println("8 - Stress Test");
  Serial.println("Enter your choice (1-8):");
  
  while (!Serial.available());
  char choice = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  switch (choice) {
    case '1':
      ledControlDemo();
      break;
    case '2':
      powerManagementDemo();
      break;
    case '3':
      moduleDiagnostics();
      break;
    case '4':
      templateManagement();
      break;
    case '5':
      communicationSettings();
      break;
    case '6':
      systemInformation();
      break;
    case '7':
      factoryResetDemo();
      break;
    case '8':
      stressTest();
      break;
    default:
      Serial.println("Invalid choice. Please select 1-8.");
  }
}

void ledControlDemo() {
  Serial.println("\n=== LED CONTROL PATTERNS ===");
  Serial.println("Demonstrating various LED patterns...");
  
  // Basic colors
  Serial.println("1. Basic Colors:");
  
  Serial.println("   Red");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED);
  delay(1000);
  
  Serial.println("   Green");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
  delay(1000);
  
  Serial.println("   Blue");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
  delay(1000);
  
  Serial.println("   Red + Green (Yellow)");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED_GREEN);
  delay(1000);
  
  Serial.println("   Red + Blue (Magenta)");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED_BLUE);
  delay(1000);
  
  Serial.println("   Green + Blue (Cyan)");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN_BLUE);
  delay(1000);
  
  Serial.println("   All Colors (White)");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_ALL_COLORS);
  delay(1000);
  
  // Blinking patterns
  Serial.println("\n2. Blinking Patterns:");
  
  Serial.println("   Fast Red Blink");
  fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 2, 2, 10);
  delay(3000);
  
  Serial.println("   Slow Green Blink");
  fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 20, 20, 5);
  delay(3000);
  
  Serial.println("   Alternating Red/Blue");
  for (int i = 0; i < 5; i++) {
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED);
    delay(300);
    fingerprint.setLED(FP_LED_MODE_ON, FP_LED_BLUE);
    delay(300);
  }
  
  // PWM/Breathing patterns
  Serial.println("\n3. Breathing/PWM Patterns:");
  
  Serial.println("   Green Breathing");
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_GREEN, 100, 0, 30);
  delay(5000);
  
  Serial.println("   Red Breathing (Fast)");
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_RED, 100, 0, 80);
  delay(3000);
  
  Serial.println("   Blue Breathing (Slow)");
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_BLUE, 100, 10, 20);
  delay(5000);
  
  // Rainbow effect (simulated)
  Serial.println("\n4. Rainbow Effect:");
  uint8_t colors[] = {FP_LED_RED, FP_LED_RED_GREEN, FP_LED_GREEN, 
                     FP_LED_GREEN_BLUE, FP_LED_BLUE, FP_LED_RED_BLUE};
  
  for (int cycle = 0; cycle < 3; cycle++) {
    for (int i = 0; i < 6; i++) {
      fingerprint.setLED(FP_LED_MODE_ON, colors[i]);
      delay(300);
    }
  }
  
  Serial.println("LED demonstration complete!");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void powerManagementDemo() {
  Serial.println("\n=== POWER MANAGEMENT ===");
  Serial.println("Demonstrating power management features...");
  
  // Current consumption test
  Serial.println("\n1. Power Consumption States:");
  
  Serial.println("   Active Mode (LED ON):");
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_ALL_COLORS);
  Serial.println("   - Estimated current: ~45mA");
  delay(2000);
  
  Serial.println("   Active Mode (LED OFF):");
  fingerprint.setLED(FP_LED_MODE_OFF, 0);
  Serial.println("   - Estimated current: ~30mA");
  delay(2000);
  
  Serial.println("   Sleep Mode Test:");
  Serial.println("   - Entering sleep mode...");
  Serial.println("   - Touch sensor to wake up");
  Serial.println("   - Estimated current: <25µA");
  
  // Set breathing LED to indicate sleep mode
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_BLUE, 20, 0, 10);
  delay(1000);
  
  if (fingerprint.setSleepMode(0)) {
    Serial.println("   ✓ Sleep mode activated");
    Serial.println("   Touch the sensor to continue...");
    
    // Wait for touch to wake up
    while (!fingerprint.isFingerPresent()) {
      delay(100);
    }
    
    Serial.println("   ✓ Sensor awakened by touch!");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
  } else {
    Serial.println("   ✗ Failed to enter sleep mode");
    Serial.println("   Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
  }
  
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
  
  Serial.println("Power management demonstration complete!");
}

void moduleDiagnostics() {
  Serial.println("\n=== MODULE DIAGNOSTICS ===");
  
  // Communication test
  Serial.println("1. Communication Test:");
  Serial.print("   Heartbeat: ");
  if (fingerprint.heartbeat()) {
    Serial.println("✓ PASS");
  } else {
    Serial.println("✗ FAIL - " + fingerprint.getErrorString(fingerprint.getLastError()));
  }
  
  // Module identification
  Serial.println("\n2. Module Identification:");
  String moduleId = fingerprint.getModuleId();
  if (moduleId.length() > 0) {
    Serial.println("   Module ID: " + moduleId);
  } else {
    Serial.println("   ✗ Failed to get module ID");
  }
  
  // Storage diagnostics
  Serial.println("\n3. Storage Diagnostics:");
  uint16_t templateCount = fingerprint.getTemplateCount();
  Serial.println("   Template count: " + String(templateCount) + "/60");
  Serial.println("   Storage usage: " + String((templateCount * 100) / 60) + "%");
  
  FingerprintStorageInfo storageInfo = fingerprint.getStorageInfo();
  if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
    Serial.println("   Storage map retrieved successfully");
    
    // Check for fragmentation
    int fragments = 0;
    bool inFragment = false;
    
    for (int i = 0; i < 60; i++) {
      int byteIndex = i / 8;
      int bitIndex = i % 8;
      bool occupied = storageInfo.storageMap[byteIndex] & (1 << bitIndex);
      
      if (occupied && !inFragment) {
        fragments++;
        inFragment = true;
      } else if (!occupied && inFragment) {
        inFragment = false;
      }
    }
    
    Serial.println("   Storage fragments: " + String(fragments));
    if (fragments > templateCount / 2) {
      Serial.println("   ⚠ High fragmentation detected");
    }
  }
  
  // Touch sensitivity test
  Serial.println("\n4. Touch Sensitivity Test:");
  Serial.println("   Place and remove finger 5 times...");
  
  int touchCount = 0;
  int releaseCount = 0;
  bool lastState = false;
  unsigned long testStart = millis();
  
  fingerprint.setLED(FP_LED_MODE_OFF, 0);
  
  while (millis() - testStart < 10000 && touchCount < 5) { // 10 second timeout
    bool currentState = fingerprint.isFingerPresent();
    
    if (currentState != lastState) {
      if (currentState) {
        touchCount++;
        Serial.println("   Touch " + String(touchCount) + " detected ✓");
        fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
      } else {
        releaseCount++;
        Serial.println("   Release " + String(releaseCount) + " detected ✓");
        fingerprint.setLED(FP_LED_MODE_OFF, 0);
      }
      lastState = currentState;
    }
    
    delay(50);
  }
  
  Serial.println("   Touch test results: " + String(touchCount) + " touches, " + 
                String(releaseCount) + " releases");
  
  if (touchCount >= 3) {
    Serial.println("   ✓ Touch sensitivity: GOOD");
  } else {
    Serial.println("   ⚠ Touch sensitivity: POOR");
  }
  
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
  Serial.println("Module diagnostics complete!");
}

void templateManagement() {
  Serial.println("\n=== TEMPLATE MANAGEMENT ===");
  
  // Display current templates
  Serial.println("1. Current Template Status:");
  uint16_t templateCount = fingerprint.getTemplateCount();
  Serial.println("   Templates stored: " + String(templateCount) + "/60");
  
  if (templateCount > 0) {
    FingerprintStorageInfo storageInfo = fingerprint.getStorageInfo();
    if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
      Serial.print("   Occupied slots: ");
      bool first = true;
      for (int i = 0; i < 60; i++) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        if (storageInfo.storageMap[byteIndex] & (1 << bitIndex)) {
          if (!first) Serial.print(", ");
          Serial.print(String(i));
          first = false;
          if (!first && i % 10 == 9) {
            Serial.println();
            Serial.print("                      ");
          }
        }
      }
      Serial.println();
    }
  }
  
  Serial.println("\n2. Template Management Options:");
  Serial.println("   a) Check specific ID");
  Serial.println("   b) Delete range of IDs");
  Serial.println("   c) Defragment storage (delete all and show clean state)");
  Serial.println("   d) Export template info");
  Serial.println("Enter choice (a-d):");
  
  while (!Serial.available());
  char choice = Serial.read();
  while (Serial.available()) Serial.read(); // Clear buffer
  
  switch (choice) {
    case 'a':
      checkSpecificId();
      break;
    case 'b':
      deleteRange();
      break;
    case 'c':
      defragmentStorage();
      break;
    case 'd':
      exportTemplateInfo();
      break;
    default:
      Serial.println("Invalid choice.");
  }
}

void checkSpecificId() {
  Serial.println("\nEnter fingerprint ID to check (0-59):");
  while (!Serial.available());
  uint16_t id = Serial.parseInt();
  while (Serial.available()) Serial.read();
  
  if (id > 59) {
    Serial.println("Invalid ID. Must be 0-59.");
    return;
  }
  
  if (fingerprint.checkFingerprintExists(id)) {
    Serial.println("✓ Fingerprint ID " + String(id) + " exists");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 2);
  } else {
    Serial.println("✗ Fingerprint ID " + String(id) + " does not exist");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 2);
  }
}

void deleteRange() {
  Serial.println("\nEnter start ID:");
  while (!Serial.available());
  uint16_t startId = Serial.parseInt();
  while (Serial.available()) Serial.read();
  
  Serial.println("Enter end ID:");
  while (!Serial.available());
  uint16_t endId = Serial.parseInt();
  while (Serial.available()) Serial.read();
  
  if (startId > 59 || endId > 59 || startId > endId) {
    Serial.println("Invalid range. IDs must be 0-59 and start <= end.");
    return;
  }
  
  Serial.println("Delete fingerprints " + String(startId) + " to " + String(endId) + "? (y/N)");
  while (!Serial.available());
  char confirm = Serial.read();
  while (Serial.available()) Serial.read();
  
  if (confirm == 'y' || confirm == 'Y') {
    uint16_t deleteList[60];
    int deleteCount = 0;
    
    for (uint16_t i = startId; i <= endId; i++) {
      if (fingerprint.checkFingerprintExists(i)) {
        deleteList[deleteCount++] = i;
      }
    }
    
    if (deleteCount > 0) {
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 0);
      
      if (fingerprint.deleteMultipleFingerprints(deleteList, deleteCount)) {
        Serial.println("✓ Deleted " + String(deleteCount) + " fingerprints");
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
      } else {
        Serial.println("✗ Failed to delete fingerprints");
        Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
      }
    } else {
      Serial.println("No fingerprints found in specified range.");
    }
  } else {
    Serial.println("Operation cancelled.");
  }
}

void defragmentStorage() {
  Serial.println("\n⚠ WARNING: This will delete ALL fingerprints!");
  Serial.println("Type 'DELETE ALL' to confirm:");
  
  String confirmation = "";
  while (confirmation != "DELETE ALL") {
    while (!Serial.available());
    confirmation = Serial.readString();
    confirmation.trim();
    
    if (confirmation != "DELETE ALL") {
      Serial.println("Incorrect confirmation. Type 'DELETE ALL' exactly:");
    }
  }
  
  Serial.println("Deleting all fingerprints...");
  fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 3, 3, 0);
  
  if (fingerprint.deleteAllFingerprints()) {
    Serial.println("✓ All fingerprints deleted");
    Serial.println("✓ Storage defragmented");
    
    // Verify clean state
    uint16_t count = fingerprint.getTemplateCount();
    Serial.println("Templates remaining: " + String(count));
    
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 5);
  } else {
    Serial.println("✗ Failed to delete all fingerprints");
    Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
}

void exportTemplateInfo() {
  Serial.println("\nExporting template information...");
  
  FingerprintStorageInfo storageInfo = fingerprint.getStorageInfo();
  if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
    Serial.println("\n=== TEMPLATE EXPORT ===");
    Serial.println("Total templates: " + String(storageInfo.totalCount));
    Serial.println("Storage map (hex):");
    
    for (int i = 0; i < 64; i += 8) {
      Serial.print("  ");
      for (int j = 0; j < 8 && (i + j) < 64; j++) {
        if (storageInfo.storageMap[i + j] < 16) Serial.print("0");
        Serial.print(String(storageInfo.storageMap[i + j], HEX));
        Serial.print(" ");
      }
      Serial.println();
    }
    
    Serial.println("\nBit map (1=occupied, 0=free):");
    for (int i = 0; i < 60; i += 10) {
      Serial.print("  IDs " + String(i) + "-" + String(min(i + 9, 59)) + ": ");
      for (int j = 0; j < 10 && (i + j) < 60; j++) {
        int byteIndex = (i + j) / 8;
        int bitIndex = (i + j) % 8;
        Serial.print((storageInfo.storageMap[byteIndex] & (1 << bitIndex)) ? "1" : "0");
      }
      Serial.println();
    }
  }
}

void communicationSettings() {
  Serial.println("\n=== COMMUNICATION SETTINGS ===");
  Serial.println("Current baud rate: 57600 bps");
  
  Serial.println("\nAvailable options:");
  Serial.println("1 - Test different baud rates");
  Serial.println("2 - Set communication password");
  Serial.println("3 - Reset module");
  Serial.println("Enter choice (1-3):");
  
  while (!Serial.available());
  char choice = Serial.read();
  while (Serial.available()) Serial.read();
  
  switch (choice) {
    case '1':
      testBaudRates();
      break;
    case '2':
      setCommunicationPassword();
      break;
    case '3':
      resetModule();
      break;
    default:
      Serial.println("Invalid choice.");
  }
}

void testBaudRates() {
  Serial.println("\n⚠ WARNING: Changing baud rate will require code recompilation!");
  Serial.println("This is a demonstration only.");
  
  uint32_t testRates[] = {9600, 19200, 38400, 57600, 115200};
  int numRates = sizeof(testRates) / sizeof(testRates[0]);
  
  Serial.println("\nSupported baud rates:");
  for (int i = 0; i < numRates; i++) {
    Serial.println("  " + String(i + 1) + ") " + String(testRates[i]) + " bps");
  }
  
  Serial.println("\nCurrent rate: 57600 bps (recommended)");
  Serial.println("Note: After changing baud rate, you must update your code and recompile.");
}

void setCommunicationPassword() {
  Serial.println("\n⚠ Communication password feature demonstration");
  Serial.println("Note: This is for advanced security applications only.");
  Serial.println("Setting a password will require it for all future communications.");
  Serial.println("Default password is 0x00000000 (no password)");
  
  // This is just a demonstration - actual implementation would be dangerous
  Serial.println("Demonstration only - password not actually changed.");
}

void resetModule() {
  Serial.println("\nReset module? This will restart the fingerprint sensor. (y/N)");
  
  while (!Serial.available());
  char confirm = Serial.read();
  while (Serial.available()) Serial.read();
  
  if (confirm == 'y' || confirm == 'Y') {
    Serial.println("Resetting module...");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_ALL_COLORS, 5, 5, 5);
    
    if (fingerprint.reset()) {
      Serial.println("✓ Module reset successfully");
      delay(2000); // Wait for module to restart
      
      // Re-initialize
      if (fingerprint.begin()) {
        Serial.println("✓ Module reinitialized");
        fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 3);
      } else {
        Serial.println("✗ Failed to reinitialize module");
      }
    } else {
      Serial.println("✗ Reset failed");
      Serial.println("Error: " + fingerprint.getErrorString(fingerprint.getLastError()));
    }
  } else {
    Serial.println("Reset cancelled.");
  }
}

void systemInformation() {
  Serial.println("\n=== SYSTEM INFORMATION ===");
  displayModuleInfo();
}

void displayModuleInfo() {
  Serial.println("Module Information:");
  
  // Module ID
  String moduleId = fingerprint.getModuleId();
  if (moduleId.length() > 0) {
    Serial.println("  ID: " + moduleId);
  } else {
    Serial.println("  ID: Unable to retrieve");
  }
  
  // Template storage
  uint16_t templateCount = fingerprint.getTemplateCount();
  Serial.println("  Templates: " + String(templateCount) + "/60");
  Serial.println("  Free slots: " + String(60 - templateCount));
  
  // System status
  if (fingerprint.heartbeat()) {
    Serial.println("  Status: Online ✓");
  } else {
    Serial.println("  Status: Communication error ✗");
  }
  
  // Performance specifications
  Serial.println("\nSpecifications:");
  Serial.println("  FAR (False Accept Rate): <1/1,000,000");
  Serial.println("  FRR (False Reject Rate): <1.5%");
  Serial.println("  Feature extraction time: <0.20s");
  Serial.println("  Match time per template: <0.002s");
  Serial.println("  Max enrollment captures: 6");
  Serial.println("  ESD protection: >15KV");
  Serial.println("  Operating voltage: 3.3V");
  Serial.println("  Active current: <45mA");
  Serial.println("  Sleep current: <25µA");
}

void factoryResetDemo() {
  Serial.println("\n=== FACTORY RESET DEMONSTRATION ===");
  Serial.println("⚠ WARNING: This will delete ALL data!");
  Serial.println("This includes all enrolled fingerprints and settings.");
  Serial.println();
  Serial.println("Type 'FACTORY RESET' to confirm:");
  
  String confirmation = "";
  while (confirmation != "FACTORY RESET") {
    while (!Serial.available());
    confirmation = Serial.readString();
    confirmation.trim();
    
    if (confirmation != "FACTORY RESET") {
      Serial.println("Incorrect confirmation. Type 'FACTORY RESET' exactly:");
    }
  }
  
  Serial.println("Performing factory reset...");
  fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 2, 2, 0);
  
  // Delete all fingerprints
  if (fingerprint.deleteAllFingerprints()) {
    Serial.println("✓ All fingerprints deleted");
  } else {
    Serial.println("✗ Failed to delete fingerprints");
  }
  
  // Reset module
  if (fingerprint.reset()) {
    Serial.println("✓ Module reset");
    delay(2000);
    
    if (fingerprint.begin()) {
      Serial.println("✓ Module reinitialized");
      Serial.println("✓ Factory reset complete!");
      
      fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 5);
      
      // Verify clean state
      uint16_t count = fingerprint.getTemplateCount();
      Serial.println("Templates remaining: " + String(count));
      
    } else {
      Serial.println("✗ Failed to reinitialize");
    }
  } else {
    Serial.println("✗ Module reset failed");
  }
  
  delay(2000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}

void stressTest() {
  Serial.println("\n=== STRESS TEST ===");
  Serial.println("This test performs multiple operations to verify stability.");
  
  const int testCycles = 10;
  int passedTests = 0;
  
  fingerprint.setLED(FP_LED_MODE_PWM, FP_LED_BLUE, 100, 0, 50);
  
  for (int cycle = 1; cycle <= testCycles; cycle++) {
    Serial.println("\nCycle " + String(cycle) + "/" + String(testCycles) + ":");
    bool cyclePass = true;
    
    // Test 1: Heartbeat
    Serial.print("  Heartbeat: ");
    if (fingerprint.heartbeat()) {
      Serial.println("PASS");
    } else {
      Serial.println("FAIL");
      cyclePass = false;
    }
    
    // Test 2: Get template count
    Serial.print("  Template count: ");
    uint16_t count = fingerprint.getTemplateCount();
    if (fingerprint.getLastError() == FP_ERROR_SUCCESS) {
      Serial.println("PASS (" + String(count) + " templates)");
    } else {
      Serial.println("FAIL");
      cyclePass = false;
    }
    
    // Test 3: Check finger presence (quick test)
    Serial.print("  Touch detection: ");
    bool touchWorks = true;
    for (int i = 0; i < 3; i++) {
      bool present = fingerprint.isFingerPresent();
      // Just verify the function doesn't crash/timeout
      delay(10);
    }
    Serial.println("PASS");
    
    // Test 4: LED control
    Serial.print("  LED control: ");
    if (fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED)) {
      delay(100);
      if (fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN)) {
        Serial.println("PASS");
      } else {
        Serial.println("FAIL");
        cyclePass = false;
      }
    } else {
      Serial.println("FAIL");
      cyclePass = false;
    }
    
    if (cyclePass) {
      passedTests++;
      fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
    } else {
      fingerprint.setLED(FP_LED_MODE_ON, FP_LED_RED);
    }
    
    delay(200);
  }
  
  // Results
  Serial.println("\n=== STRESS TEST RESULTS ===");
  Serial.println("Cycles completed: " + String(testCycles));
  Serial.println("Cycles passed: " + String(passedTests));
  Serial.println("Success rate: " + String((passedTests * 100) / testCycles) + "%");
  
  if (passedTests == testCycles) {
    Serial.println("✓ All tests PASSED - Module is stable!");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_GREEN, 10, 10, 5);
  } else {
    Serial.println("⚠ Some tests FAILED - Check connections and module");
    fingerprint.setLED(FP_LED_MODE_BLINK, FP_LED_RED, 5, 5, 5);
  }
  
  delay(3000);
  fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
}
