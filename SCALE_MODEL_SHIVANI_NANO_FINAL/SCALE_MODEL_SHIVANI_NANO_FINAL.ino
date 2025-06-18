/********************************************************************
 *  8-Strip Building Lights – “one-buffer” NeoPixel version
 *  ---------------------------------------------------------------
 *  Designed for an AVR-328P board (Uno / Nano) running 24 × 7.
 *  - One 73-pixel buffer is reused for all eight strips.
 *  - SRAM after setup ≈ 750 B   (safe on a 2 kB MCU)
 *  Hardware, modes, and button logic are identical to the
 *  original multi-buffer sketch dated 18-May-2025.
 ********************************************************************/
#include <Adafruit_NeoPixel.h>
#include <avr/wdt.h>

/* ───── compile-time Serial debug switch ───── */
#define DEBUG 1
#if DEBUG
  #define DBG_BEGIN()      Serial.begin(115200)
  #define DBG_PRINT(x)     Serial.print(x)
  #define DBG_PRINTLN(x)   Serial.println(x)
#else
  #define DBG_BEGIN()
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif

/* ───── hardware map ───── */
#define NUM_STRIPS        8
#define NUM_LEDS         73
const uint8_t STRIP_PINS[NUM_STRIPS] = {4,5,6,7,8,9,10,11};

const uint8_t RELAY1_PIN = 2;          // active-LOW modules (coil LOW = OFF)
const uint8_t RELAY2_PIN = 3;

const uint8_t BTN_ALLON    = A0;       // active-LOW → INPUT_PULLUP
const uint8_t BTN_PATTERNS = A2;
const uint8_t BTN_RELAYS   = A1;

#define FLOORS            36
#define LEDS_PER_FLOOR     2
#define GAP                0
#define ROOMS_PER_STRIP    8
#define FADE_STEP          2
#define MAX_BRIGHTNESS   255
#define FADE_DELAY         5
#define RUN_DELAY         45
#define INACTIVITY_MS  30000UL         // 30 s timeout to PATTERNS

#define NEO_TYPE (NEO_GRB + NEO_KHZ800)

/* ───── single NeoPixel object & helpers ───── */
Adafruit_NeoPixel pixels(NUM_LEDS, STRIP_PINS[0], NEO_TYPE);   // pin will change
inline void setPin(uint8_t pin){ pixels.setPin(pin); }         // thin wrapper

/* colour helpers */
const uint16_t YELLOW_HUE = uint16_t(65535UL * 60 / 360);      // ≈ 2700 K
inline uint32_t hsvYellow(uint8_t v,uint8_t s=128){
  return Adafruit_NeoPixel::ColorHSV(YELLOW_HUE,s,v);
}

/* ───── state machine ───── */
enum Mode : uint8_t { MODE_PATTERNS, MODE_ALL_ON, MODE_RELAYS_ONLY, MODE_PATTERNS_RELAYS };
Mode currentMode = MODE_PATTERNS;   // start in a dummy state
unsigned long lastInput   = 0;

/* forward decls */
void runPatterns();
void setMode(Mode m);
void pollButtons();

/* ───── setup ───── */
void setup(){
  //wdt_enable(WDTO_8S);   // enable the AVR watchdog, 8-second timeout
  DBG_BEGIN();
  DBG_PRINTLN(F("\n[BOOT] One-buffer controller starting…"));

  pixels.begin();
  pixels.setBrightness(100);
  pixels.clear();

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);      // relays OFF (active-LOW)
  digitalWrite(RELAY2_PIN, HIGH);

  pinMode(BTN_ALLON,    INPUT_PULLUP);
  pinMode(BTN_PATTERNS, INPUT_PULLUP);
  pinMode(BTN_RELAYS,   INPUT_PULLUP);

  randomSeed(analogRead(A3));          // some entropy

  DBG_PRINT(F("[BOOT] free RAM = "));
  DBG_PRINTLN(freeRam());
  delay(2000);
  setMode(MODE_ALL_ON);             // now this actually drives the hardware
  lastInput = millis();             // ← keep the timeout, or…

}
/* ─── put this near the top, after includes ─── */
int freeRam() {
  extern unsigned int __heap_start, *__brkval;
  unsigned int v;                      // stack pointer proxy
  return (int)&v -
         (int)(__brkval ? (unsigned int)__brkval : (unsigned int)&__heap_start);
}

