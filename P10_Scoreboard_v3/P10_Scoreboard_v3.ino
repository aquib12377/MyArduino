/*
 * ============================================================
 *  Dual P10 LED Scoreboard — Arduino Nano  (v3 — 2 Buttons)
 * ============================================================
 *  Only 2 buttons — one per team.
 *  Short press  →  +1 point
 *  Long press (2 s)  →  reset score to 0
 *
 *  Panel Layout (front view):
 *  ┌────────────────┬────────────────┐
 *  │   TEAM A       │   TEAM B       │  ← 3×5 mini font, rows 1–5
 *  │   ─────        │   ─────        │  ← separator line, row 7
 *  │     00         │     00         │  ← 5×7 font, rows 8–14
 *  └────────────────┴────────────────┘
 *
 *  ── Wiring: Nano → P10 Panel 1 IN ──────────────────────────
 *  D13 (SCK)  →  CLK
 *  D11 (MOSI) →  DATA
 *  D9         →  SCLK (Latch)
 *  D3         →  OE   (PWM, active LOW)
 *  D6         →  A
 *  D7         →  B
 *  5 V        →  VCC  (use external 5 V supply for panels!)
 *  GND        →  GND
 *  Panel 1 OUT → Panel 2 IN  (daisy-chain)
 *
 *  ── Button Wiring (pin → GND, internal pull-up) ─────────────
 *  D2  Team A  short press = +1  /  long press = reset to 0
 *  D4  Team B  short press = +1  /  long press = reset to 0
 *
 *  NOTE: D3/D6/D7/D9/D11/D13 are reserved by DMD2.
 *        Do NOT use them for buttons.
 * ============================================================
 */

#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>

// ── Panel config ──────────────────────────────────────────────
#define PANELS_WIDE  2
#define PANELS_TALL  1
SoftDMD dmd(PANELS_WIDE, PANELS_TALL);

// ── Button pins ───────────────────────────────────────────────
#define BTN_A  2    // Team A
#define BTN_B  4    // Team B

// ── Tuning ────────────────────────────────────────────────────
#define MAX_SCORE      99
#define DEBOUNCE_MS    30      // ms — ignore bounces shorter than this
#define HOLD_RESET_MS  2000   // ms — hold duration to trigger reset

// ── Team names (uppercase, max 7 chars) ──────────────────────
const char TEAM_A_NAME[] = "TEAM A";
const char TEAM_B_NAME[] = "TEAM B";

// ── Scores ────────────────────────────────────────────────────
int scoreA = 0;
int scoreB = 0;

// ─────────────────────────────────────────────────────────────
//  Button state machine
//  Each button has three outcomes per physical press:
//    PRESS_NONE   — nothing to report yet
//    PRESS_SHORT  — released before HOLD_RESET_MS
//    PRESS_LONG   — held for HOLD_RESET_MS (fires once, mid-hold)
// ─────────────────────────────────────────────────────────────
enum PressType { PRESS_NONE, PRESS_SHORT, PRESS_LONG };

struct Button {
  uint8_t       pin;
  bool          lastRaw;         // last debounced reading
  bool          pressed;         // currently held down
  unsigned long pressStart;      // millis() when press began
  bool          longFired;       // long-press already handled
  unsigned long lastDebounce;    // last time raw reading changed
  bool          pendingRaw;      // raw reading being debounced
};

Button btnA = { BTN_A, HIGH, false, 0, false, 0, HIGH };
Button btnB = { BTN_B, HIGH, false, 0, false, 0, HIGH };

/*
 * pollButton()
 * Call every loop iteration. Returns:
 *   PRESS_LONG  — the instant the hold threshold is crossed
 *   PRESS_SHORT — on release, if long was never fired
 *   PRESS_NONE  — nothing actionable this cycle
 */
