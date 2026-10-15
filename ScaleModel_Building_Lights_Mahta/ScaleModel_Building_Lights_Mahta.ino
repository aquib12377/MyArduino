/*
 * Miniature Building Model — WS2812 Lighting Controller  (SINGLE-COLOUR BUILD)
 * Board   : Arduino Mega 2560
 * Library : Adafruit NeoPixel
 *
 * SCALE: 41 strips x 120 LEDs = 4,920 pixels.
 * The Mega has only 8 KB SRAM, so one NeoPixel object per strip is impossible
 * (that alone needs ~14.7 KB of pixel buffers). Instead we use ONE shared
 * 120-LED buffer (~360 bytes) and re-point it to each strip's pin. RAM stays
 * ~1.2 KB total; it scales to any strip count.
 *
 * ----- COLOUR -----
 * ONE colour only: warm white RGB(255, 147, 40). Every mode and pattern uses it;
 * effects vary only BRIGHTNESS, never hue.
 *
 * ----- STRIP LAYOUT (41 strips, index = position in STRIP_PINS) -----
 * Zones are defined by PIN LIST, and they are allowed to OVERLAP. That is what
 * makes the numbers work out:
 *
 *   PODIUM      idx 1..8    pins 12,11,10,9,8,7,6,5              8 strips
 *   FLOORS      idx 9..40   the 32 tower floors, 4 groups of 8   32 strips
 *                 group 1 = floors 1-8    idx  9..16
 *                 group 2 = floors 9-16   idx 17..24
 *                 group 3 = floors 17-24  idx 25..32
 *                 group 4 = floors 25-32  idx 33..40
 *   COMMERCIAL  idx 0 (pin 13, ground shops) + idx 34..40 (the 7 "end" strips)
 *                                                          8 strips
 *
 * The 7 end strips are deliberately in BOTH the commercial zone and floor group
 * 4 — they are commercial units that sit on tower floors. 25 pure tower strips
 * + those 7 = the 32 floors. Overlap is intentional and supported.
 *
 * Brightness is assigned separately from zone membership, so the ground shops
 * can be brighter without dragging the 7 end strips with them.
 *
 * ----- CONTROLS (four buttons) -----
 *   POWER       : toggles the whole model ON (steady) / OFF.
 *   PATTERN     : press 1 -> Breathe  (smooth hue-stable fade, whole building)
 *                 press 2 -> Floor Fill (groups light 1-8, 9-16, 17-24, 25-32)
 *                 press 3 -> back to steady ON
 *   COMMERCIAL  : ground shops + the 7 end strips.
 *   PODIUM/EDAC : the 8 podium strips AND the EDAC relay together.
 *
 * ----- MODE / RELAY TABLE -----
 *   MODE          Strips lit             EDAC relay   Surround relay
 *   OFF           none                   off          off
 *   ALL           all 41                 ON           ON
 *   PATTERN       per pattern            ON           ON
 *   COMMERCIAL    shops + 7 end strips   off          off
 *   PODIUM        8 podium strips        ON           off
 *
 * ----- WHY THE OLD BUILD WENT RED WHEN FADING (fixed) -----
 * The old code scaled the colour TWICE: once by the fade level, then again by
 * Adafruit's setBrightness(35). Two rounding steps at low master brightness
 * destroy the small channels first. Measured: blue reached 0 below fade level
 * 51 and green below 10, so 54% of every fade cycle had no blue and 32% was
 * PURE RED. setBrightness() is no longer used; each channel is computed in ONE
 * rounded 32-bit step, and the level never goes below hueFloor — the lowest
 * level where all three channels still round to at least 1.
 *
 * ----- WHY LIGHTS FLICKERED TO RANDOM COLOURS (fixed) -----
 * Adafruit_NeoPixel::setPin() sets the PREVIOUS pin back to INPUT. Multiplexing
 * one buffer across 41 strips left 40 lines floating at any instant; floating
 * lines pick up crosstalk and the first chip latches it as data. The MultiStrip
 * subclass re-points port/pinMask directly, so setPin() is never called and no
 * line is ever undriven. If flicker persists, use DIAG_MODE below.
 *
 * ----- TIMING -----
 * show() holds interrupts off ~3.6 ms, so millis()/micros() lose time. ALL
 * timing — patterns, fades, debounce — runs off animClockMs(), a Timer1 /256
 * clock that overflows only every ~1.05 s. millis() is never used here.
 *
 * ----- HARDWARE CHECKLIST if flicker persists -----
 *   1. Common ground between the Mega and the 5 V LED supply.
 *   2. 330-470 ohm series resistor in each data line at the Arduino end, plus a
 *      10k pull-down to GND at the strip end.
 *   3. 1000 uF electrolytic across 5V/GND at every power injection point.
 *   4. Data wires short and paired with a ground return.
 *   5. Supply sag: measure 5 V at the FAR end of the longest run under full
 *      load. Below ~4.5 V the chips misread data as random colours.
 *   6. If strips are shorter than 120 LEDs, LOWER LEDS_PER_STRIP — scan time is
 *      directly proportional to it.
 */

