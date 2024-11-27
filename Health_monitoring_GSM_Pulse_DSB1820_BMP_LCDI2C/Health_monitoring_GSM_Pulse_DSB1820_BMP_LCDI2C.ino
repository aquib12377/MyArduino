#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BMP085.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <PulseSensorPlayground.h>

// Pin Definitions
#define DS18B20_PIN 2
#define BUZZER_PIN 6
#define GSM_TX_PIN 5
#define GSM_RX_PIN 4
#define PULSE_PIN A0

// Pulse Sensor Setup
const int LED = LED_BUILTIN;        // The onboard Arduino LED for pulse indicator.
int Threshold = 550;                // Threshold for pulse detection.

PulseSensorPlayground pulseSensor;  // Creates an instance of the PulseSensorPlayground object

// Setup for I2C LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adjust the I2C address if necessary

// Setup for DS18B20 Temperature Sensor
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// Setup for BMP180 Sensor
Adafruit_BMP085 bmp;
String phoneNumber = "+917977845638";

// GSM Module Communication
SoftwareSerial gsmSerial(5, 4);

// Variables
float temperature = 0.0;
float pressure = 0.0;
float altitude = 0.0;
int myBPM = 0;                      // BPM from Pulse Sensor
unsigned long previousMillis = 0;
const long interval = 30000;         // 30 seconds interval

void setup() {
  // Initialize Serial Monitor (optional for debugging)
  Serial.begin(9600);

  // Initialize Pulse Sensor
  pulseSensor.analogInput(PULSE_PIN);
  pulseSensor.blinkOnPulse(LED);       // Blink the LED with heartbeat
  pulseSensor.setThreshold(Threshold); // Set the threshold

  if (pulseSensor.begin()) {
    Serial.println("PulseSensor Object Created!");
  }

  // Initialize GSM
  gsmSerial.begin(9600);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();

  // Initialize DS18B20
  sensors.begin();

  // Initialize BMP180
  if (!bmp.begin()) {
    Serial.println("Could not find BMP180 sensor!");
    // /while (1);
  }

  // Set buzzer pin as output
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Display initial message
  lcd.setCursor(0, 0);
  lcd.print("Health Monitor");
  delay(2000);
  lcd.clear();
    gsmSerial.println("AT");
  updateSerial();
  gsmSerial.println("AT+CMGF=1");  // Set SMS text mode
  updateSerial();
  sendSMS();
}

void loop() {
  // Read DS18B20 temperature sensor
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);

  // Read BMP180 sensor
  pressure = bmp.readPressure();
  altitude = bmp.readAltitude();

  // Get BPM from Pulse Sensor
  if (pulseSensor.sawStartOfBeat()) {  // If a heartbeat is detected
    myBPM = pulseSensor.getBeatsPerMinute();  // Get BPM value
    Serial.println("♥ A HeartBeat Happened !");
    Serial.print("BPM: ");
    Serial.println(myBPM);
  }

  // Display on LCD
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature);
  lcd.print("C ");

  lcd.setCursor(0, 1);
  lcd.print("Press: ");
  lcd.print(pressure / 100);  // Convert Pa to hPa

  lcd.setCursor(8, 0);
  lcd.print(" P: ");
  lcd.print(myBPM);

  // Check if temperature is higher than 35°C
  if (temperature > 35.0) {
    digitalWrite(BUZZER_PIN, HIGH);  // Turn on buzzer
  } else {
    digitalWrite(BUZZER_PIN, LOW);   // Turn off buzzer
  }

  // Send SMS every 30 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 10000) {
    previousMillis = currentMillis;
    sendSMS();
  }

  delay(20);  // Short delay for Pulse Sensor
}

// Function to send SMS


void sendSMS() {
  gsmSerial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  updateSerial();
  String message = "Temperature: "+String(temperature) +"\BPM: "+String(myBPM)+"\nPressure: "+String(pressure);
  gsmSerial.println("AT+CMGS=\"" + phoneNumber + "\"");  // Send SMS to phone number
  delay(1000);
  updateSerial();
  gsmSerial.print(message);  // Message content
  delay(100);
  updateSerial();
  gsmSerial.write(26);  // Send Ctrl+Z to indicate the end of the message
  delay(1000);
  updateSerial();
}

void updateSerial() {
  delay(500);
  while (Serial.available()) {
    gsmSerial.write(Serial.read());  // Forward what Serial received to SIM800L
  }
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());  // Forward what SIM800L received to Serial
  }
}
