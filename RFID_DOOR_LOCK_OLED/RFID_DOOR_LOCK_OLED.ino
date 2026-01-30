#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <SoftwareSerial.h>   // <-- GSM

// -------------------- OLED CONFIG --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // -1 = no reset pin

// -------------------- RC522 PINS (for Nano) --------------------
#define RST_PIN 9    // Configurable, see wiring
#define SS_PIN  10   // SDA pin on RC522

MFRC522 mfrc522(SS_PIN, RST_PIN);

// -------------------- SERVO CONFIG --------------------
#define SERVO_PIN 3
const int SERVO_LOCK_ANGLE   = 10;   // adjust to your hardware
const int SERVO_UNLOCK_ANGLE = 90;   // adjust to your hardware

Servo lockServo;
int currentServoAngle = SERVO_LOCK_ANGLE;  // track current servo position

// -------------------- AUTHORIZED CARDS --------------------
// Card 1: 14 97 1A 06
byte authorizedUID1[4] = { 0x14, 0x97, 0x1A, 0x06 };
// Card 2: 49 AD 01 04
byte authorizedUID2[4] = { 0x49, 0xAD, 0x01, 0x04 };

// -------------------- BUZZER --------------------
#define BUZZER_PIN 2   // Beeps on unauthorized

// -------------------- GSM CONFIG --------------------
#define GSM_RX_PIN 7   // Nano D7  <-- GSM TX
#define GSM_TX_PIN 8   // Nano D8  <-- GSM RX
SoftwareSerial gsm(GSM_RX_PIN, GSM_TX_PIN);

// CHANGE THIS TO YOUR NUMBER
const char ALERT_NUMBER[] = "+919653215571";  // <-- put your mobile number here

// -------------------- STATE --------------------
bool isLocked = true;

// -------------------- LOCK ICON DRAWING --------------------
// Simple closed vs open lock
void drawLockIcon(bool locked) {
  int bodyX = 44;
  int bodyY = 24;
  int bodyW = 40;
  int bodyH = 28;

  // Body
  display.fillRoundRect(bodyX, bodyY, bodyW, bodyH, 5, WHITE);
  display.fillRoundRect(bodyX + 3, bodyY + 3, bodyW - 6, bodyH - 9, 4, BLACK);

  // Keyhole
  int keyX = bodyX + bodyW / 2;
  int keyY = bodyY + bodyH / 2 - 2;
  display.fillCircle(keyX, keyY, 3, WHITE);
  display.fillRect(keyX - 2, keyY + 2, 4, 7, WHITE);

  // Shackle
  int shHeight = 14;
  int baseY   = bodyY;             // where shackle touches body
  int topY    = baseY - shHeight;

  int leftX   = bodyX + 10;
  int rightX  = bodyX + bodyW - 10;

  if (locked) {
    // CLOSED: normal U shape
    display.drawLine(leftX,  baseY, leftX,  topY, WHITE);
    display.drawLine(rightX, baseY, rightX, topY, WHITE);
    display.drawLine(leftX,  topY, rightX,  topY, WHITE);
  } else {
    // OPEN: left leg stays, right side dropped a bit
    int midX = (leftX + rightX) / 2;

    // Left leg
    display.drawLine(leftX, baseY, leftX, topY, WHITE);

    // Short top segment
    display.drawLine(leftX, topY, midX, topY, WHITE);

    // Right leg shifted down
    int openTopY  = topY + 4;
    int openBaseY = baseY + 4;
    display.drawLine(midX + 4, openTopY, rightX, openBaseY, WHITE);
  }
}

// -------------------- SERVO SMOOTH MOVE --------------------
void moveServoSmooth(int targetAngle, int stepDelayMs) {
  if (targetAngle == currentServoAngle) return;

  if (targetAngle > currentServoAngle) {
    for (int a = currentServoAngle; a <= targetAngle; a++) {
      lockServo.write(a);
      delay(stepDelayMs);
    }
  } else {
    for (int a = currentServoAngle; a >= targetAngle; a--) {
      lockServo.write(a);
      delay(stepDelayMs);
    }
  }
  currentServoAngle = targetAngle;
}

