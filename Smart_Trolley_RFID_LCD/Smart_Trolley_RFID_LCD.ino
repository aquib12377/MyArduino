#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

// RFID
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Button
const int buttonPin = 2;
bool buttonPressed = false;

// Prices for each RFID card (assuming 3 items with different prices)
const float prices[] = { 100.0, 200.0, 300.0 };  // Prices for 3 items
float totalAmount = 0;

// Store RFID card UIDs
String cardUIDs[] = { "53d5b313", "13e3ee28", "3af3a14" };  // Replace with actual UIDs of your cards
bool itemScanned[] = { false, false, false };    // To track which items have been scanned

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Initialize RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Place your card");

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Trolley");

  // Initialize Button
  pinMode(buttonPin, INPUT_PULLUP);

  // Small delay to show boot screen
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Item:");
}

void loop() {
  // Check for button press
  if (digitalRead(buttonPin) == LOW && !buttonPressed) {
    Serial.println("Total Calculating..");
    buttonPressed = true;
    displayTotal();
  }

  if (digitalRead(buttonPin) == HIGH && buttonPressed) {
    buttonPressed = false;
  }

  // Check if a new RFID card is placed
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Read RFID UID
  String currentUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    currentUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.print("Card Scanned: ");
  Serial.println(currentUID);
  // Check if the scanned UID matches any of the predefined UIDs
  for (int i = 0; i < 3; i++) {
    if (currentUID.equalsIgnoreCase(cardUIDs[i])) {
      Serial.print("Card detected: ");
      Serial.println(currentUID);

      // Add price to the total and mark item as scanned
      totalAmount += prices[i];
      itemScanned[i] = true;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Item Added: ");
      lcd.setCursor(0, 1);
      lcd.print("Price: ");
      lcd.print(prices[i], 2);
      delay(2000);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Scan Item:");
      break;
    }
  }

  // Halt PICC communication
  mfrc522.PICC_HaltA();
}

void displayTotal() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Total Amount:");
  lcd.setCursor(0, 1);
  lcd.print(totalAmount, 2);
  delay(5000);  // Display the total for 5 seconds
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Item:");
}
