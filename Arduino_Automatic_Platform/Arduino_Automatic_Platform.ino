#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// -------------------------------------------------
// Pin definitions
#define EntryIR         2
#define ExitIR          3
#define platformServo1  9

// New pins for LEDs and buzzer
#define RedLED          12
#define GreenLED        11
#define Buzzer          13

// Create servo object
Servo s1;

// Create an LCD object. Parameters: (I2C address, columns, rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Track whether the platform is currently closed
bool platformClosed = false;
// Track if the exit sensor was triggered
bool exitWasLow = false;

void setup() {  
  Serial.begin(9600);

  // Set up IR sensor pins
  pinMode(EntryIR, INPUT_PULLUP);
  pinMode(ExitIR,  INPUT_PULLUP);

  // Set up our LED and buzzer pins as outputs
  pinMode(RedLED,   OUTPUT);
  pinMode(GreenLED, OUTPUT);
  pinMode(Buzzer,   OUTPUT);
  
  // Initially turn everything off or set to desired default
  digitalWrite(RedLED,   LOW);
  digitalWrite(GreenLED, HIGH); // Let's default to Green ON at startup
  digitalWrite(Buzzer,   LOW);

  // Attach servo to its pin
  s1.attach(platformServo1);
  // Start platform in the "open" position (adjust angle as needed)
  s1.write(90);

  // Initialize LCD
  lcd.begin();      // Start the LCD
  lcd.backlight();  // Turn on backlight
  lcd.clear();

  // Display an initial message
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Read the IR sensor states
  int entryState = digitalRead(EntryIR);
  int exitState  = digitalRead(ExitIR);

  // ---------------------------
  // 1) CLOSING LOGIC: If the train arrives (EntryIR == LOW)
  //    and the platform is open, close the platform.
  // ---------------------------
  if (!platformClosed && entryState == LOW) {
    platformClosed = true;
    s1.write(0);  // Close platform
    Serial.println("Train at entry -> Platform closed");
  }

  // ---------------------------
  // 2) OPENING LOGIC: If the platform is closed, watch the exit sensor.
  //    - Once the exit sensor goes LOW, set exitWasLow = true.
  //    - When exitWasLow == true and exitState goes HIGH again, the train is fully past -> open the platform.
  // ---------------------------
  if (platformClosed) {
    if (exitState == LOW) {
      exitWasLow = true;
    }
    if (exitWasLow && exitState == HIGH) {
      platformClosed = false;
      exitWasLow     = false;
      s1.write(90);  // Open platform
      Serial.println("Train passed exit -> Platform opened");
    }
  }

  // ---------------------------
  // 3) LED & Buzzer Indication
  //    - If platform is closed -> Red LED ON, Green OFF, Buzzer ON
  //    - If platform is open   -> Red LED OFF, Green ON, Buzzer OFF
  // ---------------------------
  if (platformClosed) {
    digitalWrite(RedLED,   HIGH);
    digitalWrite(GreenLED, LOW);
    digitalWrite(Buzzer,   HIGH);
  } else {
    digitalWrite(RedLED,   LOW);
    digitalWrite(GreenLED, HIGH);
    digitalWrite(Buzzer,   LOW);
  }

  // ---------------------------
  // 4) LCD Updates
  //    We'll display:
  //    Line 1: Entry & Exit sensor states
  //    Line 2: Platform status
  // ---------------------------
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

  // Short delay to avoid spamming the serial and LCD too quickly
  delay(100);
}
