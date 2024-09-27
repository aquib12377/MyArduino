#include <Keyboard.h>

const int ledPins[] = { 10,16,14,15,A0,A1,A2,A3}; //Buttons connected to digital pins 10-17
const int buttonPins[] = { 2, 3, 4, 5, 6, 7, 8, 9 }; // LEDs connected to digital pins 2-9

const int startButton = 2;  // Start button pin

bool gameActive = false;
bool gamePaused = false;
unsigned long startTime = 0;
const unsigned long gameDuration = 60000;  // 1 minute in milliseconds

int currentLed = -1;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  for (int i = 0; i < 8; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  //randomSeed(analogRead(0));  // Seed the random number generator
}

void loop() {
  // while(true)
  // {
  //   for(int i = 0; i < 8; i++)
  //   {
  //       digitalWrite(ledPins[i],HIGH);
  //       delay(1000);
  //   }

  //   for(int i = 0; i < 8; i++)
  //   {
  //       digitalWrite(ledPins[i],LOW);
  //   }
  // }
  if (digitalRead(startButton) == LOW) {
    delay(50);  // Debounce delay
    if (digitalRead(startButton) == LOW) {
      if (!gameActive) {
        startGame();
      } else {
        endGame();
      }
      while (digitalRead(startButton) == LOW)
        ;  // Wait for button release
    }
  }

  if (gameActive && !gamePaused) {
    if (millis() - startTime >= gameDuration) {
      //Serial.println("Left Time: " + String(millis() - startTime));
      endGame();
    } else {
      //Serial.println("Left Time: " + String(millis() - startTime));
      checkGameButtons();
    }
  }
}

void startGame() {
  Keyboard.press('5');
          delay(50);  // Ensure the key press is registered
          Keyboard.release('5');
  gameActive = true;
  gamePaused = false;
  startTime = millis();
  nextLed();
  Serial.println("Game started");
}

void pauseGame() {
  gamePaused = true;
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  Serial.println("Game paused");
}

void resumeGame() {
  gamePaused = false;
  digitalWrite(ledPins[currentLed], HIGH);
  Serial.println("Game resumed");
}

void stopGame() {
  Keyboard.press('3');
          delay(50);  // Ensure the key press is registered
          Keyboard.release('3');
  gameActive = false;
  gamePaused = false;
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  Serial.println("Game stopped");
}

void endGame() {
  Keyboard.press('3');
          delay(50);  // Ensure the key press is registered
          Keyboard.release('3');
  gameActive = false;
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  Serial.println("Game ended");
}

void nextLed() {
  if (currentLed != -1) {
    digitalWrite(ledPins[currentLed], LOW);
  }
  currentLed = random(1, 8);  // Select a random LED from the 7 game LEDs
  digitalWrite(ledPins[currentLed], HIGH);
  Serial.print("LED ");
  Serial.print(currentLed);
  Serial.println(" on");
}

void checkGameButtons() {
  for (int i = 1; i < 8; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(50);  // Debounce delay
      if (digitalRead(buttonPins[i]) == LOW) {
        if (i == currentLed) {
          Keyboard.press('1');
          delay(50);  // Ensure the key press is registered
          Keyboard.release('1');
          nextLed();
        }
        
      }
    }
  }
}
