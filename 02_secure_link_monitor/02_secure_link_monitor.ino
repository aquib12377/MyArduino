/* ============================================================================
   02_secure_link_monitor.ino  —  Secure Link Monitor
   Board    : Arduino Nano (ATmega328P)
   Version  : 1.0.0

   FUNCTION
     BTN1 -> LCD "Secure Link Established" + blue running LED to end of strip
     BTN2 -> LCD "Interference detected"   + buzzer starts beeping
     BTN3 -> buzzer stops beeping

   LIBRARIES (Arduino Library Manager)
     FastLED             >= 3.6.0
     LiquidCrystal_I2C   (johnrickman / "LiquidCrystal I2C" by Frank de Brabander)

   ---------------------------------------------------------------------------
   PIN MAP
     D2   BTN1  "Secure Link"     -> other leg to GND   (INPUT_PULLUP)
     D3   BTN2  "Interference"    -> other leg to GND   (INPUT_PULLUP)
     D4   BTN3  "Silence"         -> other leg to GND   (INPUT_PULLUP)
     D6   WS2812 DIN via 330-470 ohm resistor
     D8   Buzzer (+)              -> buzzer (-) to GND
     A4   LCD SDA
     A5   LCD SCL
     5V   LCD VCC, buzzer supply
     GND  common ground for EVERYTHING (Nano + LCD + buzzer + LED PSU)

   POWER — the single most common failure on this build
     60 LEDs, pure blue, brightness 200 ~= 0.94 A
     60 LEDs, white, brightness 255   ~= 3.60 A
     Nano USB supplies ~400-500 mA total. Feed the strip from a separate
     5V 3A supply, tie grounds together, and DO NOT power 60 LEDs through
     the Nano's 5V pin or the onboard regulator will shut down mid-animation.
   ========================================================================= */

#include <Wire.h>
#include <FastLED.h>
#include <LiquidCrystal_I2C.h>

/* =========================== USER CONFIGURATION ========================== */

#define FW_NAME              "Secure Link Monitor"
#define FW_VERSION           "1.0.0"
#define SERIAL_BAUD          115200

// ---- LED strip -------------------------------------------------------------
#define LED_PIN              6
#define NUM_LEDS             60      // 30 / 60 / 144 per metre — MATCH YOUR STRIP
#define LED_TYPE             WS2812B
#define COLOR_ORDER          GRB     // confirm with sketch 01, test 2
#define LED_BRIGHTNESS       200     // "bright blue" without going full 255
#define PSU_VOLTS            5
#define PSU_MILLIAMPS        2000    // set to ~70% of your PSU's real rating.
                                     // USB-only bench test: use 450.
#define RUN_STEP_MS          25      // ms per LED step. 60 x 25 = 1.5 s sweep.

// Animation style:
//   1 = FILL  -> LEDs stay lit behind the head (progress-bar style)
//   0 = COMET -> a moving dot with a fading tail, strip dark at the end
#define ANIM_FILL_MODE       1
#define COMET_FADE_AMOUNT    64      // used only when ANIM_FILL_MODE == 0

// After the sweep reaches the end:
//   1 = hold the strip solid blue until another event
//   0 = clear the strip
#define HOLD_AFTER_SWEEP     1

// Should BTN2 (interference) abort a running link animation?
//   1 = abort sweep and flash the strip red
//   0 = leave the LED animation completely independent of the buzzer
#define INTERFERENCE_ABORTS_LINK  1

// ---- Buttons ---------------------------------------------------------------
#define BTN1_PIN             2
#define BTN2_PIN             3
#define BTN3_PIN             4
#define DEBOUNCE_MS          40
#define BUTTONS_ACTIVE_LOW   1       // 1 = wired to GND with INPUT_PULLUP

// ---- Buzzer ----------------------------------------------------------------
#define BUZZER_PIN           8
// 1 = ACTIVE buzzer  (self-oscillating module, just needs DC — most common)
// 0 = PASSIVE buzzer (bare piezo, needs a tone signal)
#define BUZZER_ACTIVE_TYPE   1
#define BUZZER_TONE_HZ       2400    // passive buzzer only
#define BEEP_ON_MS           200
#define BEEP_OFF_MS          200
#define BUZZER_IDLE_LEVEL    LOW     // drive level when silent

