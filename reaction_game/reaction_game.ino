/*
 * ============================================================================
 *  REACTION GAME  -  Arduino Pro Micro (ATmega32U4, 5V/16MHz)
 * ============================================================================
 *  9 buttons total, 9 LEDs total (each LED on an ACTIVE-LOW relay):
 *    - 8 GAME buttons, each paired with 1 game LED
 *    - 1 START/STOP button (toggle), with its own status LED
 *
 *  Flow (UPDATED keystroke scheme):
 *    START pressed  -> sends keystroke '0', START LED on, lights a random LED
 *    Correct button (regular, 7 of the 8) -> sends keystroke '2'
 *    Correct button (the ONE special button) -> sends keystroke '3'
 *    STOP pressed   -> sends keystroke '1', all LEDs off (incl. START LED)
 *    40s elapsed    -> sends keystroke '1', all LEDs off (auto-stop)
 *
 *  Which game button is "special" (sends '3' instead of '2') is controlled
 *  by SPECIAL_BTN_INDEX below (0-7, indexes into GAME_BTN_PINS / LED_RELAY_PINS).
 *
 *  PIN BUDGET: 9 relays + 9 buttons = 18 = every usable Pro Micro GPIO.
 *  There are NO spare pins left. The RNG is seeded from micros() at the
 *  first start-press instead of from a floating analog pin.
 *
 *  Board: select "Arduino Leonardo" or "SparkFun Pro Micro" (5V/16MHz).
 *  Library: Keyboard (built-in, AVR 32U4 only).
 * ============================================================================
 */

#include <Keyboard.h>

// ---------------------------------------------------------------------------
//  Configuration
// ---------------------------------------------------------------------------
#define DEBUG 1                 // 1 = print Serial diagnostics, 0 = silent

constexpr uint8_t  NUM_LEDS        = 8;          // game LEDs (excl. START LED)
constexpr uint32_t GAME_DURATION   = 40000UL;   // 40 seconds
constexpr uint16_t DEBOUNCE_MS     = 25;        // button debounce window

// Relay polarity (active-LOW relay module: LOW = energized = LED ON)
constexpr uint8_t  RELAY_ON        = LOW;
constexpr uint8_t  RELAY_OFF       = HIGH;

// HID keystrokes
constexpr char KEY_START           = '2';   // sent when START is pressed
constexpr char KEY_STOP            = '3';   // sent when STOP is pressed (or auto-stop)
constexpr char KEY_CORRECT         = '1';   // sent on a correct hit, regular button
constexpr char KEY_CORRECT_SPECIAL = '5';   // sent on a correct hit, the ONE special button

// Which of the 8 game buttons (0-7) is the "special" one that sends '3'
// instead of '2' on a correct hit. All other 7 buttons send '2'.
constexpr uint8_t SPECIAL_BTN_INDEX = 7;

// ---------------------------------------------------------------------------
//  Pin map  (all 18 usable Pro Micro pins are in use)
//  Game LED relay i is paired with game button i.
// ---------------------------------------------------------------------------
const uint8_t GAME_BTN_PINS[NUM_LEDS]   = {  1,  2,  3,  4,  5,  6,  7,  8 };
const uint8_t LED_RELAY_PINS[NUM_LEDS]  = {  9, 10, 16, 14, 15, A0, A1, A2 };
constexpr uint8_t START_BTN_PIN        = 0;     // start/stop toggle button
constexpr uint8_t START_LED_PIN        = A3;    // start/stop status LED relay

// ---------------------------------------------------------------------------
//  Debounced button helper (active-LOW, INPUT_PULLUP)
// ---------------------------------------------------------------------------
struct Button {
  uint8_t  pin;
  uint8_t  stableState;       // last debounced level (HIGH = released)
  uint8_t  lastReading;       // last raw read
  uint32_t lastChangeMs;      // time of last raw change
};

constexpr uint8_t NUM_BUTTONS = NUM_LEDS + 1;   // 8 game + 1 start/stop
Button buttons[NUM_BUTTONS];                    // index NUM_LEDS = start/stop

void initButton(Button &b, uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  b.pin          = pin;
  b.stableState  = HIGH;      // released (pull-up)
  b.lastReading  = HIGH;
  b.lastChangeMs = 0;
}

