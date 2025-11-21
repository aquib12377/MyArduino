/**
 * Building_LED_Controller_I2C.ino
 * Arduino Mega 2560 for 11 floors WS2812B + I2C slave
 * Pattern command now runs a 4-part non-blocking sequencer (smooth & interruptible).
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>

#ifndef S4_TOTAL_MS
#define S4_TOTAL_MS 20000UL  // How long to stay in Stage-4 before looping back
#endif

static uint32_t s4_phaseStart = 0;    // stage-4 start time
static bool g_seqLoopEnabled = true;  // true => loop Stage1→2→4→repeat

struct RGB {
  uint8_t r, g, b;
};
// Use your existing GROUP_SIZE (4) – tasks operate on 4-LED patches
struct FadeTask {
  int floor;      // which floor
  int start;      // start index (multiple of GROUP_SIZE)
  uint8_t value;  // 0..255 brightness
  int8_t step;    // +step when rising, -step when falling
};
const int floorPins[] = { 25, 27, 29, 47, 33, 35, 37, 39, 41, 43, 45 };
const int NUM_FLOORS = 11;
const int LEDS_PER_FLOOR = 175;
const int BRIGHTNESS = 120;
Adafruit_NeoPixel floors[NUM_FLOORS];

const int room_led_counts[] = { 25, 22, 8, 24, 21, 18, 17, 25, 16 };  //{ 21,17,16,25,9,24,8,22,28 }; -- old values
const int NUM_ROOMS = sizeof(room_led_counts) / sizeof(room_led_counts[0]);
int room_start_indices[NUM_ROOMS];
const char* room_names[] = { "Room 1", "Room 2", "Room 3", "Room 4", "Duct 1", "Room 5", "Duct 2", "Room 6", "Room 7" };

// ---------- Wing / Room mapping ----------
//{ 25, 22, 8, 24, 21, 18, 17, 25, 12 };
enum RoomIdx : uint8_t {
  IDX_ROOM1 = 0,
  IDX_ROOM2 = 1,
  IDX_ROOM3 = 3,
  IDX_ROOM4 = 4,
  IDX_DUCT1 = 2,
  IDX_ROOM5 = 5,
  IDX_DUCT2 = 8,
  IDX_ROOM6 = 6,
  IDX_ROOM7 = 7
};

// Wing groups (what to light per ID)
const int WING2_ROOMS[] = { IDX_ROOM1, IDX_ROOM2, IDX_ROOM3 };             // id=1 → Rooms 1,2,3
const int WING1_ROOMS[] = { IDX_ROOM4, IDX_ROOM5, IDX_ROOM6, IDX_ROOM7 };  // id=2 → Rooms 4,5,6,7

// 30s idle → start pattern
static const unsigned long IDLE_PATTERN_MS = 60000UL;
static unsigned long g_lastCmdAt = 0;  // updated whenever a valid I2C/legacy cmd is handled

// ---------- Stage 4 (Random Room Fading) tunables ----------
static const uint16_t S4_FRAME_MS = 16;       // ~60 FPS
static const uint16_t S4_RESELECT_MS = 2500;  // how often to pick a new 80/20 set
static const uint8_t S4_PULSE_STEP = 2;       // up/down step for pulsing rooms
static const uint8_t S4_FADE_STEP = 1;        // smooth fade to 0 for 20% rooms

enum { S4_GROUP_SZ = 5 };
enum { S4_GROUPS_PER_FLOOR = (LEDS_PER_FLOOR + S4_GROUP_SZ - 1) / S4_GROUP_SZ };  // ceil

enum RMode : uint8_t { RM_OFF = 0,
                       RM_PULSE = 1,
                       RM_FADEOUT = 2 };

// Per-floor, per-group state
static uint8_t s4_mode[NUM_FLOORS][S4_GROUPS_PER_FLOOR];
static uint8_t s4_val[NUM_FLOORS][S4_GROUPS_PER_FLOOR];
static int8_t s4_step[NUM_FLOORS][S4_GROUPS_PER_FLOOR];
static uint32_t s4_nextFrameAt = 0;
static uint32_t s4_reselectAt = 0;

// Color used for the room fading


inline void markCommandSeen() {
  g_lastCmdAt = millis();
}

// ================= I2C CONFIG =================
#define MEGA_I2C_ADDR 8


// ---- New I2C opcodes (match ESP32 master) ----
enum : uint8_t {
  CMD_PATTERN = 0x10,  // start sequencer (4 patterns)
  CMD_ALL_ON = 0x11,
  CMD_ALL_OFF = 0x12,
  CMD_BHK = 0x13,  // arg1: 3 or 4
  CMD_WING_SELECT = 0x14,
  CMD_WING_CLICK = 0x15
};

// ---- Legacy single-byte (fallback) ----
enum : uint8_t {
  LEGACY_CMD_OFF = 0x00,
  LEGACY_CMD_PATTERN_RAINBOW = 0x01,  // (now mapped to sequencer)
  LEGACY_CMD_BHK3 = 0x02,
  LEGACY_CMD_BHK4 = 0x03,
  LEGACY_CMD_ALL_ON = 0x04
};

// ---- RX packet (8 bytes) ----
struct RxPacket {
  uint8_t pre;  // 0xAA
  uint8_t cmd;
  uint8_t a;
  uint8_t b;
  uint8_t c;
  uint8_t d;
  uint8_t sum;   // cmd+a+b+c+d
  uint8_t post;  // 0x55
};

volatile bool g_hasPkt = false;
volatile uint8_t g_isrRaw[8];
volatile bool g_hasLegacy = false;
volatile uint8_t g_legacyCmd = 0;

// Tunables (speed/feel)
static const uint16_t DELAY_FLOOR_STEP_MS = 250;  // Pattern1 step (instant fill)
static const uint16_t DELAY_GROUP_STEP_MS = 100;  // Pattern2 group-of-4 step
static const uint16_t RANDOM_CYCLE_MS = 1400;     // Pattern3 cycle period
static const uint8_t RANDOM_CYCLES_TOTAL = 12;    // Pattern3 total cycles
static const uint8_t GROUP_SIZE = 4;              // "group of 4" granularity
static const uint16_t FADE_FLOOR_MS = 380;        // Pattern4 single-floor fade-in
static const uint8_t PULSE_MIN = 60;              // Pattern3 min intensity (0..255)
static const uint8_t PULSE_MAX = 255;             // Pattern3 max intensity (0..255)

void onI2CRecv(int howMany) {
  uint8_t buf[16];
  int n = 0;
  while (Wire.available() && n < (int)sizeof(buf)) buf[n++] = (uint8_t)Wire.read();
  if (n >= 8) {
    for (int i = 0; i <= n - 8; i++) {
      if (buf[i] == 0xAA && buf[i + 7] == 0x55) {
        uint8_t sum = (uint8_t)(buf[i + 1] + buf[i + 2] + buf[i + 3] + buf[i + 4] + buf[i + 5]);
        if (sum == buf[i + 6]) {
          for (int k = 0; k < 8; k++) g_isrRaw[k] = buf[i + k];
          g_hasPkt = true;
          return;
        }
      }
    }
  } else if (n == 1) {
    g_legacyCmd = buf[0];
    g_hasLegacy = true;
  }
}

// =============== LED CONFIG ===================




// ===== Colors & helpers (centralized) =====


// Base colors (edit freely)
static const RGB C_WHITE = { 233, 220, 201 };
static const RGB C_TEAL = { 0, 150, 150 };            // patternOneFloorAtATime
static const RGB C_MAGENTA = { 150, 0, 150 };         // patternOneRoomOfEachFloorAtATime
static const RGB C_BHK3 = { 200, 149, 130 };          // 3BHK main color
static const RGB C_BHK4 = { 122, 211, 255 };          // 4BHK main color
static const RGB C_DUCT = { 200, 200, 80 };           // duct accent (you used 255,0,150 earlier)
static const RGB C_GOLD = { 255, 215, 180 };          // soft golden for fading tasks
static const RGB C_STAGE12 = { 255, 255, 255 };       // Stage 1/2 fill color
static const RGB C_STAGE4 = { 255, 255, 255 };        // Stage 4 fade-in target
static const RGB C_FLOORBYFLOOR = { 251, 180, 250 };  // Stage 4 fade-in target
static const RGB C_ROOMBYROOM = { 164, 135, 255 };    // Stage 4 fade-in target
static const RGB C_ROOM_FADE = C_GOLD;                // tweak if you want a different tint

// Helpers
inline uint32_t rgb32(const RGB& c) {
  return floors[0].Color(c.r, c.g, c.b);
}
inline uint32_t rgbScale(const RGB& c, uint8_t k) {
  return floors[0].Color((uint8_t)((c.r * k) / 255), (uint8_t)((c.g * k) / 255), (uint8_t)((c.b * k) / 255));
}


// --- Global state ---
int activePattern = 0;  // 0 = none, 7 = sequencer
unsigned long lastPatternStep = 0;

// === Forward decls (your existing helpers kept) ===
void printMenu();
void clearAllLeds();
void patternOneFloorAtATime();
void patternOneRoomOfEachFloorAtATime();
void lightUp3BHKs();
void lightUp4BHKs();
void turnOnAllLights();
void lightUpDuctsPermanently();
uint32_t wheel(byte);

// =============================================================================
//                              PATTERN SEQUENCER
// =============================================================================
// ---- Stage 3: Soft golden fading tasks (patternSoftColorsSmooth-like) ----
// We animate many small "breathing" patches across all floors, non-blocking.

static const uint8_t FADE_UP_STEP = 5;      // brightness step up
static const uint8_t FADE_DOWN_STEP = 5;    // brightness step down
static const uint16_t S3_FRAME_MS = 16;     // ~60 FPS
static const uint16_t S3_CYCLE_MS = 1400;   // one "cycle" window
static const uint8_t S3_CYCLES_TOTAL = 12;  // total cycles before moving to Stage 4


static const int NUM_FADE_TASKS = 160;  // tune to taste
static FadeTask s3_tasks[NUM_FADE_TASKS];

// Make a random task (spawn at low brightness starting to rise)
static FadeTask s3_makeTask() {
  FadeTask t;
  t.floor = random(NUM_FLOORS);
  int groups = (LEDS_PER_FLOOR + GROUP_SIZE - 1) / GROUP_SIZE;
  int g = random(groups);
  t.start = g * GROUP_SIZE;
  t.value = 0;
  t.step = +FADE_UP_STEP;
  return t;
}

static void s3_initTasks() {
  for (int i = 0; i < NUM_FADE_TASKS; i++) s3_tasks[i] = s3_makeTask();
}

static void s3_tickTasks() {
  // Update brightness & draw patches
  // (We do not clear; patches themselves fade down to 0)
  for (int i = 0; i < NUM_FADE_TASKS; i++) {
    FadeTask& t = s3_tasks[i];

    // Update brightness with hysteresis
    int v = (int)t.value + (int)t.step;
    if (t.step > 0) {  // rising
      if (v >= 255) {
        v = 255;
        t.step = -FADE_DOWN_STEP;
      }
    } else {  // falling
      if (v <= 0) {
        v = 0;
        t = s3_makeTask();
      }  // respawn elsewhere after full fade-out
    }
    t.value = (uint8_t)v;

    // Draw the golden-scaled color on its GROUP_SIZE region
    if (t.floor >= 0 && t.floor < NUM_FLOORS) {
      int cnt = min(GROUP_SIZE, LEDS_PER_FLOOR - t.start);
      uint32_t col = rgbScale(C_GOLD, t.value);
      floors[t.floor].fill(col, t.start, cnt);
    }
  }

  // Push out all floors once per frame
  for (int f = 0; f < NUM_FLOORS; f++) showFloor(f);
}



// Sequencer stages
enum SeqStage : uint8_t {
  S1_FWD_ON = 0,
  S1_CLEAR,
  S1_BWD_ON,
  S1_CLEAR2,
  S2_FWD_GROUPS,
  S2_CLEAR,
  S2_BWD_GROUPS,
  S2_CLEAR2,
  S3_SETUP,
  S3_RUN,
  // New Stage 4 (random room fading, infinite loop)
  S4_GROUPS_SETUP,
  S4_GROUPS_RUN,
  S_DONE
};

static const RGB C_GROUP_FADE = C_GOLD;

struct SeqState {
  SeqStage st = S_DONE;
  uint32_t nextAt = 0;
  // Stage 1
  int floorIdx = 0;
  // Stage 2
  int groupIdx = 0;
  int totalGroups = (LEDS_PER_FLOOR + GROUP_SIZE - 1) / GROUP_SIZE;
  // Stage 3
  uint8_t cyclesDone = 0;
  uint32_t cycleStart = 0;
  bool groupOn[64];
  int fadeFloor = 0;
  uint32_t fadeStart = 0;
} seq;

static inline void fillGroupOnFloor(int floorIdx, int groupIdx, uint32_t color) {
  int start = groupIdx * S4_GROUP_SZ;
  int cnt = min((int)S4_GROUP_SZ, LEDS_PER_FLOOR - start);
  if (cnt > 0) floors[floorIdx].fill(color, start, cnt);
}

static void s4_clearGroupStates() {
  for (int f = 0; f < NUM_FLOORS; ++f) {
    for (int g = 0; g < S4_GROUPS_PER_FLOOR; ++g) {
      s4_mode[f][g] = RM_OFF;
      s4_val[f][g] = 0;
      s4_step[f][g] = 0;
    }
  }
}

// Build a shuffled list of (floor, group) for all groups across all floors
static int s4_buildGroupList(int* outF, int* outG, int maxN) {
  int n = 0;
  for (int f = 0; f < NUM_FLOORS; ++f) {
    for (int g = 0; g < S4_GROUPS_PER_FLOOR; ++g) {
      if (n < maxN) {
        outF[n] = f;
        outG[n] = g;
        n++;
      }
    }
  }
  // Fisher-Yates shuffle
  for (int i = n - 1; i > 0; --i) {
    int j = random(i + 1);
    int tf = outF[i], tg = outG[i];
    outF[i] = outF[j];
    outG[i] = outG[j];
    outF[j] = tf;
    outG[j] = tg;
  }
  return n;
}

// Randomly choose ~80% pulsing, ~20% fading-to-0; init their states
static void s4_selectGroups() {
  s4_clearGroupStates();

  const int MAXN = NUM_FLOORS * S4_GROUPS_PER_FLOOR;
  static int listF[MAXN];
  static int listG[MAXN];
  int total = s4_buildGroupList(listF, listG, MAXN);

  int nPulse = (total * 80 + 50) / 100;  // round to nearest
  if (nPulse > total) nPulse = total;

  for (int i = 0; i < total; ++i) {
    int f = listF[i], g = listG[i];
    if (i < nPulse) {
      s4_mode[f][g] = RM_PULSE;
      s4_val[f][g] = 0;  // start low → rise
      s4_step[f][g] = +S4_PULSE_STEP;
    } else {
      s4_mode[f][g] = RM_FADEOUT;
      s4_val[f][g] = 255;  // start bright → fade down
      s4_step[f][g] = -S4_FADE_STEP;
    }
  }
}

// One animation frame for all floors/groups
static void s4_tickGroups() {
  for (int f = 0; f < NUM_FLOORS; ++f) {
    floors[f].clear();

    for (int g = 0; g < S4_GROUPS_PER_FLOOR; ++g) {
      uint8_t m = s4_mode[f][g];
      uint8_t& v = s4_val[f][g];
      int8_t& s = s4_step[f][g];

      if (m == RM_OFF) continue;

      if (m == RM_PULSE) {
        int nv = (int)v + (int)s;
        if (s > 0 && nv >= 255) {
          nv = 255;
          s = -S4_PULSE_STEP;
        } else if (s < 0 && nv <= 0) {
          nv = 0;
          s = +S4_PULSE_STEP;
        }
        v = (uint8_t)nv;
      } else if (m == RM_FADEOUT) {
        if (v > 0) {
          int nv = (int)v + (int)s;  // s is negative
          if (nv < 0) nv = 0;
          v = (uint8_t)nv;
        }
      }

      uint32_t col = rgbScale(C_GROUP_FADE, v);
      fillGroupOnFloor(f, g, col);
    }

    floors[f].show();
  }
}


// Helpers
inline uint32_t packRGB(uint8_t r, uint8_t g, uint8_t b) {
  return floors[0].Color(r, g, b);
}
inline void showFloor(int f) {
  floors[f].show();
}  // tiny alias

void seq_clearAll() {
  for (int f = 0; f < NUM_FLOORS; f++) {
    floors[f].clear();
    showFloor(f);
  }
}

// ---- Stage 1: Floors on forward/back (instant) ----
void seq_stage1_step(bool forward) {
  // light next floor fully white, keep previous ON
  int idx = seq.floorIdx;
  uint32_t c = rgb32(C_FLOORBYFLOOR);
  floors[idx].fill(c);
  showFloor(idx);
}

// ---- Stage 2: Groups of 4 across all floors (instant) ----
void seq_stage2_step(bool forward) {
  uint32_t c = rgb32(C_FLOORBYFLOOR);
  if (forward) {
    int start = seq.groupIdx * GROUP_SIZE;
    int cnt = min(GROUP_SIZE, LEDS_PER_FLOOR - start);
    for (int f = 0; f < NUM_FLOORS; f++) {
      floors[f].fill(c, start, cnt);
      showFloor(f);
    }
  } else {
    // reverse fill from end
    int start = (seq.totalGroups - 1 - seq.groupIdx) * GROUP_SIZE;
    int cnt = min(GROUP_SIZE, LEDS_PER_FLOOR - start);
    for (int f = 0; f < NUM_FLOORS; f++) {
      floors[f].fill(c, start, cnt);
      showFloor(f);
    }
  }
}

// ---- Stage 3: Random groups fading (80% ON / 20% OFF) ----
void seq_stage3_newCycleMask() {
  // clear
  for (int g = 0; g < seq.totalGroups; g++) seq.groupOn[g] = false;
  int wantOn = (int)(seq.totalGroups * 0.80f + 0.5f);
  // choose unique indices
  int chosen = 0;
  while (chosen < wantOn) {
    int g = random(seq.totalGroups);
    if (!seq.groupOn[g]) {
      seq.groupOn[g] = true;
      chosen++;
    }
  }
}

uint8_t seq_pulse(uint32_t now, uint32_t start, uint32_t period) {
  // cosine pulse 0..1
  float t = (float)(now - start) / (float)period;
  if (t > 1) t = 1;
  float a = 0.5f * (1.0f - cosf(2.0f * 3.1415926f * t));  // 0..1
  // Map to intensity
  int v = (int)(PULSE_MIN + a * (PULSE_MAX - PULSE_MIN));
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

void seq_stage3_draw(uint8_t v) {
  // v is intensity for ON groups this frame
  uint32_t cOn = packRGB(v, v, v);
  uint32_t cOff = packRGB(0, 0, 0);
  for (int f = 0; f < NUM_FLOORS; f++) {
    for (int g = 0; g < seq.totalGroups; g++) {
      int start = g * GROUP_SIZE;
      int cnt = min(GROUP_SIZE, LEDS_PER_FLOOR - start);
      uint32_t col = seq.groupOn[g] ? cOn : cOff;
      floors[f].fill(col, start, cnt);
    }
    showFloor(f);
  }
}

// ---- Stage 4: Floors forward/back with fade-in per floor ----
void seq_stage4_beginFloor(int f) {
  seq.fadeFloor = f;
  seq.fadeStart = millis();
  floors[f].fill(packRGB(255, 255, 255));  // prepare pixels
  floors[f].setBrightness(0);
  showFloor(f);
}

bool seq_stage4_stepFloor() {
  uint32_t now = millis();
  uint32_t dt = now - seq.fadeStart;
  uint8_t b = (dt >= FADE_FLOOR_MS) ? BRIGHTNESS : (uint8_t)((dt * BRIGHTNESS) / FADE_FLOOR_MS);
  floors[seq.fadeFloor].setBrightness(b);
  showFloor(seq.fadeFloor);
  return (dt >= FADE_FLOOR_MS);
}

// Fill a single room segment on one floor
inline void fillRoomOnFloor(int floorIdx, int roomIdx, uint32_t color) {
  int start = room_start_indices[roomIdx];
  int cnt = room_led_counts[roomIdx];
  floors[floorIdx].fill(color, start, cnt);
}

// Turn specific rooms ON (white) on all floors; clears others
void setRoomsOnAllFloors(const int* rooms, int n, uint32_t color = 0) {
  if (color == 0) color = rgb32(C_WHITE);  // default soft-white

  for (int f = 0; f < NUM_FLOORS; f++) {
    floors[f].setBrightness(BRIGHTNESS);
    floors[f].clear();
    for (int i = 0; i < n; i++) {
      fillRoomOnFloor(f, rooms[i], color);
    }
    floors[f].show();
  }
}
static const uint8_t REFUGEE_FLOORS[] = { 10, 8, 6, 4 };

static inline bool isRefugeeFloorIdx(int fIdx1based) {
  for (uint8_t i = 0; i < sizeof(REFUGEE_FLOORS); ++i)
    if (fIdx1based == REFUGEE_FLOORS[i]) return true;
  return false;
}
// Apply wing id mapping (id=1 → R1,2,3 ; id=2 → R4,5,6,7 ; id=3 handled in master)
void applyWing(uint8_t id) {
  switch (id) {
    case 1:
      setRoomsOnAllFloors(WING1_ROOMS, sizeof(WING1_ROOMS) / sizeof(WING1_ROOMS[0]));
      Serial.println(F("[WING] id=1 → Rooms 1,2,3 ON"));
      break;
    case 2:
      setRoomsOnAllFloors(WING2_ROOMS, sizeof(WING2_ROOMS) / sizeof(WING2_ROOMS[0]));
      Serial.println(F("[WING] id=2 → Rooms 4,5,6,7 ON"));
      break;
    case 3:
      // Intentionally no-op (master will handle)
      Serial.println(F("[WING] id=3 → ignored (handled in master)"));
      break;
    default:
      Serial.print(F("[WING] unknown id="));
      Serial.println(id);
      break;
  }
}


// Start / Stop
void startPatternSequencer(bool loopEnabled = true) {
  g_seqLoopEnabled = loopEnabled;  // NEW
  activePattern = 7;
  // Stage 1 forward
  seq.st = S1_FWD_ON;
  seq.floorIdx = 0;
  seq.groupIdx = 0;
  seq.cyclesDone = 0;
  seq.nextAt = millis();  // start immediately
}

void stopPatternSequencer() {
  activePattern = 0;
  seq.st = S_DONE;
}

// One tick (call from loop)
void runPatternSequencer() {
  if (activePattern != 7) return;
  uint32_t now = millis();
  if (now < seq.nextAt) return;

  switch (seq.st) {
    // ----------------- Stage 1: Floors instant -----------------
    case S1_FWD_ON:
      seq_stage1_step(true);
      seq.floorIdx++;
      if (seq.floorIdx < NUM_FLOORS) {
        seq.nextAt = now + DELAY_FLOOR_STEP_MS;
      } else {
        seq.st = S1_CLEAR;
        seq.nextAt = now + DELAY_FLOOR_STEP_MS;
      }
      break;

    case S1_CLEAR:
      seq_clearAll();
      seq.floorIdx = NUM_FLOORS - 1;
      seq.st = S1_BWD_ON;
      seq.nextAt = now + DELAY_FLOOR_STEP_MS;
      break;

    case S1_BWD_ON:
      seq_stage1_step(false);
      if (seq.floorIdx > 0) {
        seq.floorIdx--;
        seq.nextAt = now + DELAY_FLOOR_STEP_MS;
      } else {
        seq.st = S1_CLEAR2;
        seq.nextAt = now + DELAY_FLOOR_STEP_MS;
      }
      break;

    case S1_CLEAR2:
      seq_clearAll();
      seq.groupIdx = 0;
      seq.st = S2_FWD_GROUPS;
      seq.nextAt = now + DELAY_GROUP_STEP_MS;
      break;

    // ----------------- Stage 2: Groups of 4 -----------------
    case S2_FWD_GROUPS:
      seq_stage2_step(true);
      seq.groupIdx++;
      if (seq.groupIdx < seq.totalGroups) {
        seq.nextAt = now + DELAY_GROUP_STEP_MS;
      } else {
        seq.st = S2_CLEAR;
        seq.nextAt = now + DELAY_GROUP_STEP_MS;
      }
      break;

    case S2_CLEAR:
      seq_clearAll();
      seq.groupIdx = 0;
      seq.st = S2_BWD_GROUPS;
      seq.nextAt = now + DELAY_GROUP_STEP_MS;
      break;

    case S2_BWD_GROUPS:
      seq_stage2_step(false);
      seq.groupIdx++;
      if (seq.groupIdx < seq.totalGroups) {
        seq.nextAt = now + DELAY_GROUP_STEP_MS;
      } else {
        seq.st = S2_CLEAR2;
        seq.nextAt = now + DELAY_GROUP_STEP_MS;
      }
      break;

    case S2_CLEAR2:
      seq_clearAll();
      seq.st = S4_GROUPS_SETUP;
      seq.nextAt = now + 10;
      break;

      // ----------------- Stage 3: Random fading -----------------
      // Replace S3_SETUP / S3_RUN cases in runPatternSequencer() with:

      // case S3_SETUP:
      //   seq.cyclesDone = 0;
      //   seq.cycleStart = now;
      //   s3_initTasks();
      //   seq.st = S3_RUN;
      //   seq.nextAt = now + S3_FRAME_MS;
      //   break;

      //         case S3_RUN:
      //   if (now - seq.cycleStart >= S3_CYCLE_MS) {
      //     seq.cyclesDone++;
      //     seq.cycleStart = now;
      //     if (seq.cyclesDone >= S3_CYCLES_TOTAL) {
      //       seq.st = S4_GROUPS_SETUP;          // → NEW Stage 4
      //       seq.nextAt = now + 10;
      //       break;
      //     }
      //     s3_initTasks();                      // keep Stage 3 fresh
      //   }
      //   s3_tickTasks();
      //   seq.nextAt = now + S3_FRAME_MS;
      //   break;

      // -------- NEW Stage 4: Random groups-of-5 fading --------
      // -------- NEW Stage 4: Random groups-of-5 fading (LOOPABLE) --------
    case S4_GROUPS_SETUP:
      seq_clearAll();
      s4_selectGroups();  // pick 80% pulsing, 20% fading
      s4_nextFrameAt = now + S4_FRAME_MS;
      s4_reselectAt = now + S4_RESELECT_MS;
      s4_phaseStart = now;  // NEW: start timing Stage-4
      seq.st = S4_GROUPS_RUN;
      seq.nextAt = now;
      break;

    case S4_GROUPS_RUN:
      if (now >= s4_nextFrameAt) {
        s4_tickGroups();
        s4_nextFrameAt = now + S4_FRAME_MS;
      }
      if (now >= s4_reselectAt) {
        s4_selectGroups();  // re-randomize the sets
        s4_reselectAt = now + S4_RESELECT_MS;
      }

      // ---- Loop back to Stage-1 after S4_TOTAL_MS (if enabled) ----
      if (g_seqLoopEnabled && (now - s4_phaseStart >= S4_TOTAL_MS)) {
        seq_clearAll();

        // Reset Stage-1 forward pass
        seq.floorIdx = 0;
        seq.groupIdx = 0;
        seq.st = S1_FWD_ON;
        seq.nextAt = now + DELAY_FLOOR_STEP_MS;
        break;
      }

      // otherwise stay here (interruptible by any command)
      seq.nextAt = now + 1;
      break;




    case S_DONE:
    default:
      stopPatternSequencer();  // end sequence (LEDs remain at last state; we already cleared)
      break;
  }
}

// =============================================================================
//                         EXISTING UTILS (unaltered)
// =============================================================================

void printMenu() {
  Serial.println(F("\n----- LED Control Menu -----"));
  Serial.println(F("  1 - Pattern: One Floor at a Time"));
  Serial.println(F("  2 - Pattern: One Room of Each Floor at a Time"));
  Serial.println(F("  3 - Pattern SEQUENCER (CMD_PATTERN)"));
  Serial.println(F("  4 - 3BHK (Rooms 2 & 3 + ducts)"));
  Serial.println(F("  5 - 4BHK (Rooms 1,4,5,6,7 + ducts)"));
  Serial.println(F("  9 - Turn ON ALL Lights"));
  Serial.println(F("  0 - Turn All LEDs OFF"));
  Serial.println(F("  M - Show this Menu"));
  Serial.println(F("----------------------------"));
}

void clearAllLeds() {
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i].clear();
    floors[i].show();
  }
}

void turnOnAllLights() {
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i].setBrightness(BRIGHTNESS);
    floors[i].fill(rgb32(C_WHITE));
    floors[i].show();
  }
}

/**
 * @brief Operation 1.1: Lights up one entire floor at a time, cycling through all floors.
 */
