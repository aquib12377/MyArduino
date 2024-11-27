#include <Servo.h>
#include <SoftwareSerial.h>

// RFID Module EM-18 Connections (RX -> Pin 3, TX -> Pin 2)
SoftwareSerial rfidSerial(3, 2);  // RX, TX for RFID
// GSM Module Connections (RX -> Pin 10, TX -> Pin 11)
SoftwareSerial gsmSerial(5, 4);  // RX, TX for GSM

Servo servo;  // Servo for lid
const int RedLED = A2;
const int GreenLED = A3;
// Pin Definitions
const int irPin = 7;      // IR sensor pin
const int trigPin = 7;    // Ultrasonic trigger pin
const int echoPin = 8;    // Ultrasonic echo pin
const int servoPin = 9;   // Servo pin
const int buzzerPin = 9;  // Buzzer pin (optional)

// Variables
const int fullThreshold = 10;  // Set the threshold distance (in cm) for the bin being full
boolean isFull = false;        // Flag to check if the dustbin is full
boolean correctRFID = false;   // Flag to track correct RFID scan

// Correct RFID tag (example tag, replace with the actual one)
String validRFID = "0B00285DE09E";

// GSM Phone number for SMS (replace with actual number)
const String phoneNumber = "+917977845638";  // Replace ZZ with country code and xxxxxxxxxxx with phone number
bool isSMSSent = false;
// Function Prototypes
int measureDistance();
void openLid();
void closeLid();
void checkRFID();
void sendSMS();
void buzzerAlert();

void setup() {
  Serial.begin(9600);
  rfidSerial.begin(9600);  // Start RFID module serial communication
  gsmSerial.begin(9600);   // Start GSM module serial communication

  pinMode(irPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(RedLED, OUTPUT);
  pinMode(GreenLED, OUTPUT);
  digitalWrite(RedLED, LOW);
  digitalWrite(GreenLED, LOW);
  servo.attach(servoPin);
  closeLid();  // Keep the lid closed initially
  gsmSerial.listen();
  gsmSerial.println("AT");
  updateSerial();
  gsmSerial.println("AT+CMGF=1");  // Set SMS text mode
  updateSerial();
  sendSMS();
}

void loop() {
  // Measure trash level using the ultrasonic sensor
  int level = measureDistance();
  Serial.print("Trash Level: ");
  Serial.println(level);

  // Check if dustbin is full
  if (level < fullThreshold && !isSMSSent) {
    isFull = true;
    isSMSSent = true;

  } else {
    isSMSSent = false;
    isFull = false;
  }
  rfidSerial.listen();
  checkRFID();
  // If dustbin is full, require RFID card to open
  if (isFull) {
    digitalWrite(RedLED, HIGH);
    Serial.println("Dustbin is full.");
    gsmSerial.listen();
    sendSMS();  // Send SMS when the dustbin is full
    Serial.println("Waiting for cleaner to scan RFID...");
    
  } else {

    // Normal operation (IR sensor detects and opens lid)
    if (digitalRead(irPin) == LOW) {  // Assuming LOW means detection
    digitalWrite(GreenLED,HIGH);
      Serial.println("Person detected.");
      openLid();
      delay(5000);  // Keep lid open for 5 seconds
      closeLid();
        digitalWrite(GreenLED,LOW);

    }
  }

  delay(1000);  // Delay before repeating the loop
}

// Function to measure the distance using the ultrasonic sensor
int measureDistance() {
  long duration;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance in cm
  int distance = duration * 0.034 / 2;
  return distance;
}

// Function to open the lid
void openLid() {
  Serial.println("Opening lid...");
  servo.write(90);  // Move servo to 90 degrees (open position)
}

// Function to close the lid
void closeLid() {
  Serial.println("Closing lid...");
  servo.write(0);  // Move servo to 0 degrees (closed position)
}

// Function to check RFID card for cleaner
void checkRFID() {
  rfidSerial.listen();
  if (rfidSerial.available() > 0) {
    String rfidTag = "";
    while (rfidSerial.available() > 0) {
      rfidTag += (char)rfidSerial.read();
      delay(10);  // Short delay to allow all data to be read
    }

    Serial.print("RFID Tag: ");
    Serial.println(rfidTag);

    // Check if RFID matches the correct tag
    if (rfidTag.indexOf(validRFID) != -1) {
      digitalWrite(RedLED, LOW);
      Serial.println("Correct RFID scanned. Opening lid...");
      digitalWrite(GreenLED, HIGH);
      openLid();
      delay(10000);
      closeLid();
      digitalWrite(GreenLED, LOW);
    } else {
      Serial.println("Incorrect RFID scanned.");
      buzzerAlert();  // Alert for incorrect RFID
    }
  }
}

// Function to buzz the buzzer on incorrect RFID
void buzzerAlert() {
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
}

// Function to send SMS using the GSM module
void sendSMS() {
  gsmSerial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  updateSerial();
  String message = "Dustbin is full. Please empty the dustbin.\nLat: 18.9345107\nLong:72.8226356";
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