#include <Adafruit_NeoPixel.h>
#include <math.h>

/* ============================ USER CONFIG ============================ */

#define LEDS_PER_STRIP   120
#define NEO_FORMAT       (NEO_GRB + NEO_KHZ800)

// The one and only colour: warm white RGB(255, 147, 40)
#define COLOR_R  255
#define COLOR_G  147
#define COLOR_B   40

// 41 WS2812 data pins, bottom -> top (original connector order).
const uint8_t STRIP_PINS[] = {
  13,                                                  // 0     ground shops
  12, 11, 10,  9,  8,  7,  6,  5,                      // 1-8   podium
   4,  3,  2, 14, 15, 16, 17, 22,                      // 9-16  floors 1-8
  23, 25, 27, 29, 31, 33, 35, 37,                      // 17-24 floors 9-16
  39, 41, 43, 45, 47, 49, 51, A15,                     // 25-32 floors 17-24
  A14, A13, A12, A11, A10, A9, A8, A7                  // 33-40 floors 25-32
};                                                     //       A13..A7 also commercial
const uint8_t NUM_STRIPS = sizeof(STRIP_PINS) / sizeof(STRIP_PINS[0]);   // 41

/* ---- ZONES, BY PIN. Overlap between lists is allowed and expected. ---- */

// What the COMMERCIAL button lights: ground shops + the 7 "end" strips.
const uint8_t COMMERCIAL_PINS[] = { 13, A13, A12, A11, A10, A9, A8, A7 };

// What the PODIUM button lights (and it drives the EDAC relay with it).
const uint8_t PODIUM_PINS[] = { 12, 11, 10, 9, 8, 7, 6, 5,4 };

// The tower floors: every strip from this index upward. 41 - 9 = 32 floors.
#define FLOOR_FIRST_INDEX  9
#define FLOOR_GROUPS       4      // 32 floors / 4 = 8 per group

/* ---- PER-STRIP BRIGHTNESS (0-255) ----
 * Applied in our own maths, NOT via setBrightness(). Higher values also fade
 * more smoothly (more distinct steps) but cost proportionally more current.
 * Priority: GROUND_BRIGHT_PINS, then podium, then everything else. */
#define BRIGHT_GROUND      90     // ground shops — deliberately brighter
#define BRIGHT_PODIUM      35     // the 8 podium strips
#define BRIGHT_FLOORS      35     // everything else, incl. the 7 end strips
const uint8_t GROUND_BRIGHT_PINS[] = { 13 };

/* ---- CONTROL PINS — VERIFY AGAINST THE HARNESS ----
 * Leave BUTTON_DISCOVERY at 1 and watch the serial monitor: every control pin
 * that goes LOW prints its own pin number, so the harness maps itself. */
#define BTN_POWER_PIN       A1    // ON / OFF                 (confirmed)
#define BTN_PATTERN_PIN     A0    // pattern step             (confirmed)
#define BTN_COMMERCIAL_PIN  A2    // commercial zone          <-- VERIFY
#define BTN_PODIUM_PIN      A3    // podium zone + EDAC relay <-- VERIFY

const uint8_t UNUSED_CTRL_PINS[] = { A4 };

#define RELAY_EDAC_PIN      A5
#define RELAY_SURROUND_PIN  A6

#define BUTTON_DISCOVERY   1      // 1 = log every control-pin press with its pin
#define ZONE_SECOND_PRESS  0      // 0 = zone button 2nd press goes OFF, 1 = goes ALL
#define START_LIGHTS_ON    1      // 1 = all lights ON at boot

/* 1 = re-point the shared buffer via the MultiStrip subclass, so setPin() is
 * never called and no data line is ever left floating. Set to 0 only if your
 * Adafruit_NeoPixel version will not compile the subclass. */
#define USE_DIRECT_PIN_BIND  1

/* ---- FAULT-FINDING MODES — set back to 0 for normal operation ----
 *   1 = LATCH TEST. Light everything once at boot, then transmit NOTHING ever
 *       again. Colours still changing = electrical (power/ground/noise).
 *       Display staying perfect = the fault is in the transmit path.
 *   2 = LOAD TEST. Drive only the first DIAG_STRIP_LIMIT strips. If flicker
 *       appears as you raise the limit, it is supply sag, not signal. */
