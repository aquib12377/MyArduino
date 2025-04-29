#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// ————— Pin definitions —————
#define SS_PIN       10    // RC522 SDA
#define RST_PIN       9    // RC522 RST
#define SERVO_PIN     8    // Servo signal pin
#define BUTTON_PIN    3    // Unlock button (wired to GND when pressed)
#define BUZZER_PIN     4  // Buzzer pin (active HIGH)

// ————— Authorized UIDs —————
const char* allowedUIDs[] = {
  "7C:04:F3:00",
  "A3:89:40:27",
  "53:60:6D:28",
  "C3:51:34:27",
  "A3:69:D6:13",
  "03:4E:D4:13"
};
const uint8_t NUM_ALLOWED = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]);

MFRC522  rfid(SS_PIN, RST_PIN);
Servo    lockServo;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // RFID init
  SPI.begin();
  rfid.PCD_Init();

  // Servo init
  lockServo.attach(SERVO_PIN);
  lockServo.write(0);       // Start locked (0°)

  // Button with pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  Serial.println(F("Ready. Scan a card or press the button to unlock."));
}

void loop() {
  // 1) Check button press
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println(F("Button pressed → Unlocking"));
    unlock();
    // simple debounce
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
  }

  // 2) Check for new RFID card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Build UID string
  String uid;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += '0';
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i + 1 < rfid.uid.size) uid += ':';
  }
  uid.toUpperCase();

  // Compare against allowed list
  bool ok = false;
  for (uint8_t i = 0; i < NUM_ALLOWED; i++) {
    if (uid == allowedUIDs[i]) {
      ok = true;
      break;
    }
  }

  if (ok) {
    Serial.print(F("Authorized UID: "));
    Serial.println(uid);
    unlock();
  } else {
    Serial.print(F("Unauthorized UID: "));
    Serial.println(uid);
  }

  // Cleanup for next read
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// Slowly sweep servo to 90°, buzz while opening, hold, then sweep back
void unlock() {
  // Start buzzer
  digitalWrite(BUZZER_PIN, HIGH);

  // Sweep up from 0° to 90°
  for (int pos = 0; pos <= 180; pos++) {
    lockServo.write(pos);
    delay(15);    // adjust for sweep speed (15 ms per degree → ~1.35 s total)
  }

  // Stop buzzer
  digitalWrite(BUZZER_PIN, LOW);

  delay(3000);    // stay unlocked for 3 s

  // Sweep down from 90° back to 0°
  for (int pos = 180; pos >= 0; pos--) {
    lockServo.write(pos);
    delay(15);
  }
}
