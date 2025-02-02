#include <SoftwareSerial.h>

// Define pins for HC-12 communication
#define HC12_RX 2
#define HC12_TX 12

// Pins for Motor 1 (BTS7960 Driver 1)
int RPWM1 = 10;
int LPWM1 = 11;
int R_EN1 = 8;
int L_EN1 = 9;

// Pins for Motor 2 (BTS7960 Driver 2)
int RPWM2 = 5;
int LPWM2 = 3;
int R_EN2 = 6;
int L_EN2 = 7;

// Initialize SoftwareSerial for HC-12
SoftwareSerial HC12(HC12_RX, HC12_TX);

void setup() {
  // Start the serial communication
  Serial.begin(9600);
  HC12.begin(9600);

  // Initialize Motor Control pins
  for (int i = 3; i <= 12; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW); // Initialize all pins to LOW
  }

  Serial.println("HC-12 BOT Receiver Ready!");
}

void loop() {
  // Check if data is available from HC-12
  if (HC12.available()) {
    String command = HC12.readStringUntil('\n'); // Read the incoming command
    Serial.println(command);
    command.trim();
    // Process the command
    if (command == "FORWARD") {
      Serial.println("BOT Moving Forward");
      // Move forward: Motor 1 and Motor 2 forward
      digitalWrite(R_EN1, HIGH);
      digitalWrite(L_EN1, HIGH);
      digitalWrite(RPWM1, HIGH);
      digitalWrite(LPWM1, LOW);

      digitalWrite(R_EN2, HIGH);
      digitalWrite(L_EN2, HIGH);
      digitalWrite(RPWM2, HIGH);
      digitalWrite(LPWM2, LOW);

    } else if (command == "BACKWARD") {
      Serial.println("BOT Moving Backward");
      // Move backward: Motor 1 and Motor 2 backward
      digitalWrite(R_EN1, HIGH);
      digitalWrite(L_EN1, HIGH);
      digitalWrite(RPWM1, LOW);
      digitalWrite(LPWM1, HIGH);

      digitalWrite(R_EN2, HIGH);
      digitalWrite(L_EN2, HIGH);
      digitalWrite(RPWM2, LOW);
      digitalWrite(LPWM2, HIGH);

    } else if (command == "LEFT") {
      Serial.println("BOT Turning Left");
      // Turn left: Motor 1 backward, Motor 2 forward
      digitalWrite(R_EN1, HIGH);
      digitalWrite(L_EN1, HIGH);
      digitalWrite(RPWM1, LOW);
      digitalWrite(LPWM1, HIGH);

      digitalWrite(R_EN2, HIGH);
      digitalWrite(L_EN2, HIGH);
      digitalWrite(RPWM2, HIGH);
      digitalWrite(LPWM2, LOW);

    } else if (command == "RIGHT") {
      Serial.println("BOT Turning Right");
      // Turn right: Motor 1 forward, Motor 2 backward
      digitalWrite(R_EN1, HIGH);
      digitalWrite(L_EN1, HIGH);
      digitalWrite(RPWM1, HIGH);
      digitalWrite(LPWM1, LOW);

      digitalWrite(R_EN2, HIGH);
      digitalWrite(L_EN2, HIGH);
      digitalWrite(RPWM2, LOW);
      digitalWrite(LPWM2, HIGH);

    } else if (command == "STOP") {
      Serial.println("BOT Stopping");
      // Stop both motors
      digitalWrite(R_EN1, LOW);
      digitalWrite(L_EN1, LOW);
      digitalWrite(R_EN2, LOW);
      digitalWrite(L_EN2, LOW);
    }
  }
}