#define DIAG_MODE          0
#define DIAG_STRIP_LIMIT   8

// Timing / feel
#define DEBOUNCE_MS      40UL
#define FADE_MS          900UL    // cross-fade length for a static mode change
#define PATTERN_BLEND_MS 900UL    // ease-in when a pattern starts
#define FRAME_MIN_MS     20UL     // max update rate (50 fps ceiling)
#define REFRESH_MS       500UL    // force-retransmit every strip this often

// Pattern 1 — Breathe: smooth sine, hue-stable, never reaches the red zone
#define BREATHE_PERIOD   9000.0f  // ms for a full exhale + inhale

// Pattern 2 — Floor Fill: groups light 1-8, 9-16, 17-24, 25-32
#define SEQ_STEP_MS      1800UL   // gap between one group and the next
#define SEQ_RISE_MS       700UL   // fade-in of each group (must be <= SEQ_STEP_MS)
#define SEQ_HOLD_MS      2500UL   // hold with all floors lit
#define SEQ_FALL_MS      1500UL   // fade all floors back out
#define SEQ_BASE_ZONES   1        // 1 = ground shops + podium stay lit throughout

/* ========================== END USER CONFIG ========================= */

const uint8_t NUM_COMMERCIAL   = sizeof(COMMERCIAL_PINS) / sizeof(COMMERCIAL_PINS[0]);
const uint8_t NUM_PODIUM       = sizeof(PODIUM_PINS) / sizeof(PODIUM_PINS[0]);
const uint8_t NUM_GROUND_BR    = sizeof(GROUND_BRIGHT_PINS) / sizeof(GROUND_BRIGHT_PINS[0]);
const uint8_t NUM_UNUSED_CTRL  = sizeof(UNUSED_CTRL_PINS) / sizeof(UNUSED_CTRL_PINS[0]);
const uint8_t FLOOR_COUNT      = NUM_STRIPS - FLOOR_FIRST_INDEX;          // 32

enum LedMode { MODE_OFF, MODE_ALL, MODE_PATTERN, MODE_COMMERCIAL, MODE_PODIUM };
enum { PAT_BREATHE, PAT_FLOORFILL, PAT_COUNT };

/* Subclass so we can re-point the shared buffer at another pin WITHOUT calling
 * setPin(), which flips the old pin to INPUT. pin/port/pinMask are protected
 * members, so a derived class may set them directly. All 41 lines are set
 * OUTPUT LOW once in setup() and are NEVER undriven again. */
class MultiStrip : public Adafruit_NeoPixel {
public:
  MultiStrip(uint16_t n, int16_t p, neoPixelType t) : Adafruit_NeoPixel(n, p, t) {}
  void bindPin(uint8_t p) {
    pin = p;
#if defined(__AVR__)
    port    = portOutputRegister(digitalPinToPort(p));
    pinMask = digitalPinToBitMask(p);
#endif
  }
};

MultiStrip pixels(LEDS_PER_STRIP, STRIP_PINS[0], NEO_FORMAT);

LedMode ledMode        = MODE_OFF;
uint8_t currentPattern = PAT_BREATHE;

/* Software model. Everything is a "unit level" 0-255 where 0 means fully off.
 * Colour is derived at push time from (unit level, that strip's brightness),
 * so hue handling lives in exactly one place. */
uint8_t curU[NUM_STRIPS];        // unit level currently shown
uint8_t tgtU[NUM_STRIPS];        // static-mode fade target
uint8_t fromU[NUM_STRIPS];       // unit level when the current fade began
uint8_t lastOut[NUM_STRIPS];     // last OUTPUT level actually transmitted
uint8_t stripBright[NUM_STRIPS]; // per-strip brightness

// Zone membership, built at boot. A strip may be in more than one.
bool    inCommercial[NUM_STRIPS];
bool    inPodium[NUM_STRIPS];
uint8_t floorGroupOf[NUM_STRIPS];   // 0..FLOOR_GROUPS-1, or 0xFF if not a floor

// Hue-protection limits, computed at boot from the dimmest brightness in use.
uint8_t hueFloor = 1;
uint8_t rawFloor = 0;

// Scan / frame state
uint8_t  scanIdx     = 0;
bool     forceScan   = false;
uint32_t modeStart   = 0;
uint32_t lastFrame   = 0;
uint32_t lastRefresh = 0;

uint32_t bufColor = 0xFFFFFFFFul;   // buffer-content cache; impossible start value

