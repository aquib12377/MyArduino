/*
 * HighRiseBuilding.ino
 * High-rise example matching Sugee Vaayu project:
 *   - 54 floors with uniform 8 rooms × 4 LEDs layout
 *   - Refuge floors scattered throughout
 *   - Simple room-number-based presets (1BHK, 2BHK)
 *   - RGB fade + random 80% pattern
 *   - Building-level on/off command
 *
 * This shows how to handle:
 *   - Large floor counts efficiently
 *   - Uniform layouts (all floors share one layout)
 *   - Refuge floor marking
 *   - Custom animation registration
 */

#define BLED_DEBUG 1
#include <BuildingLED.h>

/* ═══════════════ SEGMENT MAP ═══════════════ */

// All floors identical: 8 rooms × 4 LEDs = 32 LEDs
static const SegmentDef ROOM_SEGMENTS[] = {
  { 4, SEG_ROOM, 1 },
  { 4, SEG_ROOM, 2 },
  { 4, SEG_ROOM, 3 },
  { 4, SEG_ROOM, 4 },
  { 4, SEG_ROOM, 5 },
  { 4, SEG_ROOM, 6 },
  { 4, SEG_ROOM, 7 },
  { 4, SEG_ROOM, 8 },
};

static const FloorLayout LAYOUTS[] = {
  { ROOM_SEGMENTS, 8, 32 },  // Layout 0: Standard 8-room
};

/* ═══════════════ FLOOR DEFINITIONS ═══════════════ */

// Helper macro: standard residential floor
#define F_RES(pin)  { pin, 0, FLOOR_RESIDENTIAL }
#define F_REF(pin)  { pin, 0, FLOOR_REFUGE }      // Refuge floor

#define NUM_FLOORS 54

// Refuge floors: 7, 14, 21, 28, 35, 42, 49  (every 7th floor)
static const FloorDef FLOORS[NUM_FLOORS] = {
  F_RES(13), F_RES(12), F_RES(11), F_RES(10), F_RES(9),  F_RES(8),   // 0-5
  F_REF(7),  F_RES(6),  F_RES(5),  F_RES(4),  F_RES(3),  F_RES(2),   // 6-11  (floor 6 = refuge)
  F_RES(14), F_REF(15),                                                 // 12-13 (floor 13 = refuge)
  F_RES(16), F_RES(17), F_RES(22), F_RES(23), F_RES(25), F_RES(27),   // 14-19
  F_REF(29), F_RES(31), F_RES(33), F_RES(35), F_RES(37), F_RES(39),   // 20-25 (floor 20 = refuge)
  F_RES(41), F_REF(43), F_RES(45), F_RES(47), F_RES(49), F_RES(51),   // 26-31 (floor 27 = refuge)
  F_RES(A15), F_RES(A14), F_REF(A13), F_RES(A12), F_RES(A11),         // 32-36 (floor 34 = refuge)
  F_RES(A10), F_RES(A9), F_RES(A8),
  F_RES(A7), F_REF(A6), F_RES(A5), F_REF(A4), F_REF(A3), F_RES(A2),  // 40-45
  F_RES(A0), F_RES(A1), F_REF(50), F_RES(48),                          // 46-49 (floor 48 = refuge)
  F_RES(46), F_RES(44), F_RES(42), F_RES(40),                           // 50-53
};

/* ═══════════════ CONFIG ═══════════════ */

#define BUILDING_ID 1

static const BuildingConfig config = {
  .floors         = FLOORS,
  .floorCount     = NUM_FLOORS,
  .layouts        = LAYOUTS,
  .layoutCount    = 1,
  .i2cAddress     = 0x08,
  .brightness     = 180,
  .idleTimeoutMs  = 0,            // No idle timeout (pattern controlled by master)
  .idleAnimationId = ANIM_NONE,

  .colors = {
    .primary    = 0x999483,       // Warm white (153, 148, 131)
    .secondary  = 0x619961,       // Sage (97, 153, 97)
    .highlight  = 0x996652,       // Coral (153, 102, 82)
    .staircase  = 0x000000,
    .duct       = 0x000000,
    .refuge     = 0x961414,       // Red (150, 20, 20)
    .fixed      = 0xFFFFFF,
    .shop       = 0x000000,
    .off        = 0x000000,
  },
};

