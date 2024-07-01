#include <SPI.h>
#include <MFRC522.h>
#include <DFRobotDFPlayerMini.h>

// RFID RC522 pins
#define SS_PIN 6
#define RST_PIN 5

// Button pins
#define START_BUTTON_PIN 2
#define STOP_BUTTON_PIN 3
#define PAUSE_BUTTON_PIN 4

MFRC522 mfrc522(SS_PIN, RST_PIN);
DFRobotDFPlayerMini myDFPlayer;

// Map RFID card UIDs to sound file numbers
const byte cardUIDs[10][4] = {
  { 0xE0, 0x14, 0xC6, 0x3B },
  { 0x50, 0x5A, 0x77, 0x59 },
  { 0x43, 0x85, 0x12, 0x50 },
  { 0x93, 0xDB, 0x02, 0xE4 },
  { 0x24, 0xAE, 0xD0, 0x09 },
  { 0xD3, 0xA0, 0x67, 0x1A },
  { 0xD3, 0xA0, 0x67, 0x1A },
  { 0xE3, 0x2B, 0xA2, 0xE4 },
  { 0x43, 0x75, 0xAD, 0xE4 },
  { 0xC3, 0x80, 0x50, 0x1D }
};

void setup() {
  Serial.begin(9600);  // Initialize hardware serial for DFPlayer Mini at 9600 baud
  Serial1.begin(9600, SERIAL_8N1, 44, 43);
  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522

  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PAUSE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(1,INPUT_PULLUP);
  if (!myDFPlayer.begin(Serial1)) {
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true)
      ;
  }
  myDFPlayer.volume(30);  // Set volume value (0~30).

  Serial.println(F("Place your RFID card near the reader..."));
}

void loop() {
  int potValue = analogRead(1);

  int vol = map(potValue, 0, 4096, 0, 30);
  vol = constrain(vol,0,30);
  myDFPlayer.volume(vol);
  Serial.println("Volume: "+String(vol));
  Serial.println("Button Start: " + String(digitalRead(START_BUTTON_PIN)));
  Serial.println("Button Stop: " + String(digitalRead(STOP_BUTTON_PIN)));
  Serial.println("Button Pause: " + String(digitalRead(PAUSE_BUTTON_PIN)));
  // Check for button presses
  if (digitalRead(START_BUTTON_PIN) == LOW) {
    myDFPlayer.play(1);  // Play a default audio file (e.g., 0001.mp3)
    Serial.println("Start button pressed, playing default audio.");
    delay(300);  // Debounce delay
  }

  if (digitalRead(STOP_BUTTON_PIN) == LOW) {
    myDFPlayer.stop();
    Serial.println("Stop button pressed, stopping audio.");
    delay(300);  // Debounce delay
  }

  if (digitalRead(PAUSE_BUTTON_PIN) == LOW) {
    myDFPlayer.pause();
    Serial.println("Pause button pressed, pausing audio.");
    delay(300);  // Debounce delay
  }

  // RFID code
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
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

    // Compare the read UID with the predefined UIDs
    for (int i = 0; i < 10; i++) {
      if (memcmp(mfrc522.uid.uidByte, cardUIDs[i], 4) == 0) {
        Serial.print("Playing sound for card ");
        Serial.println(i + 1);
        myDFPlayer.play(i + 1);  // Play the corresponding MP3 file (files should be named 0001.mp3, 0002.mp3, etc.)
        break;
      }
    }

    delay(1000);
  }
  delay(100);
}