enum { B_POWER, B_PATTERN, B_COMMERCIAL, B_PODIUM, NUM_BTN };
const uint8_t BTN_PIN[NUM_BTN] = {
  BTN_POWER_PIN, BTN_PATTERN_PIN, BTN_COMMERCIAL_PIN, BTN_PODIUM_PIN
};
bool     btnState[NUM_BTN];
bool     btnLast[NUM_BTN];
uint32_t btnTime[NUM_BTN];


/* ===================== Timer1 animation clock ======================= */

volatile uint32_t timer1Overflows = 0;
ISR(TIMER1_OVF_vect) { timer1Overflows++; }

static void setupAnimClock() {
  noInterrupts();
  TCCR1A = 0;                 // normal mode: OC1A/B/C disconnected, pins 11/12/13 free
  TCCR1B = 0;
  TCNT1  = 0;
  TIFR1  |= _BV(TOV1);
  TIMSK1  = _BV(TOIE1);
  TCCR1B  = _BV(CS12);        // start, prescaler /256 -> 1 tick = 16 us
  interrupts();
}

static uint32_t animClockMs() {
  uint32_t ovf;
  uint16_t cnt;
  uint8_t  s = SREG;
  cli();
  ovf = timer1Overflows;
  cnt = TCNT1;
  if ((TIFR1 & _BV(TOV1)) && cnt < 0x8000) ovf++;
  SREG = s;
  return ovf * 1048UL + (ovf * 576UL) / 1000UL + ((uint32_t)cnt * 16UL) / 1000UL;
}


/* ------------------- colour: ONE rounded quantisation ---------------- */

static uint32_t warmColor(uint8_t lvl, uint8_t m) {
  uint32_t k = (uint32_t)lvl * (uint32_t)m;               // 0..65025
  uint32_t r = ((uint32_t)COLOR_R * k + 32512UL) / 65025UL;
  uint32_t g = ((uint32_t)COLOR_G * k + 32512UL) / 65025UL;
  uint32_t b = ((uint32_t)COLOR_B * k + 32512UL) / 65025UL;
  return (r << 16) | (g << 8) | b;
}

// Lowest output level at which the smallest channel (blue) still rounds to >= 1.
static uint8_t findHueFloor(uint8_t m) {
  for (uint16_t l = 1; l <= 255; l++) {
    uint32_t k = l * (uint32_t)m;
    if ((((uint32_t)COLOR_B * k + 32512UL) / 65025UL) >= 1) return (uint8_t)l;
  }
  return 255;
}

// Smallest pre-gamma value whose gamma-corrected result reaches hueFloor, so
// the bottom of a fade is a real ramp rather than a flat dead zone.
static uint8_t findRawFloor(uint8_t floorLvl) {
  for (uint16_t p = 0; p <= 255; p++)
    if (Adafruit_NeoPixel::gamma8((uint8_t)p) >= floorLvl) return (uint8_t)p;
  return 255;
}

static uint8_t levelFromUnit(uint8_t u) {
  if (u == 0) return 0;
  uint8_t raw = rawFloor + (uint8_t)(((uint16_t)u * (uint16_t)(255 - rawFloor)) / 255);
  uint8_t g   = Adafruit_NeoPixel::gamma8(raw);
  return (g < hueFloor) ? hueFloor : g;
}


/* --------------------------- zones ---------------------------------- */

static bool pinInList(uint8_t pin, const uint8_t *list, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) if (list[i] == pin) return true;
  return false;
}

static int8_t indexOfPin(uint8_t pin) {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) if (STRIP_PINS[i] == pin) return (int8_t)i;
  return -1;
}

// Build zone tables and validate the configuration against STRIP_PINS.
static void buildZones() {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    inCommercial[i] = false;
    inPodium[i]     = false;
    floorGroupOf[i] = (i >= FLOOR_FIRST_INDEX)
                        ? (uint8_t)((i - FLOOR_FIRST_INDEX) * FLOOR_GROUPS / FLOOR_COUNT)
                        : 0xFF;
  }
  for (uint8_t k = 0; k < NUM_COMMERCIAL; k++) {
    int8_t idx = indexOfPin(COMMERCIAL_PINS[k]);
    if (idx < 0) { Serial.print(F("[WARN] commercial pin not in STRIP_PINS: "));
                   Serial.println(COMMERCIAL_PINS[k]); }
    else inCommercial[idx] = true;
  }
  for (uint8_t k = 0; k < NUM_PODIUM; k++) {
    int8_t idx = indexOfPin(PODIUM_PINS[k]);
    if (idx < 0) { Serial.print(F("[WARN] podium pin not in STRIP_PINS: "));
                   Serial.println(PODIUM_PINS[k]); }
    else {
      inPodium[idx] = true;
      if (inCommercial[idx]) { Serial.print(F("[WARN] pin in BOTH commercial and podium: "));
                               Serial.println(PODIUM_PINS[k]); }
    }
  }
  // Brightness: ground list wins, then podium, then everything else.
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    if      (pinInList(STRIP_PINS[i], GROUND_BRIGHT_PINS, NUM_GROUND_BR))
      stripBright[i] = BRIGHT_GROUND;
    else if (inPodium[i]) stripBright[i] = BRIGHT_PODIUM;
    else                  stripBright[i] = BRIGHT_FLOORS;
  }
}