// -------------------- ANIMATIONS --------------------
// Simple blink animation between closed & open lock, with "ACCESS GRANTED"
void animateUnlock() {
  Serial.println(F("[ANIM] Unlocking..."));
  for (int i = 0; i < 4; i++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(16, 4);
    display.print(F("ACCESS GRANTED"));

    bool showOpen = (i % 2 == 1);
    drawLockIcon(showOpen);   // blink between closed/open
    display.display();
    delay(120);
  }
}

// Simple blink animation between open & closed lock
void animateLock() {
  Serial.println(F("[ANIM] Locking..."));
  for (int i = 0; i < 4; i++) {
    display.clearDisplay();
    bool showClosed = (i % 2 == 1);
    drawLockIcon(showClosed);
    display.display();
    delay(120);
  }
}

// Idle screen: only icon, no status text
void showIdleScreen() {
  display.clearDisplay();
  drawLockIcon(isLocked);   // closed if true, open if false
  display.display();
}

// -------------------- TYPING SPLASH --------------------
void showTypingSplash() {
  const char* line1 = "Alpha";
  const char* line2 = "Electronz";

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  int x = 12;
  int y1 = 22;
  int y2 = 40;

  // Type "Alpha"
  for (uint8_t i = 0; line1[i] != '\0'; i++) {
    display.setCursor(x + i * 12, y1);  // ~12px per char at size 2
    display.print(line1[i]);
    display.display();
    delay(120);
  }

  // Type "Electronz"
  for (uint8_t i = 0; i < strlen(line2); i++) {
    display.setCursor(x + i * 12, y2);
    display.print(line2[i]);
    display.display();
    delay(120);
  }

  delay(500); // small hold
}

// -------------------- RFID HELPERS --------------------
bool isAuthorizedCard(byte *uid, byte uidSize) {
  if (uidSize != 4) return false;

  bool match1 = true;
  bool match2 = true;

  for (byte i = 0; i < 4; i++) {
    if (uid[i] != authorizedUID1[i]) match1 = false;
    if (uid[i] != authorizedUID2[i]) match2 = false;
  }

  return (match1 || match2);
}

void printUID(byte *uid, byte uidSize) {
  Serial.print(F("Card UID:"));
  for (byte i = 0; i < uidSize; i++) {
    Serial.print(uid[i] < 0x10 ? " 0" : " ");
    Serial.print(uid[i], HEX);
  }
  Serial.println();
}

// Convert UID to hex string "14 97 1A 06"
void uidToString(byte *uid, byte uidSize, char *out, size_t outSize) {
  out[0] = '\0';
  for (byte i = 0; i < uidSize; i++) {
    char buf[4];
    sprintf(buf, "%02X", uid[i]);
    strcat(out, buf);
    if (i < uidSize - 1) {
      strcat(out, " ");
    }
  }
}