void patternOneFloorAtATime() {
  uint32_t color = floors[0].Color(0, 150, 150);  // Teal color
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i].fill(color);
    floors[i].show();
    delay(500);
  }
  clearAllLeds();
  for (int i = NUM_FLOORS - 1; i >= 0; i--) {
    floors[i].fill(color);
    floors[i].show();
    delay(500);
  }
  clearAllLeds();
}

/**
 * @brief Operation 1.2: Lights up one room type on all floors simultaneously, cycling through all rooms.
 */
void patternOneRoomOfEachFloorAtATime() {
  uint32_t color = floors[0].Color(150, 0, 150);  // Magenta color
  for (int r = 0; r < NUM_ROOMS; r++) {
    Serial.print("Lighting ");
    Serial.println(room_names[r]);
    int start = room_start_indices[r];
    int count = room_led_counts[r];

    // Turn on this room for all floors
    for (int f = 0; f < NUM_FLOORS; f++) {
      floors[f].fill(color, start, count);
      floors[f].show();
    }

    delay(1000);

    // Turn off all floors before lighting the next room type
    clearAllLeds();
  }
}
/**
 * @brief Lights up Rooms 2 & 3 on all floors, with ducts on.
 */
void lightUp3BHKs() {
  uint32_t color = floors[0].Color(255, 255, 0);      // Yellow
  uint32_t ductColor = floors[0].Color(255, 255, 0);  // White (ducts)

  // Room 2 -> index 1, Room 3 -> index 2
  int room2_start = room_start_indices[5];
  int room2_count = room_led_counts[5];
  int room3_start = room_start_indices[6];
  int room3_count = room_led_counts[6];

  // Duct 1 -> index 4, Duct 2 -> index 6
  int duct1_start = room_start_indices[2];
  int duct1_count = room_led_counts[2];
  int duct2_start = room_start_indices[8];
  int duct2_count = room_led_counts[8];

  for (int f = 0; f < NUM_FLOORS; f++) {
    floors[f].clear();
    for (int i = 0; i < room2_count; i++) floors[f].setPixelColor(room2_start + i, color);
    for (int i = 0; i < room3_count; i++) floors[f].setPixelColor(room3_start + i, color);
    for (int i = 0; i < duct1_count; i++) floors[f].setPixelColor(duct1_start + i, ductColor);
    for (int i = 0; i < duct2_count; i++) floors[f].setPixelColor(duct2_start + i, ductColor);
    floors[f].show();
  }
}

