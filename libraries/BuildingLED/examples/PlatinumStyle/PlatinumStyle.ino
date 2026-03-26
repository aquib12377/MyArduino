/*
 * PlatinumStyle.ino
 * Complex example matching the Platinum project:
 *   - Ground floor with different segment layout (shops)
 *   - Residential floors with rooms + staircases + fixed-white rooms
 *   - Custom commands: Jodi mode, Booked rooms toggle
 *   - Crossfade idle animation
 * 
 * This shows how to handle:
 *   - Multiple layout types (ground vs residential)
 *   - Always-on (fixed) segments
 *   - Custom I2C commands beyond the built-in set
 *   - Project-specific presets
 */

#define BLED_DEBUG 1
#include <BuildingLED.h>

/* ═══════════════ CUSTOM COMMAND IDs ═══════════════ */

// These extend the built-in CMD_* enum
#define CMD_JODI    0x23  // Jodi mode (custom lighting combo)
#define CMD_BOOKED  0x24  // Toggle booked rooms on/off

/* ═══════════════ SEGMENT MAPS ═══════════════ */

// Ground floor: 13 segments (shops + stairs)
// Total: 4+2+4+5+3+2+5+5+4+5+3+4+9 = 55 LEDs
static const SegmentDef GROUND_SEGMENTS[] = {
  {  4, SEG_STAIRCASE, 0 },
  {  2, SEG_STAIRCASE, 0 },
  {  4, SEG_SHOP,      0 },
  {  5, SEG_SHOP,      0 },
  {  3, SEG_SHOP,      0 },
  {  2, SEG_STAIRCASE, 0 },
  {  5, SEG_SHOP,      0 },
  {  5, SEG_SHOP,      0 },
  {  4, SEG_SHOP,      0 },
  {  5, SEG_SHOP,      0 },
  {  3, SEG_SHOP,      0 },
  {  4, SEG_SHOP,      0 },
  {  9, SEG_SHOP,      0 },
};

// Residential floor: 11 segments
// Rooms 6,7,8 are "fixed" (always white, never change)
// Total: 4+2+7+9+2+7+5+6+9+10+7 = 68 LEDs
static const SegmentDef RES_SEGMENTS[] = {
  //  LEDs  Type           Room#   // Seg 0
  {  4, SEG_STAIRCASE, 0 },       // Seg 0: Stair
  {  2, SEG_STAIRCASE, 0 },       // Seg 1: Stair
  {  7, SEG_FIXED,     8 },       // Seg 2: Room 8 (always white)
  {  9, SEG_FIXED,     7 },       // Seg 3: Room 7 (always white)
  {  2, SEG_STAIRCASE, 0 },       // Seg 4: Stair
  {  7, SEG_FIXED,     6 },       // Seg 5: Room 6 (always white)
  {  5, SEG_ROOM,      5 },       // Seg 6: Room 5
  {  6, SEG_ROOM,      4 },       // Seg 7: Room 4
  {  9, SEG_ROOM,      3 },       // Seg 8: Room 3
  { 10, SEG_ROOM,      2 },       // Seg 9: Room 2
  {  7, SEG_ROOM,      1 },       // Seg 10: Room 1
};

/* ═══════════════ LAYOUTS ═══════════════ */

static const FloorLayout LAYOUTS[] = {
  { GROUND_SEGMENTS, 13, 55 },  // Layout 0: Ground
  { RES_SEGMENTS,    11, 68 },  // Layout 1: Residential
};

/* ═══════════════ FLOORS ═══════════════ */

#define NUM_FLOORS 17

static const FloorDef FLOORS[NUM_FLOORS] = {
  // Ground floor
  { 4,  0, FLOOR_GROUND },
  // Residential floors 1-16
  { 5,  1, FLOOR_RESIDENTIAL },
  { 6,  1, FLOOR_RESIDENTIAL },
  { 7,  1, FLOOR_RESIDENTIAL },
  { 8,  1, FLOOR_RESIDENTIAL },
  { 9,  1, FLOOR_RESIDENTIAL },
  { 10, 1, FLOOR_RESIDENTIAL },
  { 11, 1, FLOOR_RESIDENTIAL },
  { 12, 1, FLOOR_RESIDENTIAL },
  { 13, 1, FLOOR_RESIDENTIAL },
  { 39, 1, FLOOR_RESIDENTIAL },
  { 37, 1, FLOOR_RESIDENTIAL },
  { 35, 1, FLOOR_RESIDENTIAL },
  { 33, 1, FLOOR_RESIDENTIAL },
  { 31, 1, FLOOR_RESIDENTIAL },
  { 29, 1, FLOOR_RESIDENTIAL },
  { 27, 1, FLOOR_RESIDENTIAL },
};