// ---- LCD -------------------------------------------------------------------
#define LCD_COLS             16
#define LCD_ROWS             2
#define LCD_ADDR_PRIMARY     0x27    // PCF8574
#define LCD_ADDR_FALLBACK    0x3F    // PCF8574A
#define LCD_MSG_HOLD_MS      0       // 0 = message stays until next event.
                                     // e.g. 5000 = revert to idle after 5 s.

// ---- Diagnostics -----------------------------------------------------------
#define ENABLE_SERIAL_SIM    1       // type 1 / 2 / 3 in Serial Monitor to
                                     // simulate button presses (wiring-free test)
#define HEARTBEAT_MS         10000   // periodic status line; 0 disables

/* ========================================================================= */

CRGB leds[NUM_LEDS];                 // 60 x 3 = 180 bytes SRAM

LiquidCrystal_I2C* lcd = nullptr;    // constructed once in setup(), never freed
bool  lcdPresent   = false;
uint8_t lcdAddress = 0x00;

// ---------------------------------------------------------------- STATES ---
enum LinkState : uint8_t {
  LINK_IDLE,        // strip dark, nothing running
  LINK_RUNNING,     // sweep in progress
  LINK_HELD,        // sweep finished, strip held solid blue
  LINK_ALERT        // interference aborted the link (red)
};

enum ButtonId : uint8_t { BTN_LINK = 0, BTN_ALERT = 1, BTN_SILENCE = 2 };

struct Button {
  uint8_t  pin;
  bool     stable;        // debounced logical state: true = pressed
  bool     lastRaw;
  uint32_t lastChangeMs;
};

Button buttons[3] = {
  { BTN1_PIN, false, false, 0 },
  { BTN2_PIN, false, false, 0 },
  { BTN3_PIN, false, false, 0 }
};

LinkState linkState     = LINK_IDLE;
uint16_t  sweepIndex    = 0;
uint32_t  lastStepMs    = 0;

bool      buzzerActive  = false;     // logical: "should be beeping"
bool      buzzerOn      = false;     // physical: currently sounding
uint32_t  buzzerToggleMs = 0;

uint32_t  lastMsgMs     = 0;
bool      msgOnScreen   = false;
uint32_t  lastHeartbeat = 0;
uint32_t  buzzerStartMs = 0;

/* ============================== UTILITIES ================================ */

static void printStatus();           // forward declaration

static int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

static const __FlashStringHelper* linkStateName(LinkState s) {
  switch (s) {
    case LINK_IDLE:    return F("IDLE");
    case LINK_RUNNING: return F("RUNNING");
    case LINK_HELD:    return F("HELD");
    case LINK_ALERT:   return F("ALERT");
  }
  return F("UNKNOWN");
}

static void setLinkState(LinkState s) {
  if (s == linkState) return;
  Serial.print(F("[STATE] LINK "));
  Serial.print(linkStateName(linkState));
  Serial.print(F(" -> "));
  Serial.println(linkStateName(s));
  linkState = s;
}

/* ================================ LCD ==================================== */

// Probe the I2C bus for a backpack at the given address.
static bool i2cDevicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// Full-bus scan, used only when neither known address answers.
static uint8_t i2cScanFirstDevice() {
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    if (i2cDevicePresent(addr)) {
      Serial.print(F("[INFO] I2C device found at 0x"));
      Serial.println(addr, HEX);
      return addr;
    }
  }
  return 0x00;
}

static bool lcdInit() {
  Wire.begin();

  if (i2cDevicePresent(LCD_ADDR_PRIMARY))       lcdAddress = LCD_ADDR_PRIMARY;
  else if (i2cDevicePresent(LCD_ADDR_FALLBACK)) lcdAddress = LCD_ADDR_FALLBACK;
  else {
    Serial.println(F("[WARN] LCD not at 0x27 or 0x3F. Scanning bus..."));
    lcdAddress = i2cScanFirstDevice();
  }

  if (lcdAddress == 0x00) {
    Serial.println(F("[ERROR] No I2C device found."));
    Serial.println(F("[ERROR] Check SDA=A4, SCL=A5, VCC=5V, GND, and the"));
    Serial.println(F("[ERROR] backpack solder joints. Continuing without LCD."));
    return false;
  }

  lcd = new LiquidCrystal_I2C(lcdAddress, LCD_COLS, LCD_ROWS);
  if (lcd == nullptr) {
    Serial.println(F("[ERROR] LCD object allocation failed (out of SRAM)."));
    return false;
  }

  lcd->begin();
  lcd->backlight();
  lcd->clear();

  Serial.print(F("[INIT] LCD OK at 0x"));
  Serial.print(lcdAddress, HEX);
  Serial.print(F(" ("));
  Serial.print(LCD_COLS); Serial.print(F("x")); Serial.print(LCD_ROWS);
  Serial.println(F(")"));
  return true;
}

