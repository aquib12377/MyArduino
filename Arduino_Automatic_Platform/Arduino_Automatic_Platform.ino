#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------------------------------------------------
// Pin definitions
#define EntryIR         2
#define ExitIR          3
#define RelayPin        9  // Relay instead of servo
#define RelayPin1       A1  // Relay instead of servo

// Pins for LEDs and buzzer
#define RedLED          12
#define GreenLED        11
#define Buzzer          A0
#define OrangeLED       13

// Create an LCD object (I2C address, columns, rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Track platform status
bool platformClosed = false;
bool exitWasLow = false;

void setup() {  
  Serial.begin(9600);

  // Set up IR sensor pins
  pinMode(EntryIR, INPUT_PULLUP);
  pinMode(ExitIR,  INPUT_PULLUP);

  // Set up LED, buzzer, and relay pins as outputs
  pinMode(RedLED,    OUTPUT);
  pinMode(GreenLED,  OUTPUT);
  pinMode(OrangeLED, OUTPUT);
  pinMode(Buzzer,    OUTPUT);
  pinMode(RelayPin,  OUTPUT);
  pinMode(RelayPin1,  OUTPUT);

  // Initialize states
  digitalWrite(RedLED,    LOW);
  digitalWrite(GreenLED,  HIGH); // Green LED ON initially
  digitalWrite(OrangeLED, LOW);
  digitalWrite(Buzzer,    LOW);
  digitalWrite(RelayPin,  HIGH); // Relay OFF (ACTIVE LOW)
  digitalWrite(RelayPin1,  HIGH); // Relay OFF (ACTIVE LOW)

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  // Display initial message
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Read IR sensor states
  int entryState = digitalRead(EntryIR);
  int exitState  = digitalRead(ExitIR);

  // CLOSE Relay if entry sensor detects train
  if (!platformClosed && entryState == LOW) {
    platformClosed = true;
    digitalWrite(RelayPin, HIGH);  // Activate Relay (CLOSE)
    digitalWrite(RelayPin1, LOW);  // Activate Relay (CLOSE)
    delay(1500);
    digitalWrite(RelayPin,  HIGH); // Relay OFF (ACTIVE LOW)
  digitalWrite(RelayPin1,  HIGH); // Relay OFF (ACTIVE LOW)
    Serial.println("Train at entry -> Relay closed");
  }

  // OPEN Relay after train passes completely
  if (platformClosed) {
    if (exitState == LOW) {
      exitWasLow = true;
    }
    if (exitWasLow && exitState == HIGH) {
      platformClosed = false;
      exitWasLow     = false;
      digitalWrite(RelayPin, LOW);  // Deactivate Relay (OPEN)
      digitalWrite(RelayPin1, HIGH);  // Deactivate Relay (OPEN)
      delay(1500);
    digitalWrite(RelayPin,  HIGH); // Relay OFF (ACTIVE LOW)
  digitalWrite(RelayPin1,  HIGH); // Relay OFF (ACTIVE LOW)
      Serial.println("Train passed exit -> Relay opened");
    }
  }

  // LED & Buzzer Indication
  if (platformClosed) {
    digitalWrite(RedLED,    HIGH);
    digitalWrite(GreenLED,  LOW);
    digitalWrite(OrangeLED, HIGH);
    digitalWrite(Buzzer,    HIGH);
  } else {
    digitalWrite(RedLED,    LOW);
    digitalWrite(GreenLED,  HIGH);
    digitalWrite(OrangeLED, LOW);
    digitalWrite(Buzzer,    LOW);
  }

  // LCD Updates
  lcd.setCursor(0, 0);
  lcd.print("E:");
  lcd.print(entryState == LOW ? "L" : "H");
  lcd.print("  X:");
  lcd.print(exitState == LOW ? "L" : "H");

  lcd.setCursor(0, 1);
  if (platformClosed) {
    lcd.print("Platform:Closed ");
  } else {
    lcd.print("Platform:Open   ");
  }

  delay(100);
}