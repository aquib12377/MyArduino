#include <Wire.h>                // For I2C communication
#include <LiquidCrystal_I2C.h>   // LCD library
#include <SoftwareSerial.h>      // Bluetooth communication library

// Pin definitions
const int buttonPins[] = {2, 3, 4, 5, 6, 7}; // Push button pins
const int ledPins[] = {8, 9, 10, 11, 12, 13}; // LED pins
unsigned long lastPressTime = 0;              // Last valid button press time
const unsigned long cooldownDuration = 30000; // Cooldown duration (30 seconds)

// Variables for voting and state management
int voteCounts[] = {0, 0, 0, 0, 0, 0};       // Vote counts for each candidate
bool buttonStates[] = {false, false, false, false, false, false}; // Button press states

// Bluetooth setup
SoftwareSerial BTSerial(10, 11); // RX (Arduino TX) and TX (Arduino RX)
const long bluetoothBaudRate = 9600; // Default baud rate

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust address if needed

void setup() {
  // Setup button pins as inputs with pull-up resistors
  for (int i = 0; i < 6; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW); // Ensure LEDs are off initially
  }

  // Initialize Bluetooth and serial communication
  BTSerial.begin(bluetoothBaudRate);
  Serial.begin(9600); // Debugging

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.print("Voting Machine");
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long currentTime = millis();

  // Cooldown mechanism
  if (currentTime - lastPressTime < cooldownDuration) {
    return; // Prevent button presses during cooldown
  }

  // Check for Bluetooth commands
  if (BTSerial.available()) {
    char command = BTSerial.read();
    if (command >= '1' && command <= '6') {
      int candidate = command - '1';
      if (voteCounts[candidate] > 0) {
        voteCounts[candidate]--; // Decrease vote count
        Serial.print("Candidate ");
        Serial.print(candidate + 1);
        Serial.println(" count decremented");
      } else {
        Serial.print("Candidate ");
        Serial.print(candidate + 1);
        Serial.println(" count already 0");
      }
    }
  }

  // Check button presses
  for (int i = 0; i < 6; i++) {
    bool isPressed = !digitalRead(buttonPins[i]); // Button pressed = LOW

    if (isPressed && !buttonStates[i]) { // New button press detected
      buttonStates[i] = true;            // Register the press
      lastPressTime = currentTime;       // Start cooldown
      voteCounts[i]++;                   // Increment vote count

      if (voteCounts[i] >= 2) {          // Turn LED on if votes >= 2
        digitalWrite(ledPins[i], HIGH);
      }
    } else if (!isPressed && buttonStates[i]) { // Button released
      buttonStates[i] = false;
    }
  }

  // Update LCD display with vote counts
  updateLCD();
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("C1:");
  lcd.print(voteCounts[0]);
  lcd.print(" C2:");
  lcd.print(voteCounts[1]);
  lcd.setCursor(0, 1);
  lcd.print("C3:");
  lcd.print(voteCounts[2]);
  lcd.print(" C4:");
  lcd.print(voteCounts[3]);
  delay(2000); // Wait before switching to next set

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("C5:");
  lcd.print(voteCounts[4]);
  lcd.print(" C6:");
  lcd.print(voteCounts[5]);
  delay(2000); // Repeat
}