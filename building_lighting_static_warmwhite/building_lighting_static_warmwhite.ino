/*
 * Miniature Building Model — WS2812 Lighting Controller
 * STATIC WARM-WHITE build (no animations)
 * Board   : Arduino Mega 2560
 * Library : Adafruit NeoPixel
 *
 * SCALE: 41 strips x 120 LEDs = 4,920 pixels, driven from ONE shared 120-LED
 * buffer that is re-pointed to each strip's pin with setPin() before show()
 * (RAM stays ~1 KB; the Mega's 8 KB can't hold a buffer per strip).
 *
 * WHY THIS BUILD: every "on" state is a single constant yellowish warm white.
 * Because the colour never changes, each strip is sent ONCE when it turns on and
 * then left alone — WS2812s latch and hold their colour, so the data bus goes
 * SILENT and the picture is as steady as the wiring allows. This removes all the
 * animation/refresh-related jitter.
 *
 * NOTE ON THE REMAINING (BRIGHTNESS-DEPENDENT) FLICKER: that is a POWER/GROUND
 * issue, not code. The data stream is identical at any brightness; only current
 * changes. Going static helps a lot by not constantly re-transmitting, but if
 * lit strips still flicker at high brightness, fix the supply:
 *   - Common ground: every PSU's GND *and* the Mega's GND on one ground bus.
 *   - Inject +5V at BOTH ends of every 120-LED strip (and mid-run if possible).
 *   - Ground wire must be as heavy as the +5V wire (it carries the same current).
 *   - Measure 5V at the FAR end of a strip under full load; it should stay >4.6V.
 *
 * ----- PIN MAP (your three 16-pin connectors) -----
 *   Connector 1 (16 WS2812): 13 12 11 10 9 8 7 6 5 4 3 2 14 15 16 17   [18,19 open]
 *   Connector 2 (16 WS2812): 22 23 25 27 29 31 33 35 37 39 41 43 45 47 49 51 [53,52 open]
 *   Connector 3 (9 WS2812 + 7 control): A15..A7 = strips, A6..A0 = control
 *        A6 = SURROUND relay  A5 = EDAC relay
 *        A4 = ALL switch      A3 = COMMERCIALS  A2 = PODIUM
 *        A1 = EDAC switch     A0 = "ALL LIGHTS" switch (was ANIMATION)
 *
 * Behaviour (each button toggles ON 1st press / OFF 2nd press):
 *   ALL        : all strips warm white + both relays ON  (2nd press = all OFF + relays OFF)
 *   COMMERCIALS: 7 "end" strips + the shops pin warm white, others OFF, relays untouched
 *   PODIUM     : only the 8 podium strips warm white,      others OFF, relays untouched
 *   ALL LIGHTS : all strips warm white, RELAYS UNTOUCHED   (repurposed animation button)
 *   EDAC       : toggles the EDAC relay ONLY — strips left exactly as they are
 *
 * POWER WARNING: 4,920 WS2812s can draw enormous current. Keep BRIGHTNESS modest,
 * use properly sized 5V supplies, inject power along the strips. Never power this
 * many LEDs through the Arduino.
 */

#include <Adafruit_NeoPixel.h>

/* ============================ USER CONFIG ============================ */
#define IDLE_TIMEOUT_MS 6000UL   // no button for this long -> all lights ON (attract)
uint32_t lastActivity = 0;        // millis() of last button press (idle timer)
#define LEDS_PER_STRIP   120
#define BRIGHTNESS       40                 // global 0-255 (keep modest for power)
#define NEO_FORMAT       (NEO_GRB + NEO_KHZ800)

// ----- The single constant colour -----
// Yellowish warm white. Tune to taste (0xRRGGBB):
//   0xFFB04B  warm amber (original)     0xFFC85A  yellowish warm white (default)
//   0xFFD27F  soft warm white           0xFFD040  stronger yellow
#define WARM_WHITE       0xFFCF3Dul

// Self-heal refresh: 0 = set once and hold (most stable, recommended while you
// still have power sag). >0 = re-send every N ms so any rare glitch corrects
// itself, at the cost of a little bus traffic. Try 0 first.
#define REFRESH_MS       0UL

