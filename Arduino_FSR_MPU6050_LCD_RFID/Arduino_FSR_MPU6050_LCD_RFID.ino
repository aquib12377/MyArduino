#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// RC522 Pins
#define RST_PIN 5
#define SS_PIN 10
MFRC522 mfrc522(SS_PIN, RST_PIN);

// FSR Pins
#define FSR1_PIN A0
#define FSR2_PIN A1

// MPU6050
Adafruit_MPU6050 mpu;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variables
int sets = 0, reps = 0;
float angle = 0.0;
bool exerciseStarted = false;
int currentExercise = 0;

// RFID UIDs for exercises
const byte exerciseUIDs[6][4] = {
    {0xDE, 0xAD, 0xBE, 0xEF},  // Exercise 1
    {0xCA, 0xFE, 0xBA, 0xBE},  // Exercise 2
    {0xAB, 0xCD, 0xEF, 0x01},  // Exercise 3
    {0x12, 0x34, 0x56, 0x78},  // Exercise 4
    {0x9A, 0xBC, 0xDE, 0xF0},  // Exercise 5
    {0x00, 0x11, 0x22, 0x33}   // Exercise 6
};

// Functions
void resetExercise() {
  sets = 0;
  reps = 0;
  exerciseStarted = false;
  currentExercise = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan to Start");
}

void printCardUID(byte *uid, byte length) {
  Serial.print("Card UID: ");
  for (byte i = 0; i < length; i++) {
    Serial.print(uid[i], HEX);
    if (i != length - 1) Serial.print(":");
  }
  Serial.println();
}

bool validateCard(byte *uid) {
  for (int i = 0; i < 6; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (exerciseUIDs[i][j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      currentExercise = i + 1; // Set current exercise (1-based index)
      return true;
    }
  }
  return false;
}

unsigned long t = 0;

void setup() {
  Serial.begin(9600);

  // Initialize RC522
  SPI.begin();
  mfrc522.PCD_Init();

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Could not find MPU6050!");
    while (1)
      ;
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  resetExercise();

  // Initialize buzzer
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void loop() {
  // Check RC522 for a scan
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    printCardUID(mfrc522.uid.uidByte, mfrc522.uid.size);

    if (validateCard(mfrc522.uid.uidByte)) {
      exerciseStarted = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Exercise ");
      lcd.print(currentExercise);
      lcd.setCursor(0, 1);
      lcd.print("Started");
      delay(1000);
      t = millis();
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Invalid Card!");
      lcd.setCursor(0, 1);
      lcd.print("Try Again.");
      delay(2000);
      resetExercise();
    }
    mfrc522.PICC_HaltA();
  }

  if (exerciseStarted && abs(millis() - t) < 30000) {
    // Read sensors for exercises
    int fsr1Value = analogRead(FSR1_PIN);
    int fsr2Value = analogRead(FSR2_PIN);

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    switch (currentExercise) {
      case 1:  // Exercise 1 - FSR1
        if (fsr1Value > 300) reps++;
        break;
      case 2:  // Exercise 2 - FSR2
        if (fsr2Value > 300) reps++;
        break;
      case 3:  // Exercise 3 - MPU6050 Right
        if (a.acceleration.x > 5) reps++;
        break;
      case 4:  // Exercise 4 - MPU6050 Left
        if (a.acceleration.x < -5) reps++;
        break;
      case 5:  // Exercise 5 - MPU6050 Forward
        if (a.acceleration.y > 5) reps++;
        break;
      case 6:  // Exercise 6 - MPU6050 Backward
        if (a.acceleration.y < -5) reps++;
        break;
    }

    // Update reps and sets
    if (reps >= 2) {  // 2 reps = 1 set
      reps = 0;
      sets++;
    }

    if (sets >= 2) {  // 2 sets = complete
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Set Complete!");
      lcd.setCursor(0, 1);
      lcd.print("Sets: ");
      lcd.print(sets);
      delay(2000);
      resetExercise();
    } else {
      // Update LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Reps: ");
      lcd.print(reps);
      lcd.setCursor(0, 1);
      lcd.print("Sets: ");
      lcd.print(sets);
    }
    delay(500);  // Debounce delay
  } else if (abs(millis() - t) > 30000) {
    // Timeout
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    delay(500);
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    lcd.clear();
    lcd.print("Timeout! Retry.");
    delay(2000);
    resetExercise();
  }
}