PressType pollButton(Button& btn) {
  bool raw = digitalRead(btn.pin);   // LOW = pressed (active-low)
  unsigned long now = millis();

  // ── Debounce ─────────────────────────────────────────────
  if (raw != btn.pendingRaw) {
    btn.pendingRaw    = raw;
    btn.lastDebounce  = now;
  }

  bool stable = (now - btn.lastDebounce) >= DEBOUNCE_MS;
  if (!stable) return PRESS_NONE;   // still bouncing

  // ── Falling edge: button pressed ─────────────────────────
  if (btn.lastRaw == HIGH && raw == LOW) {
    btn.pressed    = true;
    btn.pressStart = now;
    btn.longFired  = false;
    btn.lastRaw    = LOW;
    return PRESS_NONE;
  }

  // ── Rising edge: button released ─────────────────────────
  if (btn.lastRaw == LOW && raw == HIGH) {
    btn.pressed = false;
    btn.lastRaw = HIGH;
    if (!btn.longFired) {
      return PRESS_SHORT;   // released before long-press threshold
    }
    return PRESS_NONE;
  }

  // ── Held down: check for long-press threshold ─────────────
  if (btn.pressed && !btn.longFired) {
    if ((now - btn.pressStart) >= HOLD_RESET_MS) {
      btn.longFired = true;
      return PRESS_LONG;
    }
  }

  return PRESS_NONE;
}

// ─────────────────────────────────────────────────────────────
//  3×5 Mini Font (embedded — no extra library needed)
//  Each glyph = 5 bytes (rows top→bottom).
//  Each byte: bit2=left col, bit1=centre col, bit0=right col.
//  Indexed by (ASCII − 32). Covers space (32) through Z (90).
// ─────────────────────────────────────────────────────────────
static const uint8_t PROGMEM font3x5[][5] = {
  {0,0,0,0,0}, // 32  space
  {2,2,2,0,2}, // 33  !
  {5,5,0,0,0}, // 34  "
  {5,7,5,7,5}, // 35  #
  {7,6,7,3,7}, // 36  $
  {5,1,2,4,5}, // 37  %
  {6,6,7,5,7}, // 38  &
  {2,2,0,0,0}, // 39  '
  {3,2,2,2,3}, // 40  (
  {6,2,2,2,6}, // 41  )
  {5,2,7,2,5}, // 42  *
  {0,2,7,2,0}, // 43  +
  {0,0,0,2,4}, // 44  ,
  {0,0,7,0,0}, // 45  -
  {0,0,0,0,2}, // 46  .
  {1,1,2,4,4}, // 47  /
  {7,5,5,5,7}, // 48  0
  {2,6,2,2,7}, // 49  1
  {7,1,7,4,7}, // 50  2
  {7,1,7,1,7}, // 51  3
  {5,5,7,1,1}, // 52  4
  {7,4,7,1,7}, // 53  5
  {7,4,7,5,7}, // 54  6
  {7,1,1,1,1}, // 55  7
  {7,5,7,5,7}, // 56  8
  {7,5,7,1,7}, // 57  9
  {0,2,0,2,0}, // 58  :
  {0,2,0,2,4}, // 59  ;
  {1,2,4,2,1}, // 60  <
  {0,7,0,7,0}, // 61  =
  {4,2,1,2,4}, // 62  >
  {7,1,3,0,2}, // 63  ?
  {7,5,7,4,7}, // 64  @
  {2,5,7,5,5}, // 65  A
  {6,5,6,5,6}, // 66  B
  {7,4,4,4,7}, // 67  C
  {6,5,5,5,6}, // 68  D
  {7,4,7,4,7}, // 69  E
  {7,4,7,4,4}, // 70  F
  {7,4,5,5,7}, // 71  G
  {5,5,7,5,5}, // 72  H
  {7,2,2,2,7}, // 73  I
  {7,1,1,5,7}, // 74  J
  {5,5,6,5,5}, // 75  K
  {4,4,4,4,7}, // 76  L
  {5,7,5,5,5}, // 77  M
  {6,5,5,5,5}, // 78  N
  {7,5,5,5,7}, // 79  O
  {7,5,7,4,4}, // 80  P
  {7,5,5,7,1}, // 81  Q
  {7,5,6,5,5}, // 82  R
  {7,4,7,1,7}, // 83  S
  {7,2,2,2,2}, // 84  T
  {5,5,5,5,7}, // 85  U
  {5,5,5,5,2}, // 86  V
  {5,5,7,7,5}, // 87  W
  {5,5,2,5,5}, // 88  X
  {5,5,7,2,2}, // 89  Y
  {7,1,2,4,7}, // 90  Z
};

