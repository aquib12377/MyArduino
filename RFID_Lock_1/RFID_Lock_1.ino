#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// Define pins for RC522 module and Servo
#define SS_PIN 10
#define RST_PIN 9
#define SERVO_PIN 3

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Create a Servo instance
Servo myservo;
//E3 02 B1 29
// Replace these with your authorized RFID card UID bytes (example values)
byte allowedUID[4] = {0xE3, 0x02, 0xB1, 0x29};

void setup() {
  Serial.begin(9600);  // Initialize serial communications with the PC
  while (!Serial);     // Wait for Serial port to be ready (for Leonardo/Micro)
  
  SPI.begin();         // Initialize SPI bus
  mfrc522.PCD_Init();  // Initialize the RC522 RFID reader
  Serial.println("Place your card on the reader...");

  // Attach the servo motor and set it to the locked position (0°)
  myservo.attach(SERVO_PIN);
  myservo.write(0);    
}

void loop() {
  // Look for new RFID cards
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Print UID for debugging
  Serial.print("Card UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check if the scanned UID matches the authorized UID
  bool authorized = true;
  if (mfrc522.uid.size != 4) { // Adjust size check if using different card types
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
    Serial.println("Access Granted!");
    // Move servo to unlocked position (90°)
    myservo.write(90);
    delay(5000);  // Keep the lock open for 5 seconds
    myservo.write(0);  // Return to locked position
  } else {
    Serial.println("Access Denied!");
  }

  // Halt further communication with the card and stop encryption
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  delay(1000);  // Short delay before next read
}
