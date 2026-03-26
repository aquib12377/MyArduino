/*
 * ============================================================
 *  Dual P10 LED Scoreboard — Arduino Nano  (v2 — Mini Font)
 * ============================================================
 *  Team name uses a built-in 3×5 pixel font (no library needed
 *  for text, only DMD2 for panel driving).
 *  Score uses the standard SystemFont5x7.
 *
 *  Panel Layout (front view):
 *  ┌────────────────┬────────────────┐
 *  │   TEAM A       │   TEAM B       │  ← 3×5 mini font, rows 1–5
 *  │   ─────        │   ─────        │  ← separator line, row 7
 *  │     00         │     00         │  ← 5×7 font, rows 8–14
 *  └────────────────┴────────────────┘
 *
 *  ── Wiring: Nano → P10 Panel 1 IN ──────────────────────────
 *  D13 (SCK)  →  CLK          D9   →  SCLK (Latch)
 *  D11 (MOSI) →  DATA         D3   →  OE   (PWM, active LOW)
 *  D6         →  A            D7   →  B
 *  5 V        →  VCC  (use external 5 V supply for panels!)
 *  GND        →  GND
 *  Panel 1 OUT → Panel 2 IN  (daisy-chain)
 *
 *  ── Button Wiring (pin → GND, internal pull-up) ─────────────
 *  D2  Team A  +1
 *  D4  Team A  −1   (hold 2 s → reset to 0)
 *  D5  Team B  +1
 *  D8  Team B  −1   (hold 2 s → reset to 0)
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
#define BTN_A_INC  2
#define BTN_A_DEC  4
#define BTN_B_INC  5
#define BTN_B_DEC  8

// ── Tuning ────────────────────────────────────────────────────
#define MAX_SCORE      99
#define DEBOUNCE_MS    50
#define HOLD_RESET_MS  2000

// ── Team names ────────────────────────────────────────────────
// 3×5 font: each char is 3 px wide + 1 px gap = 4 px/char.
// Max chars that fit in 32 px = (32+1)/4 = 8 chars.
// Use uppercase only. Space and hyphen (-) are supported.
const char TEAM_A_NAME[] = "TEAM A";
const char TEAM_B_NAME[] = "TEAM B";

// ── Scores ────────────────────────────────────────────────────
int scoreA = 0;
int scoreB = 0;

// ─────────────────────────────────────────────────────────────
//  3×5 Mini Font
//  Each glyph = 5 bytes (one per row, top→bottom).
//  Each byte: bit2=left col, bit1=centre col, bit0=right col.
//  Array indexed by (ASCII code − 32). Covers 0x20–0x5A.
// ─────────────────────────────────────────────────────────────
static const uint8_t PROGMEM font3x5[][5] = {
  {0,0,0,0,0}, // 32  (space)
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

// ─────────────────────────────────────────────────────────────
//  drawMiniChar()
//  Draws one 3×5 glyph at pixel position (x, y).
//  Pixels outside [xMin, xMax) are silently clipped.
// ─────────────────────────────────────────────────────────────
void drawMiniChar(int x, int y, char ch, int xMin, int xMax) {
  uint8_t idx = (uint8_t)ch;
  if (idx < 32 || idx > 90) idx = 32;    // out-of-range → space
  for (int row = 0; row < 5; row++) {
    uint8_t bits = pgm_read_byte(&font3x5[idx - 32][row]);
    for (int col = 0; col < 3; col++) {
      int px = x + col;
      if (px < xMin || px >= xMax) continue;   // clip
      bool on = (bits >> (2 - col)) & 1;
      dmd.setPixel(px, y + row, on ? GRAPHICS_ON : GRAPHICS_OFF);
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  drawMiniString()
//  Draws a string using the 3×5 font (4 px/char: 3 wide + 1 gap).
//  Converts lowercase to uppercase automatically.
//  Clips to [xMin, xMax).
// ─────────────────────────────────────────────────────────────
void drawMiniString(int x, int y, const char* str, int xMin, int xMax) {
  int cx = x;
  for (int i = 0; str[i]; i++) {
    char ch = str[i];
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // to uppercase
    drawMiniChar(cx, y, ch, xMin, xMax);
    cx += 4;   // 3 px glyph + 1 px gap
  }
}

// ─────────────────────────────────────────────────────────────
//  miniStringWidth()  — pixel width of string in 3×5 font
// ─────────────────────────────────────────────────────────────
int miniStringWidth(const char* str) {
  int len = strlen(str);
  if (len == 0) return 0;
  return len * 4 - 1;   // n chars × 4 px − 1 trailing gap
}

// ─────────────────────────────────────────────────────────────
//  drawPanel()
//  Clears a 32×16 region and draws team name + score.
//
//  Row map (pixels):
//    0       : top padding
//    1–5     : team name  (3×5 mini font)
//    6       : blank gap
//    7       : thin separator line
//    8–14    : score      (5×7 SystemFont)
//    15      : bottom padding
// ─────────────────────────────────────────────────────────────
void drawPanel(uint8_t panelIdx, const char* name, int score) {
  int xO   = panelIdx * 32;   // left edge of this panel
  int xEnd = xO + 31;         // rightmost column (31 or 63)
  //  Note: column xO+31 on the LEFT panel IS the divider column,
  //  so we clip text to xO+30 to avoid overwriting it.
  int xClip = xO + 30;       // exclusive clip boundary

  // ── Clear panel region ──────────────────────────────────────
  for (int x = xO; x <= xEnd; x++)
    for (int y = 0; y < 16; y++)
      dmd.setPixel(x, y, GRAPHICS_OFF);

  // ── Team name (3×5 font, centred, row y=1) ──────────────────
  int nameW  = miniStringWidth(name);
  int xName  = xO + max(0, (30 - nameW) / 2);  // centre within 30 usable px
  drawMiniString(xName, 1, name, xO, xClip);

  // ── Separator line (row y=7) ─────────────────────────────────
  for (int x = xO; x < xClip; x++)
    dmd.setPixel(x, 7, GRAPHICS_ON);

  // ── Score (5×7 font, centred, row y=8) ──────────────────────
  dmd.selectFont(SystemFont5x7);
  char buf[4];
  snprintf(buf, sizeof(buf), "%02d", score);
  int scoreW  = strlen(buf) * 6 - 1;
  int xScore  = xO + max(0, (30 - scoreW) / 2);
  dmd.drawString(xScore, 8, buf);
}

// ─────────────────────────────────────────────────────────────
//  drawDivider()  — bright vertical line between panels
// ─────────────────────────────────────────────────────────────
void drawDivider() {
  for (int y = 0; y < 16; y++)
    dmd.setPixel(31, y, GRAPHICS_ON);
}

// ─────────────────────────────────────────────────────────────
//  Button helpers
// ─────────────────────────────────────────────────────────────
struct Button {
  uint8_t       pin;
  bool          lastState;
  unsigned long pressTime;
  bool          holdFired;
  unsigned long lastDebounce;
};

Button btnAInc = { BTN_A_INC, HIGH, 0, false, 0 };
Button btnADec = { BTN_A_DEC, HIGH, 0, false, 0 };
Button btnBInc = { BTN_B_INC, HIGH, 0, false, 0 };
Button btnBDec = { BTN_B_DEC, HIGH, 0, false, 0 };

// Returns true once on falling edge (debounced)
bool readPress(Button& btn) {
  bool raw = digitalRead(btn.pin);
  unsigned long now = millis();
  if (raw != btn.lastState) btn.lastDebounce = now;
  if ((now - btn.lastDebounce) > DEBOUNCE_MS) {
    if (raw == LOW && btn.lastState == HIGH) {
      btn.pressTime = now;
      btn.holdFired = false;
      btn.lastState = raw;
      return true;
    }
    btn.lastState = raw;
  }
  return false;
}

// Returns true once after button held for HOLD_RESET_MS
bool readHold(Button& btn) {
  if (digitalRead(btn.pin) == LOW && !btn.holdFired) {
    if ((millis() - btn.pressTime) >= HOLD_RESET_MS) {
      btn.holdFired = true;
      return true;
    }
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────────────────
void setup() {
  pinMode(BTN_A_INC, INPUT_PULLUP);
  pinMode(BTN_A_DEC, INPUT_PULLUP);
  pinMode(BTN_B_INC, INPUT_PULLUP);
  pinMode(BTN_B_DEC, INPUT_PULLUP);

  dmd.setBrightness(200);
  dmd.begin();

  drawPanel(0, TEAM_A_NAME, scoreA);
  drawPanel(1, TEAM_B_NAME, scoreB);
  drawDivider();
}

void loop() {
  bool updateA = false;
  bool updateB = false;

  if (readPress(btnAInc))              { if (scoreA < MAX_SCORE) { scoreA++; updateA = true; } }
  if (readPress(btnADec))              { if (scoreA > 0)         { scoreA--; updateA = true; } }
  if (readHold(btnADec))               { scoreA = 0;               updateA = true; }

  if (readPress(btnBInc))              { if (scoreB < MAX_SCORE) { scoreB++; updateB = true; } }
  if (readPress(btnBDec))              { if (scoreB > 0)         { scoreB--; updateB = true; } }
  if (readHold(btnBDec))               { scoreB = 0;               updateB = true; }

  if (updateA) { drawPanel(0, TEAM_A_NAME, scoreA); drawDivider(); }
  if (updateB) { drawPanel(1, TEAM_B_NAME, scoreB); drawDivider(); }
}

/*
 * ============================================================
 *  QUICK REFERENCE
 * ============================================================
 *
 *  Change team names (up to 7 uppercase chars):
 *    const char TEAM_A_NAME[] = "HOME";
 *    const char TEAM_B_NAME[] = "AWAY";
 *
 *  Brightness (0–255):
 *    dmd.setBrightness(150);
 *
 *  Add reset-all button on A0:
 *    pinMode(A0, INPUT_PULLUP);
 *    // in loop():
 *    if (digitalRead(A0) == LOW) {
 *      scoreA = scoreB = 0;
 *      drawPanel(0, TEAM_A_NAME, scoreA);
 *      drawPanel(1, TEAM_B_NAME, scoreB);
 *      drawDivider();
 *      delay(200);
 *    }
 *
 *  Add buzzer on A1 (score beep):
 *    // after scoreA++ :  tone(A1, 1000, 80);
 * ============================================================
 */
