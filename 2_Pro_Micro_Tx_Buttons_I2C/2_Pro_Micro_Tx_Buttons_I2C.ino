#include <Wire.h>
#include <Keyboard.h>  // Include Keyboard library

// Pins for 13 switches (Removed button at pin 16)
const int buttonPins[] = {4, 5, 6, 7, 8, 9, 10, 14, 15, A0, A1, A2, A3}; 
const int resetButtonPin = A3;  // Reset button
int lastButtonState[13];  // Store previous states to detect button presses

bool gameActive = false;
unsigned long gameStartTime = 0;
unsigned long gameDuration = 30000;  // 60 seconds for testing purposes
int currentLed = -1;
int score = 0;

void setup() {
  Wire.begin();  // Join I2C bus as Master
  Serial.begin(9600);  // Start serial communication for debugging
  Keyboard.begin();  // Start Keyboard functionality
  
  for (int i = 0; i < 13; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);  // Setup all buttons as input with pullup
    lastButtonState[i] = HIGH;  // Initialize button states as unpressed
  }
  
  randomSeed(analogRead(0));  // Seed random number generator
}

void loop() {
  // Read the current state of the reset button
  int currentResetButtonState = digitalRead(resetButtonPin);

  // Check for reset button press (LOW indicates pressed)
  if (currentResetButtonState == LOW && lastButtonState[12] == HIGH) {  // Last button in array is now at index 12
    delay(10);  // Debounce delay
    // Re-read to confirm it's still pressed
    if (digitalRead(resetButtonPin) == LOW) {
      if (gameActive) {
        stopGame();  // Stop the game if reset is pressed while active
      } else {
        startGame();  // Start the game if reset is pressed when inactive
      }
    }
  }

  // Game logic when active
  if (gameActive) {
    if (millis() - gameStartTime >= gameDuration) {
      stopGame();  // Stop the game after the duration is up
    } else {
      //Serial.println("Time elapsed: " + String(millis() - gameStartTime) + " ms");
      checkGameButtons();  // Check for game button presses
    }
  }

  // Update the last state of the reset button
  lastButtonState[12] = currentResetButtonState;  // Adjusted index
}

// Start the game
void startGame() {
  gameActive = true;
  score = 0;
  gameStartTime = millis();
  selectRandomLed();
  Serial.println("Game started");
  Keyboard.write('2');  // Send key press '2' for game start
}

// Stop the game
void stopGame() {
  gameActive = false;
  currentLed = -1;
  sendCommand(13);  // Send reset command to turn off all LEDs
  Serial.print("Game stopped. Final score: ");
  Serial.println(score);
  Keyboard.write('3');  // Send key press '3' for game end
}

// Select a random LED to turn on
void selectRandomLed() {
  currentLed = random(0, 12);  // Random LED from 0 to 11
  sendCommand(currentLed);  // Send LED index to Pro Micro 2
  Serial.print("LED ");
  Serial.print(currentLed);
  Serial.println(" on");
}

// Check if any game button is pressed
void checkGameButtons() {
  for (int i = 0; i < 12; i++) {  // Only check game buttons (0 to 11)
    int buttonState = digitalRead(buttonPins[i]);
    
    if (buttonState == LOW && lastButtonState[i] == HIGH) {  // Button pressed
      delay(50);  // Debounce delay
      Serial.println(buttonState);
      Serial.println(i);
      Serial.println(buttonPins[i]);
      Serial.println("LED: "+String(currentLed));
      Serial.println("-------------------------");
      if (digitalRead(buttonPins[i]) == LOW && i == currentLed) {  // Correct button
        score++;  // Increase score
        Serial.print("Correct button! Score: ");
        Serial.println(score);
        selectRandomLed();  // Select a new random LED
        Keyboard.write('1');  // Send key press '1' for each score
      }
    }
    lastButtonState[i] = buttonState;  // Update last button state
  }
}

// Send command to Pro Micro 2 via I2C
void sendCommand(int buttonIndex) {
  Wire.beginTransmission(8);  // Address of Pro Micro 2 (Slave)
  Wire.write(buttonIndex);  // Send button index or reset command
  Wire.endTransmission();
  Serial.println("Sent Data: "+String(buttonIndex));
}
