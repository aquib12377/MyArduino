// ============================================================
//  Nano_SmartLock.ino
//  Original: RFID + GSM + Servo + OLED  (Alpha Electronz)
//  Added   : Fingerprint AS608 + ESP32-CAM Face Detection
//
//  Unlock Logic : ANY ONE of the three passes → UNLOCK (OR)
//  GSM Alert    : After every single failed attempt
//
//  ── Pin Map ──────────────────────────────────────────────────
//  RFID MFRC522 : SS=10, RST=9, MOSI=11, MISO=12, SCK=13
//  Servo        : D3
//  Buzzer       : D2
//  GSM SIM800L  : RX=7, TX=8
//  Fingerprint  : RX=4, TX=5   (AS608 TX→D4, AS608 RX→D5)
//  ESP32-CAM    : RX=6, TX=A0  (ESP32 TX→D6, ESP32 RX→A0)
//                 ⚠ Voltage divider on A0→ESP32 RX !
//  OLED I2C     : SDA=A4, SCL=A5
// ============================================================

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

// ── OLED ─────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── RFID ─────────────────────────────────────────────────────
#define RST_PIN  9
#define SS_PIN  10
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ── Servo ────────────────────────────────────────────────────
#define SERVO_PIN          3
const int SERVO_LOCK_ANGLE   = 10;
const int SERVO_UNLOCK_ANGLE = 90;
Servo lockServo;
int currentServoAngle = SERVO_LOCK_ANGLE;

// ── Buzzer ───────────────────────────────────────────────────
#define BUZZER_PIN  2

// ── GSM ──────────────────────────────────────────────────────
#define GSM_RX_PIN  7
#define GSM_TX_PIN  8
SoftwareSerial gsm(GSM_RX_PIN, GSM_TX_PIN);
const char ALERT_NUMBER[] = "+919653215571";

// ── Fingerprint ──────────────────────────────────────────────
// Authorised fingerprint IDs (enrol using Adafruit enrol sketch)
#define FP_RX_PIN  4
#define FP_TX_PIN  5
SoftwareSerial fpSerial(FP_RX_PIN, FP_TX_PIN);
Adafruit_Fingerprint finger(&fpSerial);
const uint8_t AUTH_FP_IDS[]  = {1, 2, 3};   // ← add your enrolled IDs
const uint8_t AUTH_FP_COUNT  = sizeof(AUTH_FP_IDS);

// ── ESP32-CAM ─────────────────────────────────────────────────
// ESP32 GPIO14(TX) → Nano D6(RX)
// Nano A0(TX)      → voltage divider → ESP32 GPIO15(RX)
#define CAM_RX_PIN  6
#define CAM_TX_PIN  A0
SoftwareSerial camSerial(CAM_RX_PIN, CAM_TX_PIN);

// ── Authorised RFID UIDs ──────────────────────────────────────
byte authorizedUID1[4] = { 0x14, 0x97, 0x1A, 0x06 };
byte authorizedUID2[4] = { 0x49, 0xAD, 0x01, 0x04 };

// ── State ─────────────────────────────────────────────────────
bool isLocked = true;

// ── Face result from ESP32 ────────────────────────────────────
bool          faceResultNew = false;
bool          faceResult    = false;
String        camBuffer     = "";

// =============================================================
//  ALL ORIGINAL FUNCTIONS PRESERVED BELOW — unchanged
// =============================================================

