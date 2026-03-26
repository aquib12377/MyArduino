/*
 * BasicBuilding.ino
 * Minimal example: 10 floors, 8 rooms each, 4 LEDs per room
 * 
 * Demonstrates the core workflow:
 *   1. Define segments (room layout within each floor)
 *   2. Define floor layouts (which segment map to use)
 *   3. Define floor list (pin, layout, type)
 *   4. Build config, create controller, done
 */

// ─── Enable debug output (set to 0 for production) ───
#define BLED_DEBUG 1

#include <BuildingLED.h>

/* ════════════════════════════════════════════════════════
 *  STEP 1: Define segment maps
 *  Each floor is a strip of LEDs divided into segments.
 *  Segments can be rooms, staircases, ducts, etc.
 * ════════════════════════════════════════════════════════ */

// Residential floor: 8 rooms × 4 LEDs = 32 LEDs per floor
// Room numbers are 1-based logical IDs (what your UI sends)
static const SegmentDef RES_SEGMENTS[] = {
  // { ledCount, type,          roomNumber }
  {  4, SEG_STAIRCASE, 0 },   // Seg 0: Staircase (not a room)
  {  4, SEG_ROOM,      1 },   // Seg 1: Room 1
  {  4, SEG_ROOM,      2 },   // Seg 2: Room 2
  {  4, SEG_ROOM,      3 },   // Seg 3: Room 3
  {  4, SEG_ROOM,      4 },   // Seg 4: Room 4
  {  4, SEG_ROOM,      5 },   // Seg 5: Room 5
  {  4, SEG_ROOM,      6 },   // Seg 6: Room 6
  {  4, SEG_ROOM,      7 },   // Seg 7: Room 7
  {  4, SEG_ROOM,      8 },   // Seg 8: Room 8
};

/* ════════════════════════════════════════════════════════
 *  STEP 2: Define floor layouts
 *  Layouts are templates - many floors can share the same layout
 * ════════════════════════════════════════════════════════ */

static const FloorLayout LAYOUTS[] = {
  // Layout 0: Residential
  { RES_SEGMENTS, 9, 36 },  // 9 segments, 36 total LEDs
};

/* ════════════════════════════════════════════════════════
 *  STEP 3: Define floors
 *  Map each physical floor to a pin, layout, and type
 * ════════════════════════════════════════════════════════ */

#define NUM_FLOORS 10

static const FloorDef FLOORS[NUM_FLOORS] = {
  // { pin, layoutIndex, type }
  { 22, 0, FLOOR_RESIDENTIAL },   // Floor 0
  { 23, 0, FLOOR_RESIDENTIAL },   // Floor 1
  { 24, 0, FLOOR_RESIDENTIAL },   // Floor 2
  { 25, 0, FLOOR_RESIDENTIAL },   // Floor 3
  { 26, 0, FLOOR_RESIDENTIAL },   // Floor 4
  { 27, 0, FLOOR_REFUGE },        // Floor 5 (refuge floor!)
  { 28, 0, FLOOR_RESIDENTIAL },   // Floor 6
  { 29, 0, FLOOR_RESIDENTIAL },   // Floor 7
  { 30, 0, FLOOR_RESIDENTIAL },   // Floor 8
  { 31, 0, FLOOR_RESIDENTIAL },   // Floor 9
};

/* ════════════════════════════════════════════════════════
 *  STEP 4: Build config and create controller
 * ════════════════════════════════════════════════════════ */

static const BuildingConfig config = {
  .floors         = FLOORS,
  .floorCount     = NUM_FLOORS,
  .layouts        = LAYOUTS,
  .layoutCount    = 1,
  .i2cAddress     = 0x08,
  .brightness     = 180,
  .idleTimeoutMs  = 60000,      // Auto-animate after 60s idle
  .idleAnimationId = ANIM_FLOOR_CHASE,  // Chase animation on idle

  .colors = {
    .primary    = 0x00C8B4,     // Turquoise
    .secondary  = 0xFF6400,     // Orange
    .highlight  = 0xFFFF00,     // Yellow
    .staircase  = 0x333333,     // Dim white
    .duct       = 0xC8C850,     // Yellowish
    .refuge     = 0x961414,     // Red
    .fixed      = 0xFFFFFF,     // White
    .shop       = 0xFFD700,     // Gold
    .off        = 0x000000,
  },
};

BuildingLED building(config);

/* ════════════════════════════════════════════════════════
 *  STEP 5 (Optional): Register presets
 * ════════════════════════════════════════════════════════ */

void preset1BHK(BuildingLED& b) {
  b.clearAll();
  const uint8_t rooms[] = {1, 4, 5, 8};
  b.lightRooms(rooms, 4, b.colors().primary);
  b.applyRefugeFloors();
  b.showAll();
}

void preset2BHK(BuildingLED& b) {
  b.clearAll();
  const uint8_t rooms[] = {2, 3, 6, 7};
  b.lightRooms(rooms, 4, b.colors().secondary);
  b.applyRefugeFloors();
  b.showAll();
}

/* ════════════════════════════════════════════════════════
 *  SETUP & LOOP
 * ════════════════════════════════════════════════════════ */

void setup() {
  Serial.begin(115200);
  delay(100);
  
  building.begin();

  // Register presets
  building.registerPreset(1, preset1BHK);
  building.registerPreset(2, preset2BHK);

  // Start with all on
  building.allOn();
}

void loop() {
  building.update();
}
