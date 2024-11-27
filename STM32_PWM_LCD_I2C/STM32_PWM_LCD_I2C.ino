#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

int potPin = PA0;   // Potentiometer connected to PA0 (Analog input)
int pwmPin = PA9;   // PWM output on pin A9
int potValue = 0;   // Variable to store potentiometer value
int pwmValue = 0;   // Variable to store PWM value (0-255)
float dutyCycle = 0; // Duty cycle percentage

void setup()
{
	// initialize the LCD
	lcd.begin();

	// Turn on the backlight and print a message.
	lcd.backlight();
	lcd.print("Hello, world!");

	// Initialize PWM pin as output
	pinMode(pwmPin, PWM);

	// Start PWM on the pin with a frequency
	//analogWriteFrequency(pwmPin, 1000);  // Set PWM frequency to 1kHz
}

void loop()
{
	// Read the potentiometer value (0 to 4095 on STM32 analog pins)
	potValue = analogRead(potPin);

	// Map the potentiometer value (0 to 4095) to PWM range (0 to 255 for 8-bit resolution)
	pwmValue = map(potValue, 0, 4095, 0, 255);

	// Calculate the duty cycle percentage
	dutyCycle = (pwmValue / 255.0) * 100;

	// Set the PWM output on pin A9
	analogWrite(pwmPin, pwmValue);

	// Clear the second line of the LCD
	lcd.setCursor(0, 1);
	lcd.print("                "); // Clear any previous content

	// Print the duty cycle on the LCD
	lcd.setCursor(0, 1);  // Move to the second line
	lcd.print("Duty: ");
	lcd.print(dutyCycle, 1);  // Print the duty cycle with 1 decimal place
	lcd.print("%");

	// Small delay for stability
	delay(100);
}