// ── Lock Icon ─────────────────────────────────────────────────
void drawLockIcon(bool locked) {
  int bodyX = 44, bodyY = 24, bodyW = 40, bodyH = 28;
  display.fillRoundRect(bodyX, bodyY, bodyW, bodyH, 5, WHITE);
  display.fillRoundRect(bodyX+3, bodyY+3, bodyW-6, bodyH-9, 4, BLACK);
  int keyX = bodyX + bodyW/2, keyY = bodyY + bodyH/2 - 2;
  display.fillCircle(keyX, keyY, 3, WHITE);
  display.fillRect(keyX-2, keyY+2, 4, 7, WHITE);
  int shHeight = 14, baseY = bodyY, topY = baseY - shHeight;
  int leftX = bodyX+10, rightX = bodyX+bodyW-10;
  if (locked) {
    display.drawLine(leftX, baseY, leftX, topY, WHITE);
    display.drawLine(rightX,baseY, rightX,topY, WHITE);
    display.drawLine(leftX, topY, rightX, topY, WHITE);
  } else {
    int midX = (leftX+rightX)/2;
    display.drawLine(leftX, baseY, leftX, topY, WHITE);
    display.drawLine(leftX, topY, midX, topY, WHITE);
    display.drawLine(midX+4, topY+4, rightX, baseY+4, WHITE);
  }
}

// ── Smooth Servo ──────────────────────────────────────────────
void moveServoSmooth(int targetAngle, int stepDelayMs) {
  if (targetAngle == currentServoAngle) return;
  if (targetAngle > currentServoAngle) {
    for (int a = currentServoAngle; a <= targetAngle; a++) { lockServo.write(a); delay(stepDelayMs); }
  } else {
    for (int a = currentServoAngle; a >= targetAngle; a--) { lockServo.write(a); delay(stepDelayMs); }
  }
  currentServoAngle = targetAngle;
}

// ── Animations ───────────────────────────────────────────────
void animateUnlock() {
  for (int i = 0; i < 4; i++) {
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(16,4); display.print(F("ACCESS GRANTED"));
    drawLockIcon(i%2==1);
    display.display(); delay(120);
  }
}

void animateLock() {
  for (int i = 0; i < 4; i++) {
    display.clearDisplay();
    drawLockIcon(i%2==1);
    display.display(); delay(120);
  }
}

void showIdleScreen() {
  display.clearDisplay();
  drawLockIcon(isLocked);
  display.display();
}

// ── Typing Splash ─────────────────────────────────────────────
void showTypingSplash() {
  const char* line1 = "Alpha";
  const char* line2 = "Electronz";
  display.clearDisplay();
  display.setTextSize(2); display.setTextColor(WHITE);
  for (uint8_t i = 0; line1[i] != '\0'; i++) {
    display.setCursor(12+i*12, 22); display.print(line1[i]);
    display.display(); delay(120);
  }
  for (uint8_t i = 0; i < strlen(line2); i++) {
    display.setCursor(12+i*12, 40); display.print(line2[i]);
    display.display(); delay(120);
  }
  delay(500);
}

// ── RFID Helpers ─────────────────────────────────────────────
bool isAuthorizedCard(byte *uid, byte uidSize) {
  if (uidSize != 4) return false;
  bool m1=true, m2=true;
  for (byte i=0;i<4;i++){
    if (uid[i]!=authorizedUID1[i]) m1=false;
    if (uid[i]!=authorizedUID2[i]) m2=false;
  }
  return (m1||m2);
}

void printUID(byte *uid, byte uidSize) {
  Serial.print(F("Card UID:"));
  for (byte i=0;i<uidSize;i++){
    Serial.print(uid[i]<0x10?" 0":" ");
    Serial.print(uid[i],HEX);
  }
  Serial.println();
}

void uidToString(byte *uid, byte uidSize, char *out, size_t outSize) {
  out[0]='\0';
  for (byte i=0;i<uidSize;i++){
    char buf[4]; sprintf(buf,"%02X",uid[i]);
    strcat(out,buf);
    if (i<uidSize-1) strcat(out," ");
  }
}

// ── Buzzer ───────────────────────────────────────────────────
void beepUnauthorized() {
  for (int i=0;i<3;i++){
    digitalWrite(BUZZER_PIN,HIGH); delay(150);
    digitalWrite(BUZZER_PIN,LOW);  delay(150);
  }
}

void beepSuccess() {   // short single beep on unlock
  digitalWrite(BUZZER_PIN,HIGH); delay(100);
  digitalWrite(BUZZER_PIN,LOW);
}

// ── GSM Helpers ──────────────────────────────────────────────
void gsmFlushInput() {
  while (gsm.available()) Serial.write(gsm.read());
}

