// Smart Parking System – Arduino Uno (directional entry/exit)
// Components:
//   * IR1 beam sensor – pin 13  (active‑LOW)
//   * IR2 beam sensor – pin 12  (active‑LOW)
//     – Sequence IR1→IR2 = entry, IR2→IR1 = exit
//   * 6× slot IR sensors – pins 7,6,5,4,3,2 (active‑LOW; LOW when car present)
//   * SG90/servo barrier – pin 9 (shared for entry & exit lane)
//   * 16×2 LCD with I²C backpack (address 0x27)
//
// Libraries required:
//   Servo.h (built‑in)
//   LiquidCrystal_I2C by Frank de Brabander (or compatible)
// -------------------------------------------------------------

#include <Servo.h>
#include <LiquidCrystal_I2C.h>

// ---------------- Pin assignments ---------------------------
const uint8_t PIN_IR1   = 13;  // Lane sensor 1 (near entry side)
const uint8_t PIN_IR2   = 12;  // Lane sensor 2 (near exit side)
const uint8_t SLOT_PINS[6] = {7, 6, 5, 4, 3, 2};
const uint8_t PIN_SERVO = 9;

// ---------------- Hardware objects --------------------------
Servo barrierServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- Parameters --------------------------------
const uint8_t SERVO_CLOSED = 0;   // barrier down angle
const uint8_t SERVO_OPEN   = 90;  // barrier up angle

const uint8_t TOTAL_SLOTS = 6;
const unsigned long CROSS_TIMEOUT_MS = 2500; // max time for car to move between beams

// ---------------- Runtime variables -------------------------
uint8_t freeSlots = TOTAL_SLOTS;

enum DirState { IDLE, IR1_BLOCKED, IR2_BLOCKED } dirState = IDLE;
unsigned long stateStart = 0;

// -------------------------------------------------------------
void setup() {
  pinMode(PIN_IR1, INPUT_PULLUP);
  pinMode(PIN_IR2, INPUT_PULLUP);
  for (uint8_t i = 0; i < TOTAL_SLOTS; i++) {
    pinMode(SLOT_PINS[i], INPUT_PULLUP);
  }

  barrierServo.attach(PIN_SERVO);
  barrierServo.write(SERVO_CLOSED);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Parking System");
  delay(1500);

  freeSlots = countFreeSlots();
  updateLCD();
}

// -------------------------------------------------------------
void loop() {
  directionalLogic();

  // Refresh slot status & LCD every second
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 1000) {
    freeSlots = countFreeSlots();
    updateLCD();
    lastRefresh = millis();
  }
}

// ---------------- Directional lane logic --------------------
void directionalLogic() {
  bool ir1 = digitalRead(PIN_IR1) == LOW; // true when beam broken
  bool ir2 = digitalRead(PIN_IR2) == LOW;

  switch (dirState) {
    case IDLE:
      if (ir1) {
        dirState = IR1_BLOCKED;
        stateStart = millis();
      } else if (ir2) {
        dirState = IR2_BLOCKED;
        stateStart = millis();
      }
      break;

    case IR1_BLOCKED:
      if (ir2) { // sequence IR1 then IR2 => ENTRY
        processEntry();
        waitForBeamsClear();
        dirState = IDLE;
      } else if (millis() - stateStart > CROSS_TIMEOUT_MS) {
        dirState = IDLE; // timeout
      }
      break;

    case IR2_BLOCKED:
      if (ir1) { // sequence IR2 then IR1 => EXIT
        processExit();
        waitForBeamsClear();
        dirState = IDLE;
      } else if (millis() - stateStart > CROSS_TIMEOUT_MS) {
        dirState = IDLE;
      }
      break;
  }
}

void waitForBeamsClear() {
  // Wait until both sensors unblocked to avoid double counts
  while (digitalRead(PIN_IR1) == LOW || digitalRead(PIN_IR2) == LOW) {
    delay(10);
  }
}

// ---------------- Entry / Exit handlers --------------------
void processEntry() {
  if (freeSlots == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Parking FULL!");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    delay(2000);
    updateLCD();
    return; // keep barrier closed
  }

  openBarrier();
  freeSlots = constrain(freeSlots - 1, 0, TOTAL_SLOTS);
  updateLCD();
  closeBarrier();
}

void processExit() {
  openBarrier();
  freeSlots = constrain(freeSlots + 1, 0, TOTAL_SLOTS);
  updateLCD();
  closeBarrier();
}

// ---------------- Barrier helpers ---------------------------
void openBarrier() {
  barrierServo.write(SERVO_OPEN);
  delay(600);
}

void closeBarrier() {
  barrierServo.write(SERVO_CLOSED);
  delay(600);
}

// ---------------- Slot counting -----------------------------
uint8_t countFreeSlots() {
  uint8_t freeCnt = 0;
  for (uint8_t i = 0; i < TOTAL_SLOTS; i++) {
    if (digitalRead(SLOT_PINS[i]) == HIGH) { // HIGH = empty (active‑LOW sensor)
      freeCnt++;
    }
  }
  return freeCnt;
}

// ---------------- LCD display -------------------------------
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Slots Free: ");
  lcd.print(freeSlots);
  lcd.setCursor(0, 1);
  lcd.print("Welcome!");
}
