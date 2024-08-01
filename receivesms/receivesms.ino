#include <SoftwareSerial.h>

// Motor control pins
const int motor1Pin1 = 10;
const int motor1Pin2 = 11;
const int motor2Pin1 = 12;
const int motor2Pin2 = 13;

SoftwareSerial mySerial(2, 3);  //SIM800L Tx & Rx is connected to Arduino #3 & #2

void setup() {
  Serial.begin(9600);

  // Initialize motor control pins
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Begin serial communication with Arduino and SIM800L
  mySerial.begin(9600);

  Serial.println("Initializing...");
  delay(1000);

  mySerial.println("AT");  // Once the handshake test is successful, it will back to OK
  updateSerial();

  mySerial.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
  mySerial.println("AT+CNMI=1,2,0,0,0");  // Decides how newly arrived SMS messages should be handled
  updateSerial();
}

void loop() {
  String c = readSMS();
  if (c.length() > 0) {
    Serial.println("Received SMS: " + c);

    if (c.indexOf("START") >= 0) {
      startMotors();
    } else if (c.indexOf("STOP") >= 0) {
      stopMotors();
    }
  }
}

void updateSerial() {
  delay(500);
  while (Serial.available()) {
    mySerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());  // Forward what Software Serial received to Serial Port
  }
}

String readSMS() {
  String sms = "";
  delay(800);
  while (Serial.available()) {
    mySerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.println("SMS Starts");
    sms = mySerial.readString();
    Serial.println("Unparsed SMS: "+String(sms));
    sms.trim();
    if (sms.startsWith("+CMT:")) {
      int lIdx = sms.indexOf("\n");
      Serial.println("Last Index: " + String(lIdx));
      sms = sms.substring(lIdx);
      sms.replace("\n", "");
    }
    Serial.println(sms);  // Forward what Software Serial received to Serial Port
    Serial.println("SMS Ends");
  }
  return sms;
}

void startMotors() {
  // Set motors to move forward
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
  Serial.println("Motors started");
}

void stopMotors() {
  // Stop the motors
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);
  Serial.println("Motors stopped");
}
