#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>  // uintptr_t
#include <stdarg.h>

// ===== I2C command structures =====
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
  uint8_t command;     // 1=solid, 2=animated rainbow, 3=off, 4=demo pattern
  uint8_t r, g, b;     // for solid/pattern; for rainbow: r = speed
  uint8_t brightness;  // 0..255
};

enum EffectMode : uint8_t {
  MODE_OFF     = 0,
  MODE_SOLID   = 1,
  MODE_RAINBOW = 2,
  MODE_PATTERN = 3   // new global demo pattern
};

struct StripState {
  Adafruit_NeoPixel* strip = nullptr;
  uint8_t pin = 255;
  uint16_t nled = 0;

  EffectMode mode = MODE_OFF;
  uint16_t segStart = 0;
  uint16_t segCount = 0;

  // Target/static parameters from command
  uint8_t br = 128;         // "logical" brightness from command
  uint8_t cr = 0, cg = 0, cb = 0;

  // Animation for rainbow
  uint16_t phase = 0;       // 0..65535 wraps
  uint8_t  speed = 3;       // integer speed (1..20 ok)

  // Extra state for cascading solid ON
  uint8_t  currentBr = 0;       // actual brightness used when drawing
  uint8_t  targetBr  = 128;     // where we want to end up
  uint16_t waveDelayFrames = 0; // frames to wait before starting fade
  bool     fadeActive = false;  // true while we are fading in

  bool dirty = false;  // request a redraw (e.g., new command)
};


// ===== CONFIG =====
#define I2C_SLAVE_ADDRESS 8
#define MAX_STRIPS 20
#define MAX_LEDS_PER_STRIP 150

#define DEBUG 1
const unsigned long MEM_CHECK_INTERVAL = 5000UL;
const uint16_t FRAME_INTERVAL_MS = 20;  // ~50 FPS animation
#define APPLY_ANIM_TO_FULL_STRIP 0      // 0: animate just the commanded segment; 1: whole strip

// How many animation frames to delay each floor.
// FRAME_INTERVAL_MS = 20ms, so 5 frames = 100ms per floor.
#define WAVE_FRAMES_PER_FLOOR 5         // tweak this to speed up / slow down wave
#define FADE_STEP 8                     // brightness step per frame (0..255)

// Demo pattern constants
#define ROOMS_PER_FLOOR       4         // how many "rooms" per floor (strip)
#define PATTERN_HOLD_FRAMES   15        // frames each step stays (~300ms)
#define BREATH_CYCLE_FRAMES   80        // frames for one breath up+down

// Command types
#define CMD_INIT 1
#define CMD_LED  2

#define REFUGEE_FLOOR_STRIP_INDEX 6   // 0-based => 7th floor
#define REFUGEE_LED_TAIL_COUNT    27

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

StripState S[MAX_STRIPS];

// ===== I2C buffer & flags =====
volatile bool newDataAvailable = false;
volatile int bytes_received = 0;
byte i2c_buffer[32];

// ---- Global pattern state (for command 4) ----
bool     gPatternActive = false;
uint8_t  gPatternPhase  = 0;   // 0: floor up, 1: floor down, 2: rooms, 3: breathing, 4: even/odd crossfade
uint16_t gPatternStep   = 0;
uint32_t gPatternFrame  = 0;

// ---- Forward declarations ----
static void hardClearChain(uint8_t pin, uint16_t pixels);

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

void resetStripState(StripState &st) {
  st.mode = MODE_OFF;
  st.segStart = 0;
  st.segCount = 0;

  st.br = 128;
  st.cr = st.cg = st.cb = 0;

  st.phase = 0;
  st.speed = 3;

  st.currentBr = 0;
  st.targetBr  = 128;
  st.waveDelayFrames = 0;
  st.fadeActive = false;

  st.dirty = false;
}

// ===== I2C receive ISR =====
void receiveEvent(int numBytes) {
  if (numBytes <= 0) {
    while (Wire.available()) Wire.read();
    return;
  }
  if (numBytes > (int)sizeof(i2c_buffer)) {
    numBytes = sizeof(i2c_buffer);
  }

  bytes_received = Wire.readBytes((char*)i2c_buffer, numBytes);
  newDataAvailable = true;
}

