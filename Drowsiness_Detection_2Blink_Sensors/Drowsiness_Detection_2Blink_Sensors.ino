#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================
// Pin Definitions
// =====================
const int IR1_PIN = 4;        // IR Sensor for Person 1
const int IR2_PIN = 5;        // IR Sensor for Person 2
const int BUZZER_PIN = 3;     // Buzzer
const int RELAY_PIN = 2;      // Relay controlling the motor

// =====================
// LCD Configuration
// =====================
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address 0x27, 16 columns, 2 rows
// *Note:* If your LCD doesn't display, try changing 0x27 to 0x3F

// =====================
// Timing Variables
// =====================
unsigned long eye1ClosedStartTime = 0;
unsigned long eye2ClosedStartTime = 0;

// =====================
// State Flags
// =====================
bool eye1Closed = false;
bool eye2Closed = false;
bool relayOff = false;

// Flags to prevent repeated alerts
bool beeped2Sec1 = false;
bool beeped5Sec1 = false;
bool beeped2Sec2 = false;
bool beeped5Sec2 = false;

void setup() {
  // Initialize Serial Monitor for debugging (optional)
  Serial.begin(9600);
  
  // Initialize pin modes
  pinMode(IR1_PIN, INPUT_PULLUP);
  pinMode(IR2_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Ensure relay is ON at startup
  digitalWrite(RELAY_PIN, LOW);
  
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Drowsiness");
  lcd.setCursor(0, 1);
  lcd.print("System");
  delay(2000); // Display initial message for 2 seconds
}

void loop() {
  // Read the state of IR sensors
  int eye1State = digitalRead(IR1_PIN);
  int eye2State = digitalRead(IR2_PIN);
  Serial.print("Eye 1 = ");
  Serial.println(eye1State);
  Serial.print("Eye 2 = ");
  Serial.println(eye2State);
  // Assuming IR sensor outputs LOW when eye is closed
  bool eye1IsClosed = (eye1State == HIGH);
  bool eye2IsClosed = (eye2State == HIGH);
  
  unsigned long currentTime = millis();
  
  // =====================
  // Handle Person 1
  // =====================
  if (eye1IsClosed) {
    if (!eye1Closed) {
      eye1Closed = true;
      eye1ClosedStartTime = currentTime;
      // Reset beep flags for Person 1
      beeped2Sec1 = false;
      beeped5Sec1 = false;
    }
    unsigned long duration1 = (currentTime - eye1ClosedStartTime) / 1000; // Duration in seconds
    
    if (duration1 >= 2 && !beeped2Sec1) {
      beep3Times();
      beeped2Sec1 = true;
    }
    if (duration1 >= 5 && !relayOff && !beeped5Sec1) {
      longBeep();
      // Turn off relay (stop motor)
      digitalWrite(RELAY_PIN, HIGH); // Adjust if your relay is active LOW
      relayOff = true;
      beeped5Sec1 = true;
    }
  } else {
    eye1Closed = false;
    // Reset beep flags when eyes are open
    beeped2Sec1 = false;
    beeped5Sec1 = false;
  }
  
  // =====================
  // Handle Person 2
  // =====================
  if (eye2IsClosed) {
    if (!eye2Closed) {
      eye2Closed = true;
      eye2ClosedStartTime = currentTime;
      // Reset beep flags for Person 2
      beeped2Sec2 = false;
      beeped5Sec2 = false;
    }
    unsigned long duration2 = (currentTime - eye2ClosedStartTime) / 1000; // Duration in seconds
    
    if (duration2 >= 2 && !beeped2Sec2) {
      beep3Times();
      beeped2Sec2 = true;
    }
    if (duration2 >= 5 && !relayOff && !beeped5Sec2) {
      longBeep();
      // Turn off relay (stop motor)
      digitalWrite(RELAY_PIN, HIGH); // Adjust if your relay is active LOW
      relayOff = true;
      beeped5Sec2 = true;
    }
  } else {
    eye2Closed = false;
    // Reset beep flags when eyes are open
    beeped2Sec2 = false;
    beeped5Sec2 = false;
  }
  
  // =====================
  // Update LCD Display
  // =====================
  lcd.setCursor(0, 0);
  lcd.print("Drowsiness System");
  
  lcd.setCursor(0, 1);
  lcd.print("P1:");
  lcd.print(eye1IsClosed ? "C " : "O ");
  lcd.print("P2:");
  lcd.print(eye2IsClosed ? "C " : "O ");
  
  // Optional: Display durations
  /*
  lcd.setCursor(0, 0);
  lcd.print("P1:");
  lcd.print(eye1IsClosed ? "C" : "O");
  lcd.print(" P2:");
  lcd.print(eye2IsClosed ? "C" : "O");
  */
  lcd.setCursor(0, 1);
  if (eye1IsClosed) {
    lcd.print("P1:");
    lcd.print((currentTime - eye1ClosedStartTime) / 1000);
    lcd.print("s ");
  }
  if (eye2IsClosed) {
    lcd.print("P2:");
    lcd.print((currentTime - eye2ClosedStartTime) / 1000);
    lcd.print("s ");
  }
  
  
  delay(100); // Small delay to reduce flickering
}

// =====================
// Buzzer Functions
// =====================

// Function to beep buzzer 3 consecutive times
void beep3Times() {
  for(int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

// Function to beep buzzer with a long beep
void longBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000); // Long beep duration
  digitalWrite(BUZZER_PIN, LOW);
}
