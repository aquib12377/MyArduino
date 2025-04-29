#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// —— PIN DEFINITIONS ——
// RC522 (MFRC522) connections to Nano:
//   SDA/SS  -> D10
//   SCK     -> D13
//   MOSI    -> D11
//   MISO    -> D12
//   RST     -> D8
#define RFID_SS_PIN   10
#define RFID_RST_PIN  9

// IR sensors (active LOW)
#define IR1_PIN   A0   // Car presence #1
#define IR2_PIN   A1   // Car presence #2

// Gate servo
#define SERVO_PIN 5

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
Servo gateServo;

// Replace these bytes with your card’s UID
const byte AUTHORIZED_UID[4] = { 0x93, 0xE6, 0x2A, 0xFB };

// Gate positions (adjust if needed)
const uint8_t GATE_CLOSED_ANGLE =   0;
const uint8_t GATE_OPEN_ANGLE   =  90;

// How long before auto‐closing if IR2 never trips (ms)
const unsigned long CLOSE_TIMEOUT = 30000;

enum State {
  WAIT_FOR_CARD,
  WAIT_FOR_IR1,
  WAIT_FOR_IR2
};

State state = WAIT_FOR_CARD;
unsigned long lastActionTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}  // wait for Serial on native USB boards

  // Init SPI bus and RC522
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println(F("RFID reader ready"));

  // IR sensors
  pinMode(IR1_PIN, INPUT_PULLUP);
  pinMode(IR2_PIN, INPUT_PULLUP);

  // Servo (gate)
  gateServo.attach(SERVO_PIN);
  gateServo.write(GATE_CLOSED_ANGLE);
  Serial.println(F("Gate closed"));
}

void loop() {
  switch (state) {

    // —— State 1: Wait for an authorized RFID card —— 
    case WAIT_FOR_CARD:
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        Serial.print(F("Card detected, UID: "));
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          Serial.print(mfrc522.uid.uidByte[i], HEX);
          Serial.print(' ');
        }
        Serial.println();

        if (isAuthorized()) {
          Serial.println(F("✔ Access granted"));
          state = WAIT_FOR_IR1;
        } else {
          Serial.println(F("✖ Access denied"));
        }
        mfrc522.PICC_HaltA();  // stop reading until next card
      }
      break;


    // —— State 2: Wait for car at IR1 to open gate —— 
    case WAIT_FOR_IR1:
      if (digitalRead(IR1_PIN) == LOW) {
        openGate();
        lastActionTime = millis();
        state = WAIT_FOR_IR2;
      }
      break;


    // —— State 3: Wait for car to clear IR2, then close gate —— 
    case WAIT_FOR_IR2:
      // Car crossed second beam?
      if (digitalRead(IR2_PIN) == LOW) {
        while(digitalRead(IR2_PIN) == LOW){}
        closeGate();
        state = WAIT_FOR_CARD;
      }
      // Safety: timeout auto‐close
      else if (millis() - lastActionTime > CLOSE_TIMEOUT) {
        Serial.println(F("Timeout — closing gate"));
        closeGate();
        state = WAIT_FOR_CARD;
      }
      break;
  }
}


// Checks if the scanned UID matches AUTHORIZED_UID
bool isAuthorized() {
  if (mfrc522.uid.size != sizeof(AUTHORIZED_UID)) return false;
  for (byte i = 0; i < sizeof(AUTHORIZED_UID); i++) {
    if (mfrc522.uid.uidByte[i] != AUTHORIZED_UID[i]) return false;
  }
  return true;
}

void openGate() {
  gateServo.write(GATE_OPEN_ANGLE);
  Serial.println(F("Gate opened"));
}

void closeGate() {
  gateServo.write(GATE_CLOSED_ANGLE);
  Serial.println(F("Gate closed"));
}
