/*
 * ================================================================
 *  NANO ADDITIONS — paste these into your existing sketch
 *  (Your existing code stays EXACTLY as-is)
 * ================================================================
 *
 *  WIRING (add these two wires):
 *    ESP32-CAM GPIO 12  -->  Nano D6
 *    ESP32-CAM GND      -->  Nano GND
 */


// ============================================================
// ADD #1:  Put this with your other #define lines (near top)
// ============================================================
#define ESP_SIGNAL_PIN 6   // D6 — receives unlock pulse from ESP32-CAM


// ============================================================
// ADD #2:  Put these two lines inside setup(), after your
//          existing setup code (e.g. after showIdleScreen();)
// ============================================================
  pinMode(ESP_SIGNAL_PIN, INPUT);  // or INPUT_PULLDOWN if using Nano Every


// ============================================================
// ADD #3:  Put this block at the VERY TOP of loop(),
//          BEFORE the existing RFID check.
//          (i.e. before "if (!mfrc522.PICC_IsNewCardPresent())")
// ============================================================

  // --- ESP32-CAM unlock signal (face or fingerprint) ---
  if (digitalRead(ESP_SIGNAL_PIN) == HIGH) {
    Serial.println(F("[ESP32] Face/Finger unlock signal received!"));
    setLockState(false);   // unlock for 5 s then auto-relock (same as RFID)

    // Wait for signal to go LOW before continuing
    while (digitalRead(ESP_SIGNAL_PIN) == HIGH) {
      delay(10);
    }
    return;  // skip RFID check this cycle
  }