BuildingLED building(config);

/* ═══════════════ CUSTOM ANIMATION: RGB FADE ═══════════════ */

// Smooth rainbow hue cycle across all floors simultaneously
bool animRGBFade(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  // Full cycle in ~30 seconds at 50ms/frame = 600 frames
  const uint16_t CYCLE_FRAMES = 600;
  uint16_t hueStep = step % CYCLE_FRAMES;
  
  // Map to 0-255 hue range
  uint8_t hue8 = (uint8_t)((uint32_t)hueStep * 255 / CYCLE_FRAMES);
  
  // Simple HSV to RGB (hue only, full sat, brightness from config)
  uint8_t region = hue8 / 43;
  uint8_t remainder = (hue8 % 43) * 6;
  uint8_t br = 153; // match BRIGHTNESS scaled value

  uint8_t r, g, bl;
  switch (region) {
    case 0:  r = br; g = (br * remainder) >> 8; bl = 0; break;
    case 1:  r = br - ((br * remainder) >> 8); g = br; bl = 0; break;
    case 2:  r = 0; g = br; bl = (br * remainder) >> 8; break;
    case 3:  r = 0; g = br - ((br * remainder) >> 8); bl = br; break;
    case 4:  r = (br * remainder) >> 8; g = 0; bl = br; break;
    default: r = br; g = 0; bl = br - ((br * remainder) >> 8); break;
  }

  uint32_t col = b.rgb(r, g, bl);
  for (uint8_t f = 0; f < b.getFloorCount(); f++) {
    b.setFloor(f, col);
  }
  b.showAll();
  return false; // loops forever
}

/* ═══════════════ PRESETS ═══════════════ */

void preset1BHK(BuildingLED& b) {
  b.clearAll();
  const uint8_t rooms[] = {1, 4, 5, 8};
  b.lightRooms(rooms, 4, b.colors().secondary); // sage green
  b.applyRefugeFloors();
  b.showAll();
}

void preset2BHK(BuildingLED& b) {
  b.clearAll();
  const uint8_t rooms[] = {2, 3, 6, 7};
  b.lightRooms(rooms, 4, b.colors().highlight); // coral
  b.applyRefugeFloors();
  b.showAll();
}

/* ═══════════════ CUSTOM COMMAND HANDLER ═══════════════ */

#define CMD_BUILDING 0x30

void handleCustomCmd(BuildingLED& b, const BLEDPacket& pkt) {
  switch (pkt.cmd) {
    case CMD_BUILDING: {
      uint8_t bldId = pkt.a;
      uint8_t action = pkt.b;
      
      // Respond if addressed to us (BUILDING_ID) or broadcast (3)
      if (bldId == BUILDING_ID || bldId == 3) {
        if (action == 1) {
          b.stopAnimation();
          b.allOn();
        } else {
          b.stopAnimation();
          b.allOff();
        }
      }
      break;
    }
    default:
      break;
  }
}

/* ═══════════════ SETUP / LOOP ═══════════════ */

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n=== HighRise Building Controller ==="));
  Serial.print(F("Floors: ")); Serial.println(NUM_FLOORS);

  building.begin();

  // Register custom animation
  building.registerAnimation(ANIM_USER_START, animRGBFade, 50, true);

  // Register presets
  building.registerPreset(1, preset1BHK);
  building.registerPreset(2, preset2BHK);

  // Custom command handler
  building.onCommand(handleCustomCmd);

  // Start with all on
  building.allOn();

  Serial.println(F("[BOOT] Ready!"));
}

void loop() {
  building.update();
}
