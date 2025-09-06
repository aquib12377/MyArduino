/* =======================================================================
   Alpha Electronz | alphaelectronz.tech
   Sketch: OLED Intro + Animated Welcome + Exit         Version: v1.0
   Board:  Arduino Nano/Uno or ESP32                    Date: 2025-09-05

   Purpose:
     Show a cool intro animation on 0.96" SSD1306 OLED, then display
     "Welcome to Alpha Electronz" with animations, then exit nicely.

   Hardware:
     - OLED SSD1306 128x64 I2C (addr 0x3C typical)
     - I2C pins: Nano/Uno -> SDA=A4, SCL=A5  |  ESP32 -> SDA=21, SCL=22

   Libraries (Library Manager):
     Adafruit SSD1306, Adafruit GFX

   Notes:
     - If your OLED uses 0x3D, change the address in display.begin().
     - Keep USB power stable; animation updates often.
   License: MIT • © 2025 Alpha Electronz
   ======================================================================= */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- helpers ----------------
float easeInOut(float t) {            // 0..1 → 0..1, smooth
  return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
}

int textWidthPx(const String& s, uint8_t size) {
  // default 5x7 font with 1px spacing → 6px per char
  return s.length() * 6 * size;
}

void centerPrint(const String& s, int16_t y, uint8_t size = 1) {
  int w = textWidthPx(s, size);
  int16_t x = (SCREEN_WIDTH - w) / 2;
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(s);
}

// --------------- animations ---------------
void introOrbitDots(uint16_t durationMs = 1600) {
  const int16_t cx = SCREEN_WIDTH/2, cy = SCREEN_HEIGHT/2;
  const int16_t rOuter = 22;
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    float t = (millis() - start) / 1000.0f; // seconds
    display.clearDisplay();
    // faint outer ring
    display.drawCircle(cx, cy, rOuter, SSD1306_WHITE);
    // 8 orbiting dots
    for (int i = 0; i < 8; i++) {
      float ang = t * 3.2f + i * (6.28318f / 8);   // rotate
      int x = (int)(cx + cosf(ang) * (rOuter - 4));
      int y = (int)(cy + sinf(ang) * (rOuter - 4));
      display.fillCircle(x, y, 1, SSD1306_WHITE);
    }
    // pulsing inner circle
    int rPulse = 4 + (int)(3 * (0.5f + 0.5f * sinf(t * 6)));
    display.drawCircle(cx, cy, rPulse, SSD1306_WHITE);

    display.display();
    delay(16); // ~60 FPS
  }
}

void introPulseRings() {
  const int16_t cx = SCREEN_WIDTH/2, cy = SCREEN_HEIGHT/2;
  for (int r = 3; r <= 30; r += 3) {
    display.clearDisplay();
    for (int k = r; k >= 3; k -= 6) display.drawCircle(cx, cy, k, SSD1306_WHITE);
    display.display();
    delay(40);
  }
  for (int r = 30; r >= 3; r -= 3) {
    display.clearDisplay();
    for (int k = r; k >= 3; k -= 6) display.drawCircle(cx, cy, k, SSD1306_WHITE);
    display.display();
    delay(35);
  }
}

void showWelcomeText() {
  String l1 = "Welcome to";
  String l2 = "Alpha";
  String l3 = "Electronz";

  // Move line 1 into the blue zone (rows >= 16)
  int16_t y1 = 18;        // was 10 → now below the yellow band
  int16_t y2Target = 30;  // keep spacing tidy
  int16_t y3Target = 46;  // fits within 64 px height
  int16_t yStart  = SCREEN_HEIGHT;
  const uint16_t frames = 40;

  // 1) typewriter for "Welcome to" (now blue)
  for (uint8_t i = 1; i <= l1.length(); i++) {
    display.clearDisplay();
    centerPrint(l1.substring(0, i), y1, 1);
    display.display();
    delay(55);
  }

  // 2) bounce-in "Alpha"
  for (uint16_t f = 0; f <= frames; f++) {
    float t = easeInOut((float)f / frames);
    int16_t y2 = yStart + (int16_t)((y2Target - yStart) * t);
    display.clearDisplay();
    centerPrint(l1, y1, 1);
    centerPrint(l2, y2, 2);
    display.display();
    delay(20);
  }

  // 3) bounce-in "Electronz"
  for (uint16_t f = 0; f <= frames; f++) {
    float t = easeInOut((float)f / frames);
    int16_t y3 = yStart + (int16_t)((y3Target - yStart) * t);
    display.clearDisplay();
    centerPrint(l1, y1, 1);
    centerPrint(l2, y2Target, 2);
    centerPrint(l3, y3, 2);
    display.display();
    delay(20);
  }

  // underline under Electronz
  int uw = textWidthPx(l3, 2);
  int16_t ux = (SCREEN_WIDTH - uw) / 2;
  int16_t uy = y3Target + 16;
  for (int i = 0; i < uw; i += 4) {
    display.drawLine(ux, uy, ux + i, uy, SSD1306_WHITE);
    display.display();
    delay(10);
  }

  delay(1000);
}


void exitCurtainAndFade() {
  // curtain close from sides
  for (int step = 0; step <= SCREEN_WIDTH/2; step += 4) {
    display.fillRect(0, 0, step, SCREEN_HEIGHT, SSD1306_WHITE);
    display.fillRect(SCREEN_WIDTH - step, 0, step, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
    delay(18);
  }

  // shrink a black box from center to "fade out"
  for (int w = SCREEN_WIDTH; w >= 0; w -= 6) {
    int h = map(w, 0, SCREEN_WIDTH, 0, SCREEN_HEIGHT);
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE); // white frame
    display.fillRect(x, y, w, h, SSD1306_BLACK);                        // black window
    display.display();
    delay(14);
  }
  display.clearDisplay();
  display.display();
}

// --------------- setup/loop ---------------
void setup() {
  // For ESP32 you can optionally set Wire.begin(SDA,SCL) before display.begin()
  // Wire.begin(21,22); // uncomment for ESP32 if needed

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // try 0x3D if blank
    for (;;); // halt if OLED not found
  }
  display.clearDisplay();
  display.display();

  introOrbitDots(1600);
  introPulseRings();
  showWelcomeText();
  exitCurtainAndFade();
}

void loop() {
  // idle or restart the sequence (uncomment to loop)
  delay(1000);
  introOrbitDots(1600);
  introPulseRings();
  showWelcomeText();
  exitCurtainAndFade();
}