static bool stripLitInMode(uint8_t i) {
  switch (ledMode) {
    case MODE_ALL:        return true;
    case MODE_COMMERCIAL: return inCommercial[i];
    case MODE_PODIUM:     return inPodium[i];
    default:              return false;
  }
}

static void applyRelays() {
  bool edac     = (ledMode == MODE_ALL || ledMode == MODE_PATTERN || ledMode == MODE_PODIUM);
  bool surround = (ledMode == MODE_ALL || ledMode == MODE_PATTERN);
  digitalWrite(RELAY_EDAC_PIN,     edac     ? LOW : HIGH);   // active LOW
  digitalWrite(RELAY_SURROUND_PIN, surround ? LOW : HIGH);
}

static inline void ensureBuffer(uint32_t c) {
  if (bufColor != c) { pixels.fill(c); bufColor = c; }
}


/* ---------------- CRITICAL: keep every data line driven ---------------
 * Adafruit_NeoPixel::setPin() does:
 *     if (begun && (pin >= 0)) pinMode(pin, INPUT);   // <-- old pin FLOATS
 * With USE_DIRECT_PIN_BIND, setPin() is never called: only port/pinMask move,
 * so no line is ever high-impedance. The fallback path restores the previous
 * pin right after setPin() floats it, leaving a ~10 us window per change. */
uint8_t activePin = 0xFF;

static void selectStrip(uint8_t p) {
  if (p == activePin) return;
#if USE_DIRECT_PIN_BIND
  pixels.bindPin(p);
#else
  uint8_t prev = activePin;
  pixels.setPin(p);
  if (prev != 0xFF) { pinMode(prev, OUTPUT); digitalWrite(prev, LOW); }
#endif
  activePin = p;
}


/* ------------------------ pattern evaluation ------------------------ */
// Fill out[] with each strip's UNIT level at time 'now'. No colour maths here.

static void computePattern(uint8_t which, uint32_t now, uint8_t *out) {

  if (which == PAT_BREATHE) {
    // Whole building on one smooth sine. The unit level never reaches 0, so the
    // hue is identical at the top and bottom of every breath.
    float phase = (float)(now % (uint32_t)BREATHE_PERIOD) / BREATHE_PERIOD;
    float s     = (sinf(phase * TWO_PI - HALF_PI) + 1.0f) * 0.5f;              // 0..1
    uint8_t u   = (uint8_t)(1.0f + s * 254.0f);                                // 1..255
    for (uint8_t i = 0; i < NUM_STRIPS; i++) out[i] = u;

} else {  // PAT_FLOORFILL — cumulative groups of 8

    const uint32_t fillMs =
        (uint32_t)FLOOR_GROUPS * SEQ_STEP_MS;

    const uint32_t cycle =
        fillMs + SEQ_HOLD_MS + SEQ_FALL_MS;

    uint32_t cyc = (now - modeStart) % cycle;

    uint8_t gu[FLOOR_GROUPS] = {0, 0, 0, 0};

    // ------------------------------------------------
    // GROUP 1 : Floors 1-8
    // ------------------------------------------------
    if (cyc < SEQ_STEP_MS) {

        float rise =
            (float)cyc / (float)SEQ_RISE_MS;

        if (rise > 1.0f)
            rise = 1.0f;

        gu[0] = (uint8_t)(rise * 255.0f);

        gu[1] = 0;
        gu[2] = 0;
        gu[3] = 0;
    }

    // ------------------------------------------------
    // GROUP 2 : Floors 9-16
    // Group 1 remains ON
    // ------------------------------------------------
    else if (cyc < (SEQ_STEP_MS * 2UL)) {

        uint32_t t =
            cyc - SEQ_STEP_MS;

        float rise =
            (float)t / (float)SEQ_RISE_MS;

        if (rise > 1.0f)
            rise = 1.0f;

        gu[0] = 255;
        gu[1] = (uint8_t)(rise * 255.0f);

        gu[2] = 0;
        gu[3] = 0;
    }

    // ------------------------------------------------
    // GROUP 3 : Floors 17-24
    // Groups 1 + 2 remain ON
    // GROUP 4 IS EXPLICITLY OFF
    // ------------------------------------------------
    else if (cyc < (SEQ_STEP_MS * 3UL)) {

        uint32_t t =
            cyc - (SEQ_STEP_MS * 2UL);

        float rise =
            (float)t / (float)SEQ_RISE_MS;

        if (rise > 1.0f)
            rise = 1.0f;

        gu[0] = 255;
        gu[1] = 255;
        gu[2] = (uint8_t)(rise * 255.0f);

        gu[3] = 0;   // IMPORTANT: group 4 definitely OFF
    }

    // ------------------------------------------------
    // GROUP 4 : Floors 25-32
    // Groups 1 + 2 + 3 remain ON
    // ------------------------------------------------
    else if (cyc < (SEQ_STEP_MS * 4UL)) {

        uint32_t t =
            cyc - (SEQ_STEP_MS * 3UL);

        float rise =
            (float)t / (float)SEQ_RISE_MS;

        if (rise > 1.0f)
            rise = 1.0f;

        gu[0] = 255;
        gu[1] = 255;
        gu[2] = 255;
        gu[3] = (uint8_t)(rise * 255.0f);
    }

    // ------------------------------------------------
    // HOLD : all 4 groups ON
    // ------------------------------------------------
    else if (cyc < fillMs + SEQ_HOLD_MS) {

        gu[0] = 255;
        gu[1] = 255;
        gu[2] = 255;
        gu[3] = 255;
    }

    // ------------------------------------------------
    // FADE ALL OUT
    // ------------------------------------------------
    else {

        float f =
            (float)(cyc - fillMs - SEQ_HOLD_MS) /
            (float)SEQ_FALL_MS;

        if (f > 1.0f)
            f = 1.0f;

        uint8_t u =
            (uint8_t)((1.0f - f) * 255.0f);

        gu[0] = u;
        gu[1] = u;
        gu[2] = u;
        gu[3] = u;
    }

    // Apply calculated level to each floor
    for (uint8_t i = 0; i < NUM_STRIPS; i++) {

        if (floorGroupOf[i] != 0xFF) {

            out[i] =
                gu[floorGroupOf[i]];

        } else {

            out[i] =
                SEQ_BASE_ZONES ? 255 : 0;
        }
    }
}}