void gsmSendCommand(const char *cmd, unsigned long waitMs=500) {
  Serial.print(F("[GSM] >> ")); Serial.println(cmd);
  gsm.println(cmd); delay(waitMs); gsmFlushInput();
}

void initGSM() {
  Serial.println(F("[GSM] Initializing..."));
  gsm.listen();
  delay(3000);
  gsmSendCommand("AT",1000);
  gsmSendCommand("ATE0",500);
  gsmSendCommand("AT+CMGF=1",500);
  gsmSendCommand("AT+CSCS=\"GSM\"",500);
  Serial.println(F("[GSM] Ready"));
  camSerial.listen();   // hand back to CAM after GSM init
}

// ── SMS Alert — now includes METHOD that failed ───────────────
// method: "RFID" / "Fingerprint" / "Face"
void sendFailedSMS(const char* method, byte *uid=nullptr, byte uidSize=0) {
  gsm.listen();
  gsmFlushInput();

  gsm.print(F("AT+CMGS=\"")); gsm.print(ALERT_NUMBER); gsm.println(F("\""));
  Serial.print(F("[GSM] >> AT+CMGS=\"")); Serial.print(ALERT_NUMBER); Serial.println(F("\""));
  delay(500);

  gsm.print(F("ALERT: Failed attempt via ")); gsm.print(method);

  // Append UID if it was an RFID attempt
  if (uid != nullptr && uidSize > 0) {
    char uidStr[32]; uidStr[0]='\0';
    uidToString(uid, uidSize, uidStr, sizeof(uidStr));
    gsm.print(F(" | UID=")); gsm.print(uidStr);
  }

  gsm.println(F(" — Unauthorized access at door."));
  gsm.write(26);   // Ctrl+Z
  delay(3000);
  gsmFlushInput();
  Serial.println(F("[GSM] SMS sent"));

  camSerial.listen();   // restore CAM listening
}

// ── ACCESS DENIED screen (original) ──────────────────────────
void showDeniedScreen(const char* method) {
  for (int i=0;i<4;i++){
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(16,4); display.print(F("ACCESS DENIED"));
    display.setCursor(16,14); display.print(method);
    drawLockIcon(true);
    display.drawLine(20,52,108,12,WHITE);
    display.drawLine(20,12,108,52,WHITE);
    display.display(); delay(150);

    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(16,4); display.print(F("ACCESS DENIED"));
    display.setCursor(16,14); display.print(method);
    drawLockIcon(true);
    display.display(); delay(150);
  }
  showIdleScreen();
}

// ── Unlock screen shows which method unlocked ─────────────────
void showUnlockScreen(const char* method) {
  for (int i=0;i<4;i++){
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(10,4); display.print(F("ACCESS GRANTED"));
    display.setCursor(10,14); display.print(method);
    drawLockIcon(i%2==1);
    display.display(); delay(120);
  }
}

// ── setLockState — original logic, now shows method ──────────
void setLockState(bool lock, const char* method="") {
  const int stepMs = 10;
  if (lock) {
    moveServoSmooth(SERVO_LOCK_ANGLE, stepMs);
    animateLock();
    isLocked = true;
    showIdleScreen();
  } else {
    moveServoSmooth(SERVO_UNLOCK_ANGLE, stepMs);
    showUnlockScreen(method);
    beepSuccess();
    isLocked = false;
    showIdleScreen();
    delay(5000);                         // stay open 5 s
    moveServoSmooth(SERVO_LOCK_ANGLE, stepMs);
    animateLock();
    isLocked = true;
    showIdleScreen();
  }
}

// =============================================================
//  NEW HELPERS
// =============================================================

