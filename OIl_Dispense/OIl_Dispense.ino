#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -----------------------------
// Constants and Definitions
// -----------------------------

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 7, 8, 9, 10 };  // Connect to the row pinouts of the keypad
byte colPins[COLS] = { 3, 4, 5, 6 };   // Connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD configuration
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Set the LCD I2C address, 20 columns and 4 rows

// Relay pins
const int solenoidRelayPin = A2;  // Relay controlling the solenoid valve
const int pumpRelayPin = A3;      // Relay controlling the AC pump

// Flow sensor configuration
byte sensorInterrupt = 0;  // 0 = digital pin 2
byte sensorPin = 2;        // Pin for flow sensor input
float calibrationFactor = 4.5;  // Pulses per second per litre/minute

volatile byte pulseCount;       // Pulse count from the flow sensor
float flowRate;                 // Calculated flow rate in L/min
unsigned int flowMilliLitres;   // Flow in milliliters for each second
unsigned long totalMilliLitres; // Total flow in milliliters since dispensing started
unsigned long oldTime = 0;      // Timer for flow calculations

// Start/Repeat button configuration
const int startButtonPin = 12;  // Pin connected to the start/repeat button
bool lastButtonState = HIGH;    // Previous state of the button
bool repeatLast = false;        // Flag to indicate repeat dispensing

// Dispensing variables
float desiredLiters = 0.0;        // Liters entered by the user
float lastDispensedLiters = 0.0;  // Last dispensed liters
bool dispensing = false;          // Flag to indicate if dispensing is active

// Emergency stop configuration
const int emergencyPin = 11;     // Pin for the emergency stop switch
bool emergencyStop = false;      // Flag for emergency stop

// Timing variables
unsigned long lastDebounceTime = 0;        // Last time the button state was toggled
const unsigned long debounceDelay = 50;    // Debounce delay in milliseconds
unsigned long lastLCDUpdate = 0;           // Last time the LCD was updated
unsigned long lastEmergencyCheck = 0;      // Last time emergency stop was checked
const unsigned long lcdUpdateInterval = 500; // Update LCD every 500ms
const unsigned long emergencyCheckInterval = 100; // Check emergency every 100ms

// -----------------------------
// Function Prototypes
// -----------------------------
void flowPulse();
void startDispensing(float liters);
void stopDispensing();
void updateLCD();
void displayError(const char* message);
void checkEmergencyStop();
void pulseCounter();  // Interrupt handler for flow sensor

// -----------------------------
// Setup Function
// -----------------------------
void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  Serial.println("=== Oil Dispenser Initialized ===");
  Initialize();
}

void Initialize() {
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Oil Dispenser");
  lcd.setCursor(0, 1);
  lcd.print("Enter Litres: ");
  Serial.println("LCD initialized.");

  // Initialize relay pins
  pinMode(solenoidRelayPin, OUTPUT);
  pinMode(pumpRelayPin, OUTPUT);
  digitalWrite(solenoidRelayPin, HIGH);
  digitalWrite(pumpRelayPin, HIGH);
  Serial.println("Relay pins initialized and set to LOW.");

  // Initialize flow sensor
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);  // Internal pull-up
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);  // Setup Interrupt
  Serial.println("Flow sensor initialized with interrupt.");

  // Initialize start/repeat button
  pinMode(startButtonPin, INPUT_PULLUP);
  Serial.println("Start/Repeat button initialized.");

  // Initialize emergency stop switch
  pinMode(emergencyPin, INPUT_PULLUP);
  Serial.println("Emergency stop switch initialized.");

  // Initial states
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  oldTime = millis();
  Serial.println("Setup complete. Awaiting user input.");
}

