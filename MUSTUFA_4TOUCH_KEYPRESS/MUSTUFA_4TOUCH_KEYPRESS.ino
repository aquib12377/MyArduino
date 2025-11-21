// Arduino Pro Micro (ATmega32U4) - 4 touch sensors (active HIGH) -> keyboard keys
// Maps 4 sensors to 4 keyboard keys, with debounce and proper press/release behavior.

#include <Keyboard.h>

// ---------- CONFIG ----------
const uint8_t sensorPins[4] = {A3, A2, A1, A0}; // Digital pins where sensors are connected
// Map each sensor to a key. You may use 'a', 'b', KEY_LEFT_CTRL, KEY_RETURN, etc.
// For ASCII keys use characters in single quotes: 'a', '1', etc.
// For special keys use KEY_ constants defined in Keyboard.h (e.g. KEY_ENTER is not standard here — use '\n' or KEY_RETURN depending on your lib).
const uint8_t keyMap[4] = {'1', '2', '3', '4'}; 

// Debounce time (ms)
const unsigned long DEBOUNCE_MS = 40;
// ----------------------------

bool keyState[4] = {false, false, false, false};      // current debounced state (true = active/touched)
unsigned long lastChangeTime[4] = {0,0,0,0};          // last time the raw read changed
bool lastRaw[4] = {false, false, false, false};       // last raw read to detect bouncing

void setup() {
  // Initialize sensor pins as inputs. We don't enable INPUT_PULLUP because sensors are ACTIVE HIGH.
  // If you only have sensors that output LOW when idle, use INPUT_PULLUP and invert logic below.
  for (int i = 0; i < 4; ++i) {
    pinMode(sensorPins[i], INPUT);
  }

  // Start Keyboard emulation
  Keyboard.begin();
  // small delay to let host enumerate
  delay(50);
}

void loop() {
  unsigned long now = millis();

  for (int i = 0; i < 4; ++i) {
    bool raw = digitalRead(sensorPins[i]) == HIGH; // sensor is ACTIVE HIGH

    // If raw changed, update the lastChangeTime
    if (raw != lastRaw[i]) {
      lastChangeTime[i] = now;
      lastRaw[i] = raw;
    }

    // If raw has been stable longer than debounce, commit the change
    if ((now - lastChangeTime[i]) >= DEBOUNCE_MS) {
      if (raw != keyState[i]) {
        // State changed (debounced)
        keyState[i] = raw;
        if (keyState[i]) {
          // Sensor activated: send key press
          Keyboard.press(keyMap[i]);
        } else {
          // Sensor released: release key
          Keyboard.release(keyMap[i]);
        }
      }
    }
  }

  // Do not block; allow CPU to service USB
  // small yield to avoid high CPU usage
  delay(1);
}