// 41 WS2812 data pins, bottom-floor -> top order.
const uint8_t STRIP_PINS[] = {
  13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2, 14, 15, 16, 17,
  22, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51,
  A15, A14, A13, A12, A11, A10, A9, A8, A7
};
const uint8_t NUM_STRIPS = sizeof(STRIP_PINS) / sizeof(STRIP_PINS[0]);  // 41

// COMMERCIALS = the last N strips ("end strips") PLUS the ground-floor shops pin
#define COMMERCIAL_COUNT      7
#define COMMERCIAL_EXTRA_PIN  13   // pin 13 = shops, also lit on Commercials

// PODIUM = these 8 data pins (edit to match your podium strips)
const uint8_t PODIUM_PINS[] = { 12, 11, 10, 9, 8, 7, 6, 5 };
const uint8_t PODIUM_COUNT  = sizeof(PODIUM_PINS) / sizeof(PODIUM_PINS[0]);

// Buttons (index order = enum below). Wired button -> pin -> GND, internal pull-ups.
enum { B_ALL, B_COMMERCIAL, B_PODIUM, B_EDAC, B_FULL, NUM_BTN };
const uint8_t BTN_PIN[NUM_BTN] = { A4, A3, A2, A1, A0 };

// Relays (active LOW)
#define RELAY_EDAC_PIN      A5
#define RELAY_SURROUND_PIN  A6

#define DEBOUNCE_MS     50

/* ========================== END USER CONFIG ========================= */

// One shared buffer drives every strip
Adafruit_NeoPixel pixels(LEDS_PER_STRIP, STRIP_PINS[0], NEO_FORMAT);

enum LedMode { MODE_OFF, MODE_ALL, MODE_COMMERCIAL, MODE_PODIUM, MODE_FULL };
LedMode ledMode = MODE_OFF;

bool relayEdac     = false;
bool relaySurround = false;

// Per-strip desired colour (WARM_WHITE or 0) and the last colour actually sent.
uint32_t stripColor[NUM_STRIPS];
uint32_t lastPushed[NUM_STRIPS];

uint8_t  scanIdx   = 0;
bool     forceShow = false;          // set by the optional refresh timer
uint32_t lastRefresh = 0;

// Buffer-content cache so we re-fill the shared buffer only when the colour changes
uint32_t bufColor = 0xFFFFFFFFul;    // sentinel: no real 24-bit colour equals this

// Button debounce
bool     btnState[NUM_BTN];
bool     btnLast[NUM_BTN];
uint32_t btnTime[NUM_BTN];


/* --------------------------- helpers -------------------------------- */

static bool pinIsPodium(uint8_t pin) {
  for (uint8_t i = 0; i < PODIUM_COUNT; i++)
    if (PODIUM_PINS[i] == pin) return true;
  return false;
}

static void applyRelays() {
  digitalWrite(RELAY_EDAC_PIN,     relayEdac     ? LOW : HIGH);   // active LOW
  digitalWrite(RELAY_SURROUND_PIN, relaySurround ? LOW : HIGH);
}

static inline void ensureBuffer(uint32_t c) {
  if (bufColor != c) { pixels.fill(c); bufColor = c; }
}


/* ----------------------- mode application --------------------------- */
// Sets each strip's desired colour instantly (no fade). The scan in loop()
// then transmits only the strips whose colour actually changed.

static void applyMode() {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) stripColor[i] = 0;   // default off

  if (ledMode == MODE_ALL || ledMode == MODE_FULL) {            // whole building
    for (uint8_t i = 0; i < NUM_STRIPS; i++) stripColor[i] = WARM_WHITE;

  } else if (ledMode == MODE_COMMERCIAL) {                      // end strips + shops
    for (uint8_t i = NUM_STRIPS - COMMERCIAL_COUNT; i < NUM_STRIPS; i++)
      stripColor[i] = WARM_WHITE;
    for (uint8_t i = 0; i < NUM_STRIPS; i++)
      if (STRIP_PINS[i] == COMMERCIAL_EXTRA_PIN) stripColor[i] = WARM_WHITE;

  } else if (ledMode == MODE_PODIUM) {                          // podium strips only
    for (uint8_t i = 0; i < NUM_STRIPS; i++)
      if (pinIsPodium(STRIP_PINS[i])) stripColor[i] = WARM_WHITE;
  }

  scanIdx = 0;   // restart the scan so the new state renders promptly
}


/* --------------------------- buttons -------------------------------- */

