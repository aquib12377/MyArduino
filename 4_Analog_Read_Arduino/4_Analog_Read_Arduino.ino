#include <Wire.h>              // Include I2C library
#include <LiquidCrystal_I2C.h> // Include LCD library

// Define I2C LCD (Address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int flex1 = A0;  // Flex Sensor 1
const int flex2 = A1;  // Flex Sensor 2
const int flex3 = A2;  // Flex Sensor 3

int prevValue1 = 0, prevValue2 = 0, prevValue3 = 0;  // Store previous values
const int threshold = 10;  // Define threshold for major change

String lastCommand = "";  // Store the last sent command

void setup() {
    Serial.begin(9600);    // Start Serial (for debugging & HC-05)
    lcd.begin();           // Start LCD
    lcd.backlight();       // Turn on LCD backlight
    lcd.setCursor(0, 0);
    lcd.print("Ready..."); // Display initial message
}

void loop() {
    int value1 = analogRead(flex1);  // Read sensor 1
    int value2 = analogRead(flex2);  // Read sensor 2
    int value3 = analogRead(flex3);  // Read sensor 3

    String command = "";  // Store the new command

    // Detect significant changes and assign a message
    if (abs(value1 - prevValue1) > threshold) {
        command = (value1 > prevValue1) ? "HELP!" : "WASHROOM!";
    } else if (abs(value2 - prevValue2) > threshold) {
        command = (value2 > prevValue2) ? "HELLO!" : "BYE!";
    } else if (abs(value3 - prevValue3) > threshold) {
        command = (value3 > prevValue3) ? "YES!" : "NO!";
    }

    // Send to HC-05 and update LCD only if command has changed
    if (command != "" && command != lastCommand) {
        Serial.println(command);  // Send to HC-05 via Serial
        lcd.clear();              // Clear LCD
        lcd.setCursor(0, 0);
        lcd.print("Command:");     // Display label
        lcd.setCursor(0, 1);
        lcd.print(command);        // Display detected command
        lastCommand = command;     // Store last command
    }

    // Update previous values
    prevValue1 = value1;
    prevValue2 = value2;
    prevValue3 = value3;

    delay(500);  // Wait before next reading
}