// ===== LED helpers =====
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

// Hard clear chain on a given pin up to "pixels" LEDs
static void hardClearChain(uint8_t pin, uint16_t pixels) {
  Adafruit_NeoPixel tmp(pixels, pin, NEO_GRB + NEO_KHZ800);
  tmp.begin();
  tmp.setBrightness(255);
  tmp.clear();
  tmp.show();  // send OFF to *all* 'pixels' LEDs on this pin
  delay(1);    // safety latch (>50us)
}

// ===== Command handlers =====
void handleInitCommand(const uint8_t* buf, int len) {
  if (len < (int)sizeof(I2C_Initialize_Command)) {
    debugPrint("[INIT] Packet too short: %d\n", len);
    return;
  }

  I2C_Initialize_Command cmd;
  memcpy(&cmd, buf, sizeof(cmd));

  uint8_t idx = cmd.strip_index;
  if (idx >= MAX_STRIPS) {
    debugPrint("[INIT] Invalid strip index %u\n", idx);
    return;
  }

  // Hard-clear the physical chain on that pin (idempotent)
  hardClearChain(cmd.pin, MAX_LEDS_PER_STRIP);

  bool needRecreate = false;
  uint16_t newCount = min(cmd.led_count, (uint16_t)MAX_LEDS_PER_STRIP);

  if (!S[idx].strip) {
    needRecreate = true;
  } else if (S[idx].pin != cmd.pin || S[idx].nled != newCount) {
    needRecreate = true;
  }

  if (needRecreate) {
    if (S[idx].strip) {
      // Fully clear old strip
      S[idx].strip->setBrightness(255);
      S[idx].strip->clear();
      S[idx].strip->show();
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
      resetStripState(S[idx]);
      debugPrint("[INIT] Strip %u on pin %u with %u LEDs\n", idx, S[idx].pin, S[idx].nled);
    } else {
      resetStripState(S[idx]);
      debugPrint("[INIT] Strip %u allocation failed!\n", idx);
    }
  } else {
    // Same pin & count: treat as "soft reset" for that strip
    StripState &st = S[idx];
    if (st.strip) {
      st.strip->clear();
      st.strip->show();
    }
    resetStripState(st);
    debugPrint("[INIT] Strip %u re-init (same config), soft reset\n", idx);
  }
}