/**
 * @brief Lights up Rooms 1,4,5,6,7 on all floors, with ducts on.
 */
void lightUp4BHKs() {
  uint32_t color = floors[0].Color(0, 255, 255);      // Cyan
  uint32_t ductColor = floors[0].Color(0, 255, 255);  // White (ducts)

  const int rooms_to_light[] = { 0, 1, 3, 4, 7 };  // indices
  int num_rooms_to_light = sizeof(rooms_to_light) / sizeof(rooms_to_light[0]);

  int duct1_start = room_start_indices[2];
  int duct1_count = room_led_counts[2];
  int duct2_start = room_start_indices[8];
  int duct2_count = room_led_counts[8];

  for (int f = 0; f < NUM_FLOORS; f++) {
    floors[f].clear();

    for (int i = 0; i < num_rooms_to_light; i++) {
      int room_index = rooms_to_light[i];
      int start = room_start_indices[room_index];
      int count = room_led_counts[room_index];
      for (int j = 0; j < count; j++) {
        floors[f].setPixelColor(start + j, color);
      }
    }

    for (int i = 0; i < duct1_count; i++) floors[f].setPixelColor(duct1_start + i, ductColor);
    for (int i = 0; i < duct2_count; i++) floors[f].setPixelColor(duct2_start + i, ductColor);

    floors[f].show();
  }
}

