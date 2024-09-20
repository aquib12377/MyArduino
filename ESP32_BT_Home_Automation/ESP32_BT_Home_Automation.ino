#include <ESP32Servo.h>

#include <BluetoothSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Bluetooth Serial object
BluetoothSerial SerialBT;

// Servo object
Servo doorServo;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust the I2C address if needed

// GPIO pins
const int ledPins[] = { 15, 2, 4, 16 }; // Replace with your LED pins
const int relayPin = 17;
const int servoPin = 18;

void setup() {
  // Start Serial communication for debugging
  Serial.begin(115200);

  // Start Bluetooth
  SerialBT.begin("ESP32_Home_Automation");

  // Setup LEDs as outputs
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  // Setup relay as output
  pinMode(relayPin, OUTPUT);
  
  // Attach the servo
  doorServo.attach(servoPin);
  doorServo.write(0); // Initial position
  
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Home Automation");
}

void loop() {
  // Check if there is data received over Bluetooth
  if (SerialBT.available()) {
    char receivedChar = SerialBT.read();
    
    // Process received command
    switch (receivedChar) {
      case '1':
        digitalWrite(ledPins[0], HIGH);
        break;
      case '2':
        digitalWrite(ledPins[0], LOW);
        break;
      case '3':
        digitalWrite(ledPins[1], HIGH);
        break;
      case '4':
        digitalWrite(ledPins[1], LOW);
        break;
      case '5':
        digitalWrite(ledPins[2], HIGH);
        break;
      case '6':
        digitalWrite(ledPins[2], LOW);
        break;
      case '7':
        digitalWrite(ledPins[3], HIGH);
        break;
      case '8':
        digitalWrite(ledPins[3], LOW);
        break;
      case 'R': // Relay ON
        digitalWrite(relayPin, HIGH);
        break;
      case 'r': // Relay OFF
        digitalWrite(relayPin, LOW);
        break;
      case 'D': // Open door
        doorServo.write(90); // Adjust the angle as needed
        break;
      case 'd': // Close door
        doorServo.write(0);
        break;
      default:
        // If the received data is a string to display on the LCD
        String message = "";
        message += receivedChar;
        while (SerialBT.available()) {
          message += (char)SerialBT.read();
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(message);
        break;
    }
  }
}
