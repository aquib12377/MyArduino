#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>  // uintptr_t

// ===== CONFIG =====
#define I2C_SLAVE_ADDRESS 8
#define MAX_STRIPS 8
#define MAX_LEDS_PER_STRIP 100

#define DEBUG 1
const unsigned long MEM_CHECK_INTERVAL = 5000UL;
const uint16_t FRAME_INTERVAL_MS = 20;  // ~50 FPS animation
#define APPLY_ANIM_TO_FULL_STRIP 0      // 0: animate just the commanded segment; 1: whole strip

// ===== Debug helpers =====
void debugPrint(const char* fmt, ...) {
#if DEBUG
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
#endif
}

int freeMemory() {
  extern unsigned int __heap_start;
  extern void* __brkval;
  int v;
  uintptr_t heapend = (__brkval == 0) ? (uintptr_t)&__heap_start : (uintptr_t)__brkval;
  uintptr_t stackptr = (uintptr_t)&v;
  return (stackptr > heapend) ? (int)(stackptr - heapend) : 0;
}

void printFreeMemory() {
#if DEBUG
  debugPrint("--- Free memory: %d bytes ---\n", freeMemory());
#endif
}

struct __attribute__((packed)) I2C_Initialize_Command {
  uint8_t command_type;  // 1
  uint8_t strip_index;
  uint8_t pin;
  uint16_t led_count;
};

struct __attribute__((packed)) I2C_Led_Command {
  uint8_t command_type;  // 2
  uint8_t strip_index;
  uint16_t start_led;
  uint16_t led_count;
  uint8_t command;     // 1=solid, 2=animated rainbow, 3=off
  uint8_t r, g, b;     // for solid; for rainbow: r = speed (1..10 typical)
  uint8_t brightness;  // 0..255
};

enum EffectMode : uint8_t { MODE_OFF = 0,
                            MODE_SOLID = 1,
                            MODE_RAINBOW = 2 };

struct StripState {
  Adafruit_NeoPixel* strip = nullptr;
  uint8_t pin = 255;
  uint16_t nled = 0;

  EffectMode mode = MODE_OFF;
  uint16_t segStart = 0;
  uint16_t segCount = 0;
  uint8_t br = 128;

  uint8_t cr = 0, cg = 0, cb = 0;

  uint16_t phase = 0;  // 0..65535 wraps
  float speed = 0.1;   // steps per frame (1..20 ok)

  bool dirty = false;  // request a redraw (e.g., new command)
};

StripState S[MAX_STRIPS];

volatile bool newDataAvailable = false;
volatile int bytes_received = 0;
byte i2c_buffer[32];

static inline uint32_t wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) return Adafruit_NeoPixel::Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) {
    pos -= 85;
    return Adafruit_NeoPixel::Color(0, pos * 3, 255 - pos * 3);
  }
  pos -= 170;
  return Adafruit_NeoPixel::Color(pos * 3, 255 - pos * 3, 0);
}

void receiveEvent(int numBytes) {
  if (numBytes <= 0 || numBytes > (int)sizeof(i2c_buffer)) {
    while (Wire.available()) Wire.read();
    return;
  }
  bytes_received = Wire.readBytes((char*)i2c_buffer, numBytes);
  newDataAvailable = true;
}

void fillSegment(Adafruit_NeoPixel* strip, uint16_t start, uint16_t cnt, uint32_t color) {
  if (!strip) return;
  uint16_t total = strip->numPixels();
  uint16_t end = start + cnt;
  if (start >= total) return;
  if (end > total) end = total;
  for (uint16_t i = start; i < end; ++i) {
    strip->setPixelColor(i, color);
  }
}

void handleInitCommand() {
  I2C_Initialize_Command cmd;
  memcpy(&cmd, i2c_buffer, (size_t)min(bytes_received, (int)sizeof(cmd)));

  uint8_t idx = cmd.strip_index;
  if (idx >= MAX_STRIPS) return;

  hardClearChain(cmd.pin, MAX_LEDS_PER_STRIP);

  bool needRecreate = false;
  uint16_t newCount = min(cmd.led_count, (uint16_t)MAX_LEDS_PER_STRIP);

  if (!S[idx].strip) needRecreate = true;
  else if (S[idx].pin != cmd.pin || S[idx].nled != newCount) needRecreate = true;

  if (needRecreate) {
    if (S[idx].strip) {
      S[idx].strip->setBrightness(255);  // ensure strong write-out
      S[idx].strip->clear();
      S[idx].strip->show();  // physically turn off *all* previously-addressed LEDs
      delete S[idx].strip;
      S[idx].strip = nullptr;
    }

    S[idx].nled = newCount;
    S[idx].pin = cmd.pin;
    S[idx].strip = new Adafruit_NeoPixel(S[idx].nled, S[idx].pin, NEO_GRB + NEO_KHZ800);

    if (S[idx].strip) {
      S[idx].strip->begin();
      S[idx].strip->clear();
      S[idx].strip->show();

      S[idx].mode = MODE_OFF;
      S[idx].dirty = false;
      S[idx].segStart = 0;
      S[idx].segCount = 0;
      debugPrint("[INIT] Strip %u on pin %u with %u LEDs\n", idx, S[idx].pin, S[idx].nled);
    } else {
      debugPrint("[INIT] Strip %u allocation failed!\n", idx);
    }
  }
}
static void hardClearChain(uint8_t pin, uint16_t pixels) {
  Adafruit_NeoPixel tmp(pixels, pin, NEO_GRB + NEO_KHZ800);
  tmp.begin();
  tmp.setBrightness(255);
  tmp.clear();
  tmp.show();  // send OFF to *all* 'pixels' LEDs on this pin
  delay(1);    // safety latch (>50us)
}


