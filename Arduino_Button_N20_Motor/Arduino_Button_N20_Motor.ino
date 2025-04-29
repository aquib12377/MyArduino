/* ───────────────────────────────────────────────
   Two‑Relay Toggle Timer
   ----------------------------------------------
   • Press #1 → Relay‑1 ON   for 10 s
   • Press #2 → Relay‑2 ON   for 10 s
   • Press #3 → Relay‑1 ON   for 10 s   (and so on)
   • Only one relay can be ON at a time.
   • Uses millis() ‑‑ no blocking delay().
 */

const byte BUTTON_PIN   = 3;   // push‑button (active‑LOW)
const byte RELAY1_PIN   = A4;   // ACTIVE‑LOW relay outputs
const byte RELAY2_PIN   = 9;

const unsigned long ON_TIME_MS = 2000UL;   // 10 seconds
const unsigned long DEBOUNCE_MS = 25;

bool lastButton = HIGH;        // previous raw button state
unsigned long lastDebounce = 0;

bool relay1On = false, relay2On = false;
unsigned long relayStart = 0;  // time the current relay was energised
bool nextIsRelay1 = true;      // toggles each press

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  relayOff();                  // ensure both OFF at start

  Serial.begin(9600);
}

void loop() {
  unsigned long now = millis();

  /* ───── Button edge‑detect with debounce ───── */
  bool raw = digitalRead(BUTTON_PIN);          // LOW when pressed
  if (raw != lastButton) {                     // state changed
    lastDebounce = now;
    lastButton = raw;
  }
  bool pressed = (raw == LOW) && (now - lastDebounce > DEBOUNCE_MS);

  if (pressed) {
    // --- Toggle which relay will run ---
    relayOff();                                // stop any running relay
    if (nextIsRelay1) {
      digitalWrite(RELAY1_PIN, LOW);           // ACTIVE‑LOW → turn ON
      relay1On = true;
      Serial.println("Relay‑1 ON");
    } else {
      digitalWrite(RELAY2_PIN, LOW);
      relay2On = true;
      Serial.println("Relay‑2 ON");
    }
    relayStart = now;
    nextIsRelay1 = !nextIsRelay1;              // flip for next press
  }

  /* ───── Auto‑release after 10 s ───── */
  if ((relay1On || relay2On) && (now - relayStart >= ON_TIME_MS)) {
    relayOff();
    Serial.println("Relay timeout → both OFF");
  }
  delay(100);
}

/* Helper: turn both relays OFF */
inline void relayOff() {
  digitalWrite(RELAY1_PIN, HIGH);   // ACTIVE‑LOW → OFF
  digitalWrite(RELAY2_PIN, HIGH);
  relay1On = relay2On = false;
}
