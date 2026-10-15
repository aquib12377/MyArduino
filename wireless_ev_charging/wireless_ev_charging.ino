/*
 * ============================================================
 *  Wireless EV Charging + V2G (Vehicle-to-Grid) Demo
 *  Board : Arduino Nano (ATmega328P)
 *  Ver   : 1.1  (added V2G buzzer + simulated output voltage)
 * ============================================================
 *  Pins
 *    A0  -> Relay 1 (Load 1, grid side)      ACTIVE LOW
 *    A1  -> Relay 2 (Load 2, grid side)      ACTIVE LOW
 *    A2  -> Relay 3 (Wireless charging TX)   ACTIVE LOW
 *    D2  -> Button / switch to GND           (INPUT_PULLUP)
 *    D3  -> IR module OUT                    ACTIVE LOW
 *    D4  -> Buzzer (+)                       ACTIVE HIGH
 *    A4  -> LCD I2C SDA
 *    A5  -> LCD I2C SCL
 *    A7  -> leave unconnected (random seed)
 *
 *  Logic
 *    Button ON                  -> A0 + A1 ON  (car feeds grid, V2G)
 *                                  + 3 beeps + voltage ramp 10V -> ~12V
 *    Button OFF + car detected  -> A2 ON       (wireless charging car)
 *    Button OFF + no car        -> all OFF     (idle)
 *    Relays never overlap: all OFF for DEAD_TIME_MS on every change.
 *
 *  Library: "LiquidCrystal I2C" by Frank de Brabander (Library Manager)
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------- Pins ----------------
constexpr uint8_t PIN_RELAY_LOAD1 = A0;
constexpr uint8_t PIN_RELAY_LOAD2 = A1;
constexpr uint8_t PIN_RELAY_WPT   = A2;   // wireless power transfer module
constexpr uint8_t PIN_BUTTON      = 2;
constexpr uint8_t PIN_IR          = 3;
constexpr uint8_t PIN_BUZZER      = 4;
constexpr uint8_t PIN_RAND_SEED   = A7;   // floating analog pin

// ---------------- Active levels ----------------
constexpr uint8_t RELAY_ON   = LOW;
constexpr uint8_t RELAY_OFF  = HIGH;
constexpr uint8_t IR_ACTIVE  = LOW;
constexpr uint8_t BTN_ACTIVE = LOW;       // switch/button connects pin to GND
constexpr uint8_t BUZZER_ON  = HIGH;      // active buzzer module/transistor
constexpr uint8_t BUZZER_OFF = LOW;

// ---------------- Behaviour options ----------------
// true  = latching ON/OFF switch, false = momentary push button (toggle)
constexpr bool BUTTON_IS_LATCHING = true;

// true = V2G only runs when a car is detected, false = as per spec
constexpr bool V2G_REQUIRES_CAR = false;

// Buzzer type: false = active buzzer (DC), true = passive buzzer (tone)
constexpr bool     BUZZER_IS_PASSIVE = false;
constexpr uint16_t BUZZER_FREQ_HZ    = 2500;  // passive buzzer only
constexpr uint8_t  V2G_BEEP_COUNT    = 3;
constexpr uint32_t BEEP_ON_MS        = 150;
constexpr uint32_t BEEP_OFF_MS       = 150;

// Simulated V2G output voltage (centivolts: 1000 = 10.00 V)
constexpr uint16_t V_MIN_CV          = 1000;  // 10.00 V start
constexpr uint16_t V_MAX_CV          = 1200;  // 12.00 V ceiling
constexpr uint16_t V_TARGET_LOW_CV   = 1150;  // settle target 11.50..12.00 V
constexpr uint32_t V_STEP_MS         = 300;
constexpr uint8_t  V_RAMP_STEP_MIN   = 3;     // +0.03 V
constexpr uint8_t  V_RAMP_STEP_MAX   = 12;    // +0.12 V
constexpr uint8_t  V_JITTER_CV       = 8;     // +/-0.08 V once settled

// Simulated battery % for display (no real sensor on this build)
constexpr bool    SIMULATE_SOC       = true;
constexpr bool    SOC_CUTOFF_ENABLED = false; // true = stop at FULL / RESERVE
constexpr uint8_t SOC_START          = 50;
constexpr uint8_t SOC_FULL           = 100;
constexpr uint8_t SOC_RESERVE        = 20;
constexpr uint32_t SOC_STEP_MS       = 2000;

// ---------------- Timing ----------------
constexpr uint32_t BTN_DEBOUNCE_MS   = 50;
constexpr uint32_t IR_ON_CONFIRM_MS  = 1000;
constexpr uint32_t IR_OFF_CONFIRM_MS = 1500;
constexpr uint32_t DEAD_TIME_MS      = 300;
constexpr uint32_t LCD_REFRESH_MS    = 250;
constexpr uint32_t LCD_PAGE_MS       = 2500;
constexpr uint32_t STATUS_LOG_MS     = 5000;

// ---------------- Types ----------------
enum class Mode : uint8_t {
  IDLE, CHARGING, CHARGE_FULL, V2G, V2G_RESERVE, V2G_NO_CAR
};

struct DebouncedInput {
  bool     stable;
  bool     lastRaw;
  uint32_t lastChange;
};

// ---------------- Globals ----------------
LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3F(0x3F, 16, 2);
LiquidCrystal_I2C* lcd = nullptr;

DebouncedInput btnIn = {false, false, 0};
DebouncedInput irIn  = {false, false, 0};

bool     v2gRequested   = false;
bool     carPresent     = false;
bool     irRawActive    = false;
Mode     mode           = Mode::IDLE;
bool     relaysPending  = false;
uint32_t modeChangedAt  = 0;
uint32_t sessionStartAt = 0;
uint8_t  soc            = SOC_START;
uint32_t lastSocStep    = 0;

// Buzzer state
uint8_t  beepsLeft      = 0;
bool     buzzerIsOn     = false;
uint32_t buzzerEdgeAt   = 0;

// Voltage state
uint16_t vOutCv         = V_MIN_CV;
uint16_t vTargetCv      = V_MAX_CV;
bool     vSettled       = false;
uint32_t lastVStep      = 0;

char lcdLine[2][17] = {"", ""};

// ---------------- Logging ----------------
#define LOG(tag, msg) do { Serial.print(F(tag " ")); Serial.println(msg); } while (0)

const __FlashStringHelper* modeName(Mode m) {
  switch (m) {
    case Mode::IDLE:        return F("IDLE");
    case Mode::CHARGING:    return F("CHARGING");
    case Mode::CHARGE_FULL: return F("CHARGE_FULL");
    case Mode::V2G:         return F("V2G");
    case Mode::V2G_RESERVE: return F("V2G_RESERVE");
    case Mode::V2G_NO_CAR:  return F("V2G_NO_CAR");
  }
  return F("?");
}

// ---------------- Relays ----------------
void writeRelays(bool load1, bool load2, bool wpt) {
  digitalWrite(PIN_RELAY_LOAD1, load1 ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_RELAY_LOAD2, load2 ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_RELAY_WPT,   wpt   ? RELAY_ON : RELAY_OFF);
}

void allRelaysOff() { writeRelays(false, false, false); }

void applyRelaysForMode(Mode m) {
  switch (m) {
    case Mode::V2G:      writeRelays(true,  true,  false); break;
    case Mode::CHARGING: writeRelays(false, false, true);  break;
    default:             allRelaysOff();                   break;
  }
}

void initRelays() {
  const uint8_t pins[] = {PIN_RELAY_LOAD1, PIN_RELAY_LOAD2, PIN_RELAY_WPT};
  for (uint8_t p : pins) {
    digitalWrite(p, RELAY_OFF);
    pinMode(p, OUTPUT);
    digitalWrite(p, RELAY_OFF);
  }
}

// ---------------- Buzzer (non-blocking) ----------------
void buzzerWrite(bool on) {
  buzzerIsOn = on;
  if (BUZZER_IS_PASSIVE) {
    if (on) tone(PIN_BUZZER, BUZZER_FREQ_HZ);
    else    noTone(PIN_BUZZER);
  } else {
    digitalWrite(PIN_BUZZER, on ? BUZZER_ON : BUZZER_OFF);
  }
}

void initBuzzer() {
  digitalWrite(PIN_BUZZER, BUZZER_OFF);
  pinMode(PIN_BUZZER, OUTPUT);
  buzzerWrite(false);
}

void startBeeps(uint8_t count, uint32_t now) {
  beepsLeft = count;
  buzzerWrite(true);
  buzzerEdgeAt = now;
}

void stopBeeps() {
  beepsLeft = 0;
  buzzerWrite(false);
}

void updateBuzzer(uint32_t now) {
  if (beepsLeft == 0) return;
  if (buzzerIsOn) {
    if (now - buzzerEdgeAt >= BEEP_ON_MS) {
      buzzerWrite(false);
      buzzerEdgeAt = now;
      beepsLeft--;                       // one beep finished
    }
  } else if (now - buzzerEdgeAt >= BEEP_OFF_MS) {
    buzzerWrite(true);
    buzzerEdgeAt = now;
  }
}

// ---------------- Simulated V2G voltage ----------------
void startVoltageRamp(uint32_t now) {
  vOutCv    = V_MIN_CV;
  vTargetCv = (uint16_t)random(V_TARGET_LOW_CV, V_MAX_CV + 1);
  vSettled  = false;
  lastVStep = now;
  Serial.print(F("[INFO] V2G voltage ramp: 10.00V -> "));
  Serial.print(vTargetCv / 100); Serial.print('.');
  if (vTargetCv % 100 < 10) Serial.print('0');
  Serial.print(vTargetCv % 100); Serial.println('V');
}

void updateVoltage(uint32_t now) {
  if (mode != Mode::V2G || relaysPending) return;
  if (now - lastVStep < V_STEP_MS) return;
  lastVStep = now;

  if (!vSettled) {
    // Gradual random rise
    vOutCv += (uint16_t)random(V_RAMP_STEP_MIN, V_RAMP_STEP_MAX + 1);
    if (vOutCv >= vTargetCv) {
      vOutCv = vTargetCv;
      vSettled = true;
      LOG("[INFO]", F("V2G voltage settled"));
    }
  } else {
    // Small random wander around target
    int16_t v = (int16_t)vTargetCv + (int16_t)random(-V_JITTER_CV, V_JITTER_CV + 1);
    if (v < (int16_t)V_MIN_CV) v = V_MIN_CV;
    if (v > (int16_t)V_MAX_CV) v = V_MAX_CV;
    vOutCv = (uint16_t)v;
  }
}

// ---------------- Inputs ----------------
bool debounce(DebouncedInput& d, bool raw, uint32_t now,
              uint32_t onMs, uint32_t offMs) {
  if (raw != d.lastRaw) {
    d.lastRaw = raw;
    d.lastChange = now;
  }
  if (raw != d.stable) {
    uint32_t need = raw ? onMs : offMs;
    if (now - d.lastChange >= need) {
      d.stable = raw;
      return true;
    }
  }
  return false;
}

void readInputs(uint32_t now) {
  bool btnRaw = (digitalRead(PIN_BUTTON) == BTN_ACTIVE);
  if (debounce(btnIn, btnRaw, now, BTN_DEBOUNCE_MS, BTN_DEBOUNCE_MS)) {
    if (BUTTON_IS_LATCHING)  v2gRequested = btnIn.stable;
    else if (btnIn.stable)   v2gRequested = !v2gRequested;
    Serial.print(F("[INFO] Button -> V2G "));
    Serial.println(v2gRequested ? F("ON") : F("OFF"));
  }

  irRawActive = (digitalRead(PIN_IR) == IR_ACTIVE);
  if (debounce(irIn, irRawActive, now, IR_ON_CONFIRM_MS, IR_OFF_CONFIRM_MS)) {
    carPresent = irIn.stable;
    if (carPresent) {
      soc = SOC_START;
      lastSocStep = now;
      LOG("[INFO]", F("Car DETECTED"));
    } else {
      LOG("[INFO]", F("Car LEFT"));
    }
  }
}

// ---------------- State machine ----------------
Mode computeMode() {
  if (v2gRequested) {
    if (V2G_REQUIRES_CAR && !carPresent) return Mode::V2G_NO_CAR;
    if (SIMULATE_SOC && SOC_CUTOFF_ENABLED && carPresent && soc <= SOC_RESERVE)
      return Mode::V2G_RESERVE;
    return Mode::V2G;
  }
  if (carPresent) {
    if (SIMULATE_SOC && SOC_CUTOFF_ENABLED && soc >= SOC_FULL)
      return Mode::CHARGE_FULL;
    return Mode::CHARGING;
  }
  return Mode::IDLE;
}

void updateMode(uint32_t now) {
  Mode next = computeMode();
  if (next != mode) {
    Serial.print(F("[STATE] "));
    Serial.print(modeName(mode));
    Serial.print(F(" -> "));
    Serial.println(modeName(next));

    if (mode == Mode::V2G) stopBeeps();  // leaving V2G: silence buzzer

    allRelaysOff();                      // break-before-make
    mode = next;
    relaysPending = true;
    modeChangedAt = now;
    sessionStartAt = now;
    lastSocStep = now;
  }

  if (relaysPending && (now - modeChangedAt >= DEAD_TIME_MS)) {
    applyRelaysForMode(mode);
    relaysPending = false;

    if (mode == Mode::V2G) {             // reverse charging actually started
      LOG("[INFO]", F("Reverse charging STARTED -> 3 beeps"));
      startBeeps(V2G_BEEP_COUNT, now);
      startVoltageRamp(now);
    }
  }
}

void updateSoc(uint32_t now) {
  if (!SIMULATE_SOC || !carPresent || relaysPending) return;
  if (now - lastSocStep < SOC_STEP_MS) return;
  lastSocStep = now;
  if (mode == Mode::CHARGING && soc < 100) soc++;
  else if (mode == Mode::V2G && soc > 0)   soc--;
}

// ---------------- LCD ----------------
bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void initLcd() {
  Wire.begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);
#endif
  if (i2cPresent(0x27))      lcd = &lcd27;
  else if (i2cPresent(0x3F)) lcd = &lcd3F;

  if (!lcd) {
    LOG("[WARN]", F("LCD not found at 0x27/0x3F - running without display"));
    return;
  }
  lcd->begin();
  lcd->backlight();
  lcd->clear();
  lcd->setCursor(0, 0); lcd->print(F(" WIRELESS  EV  "));
  lcd->setCursor(0, 1); lcd->print(F(" CHARGING + V2G "));
  LOG("[INIT]", lcd == &lcd27 ? F("LCD OK at 0x27") : F("LCD OK at 0x3F"));
  delay(1500);                           // splash, boot only
  lcd->clear();
}

void fmtTime(char* out, size_t n, uint32_t secs) {
  if (secs < 6000UL) {
    snprintf(out, n, "%02lu:%02lu",
             (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  } else {
    snprintf(out, n, "%luh%02lu",
             (unsigned long)(secs / 3600), (unsigned long)((secs / 60) % 60));
  }
}

void setLine(uint8_t row, const char* text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  if (strcmp(buf, lcdLine[row]) != 0) {
    strcpy(lcdLine[row], buf);
    if (lcd) { lcd->setCursor(0, row); lcd->print(buf); }
  }
}

void renderLcd(uint32_t now) {
  static uint32_t lastDraw = 0;
  if (now - lastDraw < LCD_REFRESH_MS) return;
  lastDraw = now;

  char l1[17], l2[17], t[8];
  fmtTime(t, sizeof(t), (now - sessionStartAt) / 1000UL);
  bool page2   = ((now / LCD_PAGE_MS) % 2) == 1;
  bool showSoc = SIMULATE_SOC && carPresent;

  switch (mode) {
    case Mode::IDLE:
      strcpy(l1, "WIRELESS EV CHG");
      strcpy(l2, irRawActive ? "Car detected..." : "Waiting for car");
      break;

    case Mode::CHARGING:
      strcpy(l1, "EV CHARGING >>>");
      if (page2)        strcpy(l2, "Wireless coil ON");
      else if (showSoc) snprintf(l2, sizeof(l2), "SoC:%3u%%  %s", soc, t);
      else              snprintf(l2, sizeof(l2), "Coil ON   %s", t);
      break;

    case Mode::CHARGE_FULL:
      strcpy(l1, "BATTERY FULL");
      strcpy(l2, "Remove car  100%");
      break;

    case Mode::V2G:
      strcpy(l1, "V2G: CAR > GRID");
      if (!page2) {
        // Voltage page (always first so the ramp is visible)
        snprintf(l2, sizeof(l2), "Out:%2u.%02uV %s",
                 vOutCv / 100, vOutCv % 100, t);
      } else if (showSoc) {
        snprintf(l2, sizeof(l2), "SoC:%3u%% L1+L2ON", soc);
      } else {
        strcpy(l2, "Load1+Load2 ON");
      }
      break;

    case Mode::V2G_RESERVE:
      strcpy(l1, "V2G STOPPED");
      snprintf(l2, sizeof(l2), "Reserve %u%% hit", SOC_RESERVE);
      break;

    case Mode::V2G_NO_CAR:
      strcpy(l1, "V2G MODE");
      strcpy(l2, "No car detected");
      break;
  }

  if (relaysPending) strcpy(l2, "Switching...");

  setLine(0, l1);
  setLine(1, l2);
}

// ---------------- Periodic status ----------------
void logStatus(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < STATUS_LOG_MS) return;
  last = now;
  Serial.print(F("[INFO] Mode=")); Serial.print(modeName(mode));
  Serial.print(F(" Btn="));        Serial.print(v2gRequested ? F("ON") : F("OFF"));
  Serial.print(F(" Car="));        Serial.print(carPresent ? F("YES") : F("NO"));
  Serial.print(F(" L1/L2/WPT="));
  Serial.print(digitalRead(PIN_RELAY_LOAD1) == RELAY_ON);
  Serial.print(digitalRead(PIN_RELAY_LOAD2) == RELAY_ON);
  Serial.print(digitalRead(PIN_RELAY_WPT)   == RELAY_ON);
  if (mode == Mode::V2G) {
    Serial.print(F(" Vout="));
    Serial.print(vOutCv / 100); Serial.print('.');
    if (vOutCv % 100 < 10) Serial.print('0');
    Serial.print(vOutCv % 100); Serial.print('V');
  }
  if (SIMULATE_SOC && carPresent) {
    Serial.print(F(" SoC=")); Serial.print(soc); Serial.print('%');
  }
  Serial.println();
}

// ---------------- Setup / Loop ----------------
void setup() {
  initRelays();                          // relays safe first
  initBuzzer();                          // buzzer silent at boot

  Serial.begin(115200);
  LOG("[BOOT]", F("Wireless EV Charging + V2G v1.1 | Nano | 115200"));
  LOG("[INIT]", F("Relays OK, all OFF"));
  LOG("[INIT]", BUZZER_IS_PASSIVE ? F("Buzzer: passive on D4")
                                  : F("Buzzer: active on D4"));

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_IR, INPUT_PULLUP);
  LOG("[INIT]", BUTTON_IS_LATCHING ? F("Button: latching switch on D2")
                                   : F("Button: momentary toggle on D2"));
  LOG("[INIT]", F("IR sensor on D3 (active LOW)"));

  randomSeed(analogRead(PIN_RAND_SEED) ^ micros());

  initLcd();

  uint32_t now = millis();
  bool btnNow = (digitalRead(PIN_BUTTON) == BTN_ACTIVE);
  btnIn = {btnNow, btnNow, now};
  irIn  = {false, digitalRead(PIN_IR) == IR_ACTIVE, now};
  v2gRequested = BUTTON_IS_LATCHING ? btnIn.stable : false;

  if (v2gRequested) LOG("[WARN]", F("Button already ON at boot -> V2G will start"));
  sessionStartAt = now;
  LOG("[STATE]", F("IDLE"));
}

void loop() {
  uint32_t now = millis();
  readInputs(now);
  updateMode(now);
  updateBuzzer(now);
  updateVoltage(now);
  updateSoc(now);
  renderLcd(now);
  logStatus(now);
}