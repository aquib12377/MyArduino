/*
 * ============================================================
 *  Dual P10 LED Panel Counter — Arduino Nano
 * ============================================================
 *  Two P10 panels daisy-chained (series), each showing its
 *  own independent up-counter.
 *
 *  Library required:
 *    DMD2  → Install via Arduino Library Manager
 *            (search "DMD2" by Freetronics)
 *    SystemFont5x7.h is bundled with DMD2
 *
 *  Wiring (Arduino Nano → P10 Panel 1 IN connector)
 *  ─────────────────────────────────────────────────
 *  Nano D13  (SCK)   →  CLK
 *  Nano D11  (MOSI)  →  DATA (R)
 *  Nano D9           →  SCLK  (Latch)
 *  Nano D3   (PWM)   →  OE    (Output Enable, active LOW)
 *  Nano D6           →  A
 *  Nano D7           →  B
 *  Nano 5 V          →  VCC  (use external 5 V if >1 panel)
 *  Nano GND          →  GND
 *
 *  Panel 1 OUT → Panel 2 IN  (daisy-chain)
 *  Both panels share the same data lines; only DATA/CLK hop.
 *
 *  Panel layout (as seen from the front)
 *  ─────────────────────────────────────
 *  [ Panel 1 (LEFT)  ][ Panel 2 (RIGHT) ]
 *    columns 0-31          columns 32-63
 *
 *  Each panel is 32 × 16 pixels.
 *  Total canvas: 64 wide × 16 tall.
 *
 *  Counter behaviour
 *  ─────────────────
 *  Panel 1 (LEFT)  : counts  0 – 99, resets, increments every INTERVAL_1 ms
 *  Panel 2 (RIGHT) : counts  0 – 99, resets, increments every INTERVAL_2 ms
 *
 *  Adjust INTERVAL_x and MAX_COUNT to taste.
 * ============================================================
 */

#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>   // bundled with DMD2

// ── Panel configuration ───────────────────────────────────────
#define PANELS_WIDE   2   // two panels side-by-side (left → right)
#define PANELS_TALL   1   // single row of panels

SoftDMD dmd(PANELS_WIDE, PANELS_TALL);  // software-driven DMD

// ── Counter settings ─────────────────────────────────────────
const unsigned long INTERVAL_1 = 500;   // ms between ticks — Panel 1
const unsigned long INTERVAL_2 = 750;   // ms between ticks — Panel 2
const int           MAX_COUNT  = 100;   // rolls over at this value

// ── Runtime state ─────────────────────────────────────────────
int           counter1       = 0;
int           counter2       = 0;
unsigned long lastTick1      = 0;
unsigned long lastTick2      = 0;

// ── Helpers ───────────────────────────────────────────────────

/*
 * drawCounter()
 *  Draws a right-aligned number inside a 32×16 pixel panel region.
 *
 *  panelIndex : 0 = left panel, 1 = right panel
 *  value      : integer to display (0-99)
 *
 *  The function clears only the target panel's columns before
 *  drawing, so both panels are updated independently.
 */
void drawCounter(uint8_t panelIndex, int value) {
  // X origin of this panel in the overall canvas
  int xOrigin = panelIndex * 32;   // each P10 panel is 32 px wide

  // Clear the 32-column band for this panel
  for (int x = xOrigin; x < xOrigin + 32; x++) {
    for (int y = 0; y < 16; y++) {
      dmd.setPixel(x, y, GRAPHICS_OFF);
    }
  }

  // Build the string (up to 2 digits)
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", value);

  // Use the 5×7 system font (each char ~6 px wide incl. spacing)
  dmd.selectFont(SystemFont5x7);

  // Measure string width so we can centre it in the 32-px panel
  int strW = strlen(buf) * 6 - 1;          // approx pixel width
  int xText = xOrigin + (32 - strW) / 2;  // centred x
  int yText = (16 - 7) / 2;               // centred y (font is 7 px tall)

  dmd.drawString(xText, yText, buf);
}

/*
 * drawDivider()
 *  Draws a thin vertical line between the two panels (column 31)
 *  as a visual separator. Call once after setup.
 */
void drawDivider() {
  for (int y = 0; y < 16; y++) {
    dmd.setPixel(31, y, GRAPHICS_ON);
  }
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  dmd.setBrightness(255);   // 0-255; reduce if panels are too bright
  dmd.begin();

  // Initial draw
  drawCounter(0, counter1);
  drawCounter(1, counter2);
  drawDivider();
}

// ── Main loop ────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Panel 1 tick ──────────────────────────────────────────
  if (now - lastTick1 >= INTERVAL_1) {
    lastTick1 = now;
    counter1++;
    if (counter1 >= MAX_COUNT) counter1 = 0;
    drawCounter(0, counter1);
    drawDivider();   // restore divider after clear
  }

  // ── Panel 2 tick ──────────────────────────────────────────
  if (now - lastTick2 >= INTERVAL_2) {
    lastTick2 = now;
    counter2++;
    if (counter2 >= MAX_COUNT) counter2 = 0;
    drawCounter(1, counter2);
    drawDivider();   // restore divider after clear
  }
}

/*
 * ============================================================
 *  OPTIONAL EXTENSIONS — uncomment what you need
 * ============================================================
 *
 *  1. RESET BUTTON
 *     Connect a push-button between D2 and GND.
 *     Add in setup():   pinMode(2, INPUT_PULLUP);
 *     Add in loop():
 *       if (digitalRead(2) == LOW) {
 *         counter1 = 0; counter2 = 0;
 *         drawCounter(0, counter1);
 *         drawCounter(1, counter2);
 *         drawDivider();
 *         delay(200);   // debounce
 *       }
 *
 *  2. SERIAL CONTROL
 *     Add in loop():
 *       if (Serial.available()) {
 *         char cmd = Serial.read();
 *         if (cmd == 'r') { counter1=0; counter2=0; }
 *         if (cmd == '+') { counter1++; }
 *         if (cmd == '-') { counter2--; }
 *       }
 *
 *  3. BRIGHTNESS CONTROL (potentiometer on A0)
 *     Add in loop():
 *       dmd.setBrightness(map(analogRead(A0), 0, 1023, 0, 255));
 *
 *  4. LARGE SINGLE COUNTER spanning both panels
 *     Replace drawCounter calls with a single drawString at x=0
 *     and remove the divider.  Use a larger font (e.g. Arial_Black_16).
 * ============================================================
 */
