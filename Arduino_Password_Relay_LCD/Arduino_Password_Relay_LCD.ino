#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

#define RELAY_PIN A0
#define PASSWORD_LENGTH 5  // 4 digits + null terminator

char correctPassword[] = "1234";
char enteredPassword[PASSWORD_LENGTH];
byte passwordIndex = 0;
bool relayState = false; // false = OFF, true = ON

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad setup for 4x3 layout
const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {2, 3, 4, 5}; // Rows
byte colPins[COLS] = {6, 7, 8};    // Columns

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF initially (active LOW)

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    Serial.print("Key Pressed: ");
    Serial.println(key);

    if (key == '*') {
      resetPasswordEntry();
      return;
    }

    if (passwordIndex < PASSWORD_LENGTH - 1 && key >= '0' && key <= '9') {
      enteredPassword[passwordIndex++] = key;
      enteredPassword[passwordIndex] = '\0';

      // Show masked input
      lcd.setCursor(0, 1);
      for (byte i = 0; i < passwordIndex; i++) {
        lcd.print("*");
      }
      for (byte i = passwordIndex; i < 4; i++) {
        lcd.print(" ");
      }

      if (passwordIndex == PASSWORD_LENGTH - 1) {
        if (strcmp(enteredPassword, correctPassword) == 0) {
          Serial.println("Password Correct!");
          toggleRelay();
        } else {
          Serial.println("Wrong Password!");
          lcd.setCursor(0, 1);
          lcd.print("Wrong Password! ");
          delay(2000);
        }
        resetPasswordEntry();
      }
    }
  }
}

void toggleRelay() {
  if (!relayState) {
    digitalWrite(RELAY_PIN, LOW); // Turn ON relay (Active LOW)
    relayState = true;
    lcd.setCursor(0, 1);
    lcd.print("Relay ON         ");
    Serial.println("Relay Turned ON");
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Turn OFF relay
    relayState = false;
    lcd.setCursor(0, 1);
    lcd.print("Relay OFF        ");
    Serial.println("Relay Turned OFF");
  }
  delay(1000); // Optional: Pause to show status before resetting prompt
}

void resetPasswordEntry() {
  memset(enteredPassword, 0, PASSWORD_LENGTH);
  passwordIndex = 0;
  lcd.setCursor(0, 0);
  lcd.print("Enter Password: ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}