void drawMiniChar(int x, int y, char ch, int xMin, int xMax) {
  uint8_t idx = (uint8_t)ch;
  if (idx < 32 || idx > 90) idx = 32;
  for (int row = 0; row < 5; row++) {
    uint8_t bits = pgm_read_byte(&font3x5[idx - 32][row]);
    for (int col = 0; col < 3; col++) {
      int px = x + col;
      if (px < xMin || px >= xMax) continue;
      dmd.setPixel(px, y + row, ((bits >> (2 - col)) & 1) ? GRAPHICS_ON : GRAPHICS_OFF);
    }
  }
}

void drawMiniString(int x, int y, const char* str, int xMin, int xMax) {
  int cx = x;
  for (int i = 0; str[i]; i++) {
    char ch = str[i];
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    drawMiniChar(cx, y, ch, xMin, xMax);
    cx += 4;
  }
}

int miniStringWidth(const char* str) {
  int len = strlen(str);
  return (len == 0) ? 0 : len * 4 - 1;
}

// ─────────────────────────────────────────────────────────────
//  drawPanel()
//  Row map:
//    0       : padding
//    1–5     : team name  (3×5 mini font, centred)
//    6       : gap
//    7       : separator line
//    8–14    : score      (5×7 SystemFont, centred)
//    15      : padding
// ─────────────────────────────────────────────────────────────
void drawPanel(uint8_t panelIdx, const char* name, int score) {
  int xO    = panelIdx * 32;
  int xClip = xO + 30;   // stay clear of divider at col 31

  // Clear panel
  for (int x = xO; x < xO + 32; x++)
    for (int y = 0; y < 16; y++)
      dmd.setPixel(x, y, GRAPHICS_OFF);

  // Team name — centred in 30 px
  int xName = xO + max(0, (30 - miniStringWidth(name)) / 2);
  drawMiniString(xName, 1, name, xO, xClip);

  // Separator
  for (int x = xO; x < xClip; x++)
    dmd.setPixel(x, 7, GRAPHICS_ON);

  // Score — centred in 30 px
  dmd.selectFont(SystemFont5x7);
  char buf[4];
  snprintf(buf, sizeof(buf), "%02d", score);
  int xScore = xO + max(0, (30 - (int)(strlen(buf) * 6 - 1)) / 2);
  dmd.drawString(xScore, 8, buf);
}

void drawDivider() {
  for (int y = 0; y < 16; y++)
    dmd.setPixel(31, y, GRAPHICS_ON);
}

// ─────────────────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────────────────
void setup() {
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  dmd.setBrightness(200);
  dmd.begin();

  drawPanel(0, TEAM_A_NAME, scoreA);
  drawPanel(1, TEAM_B_NAME, scoreB);
  drawDivider();
}

void loop() {
  bool updateA = false;
  bool updateB = false;

  // ── Team A button ─────────────────────────────────────────
  switch (pollButton(btnA)) {
    case PRESS_SHORT:
      if (scoreA < MAX_SCORE) { scoreA++; updateA = true; }
      break;
    case PRESS_LONG:
      scoreA = 0; updateA = true;
      break;
    default: break;
  }

  // ── Team B button ─────────────────────────────────────────
  switch (pollButton(btnB)) {
    case PRESS_SHORT:
      if (scoreB < MAX_SCORE) { scoreB++; updateB = true; }
      break;
    case PRESS_LONG:
      scoreB = 0; updateB = true;
      break;
    default: break;
  }

  // ── Redraw only changed panels ───────────────────────────
  if (updateA) { drawPanel(0, TEAM_A_NAME, scoreA); drawDivider(); }
  if (updateB) { drawPanel(1, TEAM_B_NAME, scoreB); drawDivider(); }
}

/*
 * ============================================================
 *  QUICK REFERENCE
 * ============================================================
 *
 *  Button behaviour:
 *    Short press  →  score + 1
 *    Hold 2 s     →  score reset to 0
 *
 *  Change team names (uppercase, max 7 chars):
 *    const char TEAM_A_NAME[] = "HOME";
 *    const char TEAM_B_NAME[] = "AWAY";
 *
 *  Change hold duration:
 *    #define HOLD_RESET_MS  3000   // 3 seconds
 *
 *  Change max score:
 *    #define MAX_SCORE  999   ← also change "%02d" to "%03d" in drawPanel
 *
 *  Add buzzer beep on score (buzzer on A1):
 *    // after scoreA++ :  tone(A1, 1000, 80);
 *
 *  Brightness (0–255):
 *    dmd.setBrightness(150);
 * ============================================================
 */
