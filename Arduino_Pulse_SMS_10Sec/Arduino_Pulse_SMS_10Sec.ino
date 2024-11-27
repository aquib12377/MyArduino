/*  
  Getting_BPM_to_Monitor prints the BPM to the Serial Monitor and now to an I2C LCD.
  It uses the PulseSensorPlayground Library and the LiquidCrystal_I2C Library.

  Tutorial Webpage: https://pulsesensor.com/pages/getting-advanced
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PulseSensorPlayground.h>  // Includes the PulseSensorPlayground Library.
#include <SoftwareSerial.h>

SoftwareSerial mySerial(3, 2);  //SIM800L Tx & Rx is connected to Arduino #3 & #2
// Variables
const int PulseWire = 0;      // PulseSensor PURPLE WIRE connected to ANALOG PIN 0
const int LED = LED_BUILTIN;  // The on-board Arduino LED
int Threshold = 512;          // Adjust this threshold as needed

// Create instances of the PulseSensor and LCD objects
PulseSensorPlayground pulseSensor;   // PulseSensor object
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address to 0x27 for a 16 chars and 2 line display

// Variables for BPM display
int myBPM = 0;                           // Variable to hold the current BPM value
unsigned long lastUpdate = 0;            // Timer for LCD updates
const unsigned long lcdInterval = 1000;  // Update LCD every 1 second

void setup() {
  Serial.begin(115200);  // Initialize Serial communication for debugging

  // Initialize the LCD
  lcd.begin();      // Initialize the LCD
  lcd.backlight();  // Turn on the backlight
  lcd.clear();      // Clear the display
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Configure the PulseSensor object
  pulseSensor.analogInput(PulseWire);
  pulseSensor.blinkOnPulse(LED);  // Blink the LED on each heartbeat
  pulseSensor.setThreshold(Threshold);

  // Start the PulseSensor
  if (pulseSensor.begin()) {
    Serial.println("PulseSensor initialized successfully!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PulseSensor Ready");
  } else {
    Serial.println("PulseSensor initialization failed.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Init Fail");
    while (1)
      ;  // Halt execution if sensor fails to initialize
  }

  mySerial.begin(9600);

  Serial.println("Initializing...");
  delay(1000);

  mySerial.println("AT");  //Once the handshake test is successful, it will back to OK
  updateSerial();

  mySerial.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
}

void sendSMS(String mob, String message) {
  mySerial.println("AT+CMGS=\"" + mob + "\"");  //change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  mySerial.print(message);  //text content
  updateSerial();
  mySerial.write(26);
}

void loop() {
  sendSMS("+917977845638", "Test SMS");
  // Check if a heartbeat was detected
  if (pulseSensor.sawStartOfBeat()) {
    myBPM = pulseSensor.getBeatsPerMinute();  // Get the BPM value
    Serial.println("♥ A HeartBeat Happened!");
    Serial.print("BPM: ");
    Serial.println(myBPM);
  }

  // Update the LCD every second
  if (millis() - lastUpdate >= lcdInterval) {
    lastUpdate = millis();

    // Update the LCD display
    lcd.clear();
    lcd.setCursor(0, 0);

    if (myBPM > 0) {
      lcd.print("Heart Rate:");
      lcd.setCursor(0, 1);
      lcd.print("BPM: ");
      lcd.print(myBPM);
    } else {
      lcd.print("No Heartbeat");
      lcd.setCursor(0, 1);
      lcd.print("Detected");
    }
  }

  delay(20);  // Small delay for stability
}

void updateSerial() {
  delay(500);
  while (Serial.available()) {
    mySerial.write(Serial.read());  //Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());  //Forward what Software Serial received to Serial Port
  }
}