void handleLedCommand(const uint8_t* buf, int len) {
  if (len < (int)sizeof(I2C_Led_Command)) {
    debugPrint("[LED] Packet too short: %d\n", len);
    return;
  }

  I2C_Led_Command cmd;
  memcpy(&cmd, buf, sizeof(cmd));

  uint8_t idx = cmd.strip_index;
  if (idx >= MAX_STRIPS || !S[idx].strip) {
    debugPrint("[LED] Strip %u not ready\n", idx);
    return;
  }

  // Any normal LED command cancels the global pattern
  if (cmd.command != 4) {
    gPatternActive = false;
  }

  StripState& st = S[idx];
  st.segStart = cmd.start_led;
  st.segCount = cmd.led_count;
  st.br       = cmd.brightness;

  switch (cmd.command) {
    case 1: {  // SOLID  (used for "turn on all lights")
      st.mode = MODE_SOLID;
      st.cr = cmd.r;
      st.cg = cmd.g;
      st.cb = cmd.b;

      // --- CASCADING FADE-IN LOGIC ---
      st.targetBr  = cmd.brightness;              // final brightness (e.g. 150)
      st.currentBr = 0;                           // start from 0
      st.fadeActive = true;
      st.waveDelayFrames = WAVE_FRAMES_PER_FLOOR; // you can multiply by idx if you want per-floor delay

      // We do NOT show immediately here; animateStrips() will fade in.
      st.dirty = true;
      debugPrint("[LED] Solid (wave) strip=%u start=%u cnt=%u RGB=(%u,%u,%u) targetBr=%u delayFrames=%u\n",
                 idx, st.segStart, st.segCount, st.cr, st.cg, st.cb,
                 st.targetBr, st.waveDelayFrames);
      break;
    }

    case 2: {  // ANIMATED RAINBOW (runs until next cmd)
      st.mode = MODE_RAINBOW;
      st.speed = cmd.r > 0 ? cmd.r : 3;  // r used as speed (1..20 recommended)
      st.phase = 0;                      // reset phase
      st.fadeActive = false;             // not used here
      st.waveDelayFrames = 0;
      st.currentBr = st.br;              // use commanded brightness directly
      st.targetBr  = st.br;

      st.strip->setBrightness(st.currentBr);
      st.dirty = true;  // animator will handle dynamic pattern
      debugPrint("[LED] Rainbow strip=%u start=%u cnt=%u speed=%u br=%u\n",
                 idx, st.segStart, st.segCount, st.speed, st.br);
      break;
    }

    case 3: {  // OFF
      st.mode = MODE_OFF;
      st.fadeActive = false;
      st.waveDelayFrames = 0;
      st.currentBr = 0;
      st.targetBr  = 0;
      st.strip->setBrightness(0);
      fillSegment(st.strip, st.segStart, st.segCount, 0);
      st.strip->show();
      st.dirty = false;
      debugPrint("[LED] Off strip=%u start=%u cnt=%u\n",
                 idx, st.segStart, st.segCount);
      break;
    }

        case 4: {  // DEMO PATTERN: floors up/down, rooms, fades
      // --- Safe defaults in case Python sends 0s ---
      uint8_t baseBr = cmd.brightness;
      if (baseBr == 0) baseBr = 150;  // default visible brightness

      uint8_t r = cmd.r;
      uint8_t g = cmd.g;
      uint8_t b = cmd.b;
      if ((r | g | b) == 0) { // all zero = black -> pick warm white
        r = 255; g = 200; b = 150;
      }

      // Initialize global pattern engine
      gPatternActive = true;
      gPatternPhase  = 0;
      gPatternStep   = 0;
      gPatternFrame  = 0;

      // Apply to ALL existing strips, not just idx
      for (uint8_t i = 0; i < MAX_STRIPS; ++i) {
        if (!S[i].strip) continue;
        StripState &ps = S[i];
        ps.mode      = MODE_PATTERN;
        ps.cr        = r;
        ps.cg        = g;
        ps.cb        = b;
        ps.br        = baseBr;
        ps.targetBr  = baseBr;
        ps.currentBr = baseBr;
        ps.waveDelayFrames = 0;
        ps.fadeActive = false;
        ps.segStart  = 0;
        ps.segCount  = ps.nled;  // use full strip for patterns
        ps.dirty     = true;
      }

      debugPrint("[LED] Demo pattern START (cmd=4) br=%u rgb=(%u,%u,%u)\n",
                 baseBr, r, g, b);
      break;
    }


    default:
      debugPrint("[LED] Unknown command=%u\n", cmd.command);
      break;
  }
}