// Writes a flash-string to one row, padded with spaces so leftover characters
// from a previous longer message are erased. Avoids lcd.clear() flicker.
static void lcdRow(uint8_t row, const __FlashStringHelper* text) {
  if (!lcdPresent || row >= LCD_ROWS) return;
  lcd->setCursor(0, row);

  uint8_t written = 0;
  PGM_P p = reinterpret_cast<PGM_P>(text);
  while (written < LCD_COLS) {
    char c = pgm_read_byte(p++);
    if (c == '\0') break;
    lcd->print(c);
    written++;
  }
  while (written++ < LCD_COLS) lcd->print(' ');
}

// "Secure Link Established" is 23 chars — it will not fit one 16-col row,
// so every message is authored as two rows.
static void lcdMessage(const __FlashStringHelper* l1,
                       const __FlashStringHelper* l2) {
  lcdRow(0, l1);
  lcdRow(1, l2);
  lastMsgMs   = millis();
  msgOnScreen = true;
}

static void lcdShowIdle() {
  lcdMessage(F("System Ready"), F("Awaiting input"));
  msgOnScreen = false;   // idle screen is not a timed message
}

/* =============================== BUZZER ================================== */

static void buzzerHardwareOn() {
#if BUZZER_ACTIVE_TYPE
  digitalWrite(BUZZER_PIN, HIGH);
#else
  tone(BUZZER_PIN, BUZZER_TONE_HZ);
#endif
  buzzerOn = true;
}

static void buzzerHardwareOff() {
#if BUZZER_ACTIVE_TYPE
  digitalWrite(BUZZER_PIN, BUZZER_IDLE_LEVEL);
#else
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, BUZZER_IDLE_LEVEL);
#endif
  buzzerOn = false;
}

static void buzzerStart() {
  if (buzzerActive) {
    Serial.println(F("[INFO] Buzzer already active — restarting beep cycle."));
  } else {
    Serial.println(F("[STATE] BUZZER OFF -> BEEPING"));
  }
  buzzerActive   = true;
  buzzerStartMs  = millis();
  buzzerToggleMs = millis();
  buzzerHardwareOn();
}

static void buzzerStop() {
  if (!buzzerActive) {
    Serial.println(F("[INFO] BTN3 pressed but buzzer was already silent."));
    return;
  }
  uint32_t ranFor = millis() - buzzerStartMs;
  buzzerActive = false;
  buzzerHardwareOff();
  Serial.print(F("[STATE] BUZZER BEEPING -> OFF (sounded for "));
  Serial.print(ranFor);
  Serial.println(F(" ms)"));
}

// Non-blocking beep pattern. Must be called every loop().
static void buzzerService() {
  if (!buzzerActive) return;
  uint32_t now      = millis();
  uint32_t interval = buzzerOn ? BEEP_ON_MS : BEEP_OFF_MS;

  if (now - buzzerToggleMs >= interval) {
    buzzerToggleMs = now;
    if (buzzerOn) buzzerHardwareOff();
    else          buzzerHardwareOn();
  }
}

/* ============================== LED STRIP ================================ */

static void stripClear() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

static void stripSolid(const CRGB& c) {
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
}

static void linkSweepStart() {
  Serial.println(F("[INFO] Starting blue running-LED sweep."));
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  sweepIndex = 0;
  lastStepMs = millis();
  setLinkState(LINK_RUNNING);
}