// ── Fingerprint check ────────────────────────────────────────
// Returns matched ID (>0), 0 if no finger, -1 if no match
int getFingerprintID() {
  fpSerial.listen();
  uint8_t p = finger.getImage();
  camSerial.listen();
  if (p != FINGERPRINT_OK) return 0;   // no finger placed

  fpSerial.listen();
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { camSerial.listen(); return -1; }

  p = finger.fingerFastSearch();
  camSerial.listen();

  if (p == FINGERPRINT_OK) {
    Serial.print(F("[FP] ID: ")); Serial.print(finger.fingerID);
    Serial.print(F("  Conf: ")); Serial.println(finger.confidence);
    return finger.fingerID;
  }
  Serial.println(F("[FP] No match"));
  return -1;
}

bool isFPAuthorised(int id) {
  for (uint8_t i=0; i<AUTH_FP_COUNT; i++)
    if (AUTH_FP_IDS[i] == (uint8_t)id) return true;
  return false;
}

// ── Poll ESP32-CAM (non-blocking) ────────────────────────────
void pollCamSerial() {
  camSerial.listen();
  while (camSerial.available()) {
    char c = (char)camSerial.read();
    if (c == '\n') {
      camBuffer.trim();
      if (camBuffer.length() > 0) {
        Serial.print(F("[CAM] ")); Serial.println(camBuffer);
        if (camBuffer == "FACE_OK")   { faceResult=true;  faceResultNew=true; }
        if (camBuffer == "FACE_FAIL") { faceResult=false; faceResultNew=true; }
        camBuffer = "";
      }
    } else {
      camBuffer += c;
      if (camBuffer.length() > 30) camBuffer = "";
    }
  }
}

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(9600);
  Serial.println(F("\n=== Smart Lock: RFID + Fingerprint + Face ==="));

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // GSM
  gsm.begin(9600);
  initGSM();

  // Fingerprint
  fpSerial.begin(57600);
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println(F("[FP] AS608 found OK"));
  } else {
    Serial.println(F("[FP] AS608 NOT found — check wiring"));
  }

  // ESP32-CAM serial
  camSerial.begin(9600);
  camSerial.listen();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[OLED] Init failed!")); while (true);
  }

  // Servo
  lockServo.attach(SERVO_PIN);
  lockServo.write(SERVO_LOCK_ANGLE);
  currentServoAngle = SERVO_LOCK_ANGLE;
  isLocked = true;

  // RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println(F("[RFID] RC522 ready"));

  // Original typing splash
  showTypingSplash();
  showIdleScreen();

  Serial.println(F("Ready — scan card / touch sensor / look at camera"));
}

// =============================================================
//  LOOP
// =============================================================
void loop() {

  // ── Always poll ESP32-CAM in the background ─────────────────
  pollCamSerial();

  // ── 1. RFID ─────────────────────────────────────────────────
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    printUID(mfrc522.uid.uidByte, mfrc522.uid.size);

    if (isAuthorizedCard(mfrc522.uid.uidByte, mfrc522.uid.size)) {
      Serial.println(F("[RFID] Authorised → UNLOCK"));
      setLockState(false, "RFID Card");

    } else {
      Serial.println(F("[RFID] Unauthorised"));
      beepUnauthorized();
      showDeniedScreen("RFID");
      sendFailedSMS("RFID", mfrc522.uid.uidByte, mfrc522.uid.size);
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }

  // ── 2. Fingerprint ──────────────────────────────────────────
  int fpID = getFingerprintID();

  if (fpID > 0) {
    if (isFPAuthorised(fpID)) {
      Serial.println(F("[FP] Authorised → UNLOCK"));
      setLockState(false, "Fingerprint");
    } else {
      Serial.println(F("[FP] Unauthorised fingerprint"));
      beepUnauthorized();
      showDeniedScreen("Fingerprint");
      sendFailedSMS("Fingerprint");
    }
    return;
  }

  // ── 3. Face (result from ESP32-CAM) ─────────────────────────
  if (faceResultNew) {
    faceResultNew = false;

    if (faceResult) {
      Serial.println(F("[CAM] Face authorised → UNLOCK"));
      setLockState(false, "Face");
    } else {
      Serial.println(F("[CAM] Unknown face"));
      beepUnauthorized();
      showDeniedScreen("Face");
      sendFailedSMS("Face");
    }
    return;
  }
}
