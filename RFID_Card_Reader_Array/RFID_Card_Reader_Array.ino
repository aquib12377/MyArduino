  #include <SPI.h>
  #include <MFRC522.h>

  // RFID RC522 pins
  #define SS_PIN 6
  #define RST_PIN 5

  MFRC522 mfrc522(SS_PIN, RST_PIN);

  void setup() {
    Serial.begin(115200); // Initialize serial communications with the PC
    SPI.begin();          // Init SPI bus
    mfrc522.PCD_Init();   // Init MFRC522

    Serial.println(F("Place your RFID card near the reader..."));
  }

  void loop() {
    // Look for new cards
    if (!mfrc522.PICC_IsNewCardPresent()) {
      //Serial.println("No New Card");
      return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
      //Serial.println("No Cards");
      return;
    }

    // Print Card UID in the desired format
    Serial.print("{");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print("0x");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
      if (i < mfrc522.uid.size - 1) {
        Serial.print(", ");
      }
    }
    Serial.println("}");

    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
  }