/* ───── strip-render helpers ───── */
void clearAllStrips(){
  pixels.clear();
  for(uint8_t s=0;s<NUM_STRIPS;++s){
    setPin(STRIP_PINS[s]);
    pixels.show();
  }
}

void showStrip(uint8_t s){
  setPin(STRIP_PINS[s]);
  pixels.show();
}

/* draw one floor on the in-RAM buffer (no show yet) */
void drawFloorBuffer(uint8_t floor,uint32_t col){
  int start = floor * (LEDS_PER_FLOOR + GAP);
  if(start + LEDS_PER_FLOOR > NUM_LEDS) return;
  for(int i=0;i<LEDS_PER_FLOOR;++i) pixels.setPixelColor(start+i,col);
}

/* ───── pattern: alternate even/odd fade ───── */
void patternAlternateFade(){
  DBG_PRINTLN(F("[PATTERN] Alternate Fade"));
  auto renderFrame = [&](uint8_t evenB,uint8_t oddB){
    for(uint8_t s=0;s<NUM_STRIPS;++s){
      pixels.clear();
      bool stripEven = !(s&1);
      for(uint8_t f=0; f<FLOORS; ++f){
        bool floorEven = !(f&1);
        uint8_t v = (stripEven == floorEven) ? evenB : oddB;
        drawFloorBuffer(f, hsvYellow(v));
      }
      showStrip(s);
    }
  };

  for(int lvl=0; lvl<=MAX_BRIGHTNESS && currentMode==MODE_PATTERNS; ++lvl){
    renderFrame(0, MAX_BRIGHTNESS-lvl); delay(FADE_DELAY); pollButtons();
  }
  for(int lvl=0; lvl<=MAX_BRIGHTNESS && currentMode==MODE_PATTERNS; ++lvl){
    renderFrame(lvl,0); delay(FADE_DELAY); pollButtons();
  }
  for(int lvl=MAX_BRIGHTNESS; lvl>=0 && currentMode==MODE_PATTERNS; --lvl){
    renderFrame(lvl,0); delay(FADE_DELAY); pollButtons();
  }
  for(int lvl=MAX_BRIGHTNESS; lvl>=0 && currentMode==MODE_PATTERNS; --lvl){
    renderFrame(0, MAX_BRIGHTNESS-lvl); delay(FADE_DELAY); pollButtons();
  }
}

/* ───── pattern: floor chase up & down ───── */
void patternChase(){
  DBG_PRINTLN(F("[PATTERN] Chase"));
  auto renderFloor = [&](uint8_t floor){
    for(uint8_t s=0;s<NUM_STRIPS;++s){
      pixels.clear();
      drawFloorBuffer(floor, hsvYellow(MAX_BRIGHTNESS));
      showStrip(s);
    }
  };
  for(uint8_t f=0; f<FLOORS && currentMode==MODE_PATTERNS; ++f){
    renderFloor(f); delay(RUN_DELAY); pollButtons();
  }
  for(int f=FLOORS-1; f>=0 && currentMode==MODE_PATTERNS; --f){
    renderFloor(f); delay(RUN_DELAY); pollButtons();
  }
}

/* ───── pattern: random rooms breathing ───── */
static uint8_t rooms[NUM_STRIPS][ROOMS_PER_STRIP];
void newRoomSet(){
  DBG_PRINTLN(F("[PATTERN] New Random-Room set"));
  for(uint8_t s=0;s<NUM_STRIPS;++s){
    bool used[FLOORS] = {0};
    for(uint8_t i=0;i<ROOMS_PER_STRIP;++i){
      uint8_t f; do{ f = random(FLOORS); } while(used[f]);
      used[f]=true; rooms[s][i]=f;
    }
  }
}

void stepRandomRoomsFade(){
  static uint8_t brightness = 0; static int8_t dir = 1; static bool needNew = true;
  if(needNew){ newRoomSet(); needNew = false; }

  for(uint8_t s=0;s<NUM_STRIPS;++s){
    pixels.clear();
    for(uint8_t i=0;i<ROOMS_PER_STRIP;++i)
      drawFloorBuffer(rooms[s][i], hsvYellow(brightness));
    showStrip(s);
  }

  brightness += dir * FADE_STEP;
  if(brightness == 0 || brightness >= MAX_BRIGHTNESS){
    dir = -dir;
    if(brightness == 0) needNew = true;
  }
  delay(5); pollButtons();
}

