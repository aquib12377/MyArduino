#include <Keyboard.h>

const int ledPins[] = { 10, 16, 14, 15, A0, A1 };  // LEDs for 6 game buttons
const int buttonPins[] = { 3, 4, 5, 6, 7, 8 };     // 6 game buttons
const int resetButton = 2;  // Reset button on pin 2
const int resetLED = A2;
bool gameActive = false;
bool gamePaused = false;
unsigned long startTime = 0;
const unsigned long gameDuration = 30000;  // 30 seconds in milliseconds

int currentLed = -1;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  for (int i = 0; i < 6; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
   pinMode(resetLED, OUTPUT);
   digitalWrite(resetLED, LOW);
  pinMode(resetButton, INPUT_PULLUP);  // Set the reset button as an input
}

void loop() {
  if (digitalRead(resetButton) == LOW) {
    delay(50);  // Debounce delay
    if (digitalRead(resetButton) == LOW) {
      if (!gameActive) {
        startGame();
      } else {
        endGame();
      }
      while (digitalRead(resetButton) == LOW);  // Wait for button release
    }
  }

  if (gameActive && !gamePaused) {
    if (millis() - startTime >= gameDuration) {
      endGame();
    } else {
      checkGameButtons();
    }
  }
}

void startGame() {
  Keyboard.press('5');
  delay(50);
  Keyboard.release('5');
  gameActive = true;
  gamePaused = false;
  startTime = millis();
  nextLed();
  Serial.println("Game started");
  digitalWrite(resetLED, HIGH);
}

void endGame() {
  Keyboard.press('3');
  delay(50);
  Keyboard.release('3');
  gameActive = false;
  for (int i = 0; i < 6; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  digitalWrite(resetLED, LOW);
  Serial.println("Game ended");
}

void nextLed() {
  if (currentLed != -1) {
    digitalWrite(ledPins[currentLed], HIGH);
  }
  currentLed = random(0, 6);  // Select a random LED from the 6 game LEDs
  digitalWrite(ledPins[currentLed], LOW);
  Serial.print("LED ");
  Serial.print(currentLed);
  Serial.println(" on");
}

void checkGameButtons() {
  for (int i = 0; i < 6; i++) {  // Loop through all 6 game buttons
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(50);  // Debounce delay
      if (digitalRead(buttonPins[i]) == LOW) {
        if (i == currentLed) {
          Keyboard.press('1');
          delay(5);
          Keyboard.release('1');
          nextLed();
        }
      }
    }
  }
}
