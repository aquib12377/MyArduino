#include <Servo.h>

const int gasSensorPin = A4;
const int buzzerPin = A5;
const int relayPin = 4;
const int bluetoothRx = 2;
const int bluetoothTx = 3;
Servo myServo;

char command;
bool gasDetected = false;

void setup() {
  pinMode(gasSensorPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);  // Buzzer off
  digitalWrite(relayPin, HIGH); // Relay inactive (Active LOW)
  
  myServo.attach(9);
  myServo.write(0); // Initialize servo to 0 degrees
  
  Serial.begin(9600); // Start serial for Bluetooth
}

void loop() {
  // Check gas sensor
  gasDetected = digitalRead(gasSensorPin) == LOW;
  
  if (gasDetected) {
    digitalWrite(buzzerPin, HIGH); // Turn on buzzer
  } else {
    digitalWrite(buzzerPin, LOW); // Turn off buzzer
  }
  
  // Check for Bluetooth command
  if (Serial.available()) {
    command = Serial.read();
    if (command == '1') {
      myServo.write(90);             // Rotate servo to 90 degrees
      digitalWrite(relayPin, LOW);   // Activate relay
    } else if (command == '0') {
      myServo.write(0);              // Rotate servo back to 0 degrees
      digitalWrite(relayPin, HIGH);  // Deactivate relay
    }
  }
}