/**
 * @brief Sets the duct LEDs on all floors to white. (Unused by default)
 */
void lightUpDuctsPermanently(bool isAwing) {
  const uint32_t ductColor = rgb32(C_DUCT);
  const uint32_t refugeeColor = floors[0].Color(255, 0, 0);  // bright red

  const int duct1_start = room_start_indices[2];  // Duct 1
  const int duct1_count = room_led_counts[2];
  const int duct2_start = room_start_indices[8];  // Duct 2
  const int duct2_count = room_led_counts[8];

  const int nRef = 3;  // number of refugee LEDs on each duct segment

  for (int f = 0; f < NUM_FLOORS; ++f) {
    for (int i = 0; i < duct1_count; ++i) floors[f].setPixelColor(duct1_start + i, ductColor);

    for (int i = 0; i < duct2_count; ++i) floors[f].setPixelColor(duct2_start + i, ductColor);

    // Overlay refugee LEDs on specified floors
    if (isRefugeeFloorIdx(f + 1)) {  // floors are 1-based: 1..NUM_FLOORS
                                     // Duct1: END 4 LEDs

      int take1 = min(nRef, duct1_count);
      int start1 = duct1_start + duct1_count - take1;
      for (int i = 0; i < take1; ++i) floors[f].setPixelColor(start1 + i, refugeeColor);

      // Duct2: START 4 LEDs
      int take2 = min(nRef, duct2_count);
      int start2 = duct2_start;
      for (int i = 0; i < take2; ++i) floors[f].setPixelColor(start2 + i, refugeeColor);
    }
    floors[f].show();
  }
}

