#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// ----- Sensor and Communication Setup -----
// ADXL345 Setup
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// GPS Setup using SoftwareSerial (adjust pins as needed)
SoftwareSerial gpsSerial(6, 7); // RX, TX for GPS module
TinyGPSPlus gps;

// GSM Setup using SoftwareSerial (adjust pins as needed)
SoftwareSerial gsmSerial(8, 9); // RX, TX for GSM module
const String phoneNumber = "+918652129103"; // Replace with the actual phone number

// Buzzer Setup
const int buzzerPin = 10; 

// ----- Thresholds and Flags -----
const float ACCIDENT_THRESHOLD = 15.0; // Acceleration threshold (m/s^2) for accident detection
bool accidentDetected = false;

// ----- Helper Function to Read GSM Response -----
String readGSMResponse(unsigned long timeout = 2000) {
  gsmSerial.listen();  // Ensure GSM is active
  String response = "";
    delay(500);
  while (Serial.available()) {
    gsmSerial.write(Serial.read());  // Forward what Serial received to SIM800L
  }
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());  // Forward what SIM800L received to Serial
  }
  return response;
}

void setup() {
  Serial.begin(9600);
  
  // Initialize ADXL345
  if (!accel.begin()) {
    Serial.println("No ADXL345 detected! Check wiring.");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_16_G); // Set range

  // Start SoftwareSerial ports
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  // Initialize buzzer pin
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Initialize GSM for SMS in text mode
  gsmSerial.listen(); // Switch listener to GSM for initialization
  delay(1000);
  gsmSerial.println("AT");
  delay(500);
  Serial.println("GSM Response: " + readGSMResponse()); // Read response after "AT"

  gsmSerial.println("AT+CMGF=1"); // Set GSM module to text mode
  delay(1000);
  Serial.println("GSM Response: " + readGSMResponse()); // Read response after setting text mode

  sendSMS("Project Started");
  Serial.println("System initialized.");
}

void loop() {
  // Read GPS data
  gpsSerial.listen();  // Activate GPS listening
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Read accelerometer data
  sensors_event_t event;
  accel.getEvent(&event);
  
  
  // Check for a major movement on any axis
  if (!accidentDetected && 
      (abs(event.acceleration.x) > ACCIDENT_THRESHOLD || 
       abs(event.acceleration.y) > ACCIDENT_THRESHOLD || 
       abs(event.acceleration.z) > ACCIDENT_THRESHOLD)) {
    
    accidentDetected = true;
    handleAccident();
  }

  delay(100); // Short delay
}

void handleAccident() {
  Serial.println("Accident detected!");

  // Beep the buzzer for 1 second
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);

  // Attempt to obtain a GPS fix for up to 10 seconds
  Serial.println("Attempting to get GPS fix...");
  unsigned long start = millis();

  // Switch to GPS listener to get fix
  gpsSerial.listen();
  while (millis() - start < 1000) { // wait up to 10 seconds
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isValid()) {
      break;
    }
  }

  // Compose SMS message with Google Maps link
  String message;
  if (gps.location.isValid()) {
    String lat = String(gps.location.lat(), 6);
    String lon = String(gps.location.lng(), 6);
    message = "Accident detected!\nLocation: https://maps.google.com/?q=" + lat + "," + lon;
  } else {
    message = "Accident detected!\nLocation: https://maps.google.com/?q=18.9687601,72.8164548,17z";
  }

  sendSMS(message);

  // Reset accident flag to allow future detections
  accidentDetected = false;
}

void sendSMS(String message) {
  gsmSerial.listen(); // Switch to GSM listener for sending SMS

  gsmSerial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  readGSMResponse();
  gsmSerial.println("AT+CMGS=\"" + phoneNumber + "\"");  // Send SMS to phone number
  delay(1000);
  readGSMResponse();
  gsmSerial.print(message);  // Message content
  delay(100);
  readGSMResponse();
  gsmSerial.write(26);  // Send Ctrl+Z to indicate the end of the message
  delay(1000);
  readGSMResponse();
}
