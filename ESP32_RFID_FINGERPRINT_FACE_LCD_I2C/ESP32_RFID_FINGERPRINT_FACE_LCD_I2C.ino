/***************************************************
  Enhanced Attendance System with RFID, Fingerprint, and LCD
  - Handles 3 students with RFID and Fingerprint verification
  - Displays attendance status on I2C LCD
****************************************************/

#include <Adafruit_Fingerprint.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ======================= LCD Definitions =======================
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust the I2C address if necessary

// ======================= RFID Definitions =======================
#define SS_PIN 5      // SDA pin
#define RST_PIN 4     // RST pin

MFRC522 rfid(SS_PIN, RST_PIN); // Create MFRC522 instance

MFRC522::MIFARE_Key key; 

// ======================= Fingerprint Definitions =======================
#define RX_PIN 16     // RX pin for ESP32 (connected to TX of fingerprint)
#define TX_PIN 17     // TX pin for ESP32 (connected to RX of fingerprint)

#if defined(__AVR__) || defined(ESP8266)
  #include <SoftwareSerial.h>
  SoftwareSerial mySerial(2, 3); // RX, TX for SoftwareSerial
#else
  #define mySerial Serial2
#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// ======================= Student Structure =======================
struct Student {
  String name;
  byte uid[4];
  uint8_t fingerprintID;
  bool attended;
};

// ======================= Students Data =======================
#define NUM_STUDENTS 3
Student students[NUM_STUDENTS] = {
  {"Javeria", {0x84, 0x6D, 0xC6, 0x71}, 2, false},
  {"Siddiqua",   {0x73, 0xD4, 0x3E, 0x95}, 3, false},//73 D4 3E 95
};

// ======================= Helper Functions =======================

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

/**
 * Initialize the LCD display
 */
void initLCD() {
  lcd.begin();                      // Initialize the LCD
  lcd.backlight();                 // Turn on the backlight
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Attendance Sys");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
}

/**
 * Find student index by UID
 */
int findStudentByUID(byte *uid, byte uidSize) {
  for (int i = 0; i < NUM_STUDENTS; i++) {
    bool match = true;
    for (byte j = 0; j < uidSize; j++) {
      if (uid[j] != students[i].uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return i;
  }
  return -1; // Not found
}

/**
 * Find student index by Fingerprint ID
 */
int findStudentByFingerprintID(uint8_t fid) {
  for (int i = 0; i < NUM_STUDENTS; i++) {
    if (students[i].fingerprintID == fid) return i;
  }
  return -1; // Not found
}

/**
 * Display message on LCD
 */
void displayLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

/**
 * Mark attendance and display on LCD
 */
void markAttendance(int studentIndex) {
  students[studentIndex].attended = true;
  String msg1 = students[studentIndex].name;
  String msg2 = "Checked In";
  displayLCD(msg1, msg2);
  Serial.print(msg1);
  Serial.println(" checked in.");
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Fingerprint: Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("Fingerprint: No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Fingerprint: Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Fingerprint: Imaging error");
      return p;
    default:
      Serial.println("Fingerprint: Unknown error");
      return p;
  }

  // Convert image to template
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Fingerprint: Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Fingerprint: Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Fingerprint: Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Fingerprint: Could not find features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Fingerprint: Invalid image");
      return p;
    default:
      Serial.println("Fingerprint: Unknown error");
      return p;
  }

  // Search for fingerprint
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint: Found a match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Fingerprint: Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Fingerprint: No match found");
    return p;
  } else {
    Serial.println("Fingerprint: Unknown error");
    return p;
  }

  // Match found
  Serial.print("Fingerprint ID: ");
  Serial.println(finger.fingerID);
  Serial.print("Confidence: ");
  Serial.println(finger.confidence);

  return finger.fingerID;
}

void checkRFID() {
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been read
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check if the PICC is of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && 
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("RFID: Tag is not MIFARE Classic."));
    return;
  }

  // Find which student scanned the card

  printHex(rfid.uid.uidByte, rfid.uid.size);

  int studentIndex = findStudentByUID(rfid.uid.uidByte, rfid.uid.size);
  if (studentIndex == -1) {
    Serial.println(F("RFID: Unknown card detected."));
    displayLCD("Unknown Card", "Access Denied");
  } else {
    Serial.print(F("RFID: Card matched with "));
    Serial.println(students[studentIndex].name);
    displayLCD("Welcome,", students[studentIndex].name);
    delay(1000);
    // Proceed to fingerprint verification
    Serial.println("Please verify your fingerprint...");
    displayLCD("Please verify", "Your Finger");
    delay(5000);
    uint8_t fingerprintID = getFingerprintID();
    if (fingerprintID == students[studentIndex].fingerprintID) {
      Serial.println("Fingerprint: Verification successful.");
      displayLCD("Finger Done", "Finger Verified");
      delay(1000);
      markAttendance(studentIndex);
      delay(1000);
      // if(digitalRead(27)==LOW)
      // {
      //   markAttendance(studentIndex);
      // }
      // else{
      //   displayLCD("Face Verification", "Face not verified");
      // }

    } else {
      Serial.println("Fingerprint: Verification failed.");
      displayLCD("Verification", "Failed");
    }
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

void setup()
{
  // Initialize Serial Monitor
  Serial.begin(9600);
  pinMode(27,INPUT_PULLUP);
  // Initialize LCD
  initLCD();

  // Initialize Fingerprint Sensor
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN); // Initialize UART2 or SoftwareSerial
  delay(5);
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint: Sensor found!");
  } else {
    Serial.println("Fingerprint: Sensor not found :(");
    displayLCD("Fingerprint", "Sensor Error");
    while (1) { delay(1); }
  }

  // Display Fingerprint Sensor Status
  Serial.println(F("Reading fingerprint sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0) {
    Serial.println("Fingerprint: No templates found. Please enroll fingerprints.");
    displayLCD("No Fingerprints", "Enrolled");
  }
  else {
    Serial.print("Fingerprint: Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
  }

  // Initialize RFID
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 

  // Initialize RFID key
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("RFID: Ready to scan."));
  Serial.print(F("RFID: Using Key:"));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();

  displayLCD("System Ready", "Scan Card");
}

void loop() {
  checkRFID();
  delay(100); // Small delay to avoid flooding
}