// Returns true exactly once on a debounced press (HIGH -> LOW transition).
bool buttonPressed(Button &b) {
  uint8_t reading = digitalRead(b.pin);

  if (reading != b.lastReading) {
    b.lastReading  = reading;
    b.lastChangeMs = millis();
  }

  if ((millis() - b.lastChangeMs) >= DEBOUNCE_MS && reading != b.stableState) {
    b.stableState = reading;
    if (b.stableState == LOW) {     // pressed edge
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
//  Game state
// ---------------------------------------------------------------------------
bool     gameRunning   = false;
bool     rngSeeded     = false;     // seed once, on first start-press
uint32_t gameStartMs   = 0;
int8_t   activeLed     = -1;        // -1 = no LED lit

// ---------------------------------------------------------------------------
//  Logging helper
// ---------------------------------------------------------------------------
#if DEBUG
  #define LOG(tag, msg)   do { Serial.print(F(tag " ")); Serial.println(F(msg)); } while (0)
  #define LOGV(tag, val)  do { Serial.print(F(tag " ")); Serial.println(val);   } while (0)
#else
  #define LOG(tag, msg)
  #define LOGV(tag, val)
#endif

// ---------------------------------------------------------------------------
//  LED / relay control
// ---------------------------------------------------------------------------
void setLed(uint8_t idx, bool on) {
  digitalWrite(LED_RELAY_PINS[idx], on ? RELAY_ON : RELAY_OFF);
}

void setStartLed(bool on) {
  digitalWrite(START_LED_PIN, on ? RELAY_ON : RELAY_OFF);
}

void allLedsOff() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) setLed(i, false);
  activeLed = -1;
}

// Pick a random LED that is NOT the currently active one, light it.
void lightNextRandomLed() {
  if (activeLed >= 0) setLed(activeLed, false);

  int8_t next;
  do {
    next = (int8_t)random(NUM_LEDS);
  } while (NUM_LEDS > 1 && next == activeLed);

  activeLed = next;
  setLed(activeLed, true);
  LOGV("[STATE] Active LED ->", activeLed);
}

// ---------------------------------------------------------------------------
//  Game control
// ---------------------------------------------------------------------------
void startGame() {
  // Seed the RNG from the timing of the first human start-press (good entropy)
  if (!rngSeeded) {
    randomSeed(micros());
    rngSeeded = true;
  }

  gameRunning = true;
  gameStartMs = millis();
  setStartLed(true);                  // status LED on while running
  Keyboard.write(KEY_START);          // send '0'
  LOG("[STATE]", "Game STARTED (sent '0')");
  lightNextRandomLed();
}

void stopGame() {
  gameRunning = false;
  allLedsOff();
  setStartLed(false);                 // status LED off when idle
  Keyboard.write(KEY_STOP);           // send '1'
  LOG("[STATE]", "Game STOPPED (sent '1')");
}

// ---------------------------------------------------------------------------
//  Setup
// ---------------------------------------------------------------------------
void setup() {
  // Force all relays OFF *before* switching pins to OUTPUT so LEDs never
  // flash on at power-up (active-LOW: HIGH = off).
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_RELAY_PINS[i], RELAY_OFF);
    pinMode(LED_RELAY_PINS[i], OUTPUT);
    digitalWrite(LED_RELAY_PINS[i], RELAY_OFF);
  }
  // Same boot-flash guard for the start/stop status LED.
  digitalWrite(START_LED_PIN, RELAY_OFF);
  pinMode(START_LED_PIN, OUTPUT);
  digitalWrite(START_LED_PIN, RELAY_OFF);

  // Buttons: index 0..NUM_LEDS-1 = game buttons, last = start/stop.
  for (uint8_t i = 0; i < NUM_LEDS; i++) initButton(buttons[i], GAME_BTN_PINS[i]);
  initButton(buttons[NUM_LEDS], START_BTN_PIN);

  Keyboard.begin();

#if DEBUG
  Serial.begin(115200);
  // Note: do NOT block on `while(!Serial)` — the game must run without a PC
  // serial monitor open. The keystrokes still reach the host either way.
  LOG("[BOOT]", "Reaction Game v1.2 | Pro Micro 32U4");
  LOGV("[BOOT] Game LEDs:", NUM_LEDS);
  LOGV("[BOOT] Duration ms:", GAME_DURATION);
  LOGV("[BOOT] Special button index:", SPECIAL_BTN_INDEX);
  LOG("[INIT]", "All relays OFF (incl. START LED), buttons armed, ready.");
#endif
}

// ---------------------------------------------------------------------------
//  Main loop  (non-blocking)
// ---------------------------------------------------------------------------
void loop() {
  // --- Start/Stop toggle button ---
  if (buttonPressed(buttons[NUM_LEDS])) {
    if (gameRunning) stopGame();
    else             startGame();
  }

  if (gameRunning) {
    // --- Auto-stop after 40s ---
    if (millis() - gameStartMs >= GAME_DURATION) {
      LOG("[STATE]", "Time up");
      stopGame();
      return;
    }

    // --- Check the matching game button for the active LED ---
    if (activeLed >= 0 && buttonPressed(buttons[activeLed])) {
      char key = ((uint8_t)activeLed == SPECIAL_BTN_INDEX) ? KEY_CORRECT_SPECIAL : KEY_CORRECT;
      Keyboard.write(key);            // '3' for the special button, '2' otherwise
      LOGV("[INFO] Correct hit, LED:", activeLed);
      lightNextRandomLed();
    }

    // --- Drain (debounce) the other game buttons so wrong/queued presses
    //     don't fire stale edges on the next LED. They are intentionally
    //     ignored (no keystroke). ---
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      if (i != (uint8_t)activeLed) buttonPressed(buttons[i]);
    }
  }
}
