// ─────────────────────────────────────────────────────────────────────────────
//  Active‑Low Relay Controller for Drowsiness Alert
//  Listens for '1'/'0' over Serial at 115200 baud and
//  energizes/de‑energizes a relay (active‑LOW).
// ─────────────────────────────────────────────────────────────────────────────

const uint8_t RELAY_PIN = 7;  // digital pin driving the relay module (active‑LOW)

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(8, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // start with relay OFF (HIGH = coil de‑energised)
  digitalWrite(8, LOW);  // start with relay OFF (HIGH = coil de‑energised)
}

void loop() {
  // check if there’s data from the Python drowsiness script
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') {
      // Drowsiness detected → energize relay (LOW)
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(8, HIGH);
    }
    else if (cmd == '0') {
      // Normal state → de‑energize relay (HIGH)
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(8, LOW);
    }
    // ignore any other characters
  }
}
