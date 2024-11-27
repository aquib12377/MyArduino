#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {7, 8, 9, 10}; 
byte colPins[COLS] = {3, 4, 5, 6}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 4);

// Relay pins
const int solenoidRelayPin = A2;
const int pumpRelayPin = A3;

// Flow sensor
const int flowSensorPin = 2; // Interrupt pin
volatile int flowPulseCount = 0;
float calibrationFactor = 4.5; // Set after calibration

// Start/Repeat button
const int startButtonPin = 12;
bool lastButtonState = HIGH;
bool repeatLast = false;

// Variables
float desiredLiters = 0.0;
float lastDispensedLiters = 0.0;
bool dispensing = false;

// Timing
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Flow sensor interrupt
void flowPulse() {
  flowPulseCount++;
  // Optional: Uncomment the next line to debug flow pulses
  // Serial.println("Flow pulse detected.");
}

void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  Serial.println("=== Oil Dispenser Initialized ===");
  
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Oil Dispenser");
  lcd.setCursor(0, 1);
  lcd.print("Enter Litres: ");
  Serial.println("LCD initialized.");
  // delay(2000);
  // lcd.clear();
  // Initialize relay pins
  pinMode(solenoidRelayPin, OUTPUT);
  pinMode(pumpRelayPin, OUTPUT);
  digitalWrite(solenoidRelayPin, LOW);
  digitalWrite(pumpRelayPin, LOW);
  Serial.println("Relay pins initialized and set to LOW.");

  // Initialize flow sensor
  pinMode(flowSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING); // Setup Interrupt
  Serial.println("Flow sensor initialized with interrupt.");

  // Initialize start button
  pinMode(startButtonPin, INPUT_PULLUP);
  Serial.println("Start/Repeat button initialized.");

  // Initial states
  Serial.println("Setup complete. Awaiting user input.");
}

void loop() {
  // Handle keypad input
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    
    if (key >= '0' && key <= '9') {
      desiredLiters = desiredLiters * 10 + (key - '0');
      lcd.setCursor(-4, 2);
      lcd.print("L: " + String(desiredLiters) + "");
      Serial.print("Updated desiredLiters: ");
      Serial.println(desiredLiters);
    }
    else if (key == 'A') { // Example: 'A' to clear input
      desiredLiters = 0.0;
      lcd.setCursor(0, 2);
      lcd.print("L: 0.0   ");
      Serial.println("Desired liters cleared.");
    }
    // Add more key functionalities if needed
  }

  // Read the state of the start button
  int buttonState = digitalRead(startButtonPin);
  
  // Debug: Print button state
  // Serial.print("Start Button State: ");
  // Serial.println(buttonState);

  // Debounce button press
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
    Serial.println("Start button state changed.");
    delay(50);
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (buttonState == LOW && lastButtonState == HIGH) { // Button pressed
      Serial.println("Start button pressed.");
      
      if (!dispensing) {
        if (desiredLiters > 0) {
          startDispensing(desiredLiters);
          lastDispensedLiters = desiredLiters;
          Serial.print("Starting dispensing of ");
          Serial.print(desiredLiters);
          Serial.println(" liters.");
          
          desiredLiters = 0.0;
          lcd.setCursor(0, 2);
          lcd.print("Litres: 0.0   ");
          Serial.println("Desired liters reset to 0.");
        }
        else if (lastDispensedLiters > 0 && repeatLast) {
          startDispensing(lastDispensedLiters);
          Serial.print("Repeating last dispense of ");
          Serial.print(lastDispensedLiters);
          Serial.println(" liters.");
        }
        else {
          lcd.setCursor(0, 3);
          lcd.print("Set litres first! ");
          Serial.println("Dispensing aborted: Desired liters not set.");
          delay(1000);
          lcd.setCursor(0, 3);
          lcd.print("                  ");
        }
      }
      else {
        // Optionally, implement stop dispensing
        Serial.println("Dispensing is already in progress.");
      }
    }
  }

  lastButtonState = buttonState;

  // Update dispensing status
  if (dispensing) {
    float litersDispensed = (flowPulseCount / calibrationFactor) / 1000.0; // Adjust as per calibration
    Serial.print("Liters dispensed so far: ");
    Serial.println(litersDispensed);

    if (litersDispensed >= desiredLiters) {
      stopDispensing();
      lcd.setCursor(-4, 3);
      lcd.print("Dispensed: " + String(litersDispensed) + "L ");
      Serial.print("Dispensing complete. Total dispensed: ");
      Serial.print(litersDispensed);
      Serial.println(" liters.");
      delay(2000);
      lcd.setCursor(-4, 3);
      lcd.print("                  ");
    }
  }

  // Optionally, display real-time flow
  
  if (dispensing) {
    float litersDispensed = (flowPulseCount / calibrationFactor) / 1000.0;
    lcd.setCursor(-4, 3);
    lcd.print("Dispensed: " + String(litersDispensed) + "L ");
  }
  
}

void startDispensing(float liters) {
  dispensing = true;
  flowPulseCount = 0;
  digitalWrite(solenoidRelayPin, HIGH);
  digitalWrite(pumpRelayPin, HIGH);
  lcd.setCursor(-4, 3);
  lcd.print("Dispensing...      ");
  Serial.println("Dispensing started: Relays activated.");
}

void stopDispensing() {
  dispensing = false;
  digitalWrite(solenoidRelayPin, LOW);
  digitalWrite(pumpRelayPin, LOW);
  Serial.println("Dispensing stopped: Relays deactivated.");
}
