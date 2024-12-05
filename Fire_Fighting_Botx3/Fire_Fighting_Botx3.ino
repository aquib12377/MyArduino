
#include "FireFightingBot.h"
#include "MotorController.h"
#include <SoftwareSerial.h>
MotorController motors(2, 3, 4, 5);   // leftPin1, leftPin2, rightPin1, rightPin2
FireFightingBot fireBot(A0, A1, A2);  // Left: A0, Center: A1, Right: A2
SoftwareSerial gsmSerial(10, 11);     // RX, TX for GSM
#define Relay 6

String phoneNumber = "+919819315374";

void setup() {
  gsmSerial.begin(9600);  // Start GSM module serial communication
  pinMode(Relay,OUTPUT);
  Serial.begin(9600);
  gsmSerial.listen();
  gsmSerial.println("AT");
  updateSerial();
  gsmSerial.println("AT+CMGF=1");  // Set SMS text mode
  updateSerial();
}

void loop() {
  FireDirection direction = fireBot.detectFire();
  delay(500);

  switch (direction) {
    case LEFT_FIRE:
      motors.left();  // Turn left
      delay(500);
      motors.stop();
      Serial.println("Fire on Left");
      break;

    case CENTER_FIRE:
      motors.forward();  // Move forward
      delay(500);
      motors.stop();
      Serial.println("Fire Ahead");
      break;

    case RIGHT_FIRE:
      motors.right();  // Turn right
      delay(500);
      motors.stop();
      Serial.println("Fire on Right");
      break;

    case NO_FIRE:
      motors.stop();
      Serial.println("No Fire Detected");
      break;
  }

  if (direction != NO_FIRE) {
    digitalWrite(Relay, LOW);
    sendSMS();
  }
  else{
    digitalWrite(Relay, HIGH);
  }
}
void sendSMS() {
  gsmSerial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  updateSerial();
  String message = "Fire detected!";
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
