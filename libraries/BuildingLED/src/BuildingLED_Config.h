/*
 * BuildingLED_Config.h
 * Configuration structs and enums for building LED controller
 * 
 * All config arrays should be declared with PROGMEM where possible
 * to save precious SRAM on Arduino Mega.
 */
#ifndef BUILDING_LED_CONFIG_H
#define BUILDING_LED_CONFIG_H

#include <Arduino.h>
#include <avr/pgmspace.h>

/* ═══════════════════════ LIMITS ═══════════════════════ */

#define BLED_MAX_FLOORS       64    // Max floors supported
#define BLED_MAX_SEGMENTS     16    // Max segments per floor
#define BLED_MAX_ROOMS        16    // Max logical rooms
#define BLED_MAX_ANIMATIONS   8     // Max registered animations
#define BLED_MAX_PRESETS      8     // Max registered presets
#define BLED_MAX_EXCEPTIONS   16    // Max exception entries

/* ═══════════════════════ SEGMENT TYPES ═══════════════════════ */

enum SegmentType : uint8_t {
  SEG_ROOM      = 0,  // Normal room - responds to room commands
  SEG_STAIRCASE = 1,  // Staircase - typically off, lit only on ALL_ON
  SEG_DUCT      = 2,  // Duct/shaft - may have special color
  SEG_REFUGE    = 3,  // Refuge area - special color on certain floors
  SEG_LOBBY     = 4,  // Lobby/common area
  SEG_SHOP      = 5,  // Commercial/shop space (ground floor)
  SEG_FIXED     = 6,  // Always-on segment (never changes with commands)
};

/* ═══════════════════════ FLOOR TYPES ═══════════════════════ */

enum FloorType : uint8_t {
  FLOOR_RESIDENTIAL = 0,  // Standard residential floor
  FLOOR_GROUND      = 1,  // Ground floor (may have different segment map)
  FLOOR_COMMERCIAL  = 2,  // Commercial floor
  FLOOR_PODIUM      = 3,  // Podium/parking
  FLOOR_REFUGE      = 4,  // Refuge floor (special color override)
};

/* ═══════════════════════ I2C PROTOCOL ═══════════════════════ */

// Standard 8-byte packet: [0xAA] [CMD] [A] [B] [C] [D] [SUM] [0x55]
// SUM = CMD + A + B + C + D (truncated to uint8_t)

#define BLED_PKT_HEADER   0xAA
#define BLED_PKT_FOOTER   0x55
#define BLED_PKT_SIZE     8

// Built-in command IDs (you can define additional ones in your sketch)
enum BLEDCommand : uint8_t {
  CMD_PATTERN     = 0x10,   // Start animation (a = animation ID)
  CMD_ALL_ON      = 0x11,   // Turn all LEDs on
  CMD_ALL_OFF     = 0x12,   // Turn all LEDs off
  CMD_KEEPALIVE   = 0x13,   // Reset idle timer only
  CMD_SET_UNIT    = 0x20,   // Set floor/segment/state  (a=floor, b=seg, c=state)
  CMD_PRESET      = 0x21,   // Apply preset (a = preset ID)
  CMD_ROOM        = 0x22,   // Light specific room on all floors (a = room#)
  CMD_BUILDING    = 0x30,   // Building-level on/off (a=building, b=action)
};

/* ═══════════════════════ PACKET STRUCT ═══════════════════════ */

struct BLEDPacket {
  uint8_t cmd;
  uint8_t a, b, c, d;
};

/* ═══════════════════════ SEGMENT DEFINITION ═══════════════════════ */

// Defines one segment within a floor's LED strip
struct SegmentDef {
  uint8_t ledCount;     // Number of LEDs in this segment
  SegmentType type;     // What kind of segment (room, stair, duct, etc.)
  uint8_t roomNumber;   // Logical room number (1-based), 0 = not a room
};

/* ═══════════════════════ FLOOR LAYOUT ═══════════════════════ */

// Defines the layout template for a type of floor
// Multiple floors can share the same layout (saves PROGMEM)
struct FloorLayout {
  const SegmentDef* segments;   // Array of segment definitions (PROGMEM)
  uint8_t segmentCount;         // Number of segments
  uint16_t totalLeds;           // Total LEDs on this floor type
};

/* ═══════════════════════ FLOOR DEFINITION ═══════════════════════ */

// Defines one physical floor
struct FloorDef {
  uint8_t pin;                  // Arduino pin for this floor's LED strip
  uint8_t layoutIndex;          // Index into layouts[] array
  FloorType type;               // Floor type
};

/* ═══════════════════════ COLOR ROLES ═══════════════════════ */

// Named color roles for the building - set once, used everywhere
struct ColorPalette {
  uint32_t primary;      // Main room color (e.g., warm white, turquoise)
  uint32_t secondary;    // Secondary accent (e.g., orange, coral)
  uint32_t highlight;    // Highlight/selection color
  uint32_t staircase;    // Staircase color (often off)
  uint32_t duct;         // Duct color
  uint32_t refuge;       // Refuge floor color (often red)
  uint32_t fixed;        // Always-on segment color (often white)
  uint32_t shop;         // Shop/commercial color
  uint32_t off;          // Off color (always 0)
};

/* ═══════════════════════ ANIMATION CALLBACK ═══════════════════════ */

// Forward declaration
class BuildingLED;

// Animation tick function: called every frame, return true when animation is done
typedef bool (*AnimTickFn)(BuildingLED& bld, uint32_t elapsed, uint16_t step);

// Animation definition
struct AnimationDef {
  AnimTickFn tickFn;        // Tick function
  uint16_t frameIntervalMs; // Milliseconds between frames
  bool looping;             // Auto-restart when done?
};

// Preset callback: applies a specific lighting configuration
typedef void (*PresetFn)(BuildingLED& bld);

/* ═══════════════════════ BUILDING CONFIG ═══════════════════════ */

// Master configuration struct - everything the library needs
struct BuildingConfig {
  // Floors
  const FloorDef* floors;         // Array of floor definitions (PROGMEM ok)
  uint8_t floorCount;

  // Layouts (shared templates)
  const FloorLayout* layouts;     // Array of layout templates
  uint8_t layoutCount;

  // I2C
  uint8_t i2cAddress;

  // Display
  uint8_t brightness;

  // Idle timeout
  uint32_t idleTimeoutMs;         // 0 = disabled

  // Default idle animation index
  uint8_t idleAnimationId;
  
  // Color palette
  ColorPalette colors;
};

#endif // BUILDING_LED_CONFIG_H
