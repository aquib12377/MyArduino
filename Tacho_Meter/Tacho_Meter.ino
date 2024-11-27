#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Initialize the LCD (I2C address, columns, rows)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust the address (0x27) if necessary

volatile unsigned long pulseCount = 0; // Counts the number of pulses
unsigned long previousMillis = 0;      // Stores the last time RPM was calculated
const long interval = 1000;            // Interval at which RPM is calculated (in milliseconds)
float rpm = 0;

void setup() {
  // Initialize the LCD
  lcd.begin();
  lcd.backlight();

  // For debugging purposes (optional)
  Serial.begin(9600);

  // Set pin 2 as input with an internal pull-up resistor
  pinMode(2, INPUT_PULLUP);

  // Attach interrupt to pin 2 (on falling edge)
  attachInterrupt(digitalPinToInterrupt(2), pulseCounter, FALLING);

  // Display an initial message on the LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tacho Meter");
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if the interval has passed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Safely read and reset the pulse count
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();

    // Calculate RPM (rotations per minute)
    rpm = (count * 60) / (interval / 1000.0);

    // Display RPM on the LCD
    lcd.setCursor(0, 1);
    lcd.print("RPM: ");
    lcd.print(rpm);

    // Optional: Print RPM to the Serial Monitor for debugging
    Serial.print("RPM: ");
    Serial.println(rpm);
  }
}

// Interrupt Service Routine (ISR) for counting pulses
void pulseCounter() {
  pulseCount++;
}
