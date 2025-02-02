#include <TinyGPS++.h>
#include <ESP32Servo.h>

// Debug Flag
#define DEBUG true

// GSM Module Pins
#define GSM_RX_PIN 15
#define GSM_TX_PIN 2
HardwareSerial GSMSerial(1);

// GPS Module Pins
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
HardwareSerial GPSSerial(2);

TinyGPSPlus gps;

// Ultrasonic Sensor Pins
#define TRIG_PIN 13
#define ECHO_PIN 12

// IR Sensor Pin
#define IR_PIN 14

// Servo Pin
#define SERVO_PIN 4
Servo servo;

// Constants
const String PHONE = "+918657623060";  // Replace with your phone number
const String HARD_CODED_LOCATION = "https://maps.app.goo.gl/uiRzpifRpRuy2rPP7";
const int MAX_PERCENTAGE = 90;  // Max distance in cm for the ultrasonic sensor

// Variables
boolean isDoorOpen = false;

// Debug Print Function
void debugPrintln(const String &message) {
  if (DEBUG) {
    Serial.println(message);
  }
}
void debugPrint(const String &message) {
  if (DEBUG) {
    Serial.print(message);
  }
}

void setup() {
  Serial.begin(115200);
  debugPrintln("Smart Dustbin Initializing...");

  // Initialize GSM
  GSMSerial.begin(9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  debugPrintln("GSM Module Initialized");

  // Initialize GPS
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  debugPrintln("GPS Module Initialized");

  // Initialize Ultrasonic Sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize IR Sensor
  pinMode(IR_PIN, INPUT);

  // Initialize Servo
  servo.attach(SERVO_PIN);
  servo.write(0);  // Ensure the door is closed
  debugPrintln("Servo Motor Initialized - Door Closed");

  // Test GSM Communication
  sendATCommand("AT");
  sendATCommand("AT+CMGF=1");  // Set SMS mode to text
}

void loop() {
  // IR sensor for door control
  if (digitalRead(IR_PIN) == LOW) {
    debugPrintln("IR Sensor Detected Object");
    openDoor();
    delay(5000);  // Keep the door open for 5 seconds
    closeDoor();
  }

  // Ultrasonic sensor for level detection
  int level = getDustbinLevel();
  debugPrintln("Dustbin Level: " + String(level) + "%");

  if (level >= MAX_PERCENTAGE) {
    String message = "Dustbin Full. Level: " + String(level) + "%";
    debugPrintln("Sending Alert SMS: " + message);
    sendAlertSMS(message);
    delay(5000);  // Wait for 30 seconds to avoid spamming
  }

  // Process GPS data
  debugPrint(String("GPS Data Start: "));

  while (GPSSerial.available() > 0) {
    char c = GPSSerial.read();
    gps.encode(c);
    //debugPrint(String(" ") + c);
  }
  debugPrintln(String("GPS Data End: "));
}

// Open the dustbin door
void openDoor() {
  if (!isDoorOpen) {
    servo.write(90);  // Open position
    isDoorOpen = true;
    debugPrintln("Door Opened");
  }
}

// Close the dustbin door
void closeDoor() {
  if (isDoorOpen) {
    servo.write(0);  // Closed position
    isDoorOpen = false;
    debugPrintln("Door Closed");
  }
}

// Get the dustbin level as a percentage
int getDustbinLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;  // Calculate distance in cm
  distance = distance == 0 ? 100 : distance;
  // Convert distance to percentage
  int percentage = map(distance, 0, MAX_PERCENTAGE, 100, 0);
  percentage = constrain(percentage, 0, 100);
  debugPrintln("Ultrasonic Sensor Distance: " + String(distance) + " cm");
  return percentage;
}

// Send SMS alert with location or a hardcoded link
void sendAlertSMS(String message) {
  if (gps.location.isValid()) {
    String locationMessage = message + "\nLocation: http://maps.google.com/maps?q=loc:";
    locationMessage += String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    debugPrintln("GPS Location Available: " + locationMessage);
    sendSMS(locationMessage);
  } else {
    String fallbackMessage = message + "\nLocation: " + HARD_CODED_LOCATION;
    debugPrintln("GPS Location Unavailable. Sending Fallback Location: " + fallbackMessage);
    sendSMS(fallbackMessage);
  }
}

// Send SMS using GSM module
void sendSMS(const String &message) {
  debugPrintln("Sending SMS: " + message);
  GSMSerial.println("AT+CMGS=\"" + PHONE + "\"");
  delay(1000);
  GSMSerial.println(message);
  delay(100);
  GSMSerial.write(0x1A);  // Send Ctrl+Z to send the SMS
  delay(3000);
  debugPrintln("SMS Sent");
}

// Send an AT command and print the response
void sendATCommand(const String &command) {
  debugPrintln("Sending AT Command: " + command);
  GSMSerial.println(command);
  delay(1000);
  while (GSMSerial.available()) {
    String response = GSMSerial.readString();
    debugPrintln("AT Command Response: " + response);
  }
}
