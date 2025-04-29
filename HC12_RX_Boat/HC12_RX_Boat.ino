#include <SoftwareSerial.h>
#include <Servo.h>

// HC-12 SoftwareSerial configuration (pins 2 & 3)
#define HC12_TX 3  // Arduino TX to HC-12 RX
#define HC12_RX 2  // Arduino RX from HC-12 TX

SoftwareSerial hc12(HC12_TX, HC12_RX);  // Create SoftwareSerial port

// Motor control pins (adjust as needed)
const int leftMotorForward = 4;
const int leftMotorBackward = 5;
const int rightMotorForward = 6;
const int rightMotorBackward = 7;

// Servo configuration
const int servoPin = 9;
Servo myServo;

// Relay control pins (for DP commands)
const int relayPin1 = 11;
const int relayPin2 = 12;
const int relayPin3 = A0;

void setup() {
  Serial.begin(9600);  // For debugging via Serial Monitor
  hc12.begin(9600);    // HC-12 baud rate

  Serial.println("HC-12 Receiver Ready");

  // Set motor control pins as outputs
  pinMode(leftMotorForward, OUTPUT);
  pinMode(leftMotorBackward, OUTPUT);
  pinMode(rightMotorForward, OUTPUT);
  pinMode(rightMotorBackward, OUTPUT);

  // Attach the servo
  myServo.attach(servoPin);

  // Set relay pins as outputs and initialize them to LOW
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  digitalWrite(relayPin1, HIGH);
  digitalWrite(relayPin2, HIGH);
  digitalWrite(relayPin3, HIGH);
}

// --- Motor control helper functions ---
void moveForward() {
  digitalWrite(leftMotorForward, HIGH);
  digitalWrite(leftMotorBackward, LOW);
  digitalWrite(rightMotorForward, HIGH);
  digitalWrite(rightMotorBackward, LOW);
}

void turnLeft() {
  // Example: run only the right motor for a left turn
  digitalWrite(leftMotorForward, LOW);
  digitalWrite(leftMotorBackward, LOW);
  digitalWrite(rightMotorForward, HIGH);
  digitalWrite(rightMotorBackward, LOW);
}

void turnRight() {
  // Example: run only the left motor for a right turn
  digitalWrite(leftMotorForward, HIGH);
  digitalWrite(leftMotorBackward, LOW);
  digitalWrite(rightMotorForward, LOW);
  digitalWrite(rightMotorBackward, LOW);
}

void stopMotors() {
  digitalWrite(leftMotorForward, LOW);
  digitalWrite(leftMotorBackward, LOW);
  digitalWrite(rightMotorForward, LOW);
  digitalWrite(rightMotorBackward, LOW);
}

void loop() {
  if (hc12.available()) {
    String receivedMessage = hc12.readStringUntil('\n');  // Read incoming message
    receivedMessage.trim();                               // Remove stray whitespace/newlines
    Serial.println(receivedMessage);
    if (receivedMessage != "ST") {
      Serial.print("Received: ");
      Serial.println(receivedMessage);
    }
    if (receivedMessage.length() > 0) {
      char cmd = receivedMessage.charAt(0);

      // Check if it's a servo command (starts with 'S' and has a number)
      if (cmd == 'A' && receivedMessage.length() > 1) {
        int angle = receivedMessage.substring(1).toInt();
        Serial.print("Setting servo to ");
        Serial.println(angle);
        myServo.write(angle);
      } else {
        // Process other commands with a switch-case
        switch (cmd) {
          case 'F':
            Serial.println("Moving forward");
            moveForward();
            break;
          case 'L':
            Serial.println("Turning left");
            turnLeft();
            break;
          case 'R':
            Serial.println("Turning right");
            turnRight();
            break;
          case 'S':
            //Serial.println("Stopping motors");
            stopMotors();
            break;
          case 'D':
            Serial.println("Activating DP: Relay1 HIGH, Relay2 LOW");
            digitalWrite(relayPin1, HIGH);
            digitalWrite(relayPin2, LOW);
            delay(2000);
            digitalWrite(relayPin1, HIGH);
            digitalWrite(relayPin2, HIGH);
            break;
          case 'P':
            Serial.println("Activating DP: Relay1 LOW, Relay2 HIGH");
            digitalWrite(relayPin1, LOW);
            digitalWrite(relayPin2, HIGH);
            delay(2000);
            digitalWrite(relayPin1, HIGH);
            digitalWrite(relayPin2, HIGH);
            break;

          case 'O':
            Serial.println("Activating DP: Relay1 HIGH, Relay2 LOW");
            digitalWrite(relayPin3, HIGH);
            delay(500);
            break;
          case 'C':
            Serial.println("Activating DP: Relay1 LOW, Relay2 HIGH");
            digitalWrite(relayPin3, LOW);
            delay(500);
            break;
          default:
            Serial.println("Unknown command");
            break;
        }
      }
    }
  }
}
