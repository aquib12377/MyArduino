# BuildingLED Library

**Reusable WS2812B building scale-model LED controller for Arduino Mega 2560**

Eliminates repetitive code when building real-estate scale model LED controllers. Define your building once, get I2C communication, room mapping, animations, presets, and idle timeout for free.

---

## Quick Start

```cpp
#define BLED_DEBUG 1
#include <BuildingLED.h>

// 1. Define segments (room layout per floor)
static const SegmentDef SEGMENTS[] = {
  { 4, SEG_STAIRCASE, 0 },  // 4 LEDs, staircase, no room number
  { 4, SEG_ROOM,      1 },  // 4 LEDs, Room 1
  { 4, SEG_ROOM,      2 },  // 4 LEDs, Room 2
  { 4, SEG_DUCT,      0 },  // 4 LEDs, duct
  { 4, SEG_ROOM,      3 },  // 4 LEDs, Room 3
};

// 2. Define layouts (templates that floors reference)
static const FloorLayout LAYOUTS[] = {
  { SEGMENTS, 5, 20 },  // 5 segments, 20 total LEDs
};

// 3. Define floors (pin, layout index, type)
static const FloorDef FLOORS[] = {
  { 22, 0, FLOOR_RESIDENTIAL },
  { 23, 0, FLOOR_RESIDENTIAL },
  { 24, 0, FLOOR_REFUGE },       // Refuge floor!
  { 25, 0, FLOOR_RESIDENTIAL },
};

// 4. Build config
static const BuildingConfig config = {
  .floors          = FLOORS,
  .floorCount      = 4,
  .layouts         = LAYOUTS,
  .layoutCount     = 1,
  .i2cAddress      = 0x08,
  .brightness      = 180,
  .idleTimeoutMs   = 60000,
  .idleAnimationId = ANIM_FLOOR_CHASE,
  .colors = {
    .primary   = 0x00C8B4,   // Main room color
    .secondary = 0xFF6400,   // Accent color
    .highlight = 0xFFFF00,
    .staircase = 0x000000,   // Stairs off
    .duct      = 0xC8C850,   // Duct color
    .refuge    = 0x961414,   // Red for refuge
    .fixed     = 0xFFFFFF,   // Always-on white
    .shop      = 0xFFD700,
    .off       = 0x000000,
  },
};

BuildingLED building(config);

void setup() {
  building.begin();
  building.allOn();
}

void loop() {
  building.update();
}
```

---

## Core Concepts

### Segments
A floor's LED strip is divided into **segments**. Each segment has:
- `ledCount` — how many consecutive LEDs
- `type` — what the segment represents (room, staircase, duct, etc.)
- `roomNumber` — logical room ID (1-based), or `0` if not a room

### Segment Types
| Type | Behavior |
|------|----------|
| `SEG_ROOM` | Normal room — responds to room/preset commands |
| `SEG_STAIRCASE` | Typically off; lit only on `ALL_ON` |
| `SEG_DUCT` | Shaft/duct — uses `colors.duct` |
| `SEG_REFUGE` | Refuge area — red on refuge floors |
| `SEG_LOBBY` | Common area |
| `SEG_SHOP` | Commercial space (ground floor) |
| `SEG_FIXED` | Always-on (never changes, uses `colors.fixed`) |

### Layouts
A **layout** is a template describing the segment structure of a floor type. Multiple floors can share the same layout, saving memory. Typical setups have 1-3 layouts (ground, residential, maybe commercial).

### Floor Definitions
Each physical floor maps to:
- An Arduino pin
- A layout index
- A floor type (`FLOOR_RESIDENTIAL`, `FLOOR_GROUND`, `FLOOR_REFUGE`, etc.)

---

## API Reference

### LED Control

```cpp
// Set a single segment
building.setSegment(floorIdx, segIdx, color);
building.setSegmentScaled(floorIdx, segIdx, baseColor, brightness); // 0-255

// Set segments by type
building.setSegmentsByType(floorIdx, SEG_SHOP, color);    // One floor
building.setSegmentsByTypeAll(SEG_STAIRCASE, color);       // All floors

// Light rooms by number across all floors
building.lightRoom(3, color);                              // Room 3 everywhere
const uint8_t rooms[] = {1, 4, 5};
building.lightRooms(rooms, 3, color);                      // Multiple rooms

// Floor-level control
building.setFloor(floorIdx, color);
building.setFloorScaled(floorIdx, baseColor, brightness);

// Apply special segments
building.applyFixedSegments(floorIdx);     // Fixed + refuge + duct
building.applyFixedSegmentsAll();
building.applyRefugeFloors();              // Override room color on refuge floors

// Bulk operations
building.allOn();       // Everything on, respecting types
building.allOff();      // Everything off
building.clearAll();    // Clear buffers (no show)
building.showAll();     // Push all strips
```

### Built-in Animations

| ID | Name | Description |
|----|------|-------------|
| `ANIM_FLOOR_SWEEP_UP` | Floor Sweep Up | Light floors bottom to top |
| `ANIM_FLOOR_SWEEP_DOWN` | Floor Sweep Down | Light floors top to bottom |
| `ANIM_FLOOR_BOUNCE` | Floor Bounce | Sweep up then down, repeat |
| `ANIM_ROOM_SWEEP` | Room Sweep | Cycle through room types |
| `ANIM_CROSSFADE` | Crossfade | Smooth crossfade between floors |
| `ANIM_RANDOM_80` | Random 80% | 80% floors randomly lit, smooth |
| `ANIM_BREATHE_ALL` | Breathe | All LEDs pulse in unison |
| `ANIM_FLOOR_CHASE` | Floor Chase | Scanner-style with trail |