void lightUpDuctsPermanentlyOff(bool isAwing) {
  const uint32_t ductColor = floors[0].Color(0, 0, 0);
  const uint32_t refugeeColor = floors[0].Color(0, 0, 0);  // bright red

  const int duct1_start = room_start_indices[2];  // Duct 1
  const int duct1_count = room_led_counts[2];
  const int duct2_start = room_start_indices[8];  // Duct 2
  const int duct2_count = room_led_counts[8];

  const int nRef = 3;  // number of refugee LEDs on each duct segment

  for (int f = 0; f < NUM_FLOORS; ++f) {
    // Base duct fill
    if (!isAwing) {
      for (int i = 0; i < duct1_count; ++i) floors[f].setPixelColor(duct1_start + i, ductColor);
    } else {
      for (int i = 0; i < duct2_count; ++i) floors[f].setPixelColor(duct2_start + i, ductColor);
    }
    // Overlay refugee LEDs on specified floors
    if (isRefugeeFloorIdx(f + 1)) {  // floors are 1-based: 1..NUM_FLOORS
      // Duct1: END 4 LEDs
      if (!isAwing) {
        int take1 = min(nRef, duct1_count);
        int start1 = duct1_start + duct1_count - take1;
        for (int i = 0; i < take1; ++i) floors[f].setPixelColor(start1 + i, refugeeColor);
      } else {
        // Duct2: START 4 LEDs
        int take2 = min(nRef, duct2_count);
        int start2 = duct2_start;
        for (int i = 0; i < take2; ++i) floors[f].setPixelColor(start2 + i, refugeeColor);
      }
    }

    floors[f].show();
  }
}

