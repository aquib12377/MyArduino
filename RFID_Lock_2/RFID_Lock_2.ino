#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// RC522 RFID module pins
#define SS_PIN 10
#define RST_PIN 9

// Servo control pin
#define SERVO_PIN 3

// Ultrasonic sensor pins (HC-SR04)
#define TRIG_PIN A0
#define ECHO_PIN A1

// Define the distance threshold (in centimeters)
// If an object is closer than this value, the servo will not open.
#define DISTANCE_THRESHOLD 30

// Create an instance of the MFRC522 class
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Create an instance of the Servo class
Servo myservo;

// Define your authorized RFID card UID (replace these with your card's UID bytes)
byte allowedUID[4] = {0xB3, 0xB0, 0xCE, 0x26};

void setup() {
  Serial.begin(9600);
  while (!Serial);  // Wait for the serial port to be available

  // Initialize SPI bus and the RC522 module
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RFID and Ultrasonic Sensor system initialized.");

  // Attach the servo and set it to the locked position (0°)
  myservo.attach(SERVO_PIN);
  myservo.write(0);

  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  delay(2000);
}

// Function to measure distance using the HC-SR04 ultrasonic sensor
long readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2; // Calculate distance in cm
  return distance;
}

// Helper function to slowly move the servo from a starting angle to an ending angle
void slowMoveServo(int startAngle, int endAngle) {
  int step = (startAngle < endAngle) ? 1 : -1;
  for (int pos = startAngle; pos != endAngle; pos += step) {
    myservo.write(pos);
    delay(15); // Adjust this delay to change the speed of movement
  }
  myservo.write(endAngle); // Ensure final position is set
}

void loop() {
  // Look for new RFID cards; if none found, exit the loop iteration
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Print the card UID for debugging
  Serial.print("Card UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check if the scanned card is authorized
  bool authorized = true;
  if (mfrc522.uid.size != 4) {
    authorized = false;
  } else {
    for (byte i = 0; i < 4; i++) {
      if (mfrc522.uid.uidByte[i] != allowedUID[i]) {
        authorized = false;
        break;
      }
    }
  }

  if (authorized) {
    Serial.println("Authorized card detected.");
    
    // Measure the distance from the ultrasonic sensor
    long distance = readUltrasonicDistance();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    // Check if an object is detected in front of the sensor
    if (distance < DISTANCE_THRESHOLD) {
      Serial.println("Object detected in front. Servo will not open.");
    } else {
      Serial.println("No object detected. Opening servo lock slowly.");
      // Slowly open the servo lock (from 0° to 90°)
      slowMoveServo(0, 90);
      delay(5000); // Keep unlocked for 5 seconds
      // Slowly close the servo lock (from 90° back to 0°)
      slowMoveServo(90, 0);
    }
  } else {
    Serial.println("Unauthorized card.");
  }

  // Halt further communication with the card and stop encryption
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  delay(1000); // Short delay before next scan
}