```cpp
building.startAnimation(ANIM_CROSSFADE);
building.stopAnimation();
```

### Custom Animations

```cpp
// Animation function: return true when done
bool myAnimation(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  uint8_t f = step % b.getFloorCount();
  b.clearAll();
  b.setFloor(f, b.colors().primary);
  b.showAll();
  return false;  // false = keep running, true = done
}

void setup() {
  building.begin();
  // Register with ID >= ANIM_USER_START (16)
  building.registerAnimation(ANIM_USER_START, myAnimation, 200, true);
}
```

### Presets

```cpp
void presetShowFlat(BuildingLED& b) {
  b.clearAll();
  const uint8_t rooms[] = {2, 3};
  b.lightRooms(rooms, 2, b.colors().secondary);
  b.applyFixedSegmentsAll();
  b.showAll();
}

void setup() {
  building.begin();
  building.registerPreset(1, presetShowFlat);  // ID 1
}
// Triggered via I2C: CMD_PRESET with a=1
```

### Custom Commands

```cpp
#define CMD_WING_SELECT 0x14

void handleCmd(BuildingLED& b, const BLEDPacket& pkt) {
  if (pkt.cmd == CMD_WING_SELECT) {
    // pkt.a, pkt.b, pkt.c, pkt.d available
    // ... your logic ...
  }
}

void setup() {
  building.begin();
  building.onCommand(handleCmd);
}
```

### Querying State

```cpp
building.getFloorCount();
building.getFloorType(floorIdx);
building.getFloorLedCount(floorIdx);
building.getSegmentCount(floorIdx);
building.getSegmentType(floorIdx, segIdx);
building.getSegmentRoom(floorIdx, segIdx);
building.findSegmentByRoom(floorIdx, roomNumber);  // Returns seg index or 255
building.isRefugeFloor(floorIdx);
building.isAnimating();
building.currentAnimation();
building.getStrip(floorIdx);  // Direct NeoPixel access for advanced use
```

---

## I2C Protocol

Standard 8-byte packet: `[0xAA] [CMD] [A] [B] [C] [D] [CHECKSUM] [0x55]`

**Checksum** = `(CMD + A + B + C + D) & 0xFF`

### Built-in Commands

| CMD | Name | Args | Description |
|-----|------|------|-------------|
| `0x10` | PATTERN | a=animId | Start animation |
| `0x11` | ALL_ON | — | All LEDs on |
| `0x12` | ALL_OFF | — | All LEDs off |
| `0x13` | KEEPALIVE | — | Reset idle timer |
| `0x20` | SET_UNIT | a=floor, b=seg, c=state | Set one segment |
| `0x21` | PRESET | a=presetId | Apply preset |
| `0x22` | ROOM | a=roomNum | Light room on all floors |

Also supports single-byte legacy commands (0x00=off, 0x01=pattern, 0x04=allOn).

---

## Memory Optimization Tips

1. **Share layouts** — If all residential floors are identical, use one layout
2. **Enable BLED_DEBUG only during development** — Set to 0 for production
3. **Use `const` arrays** — Compiler can optimize constant data
4. **Limit animation count** — `BLED_MAX_ANIMATIONS` defaults to 8

### Typical RAM Usage

| Floors | LEDs/Floor | Strips RAM | Library RAM | Total |
|--------|-----------|------------|-------------|-------|
| 10 | 32 | ~640B | ~300B | ~940B |
| 17 | 68 | ~2.3KB | ~300B | ~2.6KB |
| 54 | 32 | ~3.5KB | ~300B | ~3.8KB |

Arduino Mega has 8KB SRAM — all configurations fit comfortably.

---

## Project Structure

```
BuildingLED/
├── library.properties
├── README.md
├── src/
│   ├── BuildingLED.h          # Main class header
│   ├── BuildingLED.cpp        # Implementation
│   ├── BuildingLED_Config.h   # Types, enums, structs
│   └── BuildingLED_Debug.h    # Debug macros
└── examples/
    ├── BasicBuilding/         # Minimal 10-floor example
    ├── PlatinumStyle/         # Complex: ground+residential, fixed rooms, custom cmds
    └── HighRiseBuilding/      # 54-floor tower with refuge floors, RGB fade
```

---

## Installation

Copy the `BuildingLED` folder to your Arduino `libraries` directory:
- **Windows:** `Documents/Arduino/libraries/`
- **macOS:** `~/Documents/Arduino/libraries/`
- **Linux:** `~/Arduino/libraries/`

Restart Arduino IDE. The library appears under **Sketch > Include Library > BuildingLED**.

---

## Migration Guide

Converting an existing project to use this library:

1. **Identify your segments**: List all LED groups per floor (rooms, stairs, ducts)
2. **Create `SegmentDef` arrays**: One per unique floor layout
3. **Map rooms**: Assign `roomNumber` to segments that correspond to rooms
4. **Mark exceptions**: Use `SEG_STAIRCASE`, `SEG_DUCT`, `SEG_FIXED`, `SEG_REFUGE`
5. **Define floors**: Pin + layout index + type
6. **Move presets**: Convert BHK/wing functions to `PresetFn` callbacks
7. **Move custom commands**: Use `onCommand()` handler
8. **Move custom animations**: Convert to `AnimTickFn` (non-blocking!)

---

## Dependencies

- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) (>= 1.10.0)
- Wire (built-in)