/* ───── button / mode handling ───── */
void setMode(Mode m){
  if(m==currentMode) return;
  currentMode = m;

  switch(m){
    case MODE_ALL_ON:
      DBG_PRINTLN(F("[MODE] ALL-ON"));
      digitalWrite(RELAY1_PIN, LOW);
      digitalWrite(RELAY2_PIN, LOW);
      for(uint8_t s=0;s<NUM_STRIPS;++s){
        pixels.clear();
        for(uint16_t p=0;p<NUM_LEDS;++p)
          pixels.setPixelColor(p,hsvYellow(MAX_BRIGHTNESS));
        showStrip(s);
      }
      break;

    case MODE_RELAYS_ONLY:
      DBG_PRINTLN(F("[MODE] RELAYS-ONLY"));
      digitalWrite(RELAY1_PIN, LOW);
      digitalWrite(RELAY2_PIN, LOW);
      clearAllStrips();
      break;

    case MODE_PATTERNS:
    default:
      DBG_PRINTLN(F("[MODE] PATTERNS"));
      digitalWrite(RELAY1_PIN, HIGH);
      digitalWrite(RELAY2_PIN, HIGH);
      clearAllStrips();
      break;
  }
}

void pollButtons(){
  bool a0 = !digitalRead(BTN_ALLON);
  bool a1 = !digitalRead(BTN_PATTERNS);
  bool a2 = !digitalRead(BTN_RELAYS);

  if(a0){ DBG_PRINTLN(F("[BTN] ALL-ON"));       setMode(MODE_ALL_ON);      lastInput=millis(); }
  else if(a1){ DBG_PRINTLN(F("[BTN] PATTERNS")); setMode(MODE_PATTERNS);    lastInput=millis(); }
  else if(a2){ DBG_PRINTLN(F("[BTN] RELAYS"));   setMode(MODE_RELAYS_ONLY); lastInput=millis(); }

  if(millis()-lastInput>INACTIVITY_MS){
    DBG_PRINTLN(F("[TIMEOUT] → PATTERNS"));
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
    setMode(MODE_PATTERNS);
  }
}

/* ───── main loop ───── */
void loop(){
  //wdt_reset();
  pollButtons();
  
  switch(currentMode){
    case MODE_ALL_ON:
    case MODE_RELAYS_ONLY:
      delay(20);                 // fast button response
      break;

    case MODE_PATTERNS:
    default:
      runPatterns();             // patterns call pollButtons() internally
      break;
  }
}
void patternStripByStrip() {
  DBG_PRINTLN(F("[PATTERN] Strip-by-Strip Forward & Reverse"));

  // Forward direction
  for (uint8_t s = 0; s < NUM_STRIPS && currentMode == MODE_PATTERNS; ++s) {
    pixels.clear();
    for (uint8_t f = 0; f < FLOORS; ++f) {
      drawFloorBuffer(f, hsvYellow(MAX_BRIGHTNESS));
    }
    showStrip(s);
    delay(200);  // adjust as needed
    pollButtons();
  }

  // Reverse direction
  for (int s = NUM_STRIPS - 1; s >= 0 && currentMode == MODE_PATTERNS; --s) {
    pixels.clear();
    for (uint8_t f = 0; f < FLOORS; ++f) {
      drawFloorBuffer(f, hsvYellow(MAX_BRIGHTNESS));
    }
    showStrip(s);
    delay(200);  // adjust as needed
    pollButtons();
  }
}

/* sequence the three patterns */
void runPatterns(){
  patternChase();         if(currentMode!=MODE_PATTERNS) return;
  //patternStripByStrip();         if(currentMode!=MODE_PATTERNS) return;
  patternAlternateFade(); if(currentMode!=MODE_PATTERNS) return;

  unsigned long t0 = millis();
  while(currentMode==MODE_PATTERNS && millis()-t0<2000){
    stepRandomRoomsFade();
  }
}
