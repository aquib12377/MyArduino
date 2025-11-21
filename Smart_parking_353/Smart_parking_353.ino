#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

/* ===================== Config ===================== */
#define LCD_ADDR        0x27      // change to 0x3F if needed
#define LCD_COLS        16
#define LCD_ROWS        2

// Slot IRs (ACTIVE LOW): HIGH = clear, LOW = blocked (occupied)
const byte SLOT_PINS[3] = {4, 5, 6};
const byte TOTAL_SLOTS  = 3;

// Gate IRs (ACTIVE LOW)
const byte IR1_PIN = 2;  // Outside / first beam
const byte IR2_PIN = 3;  // Inside  / second beam

// Servo
const byte SERVO_PIN = 9;
const int  GATE_CLOSED_DEG = 0;    // tune for your hardware
const int  GATE_OPEN_DEG   = 90;

// Timing
const unsigned long DEBOUNCE_MS   = 30;
const unsigned long DIR_WINDOW_MS = 1500;  // max time between beams
const unsigned long GATE_HOLD_MS  = 2500;  // hold open before auto-close
const unsigned long UI_REFRESH_MS = 300;

/* ===================== Globals ===================== */
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
Servo gate;

enum GateState : byte { GATE_CLOSED, GATE_OPEN };
GateState gateState = GATE_CLOSED;

unsigned long gateStateSince = 0;
unsigned long lastUiUpdate   = 0;

// Debounce tracking for gate sensors (ACTIVE LOW)
struct Debounced {
  byte pin;
  bool stable;            // debounced digitalRead (true=HIGH/clear, false=LOW/blocked)
  bool prevStable;
  unsigned long lastFlipMs;
} ir1{IR1_PIN, true, true, 0}, ir2{IR2_PIN, true, true, 0};

bool readRaw(byte pin) { return digitalRead(pin); }

void debounceUpdate(Debounced &d) {
  bool raw = readRaw(d.pin);
  if (raw != d.stable) {
    if (millis() - d.lastFlipMs >= DEBOUNCE_MS) {
      d.prevStable = d.stable;
      d.stable = raw;
      d.lastFlipMs = millis();
    }
  }
}

// === Simple & robust 2-beam FSM ===
enum BeamFSM : byte { WAITING, ARMED_IR1, ARMED_IR2 };
BeamFSM beamState = WAITING;
unsigned long beamStateSince = 0;

enum LastEvent : byte { EVT_NONE, EVT_ENTRY, EVT_EXIT, EVT_DENY };
LastEvent lastEvent = EVT_NONE;

/* ===================== Helpers ===================== */

// gate control
void gateOpen()  { gate.write(GATE_OPEN_DEG);  gateState = GATE_OPEN;  gateStateSince = millis(); }
void gateClose() { gate.write(GATE_CLOSED_DEG);gateState = GATE_CLOSED;gateStateSince = millis(); }

// read slots (ACTIVE LOW => LOW means occupied)
byte countOccupied() {
  byte occ = 0;
  for (byte i = 0; i < TOTAL_SLOTS; i++) {
    if (digitalRead(SLOT_PINS[i]) == LOW) occ++;
  }
  return occ;
}

void showUI(byte occ) {
  byte free = TOTAL_SLOTS - occ;

  // Line 1: Tot/Occ/Free (fits in 16 chars)
  lcd.setCursor(0, 0);
  char l1[17];
  snprintf(l1, sizeof(l1), "T:%u O:%u F:%u   ", TOTAL_SLOTS, occ, free);
  lcd.print(l1);

  // Line 2: Gate + last event
  lcd.setCursor(0, 1);
  const char* gs = (gateState == GATE_OPEN) ? "OPEN " : "CLOSED";
  const char* ev =
    (lastEvent == EVT_ENTRY) ? "Entry" :
    (lastEvent == EVT_EXIT)  ? "Exit " :
    (lastEvent == EVT_DENY)  ? "DENY " : "Ready";

  char l2[17];
  snprintf(l2, sizeof(l2), "Gate:%s %s", gs, ev);
  lcd.print(l2);
}

/* ============ Direction / Gate FSM ============

   ACTIVE LOW (blocked = LOW):
   - ENTRY: IR1 blocks first, then IR2 within DIR_WINDOW_MS
   - EXIT : IR2 blocks first, then IR1 within DIR_WINDOW_MS
*/
void processBeams(byte freeSlots) {
  // detect fresh falling edges (HIGH->LOW = beam blocked)
  bool ir1Falling = (ir1.prevStable == true  && ir1.stable == false);
  bool ir2Falling = (ir2.prevStable == true  && ir2.stable == false);

  unsigned long now = millis();

  switch (beamState) {
    case WAITING:
      if (ir1Falling) {           // first beam = IR1
        beamState = ARMED_IR1;
        beamStateSince = now;
      } else if (ir2Falling) {    // first beam = IR2
        beamState = ARMED_IR2;
        beamStateSince = now;
      }
      break;

    case ARMED_IR1:  // expecting IR2 within window => ENTRY
      if (ir2Falling && (now - beamStateSince) <= DIR_WINDOW_MS) {
        // ENTRY confirmed
        if (freeSlots > 0) {
          gateOpen();
          lastEvent = EVT_ENTRY;
        } else {
          lastEvent = EVT_DENY;   // lot full
        }
        beamState = WAITING;
      } else if ((now - beamStateSince) > DIR_WINDOW_MS) {
        beamState = WAITING;      // timeout
      }
      // if IR1 falls again, ignore; we only care about the counterpart within window
      break;

    case ARMED_IR2:  // expecting IR1 within window => EXIT
      if (ir1Falling && (now - beamStateSince) <= DIR_WINDOW_MS) {
        // EXIT confirmed
        gateOpen();               // allow car out regardless of free slots
        lastEvent = EVT_EXIT;
        beamState = WAITING;
      } else if ((now - beamStateSince) > DIR_WINDOW_MS) {
        beamState = WAITING;      // timeout
      }
      break;
  }

  // Auto-close gate after hold time and both beams clear
  if (gateState == GATE_OPEN) {
    bool beamsClear = (ir1.stable == true && ir2.stable == true); // both HIGH = clear
    if (beamsClear && (now - gateStateSince) >= GATE_HOLD_MS) {
      gateClose();
    }
  }
}

/* ===================== Setup ===================== */
void setup() {
  for (byte i = 0; i < TOTAL_SLOTS; i++) pinMode(SLOT_PINS[i], INPUT_PULLUP); // ACTIVE LOW
  pinMode(IR1_PIN, INPUT_PULLUP);
  pinMode(IR2_PIN, INPUT_PULLUP);

  lcd.begin();    // or lcd.init(); if your lib uses init()
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Smart Parking");
  lcd.setCursor(0, 1); lcd.print("Initializing...");
  
  gate.attach(SERVO_PIN);
  gateClose();

  delay(800);
  lcd.clear();
}

/* ===================== Loop ===================== */
void loop() {
  // Debounce gate beams
  debounceUpdate(ir1);
  debounceUpdate(ir2);

  // Slots status
  byte occ = countOccupied();
  byte freeSlots = TOTAL_SLOTS - occ;

  // Direction / gate logic
  processBeams(freeSlots);

  // UI update
  if (millis() - lastUiUpdate >= UI_REFRESH_MS) {
    lastUiUpdate = millis();
    showUI(occ);
  }
}