// -----------------------------
// Main Loop Function
// -----------------------------
void loop() {
  // Check if the emergency stop switch is pressed
  unsigned long currentMillis = millis();

  if (currentMillis - lastEmergencyCheck >= emergencyCheckInterval) {
    lastEmergencyCheck = currentMillis;
    checkEmergencyStop();
  }

  // Handle keypad input
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key >= '0' && key <= '9') {
      desiredLiters = desiredLiters * 10 + (key - '0');
      updateLCD();
      Serial.print("Updated desiredLiters: ");
      Serial.println(desiredLiters);
    } else if (key == 'A') {  // 'A' to clear input
      desiredLiters = 0.0;
      updateLCD();
      Serial.println("Desired liters cleared.");
    } else if (key == 'D') {  // 'D' to toggle repeat
      repeatLast = !repeatLast;
      Serial.print("Repeat last dispense set to: ");
      Serial.println(repeatLast);
      lcd.setCursor(-4, 3);
      if (repeatLast) {
        lcd.print("Repeat: ON          ");
      } else {
        lcd.print("Repeat: OFF         ");
      }
    }
  }

  // Read the state of the start/repeat button
  int buttonState = digitalRead(startButtonPin);

  // Debounce button press
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
    Serial.println("Start/Repeat button state changed.");
  }

  if (buttonState == LOW && lastButtonState == HIGH) {  // Button pressed
    Serial.println("Start/Repeat button pressed.");

    if (!dispensing && !emergencyStop) {
      if (desiredLiters > 0) {
        startDispensing(desiredLiters);
        lastDispensedLiters = desiredLiters;
        desiredLiters = 0.0;
        updateLCD();
        Serial.print("Starting dispensing of ");
        Serial.print(lastDispensedLiters);
        Serial.println(" liters.");
      } else if (lastDispensedLiters > 0 && repeatLast) {
        startDispensing(lastDispensedLiters);
        Serial.print("Repeating last dispense of ");
        Serial.print(lastDispensedLiters);
        Serial.println(" liters.");
      } else {
        displayError("Set litres first!");
        Serial.println("Dispensing aborted: Desired liters not set.");
      }
    } else {
      Serial.println("Dispensing is already in progress or emergency stop triggered.");
    }
  }

  lastButtonState = buttonState;

  // Update dispensing status and LCD every second
  if (dispensing && (millis() - oldTime) > 1000) {
    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(sensorInterrupt);

    // Calculate flow rate in L/min
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;

    // Reset the timer for the next second
    oldTime = millis();

    // Calculate the flow in milliliters
    flowMilliLitres = (flowRate / 60) * 1000;
    
    // Update total milliliters dispensed
    totalMilliLitres += flowMilliLitres;
    
    // Update LCD with current flow information
    updateLCD();
    
    // Print debug info
    Serial.print("Flow rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");

    Serial.print("Total dispensed: ");
    Serial.print(totalMilliLitres);
    Serial.println(" mL");

    // Reset pulse counter
    pulseCount = 0;

    // Re-enable the interrupt after calculations
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);

    // Stop dispensing if the target volume has been dispensed
    if (totalMilliLitres >= lastDispensedLiters * 1000) {
      stopDispensing();
      lcd.setCursor(-4, 3);
      lcd.print("                    ");
      lcd.setCursor(-4, 3);
      lcd.print("Dispensing complete ");
      Serial.println("Dispensing complete.");
    }
  }
}

// -----------------------------
// Interrupt Service Routine
// -----------------------------
void pulseCounter() {
  pulseCount++;  // Increment the pulse counter
}

// -----------------------------
// Start Dispensing Function
// -----------------------------
void startDispensing(float liters) {
  dispensing = true;
  totalMilliLitres = 0;
  pulseCount = 0;
  digitalWrite(solenoidRelayPin, LOW);  // Activate solenoid valve
  digitalWrite(pumpRelayPin, LOW);      // Activate pump
  lcd.setCursor(-4, 3);
  lcd.print("Dispensing...      ");
  Serial.println("Dispensing started: Relays activated.");
}

// -----------------------------
// Stop Dispensing Function
// -----------------------------
void stopDispensing() {
  dispensing = false;
  digitalWrite(solenoidRelayPin, HIGH);  // Deactivate solenoid valve
  digitalWrite(pumpRelayPin, HIGH);      // Deactivate pump
  Serial.println("Dispensing stopped: Relays deactivated.");
}

// -----------------------------
// Update LCD Function
// -----------------------------
void updateLCD() {
  lcd.setCursor(-4, 2);
  lcd.print("Litres: " + String(desiredLiters) + "   ");
  lcd.setCursor(0, 1);
  lcd.print("Total Vol: " + String(totalMilliLitres / 1000.0, 2) + "");
}

// -----------------------------
// Emergency Stop Check Function
// -----------------------------
void checkEmergencyStop() {
  int emergencyState = digitalRead(emergencyPin);
  if (emergencyState == LOW && !emergencyStop) {
    emergencyStop = true;  // Set emergency stop flag
    stopDispensing();  // Stop dispensing
    lcd.setCursor(-4, 3);
    lcd.print("EMERGENCY STOP!    ");
    Serial.println("Emergency stop activated!");
  } else if (emergencyState == LOW && emergencyStop) {
    lcd.clear();
    emergencyStop = false;  // Reset emergency stop flag
    lcd.setCursor(-4, 3);
    lcd.print("EMERGENCY REVOKED!");
    Serial.println("Emergency stop deactivated!");
    Initialize();
  }
}

// -----------------------------
// Display Error Function
// -----------------------------
void displayError(const char* message) {
  lcd.setCursor(-4, 3);
  lcd.print(message);
}