// =============================================================================
//                                 SETUP / LOOP
// =============================================================================

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  Serial.println(F("Building LED Control System Initialized"));

  room_start_indices[0] = 0;
  for (int i = 1; i < NUM_ROOMS; i++) room_start_indices[i] = room_start_indices[i - 1] + room_led_counts[i - 1];

  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i] = Adafruit_NeoPixel(LEDS_PER_FLOOR, floorPins[i], NEO_GRB + NEO_KHZ800);
    floors[i].begin();
    floors[i].setBrightness(BRIGHTNESS);
    floors[i].clear();
    floors[i].show();
  }

  Wire.begin(MEGA_I2C_ADDR);
  Wire.onReceive(onI2CRecv);
  Serial.println(F("I2C slave ready @0x08"));

  // Seed randomness for Stage 3 mask variety
  randomSeed(analogRead(A0));

  printMenu();
  turnOnAllLights();
  //lightUpDuctsPermanently();
  g_lastCmdAt = millis();
}

void loop() {
  // ---------- I2C Command Dispatcher (interruptible) ----------
  if (g_hasPkt) {
    RxPacket p;
    noInterrupts();
    memcpy(&p, (const void*)g_isrRaw, 8);
    g_hasPkt = false;
    interrupts();
    if (p.pre == 0xAA && p.post == 0x55 && (uint8_t)(p.cmd + p.a + p.b + p.c + p.d) == p.sum) {
      // Any new command aborts sequencer immediately
      activePattern = 0;
      seq.st = S_DONE;
      markCommandSeen();  // <-- add this line
      //lightUpDuctsPermanently();
      switch (p.cmd) {
        case CMD_PATTERN:
          clearAllLeds();
          startPatternSequencer();
          Serial.println(F("[I2C] CMD_PATTERN -> sequencer"));
          break;

        case CMD_ALL_ON:
          turnOnAllLights();
          lightUpDuctsPermanently(true);
          lightUpDuctsPermanently(false);
          Serial.println(F("[I2C] CMD_ALL_ON"));
          break;

        case CMD_ALL_OFF:
          clearAllLeds();
          Serial.println(F("[I2C] CMD_ALL_OFF"));
          break;

        case CMD_BHK:
          if (p.a == 3) {
            lightUp3BHKs();
            lightUpDuctsPermanently(true);
            lightUpDuctsPermanentlyOff(false);
            lightUpDuctsPermanentlyOff(true);

            Serial.println(F("[I2C] CMD_BHK 3"));
          } else if (p.a == 4) {
            lightUp4BHKs();
            lightUpDuctsPermanently(false);
            lightUpDuctsPermanentlyOff(true);
            lightUpDuctsPermanentlyOff(false);

            Serial.println(F("[I2C] CMD_BHK 4"));
          } else Serial.println(F("[I2C] CMD_BHK invalid"));
          break;

        case CMD_WING_SELECT:
        case CMD_WING_CLICK:
          clearAllLeds();
          applyWing(p.a);
          if (p.a == 1) {
            const uint32_t ductColor = rgb32(C_DUCT);
            const uint32_t refugeeColor = floors[0].Color(255, 0, 0);  // bright red

            const int duct2_start = room_start_indices[8];  // Duct 2
            const int duct2_count = room_led_counts[8];

            const int nRef = 3;  // number of refugee LEDs on each duct segment

            for (int f = 0; f < NUM_FLOORS; ++f) {

              for (int i = 0; i < duct2_count; ++i) floors[f].setPixelColor(duct2_start + i, ductColor);

              // Overlay refugee LEDs on specified floors
              if (isRefugeeFloorIdx(f + 1)) {  // floors are 1-based: 1..NUM_FLOORS

                // Duct2: START 4 LEDs
                int take2 = min(nRef, duct2_count);
                int start2 = duct2_start;
                for (int i = 0; i < take2; ++i) floors[f].setPixelColor(start2 + i, refugeeColor);
              }
              floors[f].show();
            }
          }
          else if (p.a == 2) {
            const uint32_t ductColor    = rgb32(C_DUCT);
  const uint32_t refugeeColor = floors[0].Color(255, 0, 0); // bright red

  const int duct1_start = room_start_indices[2]; // Duct 1
  const int duct1_count = room_led_counts[2];

  const int nRef = 3; // number of refugee LEDs on each duct segment

  for (int f = 0; f < NUM_FLOORS; ++f) {
      for (int i = 0; i < duct1_count; ++i) floors[f].setPixelColor(duct1_start + i, ductColor);


    // Overlay refugee LEDs on specified floors
    if (isRefugeeFloorIdx(f + 1)) { // floors are 1-based: 1..NUM_FLOORS
      // Duct1: END 4 LEDs
 
       int take1  = min(nRef, duct1_count);
      int start1 = duct1_start + duct1_count - take1;
      for (int i = 0; i < take1; ++i) floors[f].setPixelColor(start1 + i, refugeeColor);

    }
    floors[f].show();
  }
          }
          //lightUpDuctsPermanently(p.a == 1 ? true : false);
          //lightUpDuctsPermanentlyOff(p.a == 1 ? false : true);
          Serial.print(F("[I2C] CMD_WING (select/click) id="));
          Serial.println(p.a);
          break;

        default:
          Serial.print(F("[I2C] Unknown cmd=0x"));
          Serial.println(p.cmd, HEX);
          break;
      }
    }
  }

  if (g_hasLegacy) {
    uint8_t c;
    noInterrupts();
    c = g_legacyCmd;
    g_hasLegacy = false;
    interrupts();
    // Any legacy command aborts sequencer immediately
    activePattern = 0;
    seq.st = S_DONE;
    markCommandSeen();
    switch (c) {
      case LEGACY_CMD_OFF:
        clearAllLeds();
        Serial.println(F("[I2C] LEGACY OFF"));
        break;
      case LEGACY_CMD_PATTERN_RAINBOW:
        clearAllLeds();
        startPatternSequencer();
        Serial.println(F("[I2C] LEGACY -> sequencer"));
        break;
      case LEGACY_CMD_BHK3: lightUp3BHKs(); break;
      case LEGACY_CMD_BHK4: lightUp4BHKs(); break;
      case LEGACY_CMD_ALL_ON: turnOnAllLights(); break;
      default: break;
    }
  }

  // // ---------- Serial Menu ----------Uncomment for debugging
  // if (Serial.available() > 0) {
  //   char command = Serial.read();
  //   // Serial commands also interrupt patterns
  //   activePattern = 0;
  //   seq.st = S_DONE;

  //   switch (command) {
  //     case '1':
  //       patternOneFloorAtATime();
  //       printMenu();
  //       break;
  //     case '2':
  //       patternOneRoomOfEachFloorAtATime();
  //       printMenu();
  //       break;
  //     case '3':
  //       clearAllLeds();
  //       startPatternSequencer();
  //       Serial.println(F("Sequencer started."));
  //       break;
  //     case '4': lightUp3BHKs(); break;
  //     case '5': lightUp4BHKs(); break;
  //     case '9': turnOnAllLights(); break;
  //     case '0':
  //       clearAllLeds();
  //       printMenu();
  //       break;
  //     case 'm':
  //     case 'M': printMenu(); break;
  //     default:
  //       while (Serial.available() > 0) Serial.read();
  //       break;
  //   }
  // }
  if (activePattern != 7) {  // not already running sequencer
    unsigned long now = millis();
    if (now - g_lastCmdAt >= IDLE_PATTERN_MS) {
      clearAllLeds();
      startPatternSequencer();
      g_lastCmdAt = now;  // reset to avoid immediate retrigger after S_DONE
      Serial.println(F("[AUTO] Idle 30s → PATTERN started"));
    }
  }
  // ---------- Run non-blocking sequencer if active ----------
  runPatternSequencer();
}