// ===== Animator =====
void animateStrips() {
  static unsigned long lastFrame = 0;
  unsigned long now = millis();
  if (now - lastFrame < FRAME_INTERVAL_MS) return;
  lastFrame = now;

  // ----- Update global pattern state if active -----
  int maxFloor = -1;
  if (gPatternActive) {
    for (uint8_t i = 0; i < MAX_STRIPS; ++i) {
      if (S[i].strip) maxFloor = i;
    }
    if (maxFloor < 0) {
      gPatternActive = false;  // nothing to animate
    } else {
      // Advance pattern frame
      gPatternFrame++;

      switch (gPatternPhase) {
        case 0: // Floors bottom -> top
          if (gPatternFrame == 1) {
            gPatternStep = 0;
          }
          if (gPatternFrame % PATTERN_HOLD_FRAMES == 0) {
            if (gPatternStep < (uint16_t)maxFloor) {
              gPatternStep++;
            } else {
              gPatternPhase = 1;
              gPatternFrame = 0;
              gPatternStep  = maxFloor;
            }
          }
          break;

        case 1: // Floors top -> bottom
          if (gPatternFrame == 1) {
            // gPatternStep is already maxFloor from phase switch
          }
          if (gPatternFrame % PATTERN_HOLD_FRAMES == 0) {
            if (gPatternStep > 0) {
              gPatternStep--;
            } else {
              gPatternPhase = 2;
              gPatternFrame = 0;
              gPatternStep  = 0; // room index
            }
          }
          break;

        case 2: // Rooms sweep: room 0..ROOMS_PER_FLOOR-1 across all floors
          if (gPatternFrame == 1) {
            gPatternStep = 0;
          }
          if (gPatternFrame % PATTERN_HOLD_FRAMES == 0) {
            if (gPatternStep + 1 < ROOMS_PER_FLOOR) {
              gPatternStep++;
            } else {
              gPatternPhase = 3;
              gPatternFrame = 0;
            }
          }
          break;

        case 3: // Global breathing
          // Stay in this for a few cycles then move to phase 4
          if (gPatternFrame > BREATH_CYCLE_FRAMES * 4UL) { // ~4 breaths
            gPatternPhase = 4;
            gPatternFrame = 0;
          }
          break;

        case 4: // Even/odd crossfade
          if (gPatternFrame > BREATH_CYCLE_FRAMES * 4UL) {
            // loop back to phase 0 for continuous show
            gPatternPhase = 0;
            gPatternFrame = 0;
            gPatternStep  = 0;
          }
          break;

        default:
          gPatternPhase = 0;
          gPatternFrame = 0;
          gPatternStep  = 0;
          break;
      }
    }
  }

  // ----- Per-strip drawing -----
  for (uint8_t idx = 0; idx < MAX_STRIPS; ++idx) {
    StripState& st = S[idx];
    if (!st.strip) continue;

    if (gPatternActive && st.mode == MODE_PATTERN) {
      // === GLOBAL DEMO PATTERN RENDER ===
      st.strip->clear();
      uint32_t col = st.strip->Color(st.cr, st.cg, st.cb);
      uint8_t  br  = st.targetBr;

      switch (gPatternPhase) {
        case 0: { // Floors bottom -> top
          if ((int)idx == (int)gPatternStep) {
            st.strip->setBrightness(br);
            fillSegment(st.strip, 0, st.nled, col);
          }
          break;
        }
        case 1: { // Floors top -> bottom
          if ((int)idx == (int)gPatternStep) {
            st.strip->setBrightness(br);
            fillSegment(st.strip, 0, st.nled, col);
          }
          break;
        }
        case 2: { // Rooms sweep
          uint16_t total = st.nled;
          if (total > 0) {
            uint16_t roomLen = total / ROOMS_PER_FLOOR;
            if (roomLen == 0) roomLen = total; // fallback: full strip
            uint16_t rIndex = gPatternStep;
            if (rIndex >= ROOMS_PER_FLOOR) rIndex = ROOMS_PER_FLOOR - 1;
            uint16_t start = rIndex * roomLen;
            uint16_t cnt   = roomLen;
            st.strip->setBrightness(br);
            fillSegment(st.strip, start, cnt, col);
          }
          break;
        }
        case 3: { // Breathing (all floors together)
          uint16_t t = gPatternFrame % BREATH_CYCLE_FRAMES; // 0..N-1
          uint16_t half = BREATH_CYCLE_FRAMES / 2;
          uint8_t breathBr;
          if (t < half) {
            breathBr = (uint8_t)((uint32_t)br * t / half);    // ramp up
          } else {
            uint16_t d = t - half;
            breathBr = (uint8_t)((uint32_t)br * (half - d) / half); // ramp down
          }
          st.strip->setBrightness(breathBr);
          fillSegment(st.strip, 0, st.nled, col);
          break;
        }
        case 4: { // Even/odd floors crossfade
          uint16_t t = gPatternFrame % BREATH_CYCLE_FRAMES;
          uint16_t half = BREATH_CYCLE_FRAMES / 2;
          uint8_t base;
          if (t < half) {
            base = (uint8_t)((uint32_t)br * t / half);
          } else {
            uint16_t d = t - half;
            base = (uint8_t)((uint32_t)br * (half - d) / half);
          }
          uint8_t evenBr, oddBr;
          evenBr = base;
          oddBr  = br - base;

          uint8_t applyBr = (idx % 2 == 0) ? evenBr : oddBr;
          st.strip->setBrightness(applyBr);
          fillSegment(st.strip, 0, st.nled, col);
          break;
        }
      }

      st.strip->show();
      st.dirty = false;
      continue; // skip other modes when pattern is active
    }

    // === NORMAL MODES (no global pattern) ===
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
        uint8_t pos = (uint8_t)(((uint32_t)(i - start) * 256) /
                                (uint32_t)max((uint16_t)1, (uint16_t)(end - start)));
        pos += (st.phase & 0xFF);  // running shift
        st.strip->setPixelColor(i, wheel(pos));
      }
      st.strip->setBrightness(st.currentBr);  // use currentBr (same as br)
      st.strip->show();
      st.dirty = false;

    } else if (st.mode == MODE_SOLID) {
      // CASCADING FADE-IN FOR SOLID ON
      if (st.waveDelayFrames > 0) {
        st.waveDelayFrames--;
        continue;  // not started for this floor yet
      }

      if (st.fadeActive) {
        if (st.currentBr + FADE_STEP >= st.targetBr) {
          st.currentBr = st.targetBr;
          st.fadeActive = false;
        } else {
          st.currentBr += FADE_STEP;
        }
      }

      // Draw solid color at current brightness
          // Draw solid color at current brightness
    st.strip->setBrightness(st.currentBr);
    uint32_t col = st.strip->Color(st.cr, st.cg, st.cb);

    // 1) Base solid fill as commanded
    fillSegment(st.strip, st.segStart, st.segCount, col);

    // 2) If this is the WHITE command on 7th floor,
    //    force last 27 LEDs to RED (refugee area)
    if (idx == REFUGEE_FLOOR_STRIP_INDEX &&
        st.cr == 255 && st.cg == 255 && st.cb == 255)  // only when pure white
    {
      uint16_t total = st.strip->numPixels();
      if (total > 0) {
        uint16_t cnt   = (total > REFUGEE_LED_TAIL_COUNT) ? REFUGEE_LED_TAIL_COUNT : total;
        uint16_t start = total - cnt;
        uint32_t redCol = st.strip->Color(255, 0, 0);
        fillSegment(st.strip, start, cnt, redCol);
      }
    }

    st.strip->show();
    st.dirty = false;


    } else if (st.dirty) {
      // Future: handle any one-shot redraw here if needed
      st.dirty = false;
    }
  }
}

