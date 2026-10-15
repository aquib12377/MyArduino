/*
 * ============================================================
 *  NanoBot Bluetooth Controller
 * ============================================================
 *  Board  : Arduino Nano (ATmega328P)
 *  Motor  : L298N — 4 DC motors (2 per channel, ENA/ENB jumpered HIGH)
 *  BT     : HC-05 via SoftwareSerial (D2 RX, D3 TX)
 *  Pump   : Single-channel relay, ACTIVE LOW, on D8
 *  Protocol: Single-char commands over Bluetooth
 *
 *  Commands:
 *    F = Forward    B = Backward
 *    L = Turn Left  R = Turn Right
 *    S = Stop       P = Toggle Pump
 *
 *  Wiring (L298N):
 *    ENA / ENB : Keep factory jumpers ON (always enabled)
 *    IN1 (D4) + IN2 (D5) → Channel A → Left  motor pair
 *    IN3 (D6) + IN4 (D7) → Channel B → Right motor pair
 *
 *  Wiring (HC-05):
 *    HC-05 TX  → Nano D2 (SoftSerial RX)
 *    HC-05 RX  → Nano D3 (SoftSerial TX) via voltage divider!
 *
 *  Wiring (Relay):
 *    IN  → D8   (ACTIVE LOW: LOW = ON, HIGH = OFF)
 *    VCC → 5V
 *    GND → GND
 *
 *  Safety:
 *    Pump auto-off after PUMP_TIMEOUT_MS (default 30s)
 * ============================================================
 */

#include <SoftwareSerial.h>

// ── App Info ─────────────────────────────────────────────────
static const char APP_NAME[]    PROGMEM = "NanoBot BT";
static const char APP_VERSION[] PROGMEM = "1.0.0";

// ── Pin Definitions ──────────────────────────────────────────
// HC-05 Bluetooth (SoftwareSerial)
constexpr uint8_t PIN_BT_RX = 2;   // Nano D2 ← HC-05 TX
constexpr uint8_t PIN_BT_TX = 3;   // Nano D3 → HC-05 RX (use divider!)

// L298N Motor Driver — Channel A (Left motors)
constexpr uint8_t PIN_IN1 = 4;
constexpr uint8_t PIN_IN2 = 5;

// L298N Motor Driver — Channel B (Right motors)
constexpr uint8_t PIN_IN3 = 6;
constexpr uint8_t PIN_IN4 = 7;

// Relay — Active LOW
constexpr uint8_t PIN_RELAY = 8;

// ── Configuration ────────────────────────────────────────────
constexpr unsigned long BT_BAUD          = 9600;
constexpr unsigned long SERIAL_BAUD      = 9600;
constexpr unsigned long PUMP_TIMEOUT_MS  = 30000UL;  // 30 seconds auto-off
constexpr unsigned long STATUS_INTERVAL  = 2000UL;   // periodic status print

// ── Relay Helpers (ACTIVE LOW logic) ─────────────────────────
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// ── Motor State Enum ─────────────────────────────────────────
enum class BotState : uint8_t {
  STOPPED,
  FORWARD,
  BACKWARD,
  TURN_LEFT,
  TURN_RIGHT
};

// ── Globals ──────────────────────────────────────────────────
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);

BotState   currentState   = BotState::STOPPED;
bool       pumpActive     = false;
unsigned long pumpStartMs = 0;
unsigned long lastStatusMs = 0;

// ── Logging Helper (saves RAM with F()) ──────────────────────
void logMsg(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  Serial.print(tag);
  Serial.print(F(" "));
  Serial.println(msg);
}

// ── Motor Control Functions ──────────────────────────────────

// Left motor pair (Channel A): IN1/IN2
// Right motor pair (Channel B): IN3/IN4

void motorsForward() {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
}

void motorsBackward() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
}

void motorsTurnLeft() {
  // Left pair backward, Right pair forward
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
}

void motorsTurnRight() {
  // Left pair forward, Right pair backward
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
}

void motorsStop() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

// ── Pump Control ─────────────────────────────────────────────
void pumpOn() {
  digitalWrite(PIN_RELAY, RELAY_ON);
  pumpActive  = true;
  pumpStartMs = millis();
  logMsg(F("[STATE]"), F("Pump ON (auto-off in 30s)"));
}

void pumpOff() {
  digitalWrite(PIN_RELAY, RELAY_OFF);
  pumpActive = false;
  logMsg(F("[STATE]"), F("Pump OFF"));
}

void pumpToggle() {
  if (pumpActive) {
    pumpOff();
  } else {
    pumpOn();
  }
}

