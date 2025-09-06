/* =======================================================================
   Alpha Electronz | alphaelectronz.tech
   Sketch: OLED DHT11 – Temp/Hum Pages (2s each)         Version: v1.0
   Board:  Arduino Nano/Uno or ESP32

   Purpose:
     Show Temperature page for 2 seconds (with thermometer + big value),
     then Humidity page for 2 seconds (wave tank + big value). No overlap.

   Hardware:
     - SSD1306 128x64 I2C OLED (addr 0x3C typical)
     - DHT11 on D2 (change DHTPIN if needed)
     - I2C: Nano/Uno -> SDA=A4, SCL=A5 | ESP32 -> SDA=21, SCL=22

   Libraries (Library Manager):
     Adafruit SSD1306, Adafruit GFX, DHT sensor library (Adafruit)
   ======================================================================= */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ----- OLED -----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----- DHT -----
#define DHTPIN   2
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

// ----- timing -----
const unsigned long PAGE_DURATION_MS = 2000; // 2s per page
const unsigned long DHT_PERIOD_MS    = 1500; // DHT11 >= 1s
unsigned long lastPageMs = 0, lastDhtMs = 0;
uint8_t currentPage = 0; // 0=temp, 1=humidity

// ----- filtering -----
float tempC = 0, hum = 0; bool haveData = false;
const float SMOOTH = 0.25f; // EMA smoothing

// ----- utils -----
float clampf(float x, float a, float b){ if(x<a) return a; if(x>b) return b; return x; }
int textWidthPx(const String& s, uint8_t size){ return s.length() * 6 * size; }

// High-contrast value box (black text on white)
void drawValueBox(int16_t x, int16_t y, const String& txt, uint8_t size){
  // y is text baseline (top-left of glyph cell). size=2 => glyph height ~16px
  int w = textWidthPx(txt, size), h = 8 * size;
  display.fillRect(x-2, y-2, w+4, h+4, SSD1306_WHITE);
  display.setTextSize(size);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(x, y);
  display.print(txt);
}

// ----- Temperature thermometer -----
void drawThermometer(int16_t x, int16_t yTop, int height, float tC){
  const float TMIN=0, TMAX=50; tC = clampf(tC, TMIN, TMAX);
  const int tubeW=12, bulbR=8;
  const int tubeH = height - bulbR - 2;
  const int xTube = x, yTube = yTop + 2;
  const int innerW = tubeW - 4;

  // outline tube + bulb
  display.drawRoundRect(xTube, yTube, tubeW, tubeH, 3, SSD1306_WHITE);
  int cx = xTube + tubeW/2;
  int cy = yTube + tubeH + bulbR - 1;
  display.drawCircle(cx, cy, bulbR, SSD1306_WHITE);
  display.drawCircle(cx, cy, bulbR-1, SSD1306_WHITE);

  // ticks every 10°C
  for(int t=0; t<=50; t+=10){
    float p = (t - TMIN) / (TMAX - TMIN);
    int yTick = yTube + tubeH - 2 - (int)((tubeH - 6) * p);
    display.drawLine(xTube + tubeW + 1, yTick, xTube + tubeW + 7, yTick, SSD1306_WHITE);
  }

  // fill mercury
  float p = (tC - TMIN) / (TMAX - TMIN);
  int fillH = (int)((tubeH - 6) * p);
  int yFillTop = yTube + tubeH - 3 - fillH;
  display.fillRect(xTube+2, yTube+2, innerW, tubeH-4, SSD1306_BLACK); // clear tube
  display.fillRect(xTube+2, yFillTop, innerW, fillH, SSD1306_WHITE);  // mercury
  display.fillCircle(cx, cy, bulbR-2, SSD1306_WHITE);
}

// ----- Humidity tank (with wave) -----
void drawHumidityTank(int16_t x, int16_t y, int w, int h, float H, float phase){
  H = clampf(H, 0, 100);
  display.drawRoundRect(x, y, w, h, 4, SSD1306_WHITE);
  int L=x+1, T=y+1, R=x+w-2, B=y+h-2;
  display.fillRect(L, T, R-L+1, h-2, SSD1306_BLACK); // clear inside
  int level = B - (int)((h-2) * (H/100.0f));
  float A = 3.2f, k = 2.0f*PI/26.0f;
  for(int xi=L; xi<=R; xi++){
    int rel = xi-L;
    float yWave = level + A * sinf(phase + rel * k);
    int yw = (int)clampf(yWave, T, B);
    display.drawLine(xi, yw, xi, B, SSD1306_WHITE);
  }
}

void setup(){
  // For ESP32 you can set Wire pins explicitly: Wire.begin(21,22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){ for(;;); }
  display.clearDisplay(); display.display();
  dht.begin();
  lastPageMs = millis();
}

void loop(){
  unsigned long now = millis();

  // Read DHT (smoothed)
  if(now - lastDhtMs >= DHT_PERIOD_MS){
    lastDhtMs = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if(!isnan(t) && !isnan(h)){
      if(!haveData){ tempC=t; hum=h; haveData=true; }
      tempC = tempC*(1.0f - SMOOTH) + t*SMOOTH;
      hum   = hum  *(1.0f - SMOOTH) + h*SMOOTH;
    }
  }

  // Switch page every 2 seconds
  if(now - lastPageMs >= PAGE_DURATION_MS){
    currentPage ^= 1; // toggle 0 <-> 1
    lastPageMs = now;
  }

  static float phase = 0; phase += 0.18f; // wave animation

  // --------------- Render current page ---------------
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if(currentPage == 0){
    // -------- TEMPERATURE PAGE --------
    display.setCursor(2, 0); display.print("TEMP");
    // Big thermometer centered-left, value box centered-right
    drawThermometer(26, 6, 52, haveData ? tempC : 0);

    String tTxt = String((int)round(haveData ? tempC : 0)) + (char)247 + "C";
    int tW = textWidthPx(tTxt, 2);
    int tX = 78 + (48 - tW)/2; if(tX < 78) tX = 78; // right column (78..127)
    drawValueBox(tX, 26, tTxt, 2); // baseline y=26 (box 20px tall fits fine)
  } else {
    // -------- HUMIDITY PAGE --------
    display.setCursor(100, 0); display.print("HUM");
    // Large tank centered, big % below it
    drawHumidityTank(20, 8, 88, 40, haveData ? hum : 0, phase);

    String hTxt = String((int)round(haveData ? hum : 0)) + "%";
    int hW = textWidthPx(hTxt, 2);
    int hX = (SCREEN_WIDTH - hW)/2; if(hX < 2) hX = 2;
    drawValueBox(hX, 52 - 14, hTxt, 2); // baseline ~38px; box bottom ~54px
  }

  display.display();
  delay(16); // ~60 FPS for smooth wave
}