static void onPress(uint8_t b) {
  switch (b) {
    case B_ALL:                                    // all strips + both relays
      if (ledMode == MODE_ALL) { ledMode = MODE_OFF; relayEdac = relaySurround = false; }
      else                     { ledMode = MODE_ALL; relayEdac = relaySurround = true;  }
      applyRelays();
      applyMode();
      break;
    case B_EDAC:                                   // relay only; strips untouched
      relayEdac = !relayEdac;
      applyRelays();
      break;
    case B_COMMERCIAL:
      ledMode = (ledMode == MODE_COMMERCIAL) ? MODE_OFF : MODE_COMMERCIAL;
      applyMode();
      break;
    case B_PODIUM:
      ledMode = (ledMode == MODE_PODIUM) ? MODE_OFF : MODE_PODIUM;
      applyMode();
      break;
    case B_FULL:                                   // all lights, relays untouched
      ledMode = (ledMode == MODE_FULL) ? MODE_OFF : MODE_FULL;
      applyMode();
      break;
  }
}

static void handleButtons(uint32_t now) {
  for (uint8_t i = 0; i < NUM_BTN; i++) {
    bool reading = (digitalRead(BTN_PIN[i]) == LOW);    // pressed = LOW
    if (reading != btnLast[i]) { btnLast[i] = reading; btnTime[i] = now; }
    if ((now - btnTime[i]) > DEBOUNCE_MS && reading != btnState[i]) {
      btnState[i] = reading;
      if (reading) {    
                lastActivity = now;                             // <-- ADD: reset idle timer                                // act on press only
        Serial.print(F("[STATE] Button pressed on pin "));
        Serial.println(BTN_PIN[i]);
        onPress(i);
      }
    }
  }
}


/* --------------------- push one strip to its pin -------------------- */

static void pushStrip(uint8_t i) {
  uint32_t c = stripColor[i];
  if (!forceShow && c == lastPushed[i]) return;   // unchanged: leave it latched
  ensureBuffer(c);
  pixels.setPin(STRIP_PINS[i]);
  pixels.show();
  lastPushed[i] = c;
}


/* ------------------------------ setup ------------------------------- */

void setup() {
  Serial.begin(115200);
  Serial.println(F("[BOOT] Building Lighting — STATIC warm white | Mega 2560"));

  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);

  // Clear every strip once at startup
  pixels.fill(0); bufColor = 0;
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    pixels.setPin(STRIP_PINS[i]);
    pixels.show();
    stripColor[i] = lastPushed[i] = 0;
  }
  Serial.println(F("[INIT] All strips cleared"));

  for (uint8_t i = 0; i < NUM_BTN; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    btnState[i] = btnLast[i] = false;
    btnTime[i] = 0;
  }

  pinMode(RELAY_EDAC_PIN, OUTPUT);
  pinMode(RELAY_SURROUND_PIN, OUTPUT);
  applyRelays();                       // both relays start OFF (HIGH)
  Serial.println(F("[INIT] Buttons (pull-up) + relays (OFF) ready"));

  lastRefresh = millis();
    lastActivity = millis();
  Serial.print(F("[INFO] Strips: "));
  Serial.print(NUM_STRIPS);
  Serial.print(F("  LEDs/strip: "));
  Serial.print(LEDS_PER_STRIP);
  Serial.print(F("  Brightness: "));
  Serial.println(BRIGHTNESS);
  Serial.println(F("[BOOT] Ready."));
}


/* ------------------------------ loop -------------------------------- */
// One strip per pass keeps button latency low. Once everything is lit and
// stable, pushStrip() sends nothing and the LED bus stays quiet.

void loop() {
  uint32_t now = millis();
  handleButtons(now);
 if (ledMode != MODE_FULL && (uint32_t)(now - lastActivity) >= IDLE_TIMEOUT_MS) {
    Serial.println(F("[STATE] Idle timeout -> all lights on"));
    ledMode = MODE_FULL;
    applyMode();
  }
  // Optional periodic self-heal (compiled out entirely when REFRESH_MS == 0).
  if (REFRESH_MS && (uint32_t)(now - lastRefresh) >= REFRESH_MS) {
    forceShow   = true;
    lastRefresh = now;
  }

  pushStrip(scanIdx);
  if (++scanIdx >= NUM_STRIPS) { scanIdx = 0; forceShow = false; }
}