void handleLedCommand() {
  I2C_Led_Command cmd;
  memcpy(&cmd, i2c_buffer, (size_t)min(bytes_received, (int)sizeof(cmd)));

  uint8_t idx = cmd.strip_index;
  if (idx >= MAX_STRIPS || !S[idx].strip) {
    debugPrint("[LED] Strip %u not ready\n", idx);
    return;
  }

  StripState& st = S[idx];
  st.br = cmd.brightness;
  st.segStart = cmd.start_led;
  st.segCount = cmd.led_count;

  switch (cmd.command) {
    case 1:
      {  // SOLID
        st.mode = MODE_SOLID;
        st.cr = cmd.r;
        st.cg = cmd.g;
        st.cb = cmd.b;
        st.strip->setBrightness(st.br);
        fillSegment(st.strip, st.segStart, st.segCount, st.strip->Color(st.cr, st.cg, st.cb));
        st.strip->show();
        st.dirty = false;  // fully applied
        debugPrint("[LED] Solid strip=%u start=%u cnt=%u RGB=(%u,%u,%u) br=%u\n",
                   idx, st.segStart, st.segCount, st.cr, st.cg, st.cb, st.br);
        break;
      }
    case 2:
      {  // ANIMATED RAINBOW (runs until next cmd)
        st.mode = MODE_RAINBOW;
        st.speed = cmd.r > 0 ? cmd.r : 3;  // r used as speed (1..20 recommended)
        st.phase = 0;                      // reset phase (or keep to continue)
        st.strip->setBrightness(st.br);
        st.dirty = true;  // request redraw in animator
        debugPrint("[LED] Rainbow strip=%u start=%u cnt=%u speed=%u br=%u\n",
                   idx, st.segStart, st.segCount, st.speed, st.br);
        break;
      }
    case 3:
      {  // OFF
        st.mode = MODE_OFF;
        fillSegment(st.strip, st.segStart, st.segCount, 0);
        st.strip->show();
        st.dirty = false;
        debugPrint("[LED] Off strip=%u start=%u cnt=%u\n", idx, st.segStart, st.segCount);
        break;
      }
    default:
      debugPrint("[LED] Unknown command=%u\n", cmd.command);
      break;
  }
}

void animateStrips() {
  static unsigned long lastFrame = 0;
  unsigned long now = millis();
  if (now - lastFrame < FRAME_INTERVAL_MS) return;
  lastFrame = now;

  for (uint8_t idx = 0; idx < MAX_STRIPS; ++idx) {
    StripState& st = S[idx];
    if (!st.strip) continue;

    if (st.mode == MODE_RAINBOW) {
      st.phase += st.speed;

      uint16_t total = st.strip->numPixels();
      uint16_t start = st.segStart;
      uint16_t cnt = st.segCount;

#if APPLY_ANIM_TO_FULL_STRIP
      start = 0;
      cnt = total;
#endif

      if (start >= total) continue;
      uint16_t end = start + cnt;
      if (end > total) end = total;

      for (uint16_t i = start; i < end; ++i) {
        uint8_t pos = (uint8_t)(((uint32_t)(i - start) * 256) / (uint32_t)max((uint16_t)1, (uint16_t)(end - start)));
        pos += (st.phase & 0xFF);  // running shift
        st.strip->setPixelColor(i, wheel(pos));
      }
      st.strip->setBrightness(st.br);
      st.strip->show();
    } else if (st.dirty) {
      st.dirty = false;
    }
  }
}

unsigned long lastMemCheck = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.setWireTimeout(30000, true);  // 30 ms timeout, auto-reset TWI on stall
  Wire.onReceive(receiveEvent);

  for (uint8_t i = 0; i < MAX_STRIPS; ++i) S[i] = StripState();

  Serial.println(F("I2C LED Slave Ready"));
  printFreeMemory();
}

void loop() {
  if (newDataAvailable) {
    noInterrupts();
    newDataAvailable = false;
    interrupts();

    uint8_t command_type = i2c_buffer[0];
    if (command_type == 1) handleInitCommand();
    else if (command_type == 2) handleLedCommand();
  }
  animateStrips();
  unsigned long now = millis();
  if (now - lastMemCheck >= MEM_CHECK_INTERVAL) {
    lastMemCheck = now;
    printFreeMemory();
  }
}