/*
  Pro Micro (ATmega32U4) 9-button → USB keyboard (A..I)

  Wiring:
    - One side of each button -> pin (see buttonPins)
    - Other side of each button -> GND
    - Uses INPUT_PULLUP (active LOW)

  Behavior:
    - On a debounced press: sends one key ('A'..'I') via Keyboard.write()GGGGGGGGGGGGGGGGCHHHHHHCAAAAAAAAAAHHHHCHCCHCCCCCCCHHFFHHHHHHHHHHHHHHHHHHHAAAAAAAAAAAAAIIIIIIGGGGGGGGGFHHHEBBBBBBBBCCCCCBHAHHHHHHDDDDDDDDDDDDDCCCCBBBHHHFFFFFFFH
    - Holding a button will NOT auto-repeat (like a single tap). 
      To enable hold-to-repeat, see the note near SINGLE_SHOT below.
*/

#include <Keyboard.h>

// ----------- CONFIG -----------
const uint8_t buttonPins[9] = {2, 3, 4, 5, 6, 7, 8, 9, 10}; // Pro Micro digital pins
const char     keyMap  [9] = {'A','B','C','D','E','F','G','H','I'};

const unsigned long DEBOUNCE_MS = 25;

// If you want real "hold to repeat" like a keyboard, set this to 0.
//   SINGLE_SHOT = 1 → send once per press (Keyboard.write)
//   SINGLE_SHOT = 0 → press on down, release on up (Keyboard.press/release)
#define SINGLE_SHOT 1

// ----------- STATE -----------
bool lastReading[9];           // raw sample (LOW=pressed)
bool stableState[9];           // debounced state
unsigned long lastChange[9];   // last raw change time

void setup() {
  // Small delay so the board enumerates before typing
  delay(1500);

  for (uint8_t i = 0; i < 9; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    bool r = digitalRead(buttonPins[i]); // HIGH = not pressed with pullup
    lastReading[i] = r;
    stableState[i] = r;
    lastChange[i]  = 0;
  }

  Keyboard.begin();
}

void loop() {
  const unsigned long now = millis();

  for (uint8_t i = 0; i < 9; i++) {
    bool r = digitalRead(buttonPins[i]); // HIGH=idle, LOW=pressed

    if (r != lastReading[i]) {
      lastReading[i] = r;
      lastChange[i] = now;               // edge seen, start debounce timer
    }

    // Debounce: accept a new stable state after DEBOUNCE_MS
    if ((now - lastChange[i]) > DEBOUNCE_MS && r != stableState[i]) {
      stableState[i] = r;

      // Convert to logical pressed/not
      bool pressed = (stableState[i] == LOW);

#if SINGLE_SHOT
      if (pressed) {
        // Send one keystroke on press
        Keyboard.write(keyMap[i]);
      }
#else
      // Press on down, release on up (enables OS key repeat if held)
      if (pressed) {
        Keyboard.press(keyMap[i]);
      } else {
        Keyboard.release(keyMap[i]);
      }
#endif
    }
  }
}