/* ----------------------- mode application --------------------------- */

static void applyMode() {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    fromU[i] = curU[i];
    tgtU[i]  = stripLitInMode(i) ? 255 : 0;
  }
  applyRelays();
  modeStart = animClockMs();
  scanIdx   = 0;
  forceScan = true;
}


/* --------------------------- buttons -------------------------------- */

static void logMode() {
  Serial.print(F("[STATE] "));
  switch (ledMode) {
    case MODE_OFF:        Serial.println(F("OFF")); break;
    case MODE_ALL:        Serial.println(F("ALL — steady warm white")); break;
    case MODE_COMMERCIAL: Serial.println(F("COMMERCIAL — shops + end strips")); break;
    case MODE_PODIUM:     Serial.println(F("PODIUM — podium strips + EDAC relay")); break;
    default:
      if (currentPattern == PAT_BREATHE) Serial.println(F("PATTERN 1/2 — Breathe"));
      else                               Serial.println(F("PATTERN 2/2 — Floor Fill"));
      break;
  }
}

static LedMode zoneExitMode() {
#if ZONE_SECOND_PRESS
  return MODE_ALL;
#else
  return MODE_OFF;
#endif
}

static void onPress(uint8_t b) {
  switch (b) {
    case B_POWER:
      ledMode = (ledMode == MODE_OFF) ? MODE_ALL : MODE_OFF;
      break;

    case B_PATTERN:
      if (ledMode != MODE_PATTERN) {
        ledMode = MODE_PATTERN;
        currentPattern = PAT_BREATHE;
      } else if (++currentPattern >= PAT_COUNT) {
        currentPattern = PAT_BREATHE;
        ledMode = MODE_ALL;                 // 3rd press returns to steady warm white
      }
      break;

    case B_COMMERCIAL:
      ledMode = (ledMode == MODE_COMMERCIAL) ? zoneExitMode() : MODE_COMMERCIAL;
      break;

    case B_PODIUM:
      ledMode = (ledMode == MODE_PODIUM) ? zoneExitMode() : MODE_PODIUM;
      break;
  }
  applyMode();
  logMode();
}

static void handleButtons(uint32_t now) {
  for (uint8_t i = 0; i < NUM_BTN; i++) {
    bool reading = (digitalRead(BTN_PIN[i]) == LOW);    // pressed = LOW (pull-ups)
    if (reading != btnLast[i]) { btnLast[i] = reading; btnTime[i] = now; }
    if ((now - btnTime[i]) > DEBOUNCE_MS && reading != btnState[i]) {
      btnState[i] = reading;
      if (reading) {
#if BUTTON_DISCOVERY
        Serial.print(F("[DEBUG] Press on pin A"));
        Serial.println(BTN_PIN[i] - A0);
#endif
        onPress(i);
      }
    }
  }
}

