#include <SoftwareSerial.h>
#include <Servo.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(3, 2); // SIM800L Tx & Rx is connected to Arduino #3 & #2

// Pins for the ultrasonic sensor
const int trigPin = 9;
const int echoPin = 10;

// Servo motor pin
Servo servo;
const int servoPin = 11;

// MQ135 sensor pin (analog)
const int mq135Pin = A0;
const int gasThreshold = 400; // Set a threshold value for hazardous gas level

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  
  // Initialize SIM800L
  mySerial.begin(9600);
  Serial.println("Initializing SIM800L...");
  delay(1000);
  
  // Send SMS initialization commands
  mySerial.println("AT");
  updateSerial();
  mySerial.println("AT+CMGF=1"); // Set SMS text mode
  updateSerial();

  // Initialize ultrasonic sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Initialize the servo motor
  servo.attach(servoPin);
  servo.write(0); // Start with the dustbin lid closed

  // MQ135 setup (analog pin A0)
  pinMode(mq135Pin, INPUT);
}

void loop() {
  // Measure distance using the ultrasonic sensor
  int distance = getDistance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println("cm");
  // If a person is detected (less than 20 cm), open the lid
  if (distance < 20) {
    Serial.println("Person detected. Opening lid.");
    servo.write(90); // Open the lid (90 degrees)
    delay(5000); // Wait for 5 seconds
    servo.write(0); // Close the lid
  }

  // Read MQ135 sensor value
  int gasLevel = analogRead(mq135Pin);
  Serial.print("Gas Level: ");
  Serial.println(gasLevel);

  // If hazardous gas detected, send an SMS
  if (gasLevel > gasThreshold) {
    Serial.println("Hazardous gas detected. Sending SMS alert.");
    sendSMS();
  }

  delay(1000); // Delay for a second before the next loop
}

// Function to measure distance using ultrasonic sensor
int getDistance() {
  long duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  return distance;
}

// Function to send SMS using SIM800L
void sendSMS() {
  mySerial.println("AT+CMGS=\"+ZZxxxxxxxxxx\""); // Replace with actual phone number
  updateSerial();
  mySerial.print("Warning! Hazardous gas detected in the area."); // SMS content
  updateSerial();
  mySerial.write(26); // Send the SMS
}

// Function to update serial communication between SIM800L and Arduino
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    mySerial.write(Serial.read()); // Forward what Serial received to SIM800L
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read()); // Forward what SIM800L received to Serial
  }
}
