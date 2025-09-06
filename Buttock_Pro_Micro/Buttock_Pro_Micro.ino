#include <Keyboard.h>

// ── I/O mapping ──────────────────────────────────────────────
const uint8_t ledPins[]    = {2, 3, 4, 5, 6, 7, 8, 9};          // 8 LEDs (active-LOW)
const uint8_t buttonPins[] = {A5, A4, A3, A2, A1, A0, 11, 12};  // 8 buttons (INPUT_PULLUP)

const uint8_t resetButton  = 13;  // Reset push-button
const uint8_t resetLED     = 10;  // Status LED

// ── game settings ────────────────────────────────────────────
const unsigned long gameDuration = 40000;  // 40 000 ms = 40 s

// ── state ────────────────────────────────────────────────────
bool          gameActive = false;
bool          gamePaused = false;
unsigned long startTime  = 0;
int           currentLed = -1;

// ── helpers ──────────────────────────────────────────────────
inline uint8_t numLeds()   { return sizeof(ledPins);   }
inline uint8_t numBtns()   { return sizeof(buttonPins); }

void allLedsOff() {
  for (uint8_t i = 0; i < numLeds(); ++i) digitalWrite(ledPins[i], HIGH);
}

// ── setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Keyboard.begin();

  for (uint8_t i = 0; i < numLeds(); ++i) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);               // LEDs OFF (active-LOW)
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(resetLED, OUTPUT);
  digitalWrite(resetLED, LOW);

  pinMode(resetButton, INPUT_PULLUP);

  /* simple LED sweep to prove wiring */
  for (uint8_t i = 0; i < numLeds(); ++i) {
    digitalWrite(ledPins[i], LOW);
    delay(300);
    digitalWrite(ledPins[i], HIGH);
  }
  Serial.println(F("Ready — press reset to start"));
}

// ── main loop ────────────────────────────────────────────────
void loop() {
  /* check reset button (debounced) */
  if (digitalRead(resetButton) == LOW) {
    delay(50);
    if (digitalRead(resetButton) == LOW) {
      if (!gameActive) startGame();
      else             endGame();
      while (digitalRead(resetButton) == LOW);    // wait for release
    }
  }

  /* run the game */
  if (gameActive && !gamePaused) {
    if (millis() - startTime >= gameDuration) endGame();
    else                                      checkGameButtons();
  }
}

// ── game control ─────────────────────────────────────────────
void startGame() {
  Keyboard.write('3');
  gameActive = true;
  gamePaused = false;
  startTime  = millis();
  nextLed();
  digitalWrite(resetLED, HIGH);
  Serial.println(F("Game started"));
}

void endGame() {
  Keyboard.write('2');
  gameActive = false;
  allLedsOff();
  digitalWrite(resetLED, LOW);
  Serial.println(F("Game ended"));
}

// ── LED sequencing ───────────────────────────────────────────
void nextLed() {
  if (currentLed != -1) digitalWrite(ledPins[currentLed], HIGH); // turn previous off
  currentLed = random(0, numLeds());                            // pick 0-7
  digitalWrite(ledPins[currentLed], LOW);                       // light new LED
  Serial.print(F("LED ")); Serial.print(currentLed); Serial.println(F(" on"));
}

// ── button checks ────────────────────────────────────────────
void checkGameButtons() {
  for (uint8_t i = 0; i < numBtns(); ++i) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(5);                                               // debounce
      if (digitalRead(buttonPins[i]) == LOW) {
        if (i == currentLed) {
          Keyboard.write('1');
          nextLed();
        }
        while (digitalRead(buttonPins[i]) == LOW);             // wait release
      }
    }
  }
}