// Non-blocking sweep. One LED advance per RUN_STEP_MS.
static void linkSweepService() {
  if (linkState != LINK_RUNNING) return;

  uint32_t now = millis();
  if (now - lastStepMs < RUN_STEP_MS) return;
  lastStepMs = now;

#if ANIM_FILL_MODE
  leds[sweepIndex] = CRGB::Blue;
#else
  fadeToBlackBy(leds, NUM_LEDS, COMET_FADE_AMOUNT);
  leds[sweepIndex] = CRGB::Blue;
#endif
  FastLED.show();

  if (sweepIndex % 10 == 0) {
    Serial.print(F("[DEBUG] sweep "));
    Serial.print(sweepIndex + 1);
    Serial.print(F("/"));
    Serial.println(NUM_LEDS);
  }

  sweepIndex++;

  if (sweepIndex >= NUM_LEDS) {
    Serial.println(F("[INFO] Sweep reached end of strip."));
#if HOLD_AFTER_SWEEP
    stripSolid(CRGB::Blue);
    setLinkState(LINK_HELD);
#else
    stripClear();
    setLinkState(LINK_IDLE);
#endif
  }
}

/* =============================== BUTTONS ================================= */

// Reads raw electrical state and returns logical "pressed".
static bool rawPressed(uint8_t pin) {
#if BUTTONS_ACTIVE_LOW
  return (digitalRead(pin) == LOW);
#else
  return (digitalRead(pin) == HIGH);
#endif
}

// Debounces one button. Returns true exactly once per press (rising edge).
static bool buttonPressedEdge(Button& b) {
  bool raw = rawPressed(b.pin);
  uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastRaw      = raw;
    b.lastChangeMs = now;
    return false;
  }

  if ((now - b.lastChangeMs) >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    return raw;              // fire on press only, not release
  }
  return false;
}

/* ============================ EVENT HANDLERS ============================= */

static void onSecureLink() {
  Serial.println(F("[EVENT] BTN1 — Secure Link"));
  lcdMessage(F("Secure Link"), F("Established"));
  linkSweepStart();          // restarts cleanly even if already running
}

static void onInterference() {
  Serial.println(F("[EVENT] BTN2 — Interference"));
  lcdMessage(F("Interference"), F("detected"));
  buzzerStart();

#if INTERFERENCE_ABORTS_LINK
  if (linkState == LINK_RUNNING || linkState == LINK_HELD) {
    Serial.println(F("[WARN] Link aborted by interference — strip set RED."));
  }
  stripSolid(CRGB::Red);
  setLinkState(LINK_ALERT);
#endif
}

static void onSilence() {
  Serial.println(F("[EVENT] BTN3 — Silence"));
  buzzerStop();
  if (lcdPresent) {
    lcdMessage(F("Alarm silenced"), F("Link idle"));
  }
#if INTERFERENCE_ABORTS_LINK
  if (linkState == LINK_ALERT) {
    stripClear();
    setLinkState(LINK_IDLE);
  }
#endif
}

// Priority order matters when two buttons are held at the same instant.
// Silence wins, because "make it stop" should never be blocked by another input.
static void serviceButtons() {
  bool p3 = buttonPressedEdge(buttons[BTN_SILENCE]);
  bool p2 = buttonPressedEdge(buttons[BTN_ALERT]);
  bool p1 = buttonPressedEdge(buttons[BTN_LINK]);

  if (p3)      onSilence();
  else if (p2) onInterference();
  else if (p1) onSecureLink();
}

#if ENABLE_SERIAL_SIM
static void serviceSerialSim() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case '1': Serial.println(F("[SIM] BTN1")); onSecureLink();   break;
      case '2': Serial.println(F("[SIM] BTN2")); onInterference(); break;
      case '3': Serial.println(F("[SIM] BTN3")); onSilence();      break;
      case '?': printStatus();                                     break;
      default : break;   // ignore CR/LF and stray characters
    }
  }
}
#endif

/* ============================== DIAGNOSTICS ============================== */

static void printStatus() {
  Serial.print(F("[INFO] link="));    Serial.print(linkStateName(linkState));
  Serial.print(F("  sweep="));        Serial.print(sweepIndex);
  Serial.print(F("/"));               Serial.print(NUM_LEDS);
  Serial.print(F("  buzzer="));       Serial.print(buzzerActive ? F("ON") : F("OFF"));
  Serial.print(F("  lcd="));          Serial.print(lcdPresent ? F("OK") : F("ABSENT"));
  Serial.print(F("  uptime="));       Serial.print(millis() / 1000);
  Serial.print(F("s  freeRAM="));     Serial.print(freeRam());
  Serial.println(F("B"));
}

