// ============================================================
//  HC-12 Transmitter — 3x4 Keypad Password + LED/Buzzer Control
//  HC-12: RX->D2, TX->D3
//  Keypad Rows: D4, D5, D6, D7  |  Cols: D8, D9, D10
//
//  Keypad Layout:
//  [ 1 ][ 2 ][ 3 ]
//  [ 4 ][ 5 ][ 6 ]
//  [ 7 ][ 8 ][ 9 ]
//  [ * ][ 0 ][ # ]
//
//  LOCKED   : 0-9 = enter digit | * = clear | # = confirm
//  UNLOCKED : 1   = LED ON      | 2 = Buzzer ON | * = lock
// ============================================================

#include <SoftwareSerial.h>
#include <Keypad.h>

// ── HC-12 ─────────────────────────────────────────────────────
SoftwareSerial HC12(2, 3); // (RX, TX)
const int SET_PIN = 5;

// ── 3x4 Keypad ────────────────────────────────────────────────
const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {10,9,8,7};  // R1–R4
byte colPins[COLS] = {6,5,4};    // C1–C3

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── Password ──────────────────────────────────────────────────
const String CORRECT_PASSWORD = "724878"; // ← change this freely, any digits 0-9
String enteredPassword = "";

// ── State ─────────────────────────────────────────────────────
enum State { LOCKED, UNLOCKED };
State state = LOCKED;

unsigned long unlockedAt = 0;
const unsigned long TIMEOUT = 15000; // 15s auto-lock

// ── Helpers ───────────────────────────────────────────────────
void sendCommand(String cmd) {
  String packet = "<" + cmd + ">";
  HC12.print(packet);
  Serial.print("[SENT] ");
  Serial.println(packet);
}

void printStatus() {
  Serial.println();
  if (state == LOCKED) {
    Serial.println("============================");
    Serial.println("  LOCKED — Enter Password");
    Serial.println("  [#] = Confirm");
    Serial.println("  [*] = Clear");
    Serial.println("============================");
  } else {
    Serial.println("============================");
    Serial.println("  UNLOCKED!");
    Serial.println("  [1] = LED ON");
    Serial.println("  [2] = Buzzer ON");
    Serial.println("  [*] = Lock");
    Serial.println("  Auto-lock in 15s");
    Serial.println("============================");
  }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  HC12.begin(9600);

  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, HIGH);

  Serial.println("=== HC-12 Keypad Transmitter Ready ===");
  printStatus();
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {

  // ── Auto-lock timeout ─────────────────────────────────────
  if (state == UNLOCKED && millis() - unlockedAt >= TIMEOUT) {
    Serial.println("[TIMEOUT] Auto-locking...");
    state = LOCKED;
    enteredPassword = "";
    printStatus();
  }

  // ── Keypad scan ───────────────────────────────────────────
  char key = keypad.getKey();
  if (!key) return;

  Serial.print("[KEY] ");
  Serial.println(key);

  // ── LOCKED ────────────────────────────────────────────────
  if (state == LOCKED) {

    if (key == '#') {
      // Confirm password
      if (enteredPassword == CORRECT_PASSWORD) {
        Serial.println("[ACCESS GRANTED]");
        state = UNLOCKED;
        unlockedAt = millis();
        enteredPassword = "";
        printStatus();
      } else {
        Serial.println("[ACCESS DENIED] Wrong password!");
        enteredPassword = "";
        Serial.println("Try again:");
      }

    } else if (key == '*') {
      // Clear input
      enteredPassword = "";
      Serial.println("[CLEARED]");

    } else {
      // Digits 0–9
      enteredPassword += key;
      Serial.print("Input: ");
      for (byte i = 0; i < enteredPassword.length(); i++) Serial.print('*');
      Serial.println();

      // Safety limit
      if (enteredPassword.length() > 10) {
        enteredPassword = "";
        Serial.println("[TOO LONG] Cleared. Try again:");
      }
    }
  }

  // ── UNLOCKED ──────────────────────────────────────────────
  else {
    unlockedAt = millis(); // reset timeout on any keypress

    if (key == '5') {
      sendCommand("LED_ON");

    } else if (key == '6') {
      sendCommand("BUZ_ON");

    } else if (key == '*') {
      Serial.println("[LOCKED] Manual lock.");
      state = LOCKED;
      enteredPassword = "";
      printStatus();
    } else {
      Serial.println("[INFO] 1=LED  2=Buzzer  *=Lock");
    }
  }

  // ── Relay any incoming HC-12 data ─────────────────────────
  if (HC12.available()) {
    Serial.print("[RX] ");
    while (HC12.available()) Serial.write(HC12.read());
    Serial.println();
  }
}