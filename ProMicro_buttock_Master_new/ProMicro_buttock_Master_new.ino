#include <Wire.h>
#include <Keyboard.h>

const byte BUTTON_READER_ADDRESS = 0x08;
const byte LED_CONTROLLER_ADDRESS = 0x09;

bool gameActive = false;
unsigned long startTime = 0;
const unsigned long gameDuration = 60000;  // 30 seconds

int currentLed = -1;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  Wire.begin();  // Initialize as I2C master
  Wire.setClock(400000);  // Set I2C clock speed to 400 kHz (Fast Mode)

    sendCommandToLedController(13);  // Activate LED

}

void loop() {
  // Check for reset button press
  if (readResetButton()) {
    if (!gameActive) {
      startGame();
    } else {
      endGame();
    }
    delay(10);  // Debounce
  }

  // Handle game logic
  if (gameActive) {
    if (millis() - startTime >= gameDuration) {
      endGame();
    } else {
      checkGameButtons();
    }
  }
}

void startGame() {
  Keyboard.press('5');
  delay(5);
  Keyboard.release('5');
  gameActive = true;
  startTime = millis();
  selectNextLed();
  Serial.println("Game started");
}

void endGame() {
  Keyboard.press('3');
  delay(5);
  Keyboard.release('3');
  gameActive = false;
  sendCommandToLedController(13);  // Turn off all LEDs
  Serial.println("Game ended");
}

void selectNextLed() {
  currentLed = random(0, 13);  // Select a random LED (0-12)
  sendCommandToLedController(currentLed);  // Activate LED
  Serial.print("LED ");
  Serial.print(currentLed);
  Serial.println(" activated");
}

void checkGameButtons() {
  int buttonStates[14];
  readButtonStates(buttonStates);

  if (buttonStates[currentLed] == 0) {  // Correct button pressed
    Keyboard.press('1');
    delay(5);
    Keyboard.release('1');
    selectNextLed();  // Select the next LED
  }
}

bool readResetButton() {
  int buttonStates[14];
  readButtonStates(buttonStates);
  delay(100);
  return buttonStates[13] == 0;  // A1 (index 13) is the reset button
}

void readButtonStates(int *buttonStates) {
  Wire.requestFrom(BUTTON_READER_ADDRESS, 14);  // Request button states
  for (int i = 0; i < 14; i++) {
    buttonStates[i] = Wire.read();
  }
}

void sendCommandToLedController(int command) {
  Wire.beginTransmission(LED_CONTROLLER_ADDRESS);
  Wire.write(command);
  Wire.endTransmission();
}
