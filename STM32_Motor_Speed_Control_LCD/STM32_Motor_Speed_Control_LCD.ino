#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define the LCD address and dimensions
// Most commonly, I2C address for a 16x2 LCD is 0x27 or 0x3F. Adjust if necessary.
LiquidCrystal_I2C lcd(0x27, 16, 2); 

int pwmPin = PA3;
int m1 = PA1;
int m2 = PA2;

void setup() {
  // Initialize the Serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait
  }

  // Initialize LCD
  lcd.begin();
  lcd.backlight(); // Turn on the backlight for the LCD

  // Initialize GPIO pins
  pinMode(PC13, OUTPUT);
  pinMode(m1, OUTPUT);
  pinMode(m2, OUTPUT);
  pinMode(PA0, INPUT_ANALOG);

  // Set initial motor direction
  digitalWrite(m1, HIGH);
  digitalWrite(m2, LOW);

  // Display initial message on LCD
  lcd.setCursor(0, 0);
  lcd.print("Motor Control");
}

void loop() {
  int potValue = analogRead(PA0);
  int speed = map(potValue, 100, 4000, 4000, 0);

  // Constrain the speed value to stay within valid PWM range
  speed = constrain(speed, 0, 4000);

  // Control motor direction and speed
  if (speed < 10) {
    digitalWrite(m1, LOW);
    digitalWrite(m2, LOW);
  } else {
    digitalWrite(m1, HIGH);
    digitalWrite(m2, LOW);
  }

  analogWrite(pwmPin, speed);

  // Print to Serial
  Serial.print("Potentiometer Value: ");
  Serial.print(potValue);
  Serial.print(" | Speed Value: ");
  Serial.println(speed);

  // Display values on the LCD
  lcd.setCursor(0, 1);
  lcd.print("Speed: ");
  lcd.print(map(speed,4000,0,100,0));
  lcd.print("  ");
  delay(100);
}
