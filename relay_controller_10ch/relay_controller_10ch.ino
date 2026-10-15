/*
  10-channel active-LOW relay controller
  --------------------------------------
  Board:   Arduino Nano
  Relays:  10x 5V active-LOW relay modules on D2-D11
  Control: Serial (COM port) commands from 3rd-party software

  Protocol (confirmed):
    'X' or 'x'   -> STOP: force every relay OFF
    '1'-'9'      -> turn ON relay 1-9 respectively
    "10"         -> turn ON relay 10, sent as the literal two characters '1' then '0'
  No line terminator is used for these - each command is just raw bytes.

  DISAMBIGUATION FIX:
  Because relay 1 is a single '1' and relay 10 is '1' immediately followed by '0',
  the sketch can't act on '1' the instant it arrives - it has to wait briefly to see
  if a '0' follows. DIGIT_TIMEOUT_MS controls that wait. If nothing else arrives
  within that window, the pending '1' commits as relay 1. If any other byte arrives
  first (another digit, X, anything), the pending '1' commits as relay 1 immediately
  and the new byte is processed normally - so it never blocks the next command.

  Two modes, switchable at runtime over Serial (type in Serial Monitor + Enter):
    MODE=LEARN  -> shows what each byte WOULD do, without touching any relay.
    MODE=RUN    -> actually drives the relays.
  Boots in LEARN mode by default.
*/

#include <ctype.h>
#include <string.h>

// ---------- Configuration ----------
constexpr uint8_t NUM_RELAYS = 10;
constexpr uint8_t relayPins[NUM_RELAYS] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr uint8_t RELAY_ON  = LOW;   // active-low module: LOW energizes the relay
constexpr uint8_t RELAY_OFF = HIGH;

constexpr long BAUD_RATE = 9600;              // must match the 3rd-party software's COM port setting
constexpr uint8_t LINE_BUFFER_SIZE = 16;      // only used for typed MODE=... commands
constexpr unsigned long DIGIT_TIMEOUT_MS = 50; // how long to wait for '0' after a lone '1'

enum Mode : uint8_t { MODE_LEARN, MODE_RUN };
Mode currentMode = MODE_RUN;

char lineBuffer[LINE_BUFFER_SIZE];
uint8_t lineIndex = 0;

bool pendingOne = false;
unsigned long pendingOneTime = 0;

// ---------- Logging helpers ----------
void logBoot(const __FlashStringHelper* msg)  { Serial.print(F("[BOOT] "));  Serial.println(msg); }
void logInit(const __FlashStringHelper* msg)  { Serial.print(F("[INIT] "));  Serial.println(msg); }
void logWarn(const __FlashStringHelper* msg)  { Serial.print(F("[WARN] "));  Serial.println(msg); }
void logState(const __FlashStringHelper* msg) { Serial.print(F("[STATE] ")); Serial.println(msg); }

// ---------- Setup ----------
void setup() {
  Serial.begin(BAUD_RATE);

  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relayPins[i], RELAY_OFF); // pre-set HIGH before OUTPUT, avoids boot glitch
    pinMode(relayPins[i], OUTPUT);
  }

  logBoot(F("10-channel relay controller v3.0"));
  Serial.print(F("[BOOT] Baud rate: "));
  Serial.println(BAUD_RATE);
  logInit(F("All 10 relay pins set OUTPUT, all de-energized (HIGH)."));
  logState(F("Booted in LEARN mode (safe dry-run). Relays held OFF."));
  Serial.println(F("Send MODE=RUN to start driving relays for real."));
  Serial.println(F("Send MODE=LEARN to return to dry-run logging."));
}

// ---------- Main loop ----------
void loop() {
  // Resolve a timed-out pending '1' even if no new byte has arrived
  if (pendingOne && (millis() - pendingOneTime >= DIGIT_TIMEOUT_MS)) {
    pendingOne = false;
    commitChannel(1);
  }

  while (Serial.available() > 0) {
    handleProtocolByte(Serial.read());
  }
}

// ---------- Protocol state machine ----------
void handleProtocolByte(char c) {
  if (c == '0') {
    if (pendingOne) {
      pendingOne = false;
      commitChannel(10);
    } else {
      Serial.print(F("[WARN] Unexpected '0' with no preceding '1' (hex "));
      Serial.print((uint8_t)c, HEX);
      Serial.println(F(")."));
    }
    return;
  }

  if (c == '1') {
    if (pendingOne) {
      commitChannel(1); // an earlier pending '1' never got its '0' - resolve it first
    }
    pendingOne = true;
    pendingOneTime = millis();
    return;
  }

  // Any other byte first flushes a pending '1' (it wasn't followed by '0')
  if (pendingOne) {
    pendingOne = false;
    commitChannel(1);
  }

  if (c == 'X' || c == 'x') {
    commitStop();
  } else if (isDigit(c)) { // '2'-'9', unambiguous
    commitChannel(c - '0');
  } else {
    bufferForLineCommand(c); // not part of the relay protocol - maybe a typed MODE=... line
  }
}

// ---------- Commit actions (mode-aware: RUN acts for real, LEARN just reports) ----------
void commitChannel(uint8_t channel) {
  if (currentMode == MODE_RUN) {
    digitalWrite(relayPins[channel - 1], RELAY_ON);
    Serial.print(F("[INFO] Relay "));
    Serial.print(channel);
    Serial.println(F(" -> ON"));
  } else {
    Serial.print(F("[INFO] would turn ON relay "));
    Serial.println(channel);
  }
}

void commitStop() {
  if (currentMode == MODE_RUN) {
    allRelaysOff();
    Serial.println(F("[INFO] STOP received -> all relays OFF"));
  } else {
    Serial.println(F("[INFO] would STOP all relays"));
  }
}

// ---------- Typed MODE=... command handling (Serial Monitor testing convenience) ----------
void bufferForLineCommand(char c) {
  if (currentMode == MODE_LEARN && c != '\n' && c != '\r') {
    Serial.print(F("[INFO] byte: '"));
    Serial.print(c);
    Serial.print(F("'  hex: "));
    if ((uint8_t)c < 0x10) Serial.print('0');
    Serial.println((uint8_t)c, HEX);
  }

  if (c == '\n' || c == '\r') {
    if (lineIndex > 0) {
      lineBuffer[lineIndex] = '\0';
      handleLine(lineBuffer);
      lineIndex = 0;
    }
  } else if (lineIndex < LINE_BUFFER_SIZE - 1) {
    lineBuffer[lineIndex++] = c;
  } else {
    logWarn(F("Typed line too long, discarding."));
    lineIndex = 0;
  }
}

void handleLine(char* line) {
  if (strcmp(line, "MODE=RUN") == 0) {
    currentMode = MODE_RUN;
    logState(F("Switched to RUN mode. Relay commands are now LIVE."));
  } else if (strcmp(line, "MODE=LEARN") == 0) {
    currentMode = MODE_LEARN;
    allRelaysOff();
    logState(F("Switched to LEARN mode (safe dry-run). All relays forced OFF."));
  } else {
    Serial.print(F("[WARN] Unrecognized typed command: "));
    Serial.println(line);
  }
}

void allRelaysOff() {
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
  }
}