#if BUTTON_DISCOVERY
bool     spyLast[NUM_UNUSED_CTRL];
uint32_t spyTime[NUM_UNUSED_CTRL];

static void spyUnusedPins(uint32_t now) {
  for (uint8_t i = 0; i < NUM_UNUSED_CTRL; i++) {
    bool reading = (digitalRead(UNUSED_CTRL_PINS[i]) == LOW);
    if (reading != spyLast[i] && (now - spyTime[i]) > DEBOUNCE_MS) {
      spyLast[i] = reading;
      spyTime[i] = now;
      if (reading) {
        Serial.print(F("[DEBUG] Press on UNUSED pin A"));
        Serial.println(UNUSED_CTRL_PINS[i] - A0);
      }
    }
  }
}
#endif


/* ----------------------- per-frame computation ---------------------- */

static void advanceFrame(uint32_t now) {
  if (ledMode == MODE_PATTERN) {
    computePattern(currentPattern, now, curU);
    uint32_t el = now - modeStart;
    if (currentPattern == PAT_BREATHE && el < PATTERN_BLEND_MS) {

    uint16_t f =
        (uint16_t)((el * 255UL) / PATTERN_BLEND_MS);

    for (uint8_t i = 0; i < NUM_STRIPS; i++) {

        curU[i] = (uint8_t)(
            ((uint16_t)fromU[i] * (255 - f) +
             (uint16_t)curU[i] * f) / 255
        );
    }
}
  } else {
    uint32_t el = now - modeStart;
    uint16_t f  = (el >= FADE_MS) ? 255 : (uint16_t)((el * 255UL) / FADE_MS);
    for (uint8_t i = 0; i < NUM_STRIPS; i++)
      curU[i] = (uint8_t)(((uint16_t)fromU[i] * (255 - f) + (uint16_t)tgtU[i] * f) / 255);
  }
}


/* --------------------- push one strip to its pin -------------------- */

bool diagLatched = false;

static void pushStrip(uint8_t i) {
#if DIAG_MODE == 1
  if (diagLatched) return;            // latch test: never transmit again
#endif
  uint8_t u = curU[i];
#if DIAG_MODE == 2
  if (i >= DIAG_STRIP_LIMIT) u = 0;   // load test: only the first N strips lit
#endif
  uint8_t out = levelFromUnit(u);
  if (!forceScan && out == lastOut[i]) return;      // nothing visibly changed
  ensureBuffer(warmColor(out, stripBright[i]));
  selectStrip(STRIP_PINS[i]);         // never plain setPin()
  pixels.show();
  lastOut[i] = out;
}


/* ------------------------------ setup ------------------------------- */