// -------------------- BUZZER HELPERS --------------------
void beepUnauthorized() {
  Serial.println(F("[BUZZER] Unauthorized beep"));
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

// -------------------- GSM HELPERS --------------------
void gsmFlushInput() {
  while (gsm.available()) {
    char c = gsm.read();
    // Optionally echo GSM data to Serial for debugging
    Serial.write(c);
  }
}

void gsmSendCommand(const char *cmd, unsigned long waitMs = 500) {
  Serial.print(F("[GSM] >> ")); Serial.println(cmd);
  gsm.println(cmd);
  delay(waitMs);
  gsmFlushInput();
}

void initGSM() {
  Serial.println(F("[GSM] Initializing GSM module..."));
  delay(3000); // give module time to boot

  // Basic handshake
  gsmSendCommand("AT", 1000);
  gsmSendCommand("ATE0", 500);        // echo off
  gsmSendCommand("AT+CMGF=1", 500);   // SMS text mode
  gsmSendCommand("AT+CSCS=\"GSM\"", 500);

  Serial.println(F("[GSM] GSM init done (check Serial output for OKs)"));
}

void sendUnauthorizedSMS(byte *uid, byte uidSize) {
  Serial.println(F("[GSM] Sending unauthorized card SMS..."));

  char uidStr[3 * 10]; // enough for up to 10 bytes "XX " each
  uidStr[0] = '\0';
  uidToString(uid, uidSize, uidStr, sizeof(uidStr));

  gsmFlushInput();

  // Start SMS
  gsm.print(F("AT+CMGS=\""));
  gsm.print(ALERT_NUMBER);
  gsm.println(F("\""));
  Serial.print(F("[GSM] >> AT+CMGS=\""));
  Serial.print(ALERT_NUMBER);
  Serial.println(F("\""));

  delay(500); // wait for '>' prompt (rough)

  // SMS body
  gsm.print(F("RFID ALERT: Unauthorized card UID="));
  gsm.print(uidStr);
  gsm.println(F(" detected at door."));

  Serial.print(F("[GSM] >> SMS BODY: RFID ALERT: Unauthorized card UID="));
  Serial.print(uidStr);
  Serial.println(F(" detected at door."));

  // End SMS with Ctrl+Z
  gsm.write(26);
  Serial.println(F("[GSM] >> Ctrl+Z sent"));

  delay(3000); // wait for sending
  gsmFlushInput();

  Serial.println(F("[GSM] SMS send attempted."));
}

// -------------------- SET LOCK STATE --------------------
// lock == false => unlock for 5 seconds, then auto lock again
void setLockState(bool lock) {
  const int servoStepDelayMs = 10; // tune for speed (higher = slower)

  if (lock) {
    // Explicit lock request
    moveServoSmooth(SERVO_LOCK_ANGLE, servoStepDelayMs);
    animateLock();
    isLocked = true;
    showIdleScreen();
  } else {
    // UNLOCK -> smooth open, 5s, smooth close
    moveServoSmooth(SERVO_UNLOCK_ANGLE, servoStepDelayMs);
    animateUnlock();
    isLocked = false;
    showIdleScreen();     // show open icon

    // Keep unlocked for 5 seconds
    delay(5000);

    // Smooth move back to lock
    moveServoSmooth(SERVO_LOCK_ANGLE, servoStepDelayMs);
    animateLock();
    isLocked = true;
    showIdleScreen();     // show closed icon
  }
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);
  Serial.println(F("\n=== RFID Door Lock + GSM Alert ==="));

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // GSM serial
  gsm.begin(9600);
  initGSM();

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[OLED] SSD1306 init failed!"));
    while (true) {
      // hang here
    }
  }

  // Servo
  lockServo.attach(SERVO_PIN);
  lockServo.write(SERVO_LOCK_ANGLE);
  currentServoAngle = SERVO_LOCK_ANGLE;
  isLocked = true;

  // SPI + RC522
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println(F("[RFID] RC522 initialized"));

  // Typing splash for "Alpha Electronz"
  showTypingSplash();

  showIdleScreen();
}

// -------------------- LOOP --------------------
void loop() {
  // Look for new card
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Got a card
  printUID(mfrc522.uid.uidByte, mfrc522.uid.size);

  bool authorized = isAuthorizedCard(mfrc522.uid.uidByte, mfrc522.uid.size);

  if (authorized) {
    Serial.println(F("[RFID] Authorized card"));
    // Always perform "open for 5s then relock" with smooth servo
    setLockState(false);
  } else {
    Serial.println(F("[RFID] Unauthorized card"));

    // BEEP + SMS on unauthorized
    beepUnauthorized();
    sendUnauthorizedSMS(mfrc522.uid.uidByte, mfrc522.uid.size);

    // ACCESS DENIED screen + flashing X
    for (int i = 0; i < 4; i++) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(16, 4);
      display.print(F("ACCESS DENIED"));

      drawLockIcon(true);  // show closed lock

      // big X over the lock
      display.drawLine(20, 52, 108, 12, WHITE);
      display.drawLine(20, 12, 108, 52, WHITE);
      display.display();
      delay(150);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(16, 4);
      display.print(F("ACCESS DENIED"));
      drawLockIcon(true);
      display.display();
      delay(150);
    }

    showIdleScreen();
  }

  // Stop reading same card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
