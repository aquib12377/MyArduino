#include <BluetoothSerial.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

BluetoothSerial SerialBT;

Servo doorServo;

LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust I2C address if needed

const int ledPins[] = {14, 27, 26, 25}; 
const int relayPin = 12;
const int servoPin = 13;

char lastCommand = '\0';  // Variable to store the last command
String lastLCDMessage = ""; // Variable to store the last LCD message

void setup() {
  // Start Serial communication for debugging
  Serial.begin(115200);

  // Start Bluetooth communication
  SerialBT.begin("ESP32_Home_Automation");

  // Set up LEDs as outputs
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  // Set up relay and servo
  pinMode(relayPin, OUTPUT);
  doorServo.attach(servoPin);
  doorServo.write(0);  // Initial position

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Home Automation");

  // Debug messages
  Serial.println("ESP32 Home Automation Initialized");
}

void loop() {
  // Check if there is data received over Bluetooth
  if (SerialBT.available()) {
    char receivedChar = SerialBT.read();
    
    // Debug the received character
    Serial.print("Received: ");
    Serial.println(receivedChar);
    
    switch (receivedChar) {
      case '1':
        if (lastCommand != '1') {
          digitalWrite(ledPins[0], HIGH);
          Serial.println("LED 1 ON");
          lastCommand = '1';
        }
        break;
      case '2':
        if (lastCommand != '2') {
          digitalWrite(ledPins[0], LOW);
          Serial.println("LED 1 OFF");
          lastCommand = '2';
        }
        break;
      case '3':
        if (lastCommand != '3') {
          digitalWrite(ledPins[1], HIGH);
          Serial.println("LED 2 ON");
          lastCommand = '3';
        }
        break;
      case '4':
        if (lastCommand != '4') {
          digitalWrite(ledPins[1], LOW);
          Serial.println("LED 2 OFF");
          lastCommand = '4';
        }
        break;
      case '5':
        if (lastCommand != '5') {
          digitalWrite(ledPins[2], HIGH);
          Serial.println("LED 3 ON");
          lastCommand = '5';
        }
        break;
      case '6':
        if (lastCommand != '6') {
          digitalWrite(ledPins[2], LOW);
          Serial.println("LED 3 OFF");
          lastCommand = '6';
        }
        break;
      case '7':
        if (lastCommand != '7') {
          digitalWrite(ledPins[3], HIGH);
          Serial.println("LED 4 ON");
          lastCommand = '7';
        }
        break;
      case '8':
        if (lastCommand != '8') {
          digitalWrite(ledPins[3], LOW);
          Serial.println("LED 4 OFF");
          lastCommand = '8';
        }
        break;
      case 'R':  // Relay ON
        if (lastCommand != 'R') {
          digitalWrite(relayPin, HIGH);
          Serial.println("Relay ON");
          lastCommand = 'R';
        }
        break;
      case 'r':  // Relay OFF
        if (lastCommand != 'r') {
          digitalWrite(relayPin, LOW);
          Serial.println("Relay OFF");
          lastCommand = 'r';
        }
        break;
      case 'D':  // Open door
        if (lastCommand != 'D') {
          doorServo.write(90);  // Adjust the angle if necessary
          Serial.println("Door Opened");
          lastCommand = 'D';
        }
        break;
      case 'd':  // Close door
        if (lastCommand != 'd') {
          doorServo.write(0);
          Serial.println("Door Closed");
          lastCommand = 'd';
        }
        break;
      default:
        // If it's a message for the LCD
        String message = "";
        message += receivedChar;
        while (SerialBT.available()) {
          message += (char)SerialBT.read();
        }
        message.trim();

        // Check if the message is different from the last message
        if (message != lastLCDMessage) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(message);
          lastLCDMessage = message; // Update the last LCD message
          
          // Debug the received message
          Serial.print("LCD Message: ");
          Serial.println(message);
        }
        break;
    }
  }
}