static void reportZones() {
  Serial.print(F("[ZONE] Commercial ("));
  Serial.print(NUM_COMMERCIAL);
  Serial.print(F(" strips), pins:"));
  for (uint8_t i = 0; i < NUM_STRIPS; i++)
    if (inCommercial[i]) { Serial.print(' '); Serial.print(STRIP_PINS[i]); }
  Serial.println();

  Serial.print(F("[ZONE] Podium ("));
  Serial.print(NUM_PODIUM);
  Serial.print(F(" strips), pins:"));
  for (uint8_t i = 0; i < NUM_STRIPS; i++)
    if (inPodium[i]) { Serial.print(' '); Serial.print(STRIP_PINS[i]); }
  Serial.println();

  Serial.print(F("[ZONE] Floors: "));
  Serial.print(FLOOR_COUNT);
  Serial.print(F(" in "));
  Serial.print(FLOOR_GROUPS);
  Serial.println(F(" groups"));
  for (uint8_t g = 0; g < FLOOR_GROUPS; g++) {
    uint8_t n = 0;
    Serial.print(F("        group "));
    Serial.print(g + 1);
    Serial.print(F(" pins:"));
    for (uint8_t i = 0; i < NUM_STRIPS; i++)
      if (floorGroupOf[i] == g) { Serial.print(' '); Serial.print(STRIP_PINS[i]); n++; }
    Serial.print(F("   ("));
    Serial.print(n);
    Serial.println(F(")"));
  }

  uint8_t both = 0;
  for (uint8_t i = 0; i < NUM_STRIPS; i++)
    if (inCommercial[i] && floorGroupOf[i] != 0xFF) both++;
  Serial.print(F("[ZONE] strips in BOTH commercial and a floor group: "));
  Serial.print(both);
  Serial.println(F(" (expected: the 7 end strips)"));
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("[BOOT] Building Lighting Controller — Mega 2560 @ 115200"));
  Serial.println(F("[BOOT] Single colour: warm white RGB(255,147,40)"));

  setupAnimClock();
  buildZones();
  reportZones();

  // Hue protection is sized for the DIMMEST brightness in use, so no strip can
  // drift to red no matter which zone it is in.
  uint8_t dimmest = 255;
  for (uint8_t i = 0; i < NUM_STRIPS; i++)
    if (stripBright[i] < dimmest) dimmest = stripBright[i];
  hueFloor = findHueFloor(dimmest);
  rawFloor = findRawFloor(hueFloor);

  uint8_t steps = 0; int16_t prev = -1;
  for (uint16_t l = hueFloor; l <= 255; l++) {
    int16_t v = (int16_t)(warmColor((uint8_t)l, dimmest) >> 16);
    if (v != prev) { steps++; prev = v; }
  }
  Serial.print(F("[INIT] hue floor "));
  Serial.print(hueFloor);
  Serial.print(F(", raw floor "));
  Serial.print(rawFloor);
  Serial.print(F(", distinct fade steps "));
  Serial.println(steps);

  // Drive EVERY data line low before anything else. From power-on until this
  // runs, all 41 lines float and the strips can latch noise.
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    pinMode(STRIP_PINS[i], OUTPUT);
    digitalWrite(STRIP_PINS[i], LOW);
  }

  pixels.begin();                       // setBrightness() deliberately NOT used
  activePin = STRIP_PINS[0];

  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    curU[i] = tgtU[i] = fromU[i] = 0;
    lastOut[i] = 0;
  }
  ensureBuffer(0);
  for (uint8_t i = 0; i < NUM_STRIPS; i++) { selectStrip(STRIP_PINS[i]); pixels.show(); }
  Serial.println(F("[INIT] All strips cleared, all data lines driven LOW"));

  for (uint8_t i = 0; i < NUM_BTN; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    btnState[i] = btnLast[i] = false;
    btnTime[i] = 0;
  }
  for (uint8_t i = 0; i < NUM_UNUSED_CTRL; i++)
    pinMode(UNUSED_CTRL_PINS[i], INPUT_PULLUP);
#if BUTTON_DISCOVERY
  for (uint8_t i = 0; i < NUM_UNUSED_CTRL; i++) { spyLast[i] = false; spyTime[i] = 0; }
  Serial.println(F("[INIT] Button discovery ON — presses print their pin number"));
#endif

  pinMode(RELAY_EDAC_PIN, OUTPUT);
  pinMode(RELAY_SURROUND_PIN, OUTPUT);
  applyRelays();

#if START_LIGHTS_ON
  ledMode = MODE_ALL;
#endif
  applyMode();
  lastFrame = lastRefresh = animClockMs();

#if USE_DIRECT_PIN_BIND
  Serial.println(F("[INIT] Direct pin bind ON — setPin() never called"));
#else
  Serial.println(F("[WARN] Fallback pin mode — lines float ~10 us per strip change"));
#endif

#if DIAG_MODE == 1
  for (uint8_t i = 0; i < NUM_STRIPS; i++) { curU[i] = 255; lastOut[i] = 0; }
  forceScan = true;
  for (uint8_t i = 0; i < NUM_STRIPS; i++) pushStrip(i);
  diagLatched = true;
  Serial.println(F("[DIAG 1] LATCH TEST — written once, transmission stopped."));
  Serial.println(F("[DIAG 1] Colours changing now = electrical, not firmware."));
#elif DIAG_MODE == 2
  Serial.print(F("[DIAG 2] LOAD TEST — only the first "));
  Serial.print(DIAG_STRIP_LIMIT);
  Serial.println(F(" strips driven."));
#endif

  logMode();
  Serial.println(F("[BOOT] Ready."));
}


/* ------------------------------ loop -------------------------------- */

void loop() {
  uint32_t now = animClockMs();
  handleButtons(now);
#if BUTTON_DISCOVERY
  spyUnusedPins(now);
#endif

  if (scanIdx == 0) {
    if ((now - lastRefresh) >= REFRESH_MS) {   // self-heal any corrupted strip
      lastRefresh = now;
      forceScan   = true;
    }
    if ((now - lastFrame) >= FRAME_MIN_MS) {
      lastFrame = now;
      advanceFrame(now);
    }
  }

  pushStrip(scanIdx);

  if (++scanIdx >= NUM_STRIPS) {
    scanIdx   = 0;
    forceScan = false;
  }
}