/* ═══════════════ CONFIG ═══════════════ */

static const BuildingConfig config = {
  .floors         = FLOORS,
  .floorCount     = NUM_FLOORS,
  .layouts        = LAYOUTS,
  .layoutCount    = 2,
  .i2cAddress     = 0x08,
  .brightness     = 180,
  .idleTimeoutMs  = 60000,
  .idleAnimationId = ANIM_CROSSFADE,
  .colors = {
    .primary    = 0x00C8B4,     // Turquoise
    .secondary  = 0xFF6400,     // Orange
    .highlight  = 0xFFFF00,     // Yellow
    .staircase  = 0x000000,     // Off (stairs stay dark)
    .duct       = 0x000000,
    .refuge     = 0x961414,     // Red
    .fixed      = 0xFFFFFF,     // White (rooms 6,7,8)
    .shop       = 0xFFFF00,     // Yellow for shops
    .off        = 0x000000,
  },
};

BuildingLED building(config);

/* ═══════════════ STATE FOR CUSTOM COMMANDS ═══════════════ */

bool g_bookedRoomsOn = true;

/* ═══════════════ CUSTOM PRESETS ═══════════════ */

void presetGroundCommercial(BuildingLED& b) {
  b.clearAll();
  // Light shops on ground floor
  b.setSegmentsByType(0, SEG_SHOP, b.colors().shop);
  // Keep fixed segments on residential floors
  b.applyFixedSegmentsAll();
  b.showAll();
}

void preset1BHK(BuildingLED& b) {
  b.clearAll();
  // 1BHK = Rooms 4 and 5
  const uint8_t rooms[] = {4, 5};
  b.lightRooms(rooms, 2, b.colors().primary);
  b.applyFixedSegmentsAll();
  b.showAll();
}

void preset2BHK(BuildingLED& b) {
  b.clearAll();
  // 2BHK = Rooms 1, 2, 3 + fixed rooms 6, 7, 8
  const uint8_t rooms[] = {1, 2, 3};
  b.lightRooms(rooms, 3, b.colors().primary);
  b.applyFixedSegmentsAll();
  b.showAll();
}

/* ═══════════════ CUSTOM COMMAND HANDLER ═══════════════ */

void applyJodi(BuildingLED& b) {
  b.clearAll();
  for (uint8_t f = 1; f < b.getFloorCount(); f++) {
    // Rooms 1,2 → orange
    uint8_t seg1 = b.findSegmentByRoom(f, 1);
    uint8_t seg2 = b.findSegmentByRoom(f, 2);
    if (seg1 != 255) b.setSegment(f, seg1, b.colors().secondary);
    if (seg2 != 255) b.setSegment(f, seg2, b.colors().secondary);

    // Rooms 3,4 → orange
    uint8_t seg3 = b.findSegmentByRoom(f, 3);
    uint8_t seg4 = b.findSegmentByRoom(f, 4);
    if (seg3 != 255) b.setSegment(f, seg3, b.colors().secondary);
    if (seg4 != 255) b.setSegment(f, seg4, b.colors().secondary);

    // Fixed segments stay white
    b.applyFixedSegments(f);
  }
  b.showAll();
}

void toggleBooked(BuildingLED& b, bool on) {
  g_bookedRoomsOn = on;
  uint32_t col = on ? b.colors().fixed : b.colors().off;
  // Rooms 6,7,8 are SEG_FIXED - override them
  for (uint8_t f = 1; f < b.getFloorCount(); f++) {
    b.setSegmentsByType(f, SEG_FIXED, col);
    b.showFloor(f);
  }
}

void handleCustomCmd(BuildingLED& b, const BLEDPacket& pkt) {
  switch (pkt.cmd) {
    case CMD_JODI:
      b.stopAnimation();
      applyJodi(b);
      break;

    case CMD_BOOKED:
      toggleBooked(b, pkt.a != 0);
      break;

    default:
      Serial.print(F("[CUSTOM] Unknown cmd=0x"));
      Serial.println(pkt.cmd, HEX);
      break;
  }
}

/* ═══════════════ SETUP / LOOP ═══════════════ */

void setup() {
  Serial.begin(115200);
  delay(50);

  building.begin();

  // Register presets
  building.registerPreset(1, presetGroundCommercial);
  building.registerPreset(2, preset1BHK);
  building.registerPreset(3, preset2BHK);

  // Register custom command handler
  building.onCommand(handleCustomCmd);

  building.allOff();

  Serial.println(F("[BOOT] Platinum-Style Ready"));
}

void loop() {
  building.update();
}
