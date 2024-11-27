#include <Wire.h>
// #include <Keyboard.h>  // Arduino Nano does not support Keyboard library natively

// Updated Pins for 14 switches on Arduino Nano
// 13 game buttons and 1 reset button
const int buttonPins[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0, A1 };
const int resetButtonPin = A1;  // Reset button is the last button (index 13)
const int numButtons = sizeof(buttonPins) / sizeof(buttonPins[0]);  // Total number of buttons

int lastButtonState[numButtons];  // Store previous states to detect button presses

bool gameActive = false;
unsigned long gameStartTime = 0;
unsigned long gameDuration = 30000;  // 30 seconds for testing purposes
int currentLed = -1;
int score = 0;
const byte I2C_ADDRESS_DEVICE1 = 0x08;  // Replace with your first device's address
const byte I2C_ADDRESS_DEVICE2 = 0x09;  // Replace with your second device's address
void setup() {
  Wire.begin();        // Join I2C bus as Master
  Serial.begin(9600);  // Start serial communication for debugging
  // Keyboard.begin();    // Not supported on Arduino Nano

  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);  // Setup all buttons as input with pullup
    lastButtonState[i] = HIGH;             // Initialize button states as unpressed
  }

  randomSeed(analogRead(0));  // Seed random number generator
}

void loop() {
  // Read the current state of the reset button
  int currentResetButtonState = digitalRead(resetButtonPin);

  // Check for reset button press (LOW indicates pressed)
  if (currentResetButtonState == LOW && lastButtonState[13] == HIGH) {  // Reset button is at index 13
    delay(10);  // Debounce delay
    // Re-read to confirm it's still pressed
    if (digitalRead(resetButtonPin) == LOW) {
      if (gameActive) {
        stopGame();  // Stop the game if reset is pressed while active
      } else {
        Serial.println("Game Started");
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
  lastButtonState[13] = currentResetButtonState;  // Update reset button state
}

// Start the game
void startGame() {
  Serial.println("Sending Start Command");
  sendCommand(20);
  Serial.println("Sent Start Command");
  delay(500);

  gameActive = true;
  score = 0;
  gameStartTime = millis();
  
  selectRandomLed();
  Serial.println("Game started");
  
  // Keyboard.write('2');  // Not supported on Arduino Nano
}

// Stop the game
void stopGame() {
  gameActive = false;
  currentLed = -1;
  sendCommand(21);  // Send reset command to turn off all LEDs (assuming 14 represents reset)
  Serial.print("Game stopped. Final score: ");
  Serial.println(score);
  //sendCommand(21);
  // Keyboard.write('3');  // Not supported on Arduino Nano
}3

// Select a random LED to turn on
void selectRandomLed() {
  currentLed = random(0, 13);  // Random LED from 0 to 12 (13 game buttons)
  sendCommand(currentLed);     // Send LED index to Nano
  Serial.print("LED ");
  Serial.print(currentLed);
  Serial.println(" on");
}

// Check if any game button is pressed
void checkGameButtons() {
  for (int i = 0; i < numButtons - 1; i++) {  // Only check game buttons (0 to 12)
    int buttonState = digitalRead(buttonPins[i]);

    if (buttonState == LOW && lastButtonState[i] == HIGH) {  // Button pressed
      delay(50);  // Debounce delay
      Serial.println(buttonState);
      Serial.println(i);
      Serial.println(buttonPins[i]);
      Serial.println("LED: " + String(currentLed));
      Serial.println("-------------------------");
      if (digitalRead(buttonPins[i]) == LOW && i == currentLed) {  // Correct button
        score++;  // Increase score
        Serial.print("Correct button! Score: ");
        Serial.println(score);
        selectRandomLed();  // Select a new random LED
        // Keyboard.write('1');  // Not supported on Arduino Nano
      }
    }
    lastButtonState[i] = buttonState;  // Update last button state
  }
}

void sendCommand(int buttonIndex) {
  // Send to Device 1
  Wire.beginTransmission(8);
  Wire.write(buttonIndex);  // Send button index or reset command
  byte error1 = Wire.endTransmission();
  if (error1 != 0) {
    Serial.print("Error sending to Device 1 at address 0x");
    Serial.println(8, HEX);
  } else {
    Serial.print("Sent Data to Device 1 (0x");
    Serial.print(8, HEX);
    Serial.print("): ");
    Serial.println(buttonIndex);
  }

  // Send to Device 2
  Wire.beginTransmission(9);
  Wire.write(buttonIndex);  // Send button index or reset command
  byte error2 = Wire.endTransmission();
  if (error2 != 0) {
    Serial.print("Error sending to Device 2 at address 0x");
    Serial.println(9, HEX);
  } else {
    Serial.print("Sent Data to Device 2 (0x");
    Serial.print(9, HEX);
    Serial.print("): ");
    Serial.println(buttonIndex);
  }
}
