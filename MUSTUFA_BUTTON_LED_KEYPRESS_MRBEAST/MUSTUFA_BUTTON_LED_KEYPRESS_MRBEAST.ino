#include <Keyboard.h>

const int BUTTON_PIN = 2;
const int RELAY_PIN  = 4;

bool buttonState = HIGH;        // debounced state
bool lastReading = HIGH;        // last raw reading
bool relayState = false;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  Keyboard.begin();

  Serial.begin(9600);
  while (!Serial);
  Serial.println("=== Ready ===");
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);

  // Detect change in raw reading
  if (reading != lastReading) {
    lastDebounceTime = millis();
    Serial.println("Button state changed, debounce timer reset");
  }

  // If stable long enough
  if ((millis() - lastDebounceTime) > debounceDelay) {

    // If debounced state actually changed
    if (reading != buttonState) {
      buttonState = reading;

      // Button pressed (LOW)
      if (buttonState == LOW) {
        relayState = !relayState;

        if (relayState) {
          digitalWrite(RELAY_PIN, LOW);
          Keyboard.write('3');
          Serial.println("Relay ON -> Sent '1'");
        } else {
          digitalWrite(RELAY_PIN, HIGH);
                    Keyboard.write('4');
          Serial.println("Relay OFF -> Sent '2'");
        }
      }
    }
  }

  lastReading = reading;
}