// ── State Transition ─────────────────────────────────────────
void setState(BotState newState) {
  if (newState == currentState) return;  // no redundant transitions

  currentState = newState;

  switch (newState) {
    case BotState::FORWARD:
      motorsForward();
      logMsg(F("[STATE]"), F("FORWARD"));
      break;

    case BotState::BACKWARD:
      motorsBackward();
      logMsg(F("[STATE]"), F("BACKWARD"));
      break;

    case BotState::TURN_LEFT:
      motorsTurnLeft();
      logMsg(F("[STATE]"), F("TURN LEFT"));
      break;

    case BotState::TURN_RIGHT:
      motorsTurnRight();
      logMsg(F("[STATE]"), F("TURN RIGHT"));
      break;

    case BotState::STOPPED:
    default:
      motorsStop();
      logMsg(F("[STATE]"), F("STOPPED"));
      break;
  }
}

// ── Command Processing ───────────────────────────────────────
void processCommand(char cmd) {
  // Accept both upper and lower case
  switch (cmd) {
    case 'F': case 'f':
      setState(BotState::FORWARD);
      btSerial.println(F("OK:FWD"));
      break;

    case 'B': case 'b':
      setState(BotState::BACKWARD);
      btSerial.println(F("OK:BWD"));
      break;

    case 'L': case 'l':
      setState(BotState::TURN_LEFT);
      btSerial.println(F("OK:LFT"));
      break;

    case 'R': case 'r':
      setState(BotState::TURN_RIGHT);
      btSerial.println(F("OK:RGT"));
      break;

    case 'S': case 's':
      setState(BotState::STOPPED);
      btSerial.println(F("OK:STP"));
      break;

    case 'P': case 'p':
      pumpToggle();
      btSerial.println(pumpActive ? F("OK:P_ON") : F("OK:P_OFF"));
      break;

    case '\n': case '\r':
      break;  // ignore line endings silently

    default:
      Serial.print(F("[WARN] Unknown cmd: 0x"));
      Serial.println((uint8_t)cmd, HEX);
      btSerial.println(F("ERR:CMD"));
      break;
  }
}

// ── Pump Safety Watchdog ─────────────────────────────────────
void checkPumpTimeout() {
  if (pumpActive && (millis() - pumpStartMs >= PUMP_TIMEOUT_MS)) {
    pumpOff();
    logMsg(F("[WARN]"), F("Pump AUTO-OFF (timeout)"));
    btSerial.println(F("WARN:P_TIMEOUT"));
  }
}

// ── Periodic Status (throttled) ──────────────────────────────
void printStatus() {
  unsigned long now = millis();
  if (now - lastStatusMs < STATUS_INTERVAL) return;
  lastStatusMs = now;

  static const char* const stateNames[] = {
    "STOPPED", "FORWARD", "BACKWARD", "LEFT", "RIGHT"
  };

  Serial.print(F("[INFO] State="));
  Serial.print(stateNames[(uint8_t)currentState]);
  Serial.print(F(" | Pump="));
  Serial.println(pumpActive ? F("ON") : F("OFF"));
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  // --- Serial Monitor ---
  Serial.begin(SERIAL_BAUD);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.print(F("[BOOT] "));
  Serial.print((__FlashStringHelper*)APP_NAME);
  Serial.print(F(" v"));
  Serial.println((__FlashStringHelper*)APP_VERSION);
  Serial.print(F("[BOOT] Serial @ "));
  Serial.print(SERIAL_BAUD);
  Serial.println(F(" baud"));
  Serial.println(F("========================================"));

  // --- Motor Pins ---
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  motorsStop();
  logMsg(F("[INIT]"), F("Motor pins configured (IN1-IN4)"));
  logMsg(F("[INIT]"), F("ENA/ENB: jumpered HIGH (no PWM)"));

  // --- Relay Pin (ensure OFF before setting OUTPUT) ---
  digitalWrite(PIN_RELAY, RELAY_OFF);   // pre-set HIGH before OUTPUT
  pinMode(PIN_RELAY, OUTPUT);
  logMsg(F("[INIT]"), F("Relay pin D8 configured (ACTIVE LOW)"));

  // --- Bluetooth ---
  btSerial.begin(BT_BAUD);
  Serial.print(F("[INIT] HC-05 SoftSerial on D"));
  Serial.print(PIN_BT_RX);
  Serial.print(F("/D"));
  Serial.print(PIN_BT_TX);
  Serial.print(F(" @ "));
  Serial.print(BT_BAUD);
  Serial.println(F(" baud"));

  // --- Ready ---
  logMsg(F("[INIT]"), F("Commands: F B L R S P (case insensitive)"));
  logMsg(F("[BOOT]"), F("System ready. Waiting for BT commands..."));
  Serial.println(F("========================================"));
}

// ── Main Loop ────────────────────────────────────────────────
void loop() {
  // 1. Read Bluetooth commands
  if (btSerial.available()) {
    char cmd = btSerial.read();
    processCommand(cmd);
  }

  // 2. Also accept commands from Serial Monitor (for testing)
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd);
  }

  // 3. Pump safety auto-off
  checkPumpTimeout();

  // 4. Periodic status log
  printStatus();
}