// ===== Setup & main loop =====
unsigned long lastMemCheck = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SLAVE_ADDRESS);
  // Timeout in microseconds. 30,000 = 30ms.
  Wire.setWireTimeout(30000, true);  // auto-reset TWI on stall
  Wire.onReceive(receiveEvent);

  for (uint8_t i = 0; i < MAX_STRIPS; ++i) {
    S[i] = StripState();
  }

  Serial.println(F("I2C LED Slave Ready"));
  printFreeMemory();
}

void loop() {
  // ---- Atomic snapshot of I2C buffer ----
  if (newDataAvailable) {
    uint8_t localBuf[32];
    int localLen;

    noInterrupts();
    localLen = bytes_received;
    if (localLen > 0 && localLen <= (int)sizeof(localBuf)) {
      memcpy(localBuf, i2c_buffer, localLen);
    } else {
      localLen = 0;
    }
    newDataAvailable = false;
    interrupts();

    if (localLen > 0) {
      uint8_t command_type = localBuf[0];
      switch (command_type) {
        case CMD_INIT:
          handleInitCommand(localBuf, localLen);
          break;
        case CMD_LED:
          handleLedCommand(localBuf, localLen);
          break;
        default:
          debugPrint("[I2C] Unknown cmd_type=%u len=%d\n", command_type, localLen);
          break;
      }
    }
  }

  animateStrips();

  unsigned long now = millis();
  if (now - lastMemCheck >= MEM_CHECK_INTERVAL) {
    lastMemCheck = now;
    printFreeMemory();
  }
}
