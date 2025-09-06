/*
  Arduino Nano 6‑Line Regulator Pattern → Speed Display on 0.96" SSD1306 OLED
  ----------------------------------------------------------------------------
  You provided these bit patterns (btn0..btn5, left→right exactly as listed):

      Speed 0 : 0 0 0 0 0 0
      Speed 1 : 1 0 0 0 0 0
      Speed 2 : 0 1 1 1 1 0
      Speed 3 : 0 1 0 1 0 1
      Speed 4 : 0 1 0 1 0 0
      Speed 5 : 0 0 1 0 1 0

  We'll read 6 digital inputs on pins D2..D7 in that order and map to 0‑5.
  Any pattern not in the table = UNKNOWN (shown as '?'; last valid speed retained internally).

  Wiring (default ACTIVE_LOW = true):
    Each regulator line/button shorts pin → GND when active.
    Pins D2..D7 each use INPUT_PULLUP, so idle = HIGH, active = LOW.

  OLED wiring (I2C):
    VCC → 5V (or 3.3V per module spec)
    GND → GND
    SDA → A4
    SCL → A5

  Libraries:
    Adafruit_GFX
    Adafruit_SSD1306

  v2.1 2025‑07‑16
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------------------- USER CONFIG --------------------
constexpr bool ACTIVE_LOW = true;                 // LOW = active (INPUT_PULLUP wiring)
constexpr uint8_t BTN_PINS[6] = {2, 3, 4, 5, 6, 7}; // btn0..btn5

constexpr uint16_t POLL_MS      = 25;             // poll interval (ms)
constexpr uint8_t  STABLE_READS = 3;              // consecutive identical reads required

// OLED params
constexpr uint8_t  OLED_ADDR = 0x3C;
constexpr int16_t  OLED_W    = 128;
constexpr int16_t  OLED_H    = 64;
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// -------------------- Pattern Table --------------------
// Encode bits into mask: bit0=btn0(D2), bit1=btn1(D3), ... bit5=btn5(D7).
// Helper macro to build mask from literal bits.
constexpr uint8_t B(uint8_t b0,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4,uint8_t b5){
  return (b0<<0) | (b1<<1) | (b2<<2) | (b3<<3) | (b4<<4) | (b5<<5);
}

struct SpeedPattern { uint8_t mask; uint8_t level; };

constexpr SpeedPattern PATTERNS[] = {
  { B(0,0,0,0,0,0), 0 }, // Speed 0
  { B(1,0,0,0,0,0), 1 }, // Speed 1
  { B(0,1,1,1,1,0), 2 }, // Speed 2
  { B(0,1,0,1,0,1), 3 }, // Speed 3
  { B(0,1,0,1,0,0), 4 }, // Speed 4
  { B(0,0,1,0,1,0), 5 }  // Speed 5
};
constexpr size_t NPAT = sizeof(PATTERNS)/sizeof(PATTERNS[0]);

// -------------------- Globals --------------------
uint8_t  lastStableMask = 0xFF; // impossible start
uint8_t  currentSpeed   = 0;    // last valid level
uint8_t  sampleMask     = 0xFF; // debounce tracker
uint8_t  sampleCount    = 0;

// -------------------- Read raw pins → 6‑bit mask --------------------
uint8_t readMaskOnce(){
  uint8_t m = 0;
  for(uint8_t i=0;i<6;i++){
    int rv = digitalRead(BTN_PINS[i]);
    bool active = (rv == (ACTIVE_LOW ? LOW : HIGH));
    if(active) m |= (1u<<i);
  }
  return m;
}

// -------------------- Debounce --------------------
bool debounceUpdate(uint8_t &stableOut){
  uint8_t m = readMaskOnce();
  if(m == sampleMask){
    if(sampleCount < 255) sampleCount++;
  }else{
    sampleMask = m;
    sampleCount = 1;
  }
  if(sampleCount >= STABLE_READS){
    stableOut = sampleMask;
    return true;
  }
  return false;
}

// -------------------- Resolve speed from mask --------------------
int8_t resolveSpeed(uint8_t m){
  for(size_t i=0;i<NPAT;i++){
    if(PATTERNS[i].mask == m) return PATTERNS[i].level;
  }
  return -1; // unknown
}

// -------------------- OLED UI --------------------
void drawUI(int8_t spd, uint8_t mask){
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(F("Speed:"));

  // Big speed (single char or '?')
  display.setTextSize(4);
  char c = (spd >= 0 && spd <= 9) ? char('0' + spd) : '?';
  char buf[2] = { c, '\0' };
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(buf, 0,0, &x1,&y1, &w,&h);
  int16_t x = (OLED_W - w)/2;
  int16_t y = 14;
  display.setCursor(x,y);
  display.print(buf);

  // Bit mask row
  display.setTextSize(1);
  display.setCursor(0,48);
  display.print(F("M:"));
  for(uint8_t i=0;i<6;i++){
    display.print((mask>>i)&1);
    if(i<5) display.print(' ');
  }

  // Bargraph: 6 segments (0..5). Filled up to current speed.
  const int barY = 58;
  const int segW = 18;
  const int segH = 4;
  const int gap  = 2;
  int segX = 1;
  for(uint8_t i=0;i<6;i++){
    if(spd >= i) display.fillRect(segX, barY, segW, segH, SSD1306_WHITE);
    else         display.drawRect(segX, barY, segW, segH, SSD1306_WHITE);
    segX += segW + gap;
  }

  display.display();
}


// -------------------- Setup --------------------
void setup(){
  Serial.begin(115200);
  Serial.println(F("6‑Line Regulator Pattern Monitor")); 

  for(uint8_t i=0;i<6;i++){
    pinMode(BTN_PINS[i], INPUT_PULLUP); // active‑LOW wiring
  }

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){
    Serial.println(F("SSD1306 init failed")); 
    for(;;);
  }
  display.clearDisplay();
  display.display();

  // Initial read
  sampleMask = readMaskOnce();
  sampleCount = STABLE_READS;
  lastStableMask = sampleMask;
  int8_t spd = resolveSpeed(sampleMask);
  if(spd >= 0) currentSpeed = (uint8_t)spd;
  drawUI(spd, sampleMask);

  Serial.print(F("Init mask="));
  for(int8_t i=5;i>=0;i--) Serial.print((sampleMask>>i)&1);
  Serial.print(F(" speed=")); Serial.println(spd);
}

// -------------------- Loop --------------------
void loop(){
  static uint32_t lastPoll = 0;
  uint32_t now = millis();
  if(now - lastPoll >= POLL_MS){
    lastPoll = now;
    uint8_t m;
    if(debounceUpdate(m)){
      if(m != lastStableMask){
        lastStableMask = m;
        int8_t spd = resolveSpeed(m);
        if(spd >= 0) currentSpeed = (uint8_t)spd; // update only if known
        Serial.print(F("Mask=")); 
        for(int8_t i=5;i>=0;i--) Serial.print((m>>i)&1);
        Serial.print(F(" -> ")); Serial.println(spd);
        drawUI(spd, m);
      }
    }
  }
}