static void banner() {
  Serial.println();
  Serial.println(F("========================================"));
  Serial.print  (F("[BOOT] ")); Serial.print(F(FW_NAME));
  Serial.print  (F(" v"));      Serial.println(F(FW_VERSION));
  Serial.println(F("[BOOT] Board       : Arduino Nano (ATmega328P)"));
  Serial.print  (F("[BOOT] Baud        : ")); Serial.println(SERIAL_BAUD);
  Serial.print  (F("[BOOT] LEDs        : ")); Serial.print(NUM_LEDS);
  Serial.print  (F(" on D")); Serial.println(LED_PIN);
  Serial.print  (F("[BOOT] Brightness  : ")); Serial.println(LED_BRIGHTNESS);
  Serial.print  (F("[BOOT] Power cap   : ")); Serial.print(PSU_MILLIAMPS);
  Serial.println(F(" mA"));
  Serial.print  (F("[BOOT] Sweep step  : ")); Serial.print(RUN_STEP_MS);
  Serial.print  (F(" ms  (total "));
  Serial.print  ((uint32_t)RUN_STEP_MS * NUM_LEDS);
  Serial.println(F(" ms)"));
  Serial.print  (F("[BOOT] Anim mode   : "));
#if ANIM_FILL_MODE
  Serial.println(F("FILL"));
#else
  Serial.println(F("COMET"));
#endif
  Serial.print  (F("[BOOT] Buzzer type : "));
#if BUZZER_ACTIVE_TYPE
  Serial.println(F("ACTIVE (DC drive)"));
#else
  Serial.println(F("PASSIVE (tone)"));
#endif
  Serial.print  (F("[BOOT] Buttons     : D"));
  Serial.print(BTN1_PIN); Serial.print(F(" D"));
  Serial.print(BTN2_PIN); Serial.print(F(" D"));
  Serial.print(BTN3_PIN);
  Serial.println(F("  (INPUT_PULLUP, active LOW)"));
#if ENABLE_SERIAL_SIM
  Serial.println(F("[BOOT] Serial sim  : send 1 / 2 / 3, or ? for status"));
#endif
  Serial.println(F("========================================"));
}

/* ================================ SETUP ================================== */

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  banner();

  // --- Buttons -------------------------------------------------------------
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].lastRaw      = rawPressed(buttons[i].pin);
    buttons[i].stable       = buttons[i].lastRaw;
    buttons[i].lastChangeMs = millis();

    if (buttons[i].stable) {
      Serial.print(F("[WARN] Button on D"));
      Serial.print(buttons[i].pin);
      Serial.println(F(" reads PRESSED at boot — stuck switch or wiring short?"));
    }
  }
  Serial.println(F("[INIT] Buttons configured (INPUT_PULLUP)."));

  // --- Buzzer --------------------------------------------------------------
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerHardwareOff();
  Serial.println(F("[INIT] Buzzer configured, silent."));

  // --- LCD -----------------------------------------------------------------
  lcdPresent = lcdInit();
  if (!lcdPresent) {
    Serial.println(F("[WARN] Running in DEGRADED mode: serial output only."));
  }

  // --- LED strip -----------------------------------------------------------
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
         .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, PSU_MILLIAMPS);
  stripClear();
  Serial.println(F("[INIT] FastLED ready, strip cleared."));

  // --- Boot self-test ------------------------------------------------------
  if (lcdPresent) {
    lcdMessage(F("Secure Link Mon"), F("v" FW_VERSION));
    delay(1200);
    lcdShowIdle();
  }

  Serial.print(F("[INFO] Free SRAM after init: "));
  Serial.print(freeRam());
  Serial.println(F(" bytes"));
  Serial.println(F("[BOOT] Ready.\n"));
  lastHeartbeat = millis();
}

/* ================================= LOOP ================================== */

void loop() {
  serviceButtons();

#if ENABLE_SERIAL_SIM
  serviceSerialSim();
#endif

  linkSweepService();
  buzzerService();

  // Optional timed revert of the LCD message back to the idle screen
#if LCD_MSG_HOLD_MS > 0
  if (msgOnScreen && (millis() - lastMsgMs >= LCD_MSG_HOLD_MS)) {
    msgOnScreen = false;
    lcdShowIdle();
    Serial.println(F("[DEBUG] LCD reverted to idle screen."));
  }
#endif

#if HEARTBEAT_MS > 0
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    printStatus();
  }
#endif
}
