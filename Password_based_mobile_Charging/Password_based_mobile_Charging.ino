/****************************************************
   Arduino Sketch: 4x4 Keypad, Relay Control, 
   and Voltage Measurement on I2C LCD
****************************************************/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// --------------------------------------------------
// 1. I2C LCD Configuration
// --------------------------------------------------
#define LCD_I2C_ADDR 0x27   // Change to match your display's address
#define LCD_COLUMNS 16
#define LCD_ROWS    2

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

// --------------------------------------------------
// 2. Keypad Configuration
// --------------------------------------------------
const byte ROWS = 4;
const byte COLS = 4;

// Define the symbols on the keypad (adjust if yours is different)
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Connect keypad ROW0, ROW1, ROW2, ROW3 to these Arduino pins:
byte rowPins[ROWS] = {12,11,10,9};
// Connect keypad COL0, COL1, COL2, COL3 to these Arduino pins:
byte colPins[COLS] = {8,7,6,5};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --------------------------------------------------
// 3. Relay Configuration
// --------------------------------------------------
const int relayPin = 2;            // Digital pin for relay
bool relayOn = false;               // Track relay status
unsigned long relayActivatedTime = 0;
const unsigned long RELAY_ON_DURATION = 15000; // 15 seconds

// --------------------------------------------------
// 4. Password Configuration
// --------------------------------------------------
const String correctPassword = "1234"; // Change as desired
String enteredPassword = "";

// --------------------------------------------------
// 5. Voltage Measurement Configuration
// --------------------------------------------------
#define ANALOG_IN_PIN A3
float adc_voltage = 0.0; 
float in_voltage = 0.0; 
float R1 = 30000.0;  // Resistor R1 (Ohms)
float R2 = 7500.0;   // Resistor R2 (Ohms)
float ref_voltage = 5.0; // Reference Voltage (5V on typical Arduino)
int adc_value = 0;

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup() {
  Serial.begin(9600);         // For debug output if needed

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  // Set up relay pin
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  // Initial LCD message
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
}

// --------------------------------------------------
// Main Loop
// --------------------------------------------------
void loop() {
  // Read keypad
  char key = keypad.getKey();
  if (key) {
    handleKeypadInput(key);
  }

  // Check relay timeout
  if (relayOn) {
    unsigned long currentMillis = millis();
    // Turn relay off after 15 seconds
    if (currentMillis - relayActivatedTime >= RELAY_ON_DURATION) {
      digitalWrite(relayPin, HIGH);
      relayOn = false;
    }
  }

  // Measure voltage and update LCD
  measureVoltage();
  displayVoltage();
}

// --------------------------------------------------
// Handle Keypad Input
// --------------------------------------------------
void handleKeypadInput(char key) {
  // '#' = Clear the entered password
  if (key == '#') {
    enteredPassword = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Password:");
    return;
  }

  // '*' = Check password
  if (key == '*') {
    checkPassword();
    return;
  }

  // Append pressed key to enteredPassword
  if (enteredPassword.length() < 16) {
    enteredPassword += key;
  }

  // Show typed password so far
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Password: ");
  lcd.print(enteredPassword);
}

// --------------------------------------------------
// Check Password
// --------------------------------------------------
void checkPassword() {
  if (enteredPassword == correctPassword) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Granted!");
    digitalWrite(relayPin, LOW);
    relayOn = true;
    relayActivatedTime = millis();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong Password!");
  }
  
  // Reset password and prompt again
  enteredPassword = "";
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
}

// --------------------------------------------------
// Measure Voltage (from your original example code)
// --------------------------------------------------
void measureVoltage() {
  // Read the Analog Input
  adc_value = analogRead(ANALOG_IN_PIN);
  
  // Determine voltage at ADC input
  adc_voltage  = (adc_value * ref_voltage) / 1024.0;
  
  // Calculate actual input voltage based on R1/R2 divider
  in_voltage = adc_voltage * (R1 + R2) / R2;

  // Print results to Serial Monitor (optional)
  Serial.print("Input Voltage = ");
  Serial.println(in_voltage, 2);
}

// --------------------------------------------------
// Display Voltage on LCD (second line)
// --------------------------------------------------
void displayVoltage() {
  // Show voltage on bottom row
  lcd.setCursor(0, 1);
  lcd.print("Volt: ");
  lcd.print(in_voltage, 2); // display with 2 decimals
  lcd.print(" V   ");
}
