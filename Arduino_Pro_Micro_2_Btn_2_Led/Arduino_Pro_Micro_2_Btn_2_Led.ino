/*
  Pro Micro – 2 Buttons / 2 LEDs / 2 HID keys
  -------------------------------------------
  • Buttons on D2 and D3 (wired to GND, INPUT_PULLUP enabled)
  • LEDs   on D16 (TX‑LED) and D10 (any external LED + resistor)
  • Each button press:
        – turns its LED ON for 3 s (non‑blocking)
        – sends its own keyboard character
*/

#include <Keyboard.h>

// ───────────── Timing constants ─────────────
const uint32_t ON_TIME_MS  = 5000L;  // LED ON duration
const uint16_t DEBOUNCE_MS = 25;    // button debounce

// ───────────── Channel definition ───────────
struct Channel {
  uint8_t btnPin;    // button input
  uint8_t ledPin;    // LED output
  char    key;       // HID key to send

  // runtime state
  bool     ledOn;
  uint32_t tStart;
  bool     lastRaw;
  uint32_t debT;

  // constructor -> sets fixed pins/keys + sensible defaults
  Channel(uint8_t bPin, uint8_t lPin, char k) :
    btnPin(bPin), ledPin(lPin), key(k),
    ledOn(false), tStart(0), lastRaw(HIGH), debT(0) {}
};

// Create the two channels
Channel ch[] = {
  Channel(2, A3, '1'),   // button D2 -> LED D16 -> sends “A”
  Channel(3, 10, '2')    // button D3 -> LED D10 -> sends “B”1111
};
// ─────────────────────────────────────────────

void setup() {
  for (Channel &c : ch) {
    pinMode(c.btnPin, INPUT_PULLUP);
    pinMode(c.ledPin, OUTPUT);
    digitalWrite(c.ledPin, HIGH);
  }
  Keyboard.begin();
}

void loop() {
  uint32_t now = millis();

  for (Channel &c : ch) {
    // ── Debounce & edge‑detect the button ──
    bool raw = digitalRead(c.btnPin);                 // LOW = pressed
    if (raw != c.lastRaw) {                           // state changed
      c.debT = now;                                   // reset debounce timer
      c.lastRaw = raw;
    }
    bool stablePress = (raw == LOW) && (now - c.debT > DEBOUNCE_MS);

    // ── New, stable press? ──
    if (stablePress && !c.ledOn) {
      c.ledOn  = true;
      c.tStart = now;
      digitalWrite(c.ledPin, LOW);
      Keyboard.write(c.key);                          // send key
    }

    // ── Auto‑off LED after ON_TIME_MS ──
    if (c.ledOn && (now - c.tStart >= ON_TIME_MS)) {
      c.ledOn = false;
      digitalWrite(c.ledPin, HIGH);
    }
  }
}
