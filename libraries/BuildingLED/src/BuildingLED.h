/*
 * BuildingLED.h
 * Main controller class for building scale-model LED systems
 * 
 * Designed for Arduino Mega 2560 with WS2812B strips.
 * Handles I2C communication, floor/room/segment mapping,
 * animations, presets, and idle timeout.
 * 
 * Usage:
 *   1. Define your segments, layouts, and floor definitions
 *   2. Create a BuildingConfig
 *   3. Instantiate BuildingLED with the config
 *   4. Register animations and presets
 *   5. Call begin() in setup(), update() in loop()
 * 
 * Author: Aquib Ansari
 * License: MIT
 */
#ifndef BUILDING_LED_H
#define BUILDING_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <avr/pgmspace.h>

#include "BuildingLED_Config.h"
#include "BuildingLED_Debug.h"

/* ═══════════════════════ BUILT-IN ANIMATION IDs ═══════════════════════ */

// Reserve IDs 0-15 for built-in animations; user animations start at 16
enum BuiltInAnim : uint8_t {
  ANIM_NONE             = 0,
  ANIM_FLOOR_SWEEP_UP   = 1,   // Light floors bottom to top, then clear
  ANIM_FLOOR_SWEEP_DOWN = 2,   // Light floors top to bottom, then clear
  ANIM_FLOOR_BOUNCE     = 3,   // Sweep up then down
  ANIM_ROOM_SWEEP       = 4,   // Light each room type across all floors
  ANIM_CROSSFADE        = 5,   // Crossfade between floors
  ANIM_RANDOM_80        = 6,   // 80% random floor breathing
  ANIM_BREATHE_ALL      = 7,   // All LEDs breathe in unison
  ANIM_FLOOR_CHASE      = 8,   // Single lit floor moving up/down
  ANIM_USER_START       = 16,  // User animations start here
};

/* ═══════════════════════ MAIN CLASS ═══════════════════════ */

class BuildingLED {
public:
  // ── Construction ──
  BuildingLED(const BuildingConfig& cfg);
  ~BuildingLED();

  // ── Lifecycle ──
  void begin();                 // Initialize strips, I2C, etc.
  void update();                // Call every loop() iteration

  // ── Strip Access (for custom code) ──
  Adafruit_NeoPixel* getStrip(uint8_t floorIdx);
  uint8_t getFloorCount() const { return _cfg.floorCount; }
  
  // ── Segment Helpers ──
  // Get the start LED index and length of a segment on a floor
  bool getSegmentRange(uint8_t floorIdx, uint8_t segIdx, uint16_t& outStart, uint16_t& outLen);
  
  // Get segment info for a floor
  uint8_t getSegmentCount(uint8_t floorIdx);
  SegmentType getSegmentType(uint8_t floorIdx, uint8_t segIdx);
  uint8_t getSegmentRoom(uint8_t floorIdx, uint8_t segIdx);
  
  // Find segment index by room number on a given floor layout
  // Returns 255 if not found
  uint8_t findSegmentByRoom(uint8_t floorIdx, uint8_t roomNumber);

  // ── LED Control ──
  // Set a segment to a color (no show)
  void setSegment(uint8_t floorIdx, uint8_t segIdx, uint32_t color);
  
  // Set a segment with brightness scaling 0-255 (no show)
  void setSegmentScaled(uint8_t floorIdx, uint8_t segIdx, uint32_t baseColor, uint8_t brightness);

  // Set all segments of a given type to a color on one floor (no show)
  void setSegmentsByType(uint8_t floorIdx, SegmentType type, uint32_t color);
  
  // Set all segments of a given type across ALL floors (no show)
  void setSegmentsByTypeAll(SegmentType type, uint32_t color);

  // Light a specific room number across all applicable floors (clears first)
  void lightRoom(uint8_t roomNumber, uint32_t color);
  
  // Light a list of room numbers across all floors (clears first)
  void lightRooms(const uint8_t* roomNumbers, uint8_t count, uint32_t color);

  // Set entire floor to a color (no show)
  void setFloor(uint8_t floorIdx, uint32_t color);
  
  // Set floor with brightness (no show)
  void setFloorScaled(uint8_t floorIdx, uint32_t baseColor, uint8_t brightness);

  // Apply fixed/always-on segments (refuge, fixed type) on a floor (no show)
  void applyFixedSegments(uint8_t floorIdx);
  
  // Apply fixed segments on ALL floors (no show)
  void applyFixedSegmentsAll();

  // Apply refuge coloring on refuge floors (no show)
  void applyRefugeFloors();

  // ── Bulk Operations ──
  void allOn();                 // All LEDs to primary color (respects types)
  void allOff();                // All LEDs off
  void clearFloor(uint8_t floorIdx);  // Clear one floor (no show)
  void clearAll();              // Clear all (no show)
  void showFloor(uint8_t floorIdx);   // Push one floor
  void showAll();               // Push all floors

  // ── Color Helpers ──
  uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
  uint32_t scaleColor(uint32_t color, uint8_t brightness);
  const ColorPalette& colors() const { return _cfg.colors; }

  // Get color for a segment based on its type
  uint32_t colorForType(SegmentType type);
  
  // Check if a floor is a refuge floor
  bool isRefugeFloor(uint8_t floorIdx);

  // ── Animation System ──
  void registerAnimation(uint8_t id, AnimTickFn fn, uint16_t frameMs, bool loop = true);
  void startAnimation(uint8_t id);
  void stopAnimation();
  bool isAnimating() const { return _animActive; }
  uint8_t currentAnimation() const { return _animId; }

  // ── Preset System ──
  void registerPreset(uint8_t id, PresetFn fn);
  void applyPreset(uint8_t id);

  // ── Command System ──
  // Register a callback for custom commands not handled by built-in logic
  typedef void (*CommandHandler)(BuildingLED& bld, const BLEDPacket& pkt);
  void onCommand(CommandHandler handler);

  // ── Idle Timeout ──
  void resetIdleTimer();
  void setIdleTimeout(uint32_t ms);
  void setIdleAnimation(uint8_t animId);

  // ── State Query ──
  FloorType getFloorType(uint8_t floorIdx);
  uint16_t getFloorLedCount(uint8_t floorIdx);

private:
  // ── Config ──
  const BuildingConfig& _cfg;

  // ── Strips ──
  Adafruit_NeoPixel** _strips;    // Dynamic array of strip pointers
  
  // ── I2C ──
  static BuildingLED* _instance;  // For ISR callback
  volatile bool _hasPkt;
  volatile uint8_t _isrRaw[BLED_PKT_SIZE];
  volatile bool _hasLegacy;
  volatile uint8_t _legacyCmd;
  
  static void _onI2CRecv(int howMany);
  bool _readPacket(BLEDPacket& pkt);
  void _handlePacket(const BLEDPacket& pkt);

  // ── Animations ──
  struct AnimSlot {
    AnimTickFn fn;
    uint16_t frameMs;
    bool loop;
    bool registered;
  };
  AnimSlot _anims[BLED_MAX_ANIMATIONS];
  bool _animActive;
  uint8_t _animId;
  uint32_t _animStartTime;
  uint32_t _animLastFrame;
  uint16_t _animStep;

  void _tickAnimation();

  // ── Presets ──
  struct PresetSlot {
    PresetFn fn;
    bool registered;
  };
  PresetSlot _presets[BLED_MAX_PRESETS];

  // ── Command handler ──
  CommandHandler _cmdHandler;

  // ── Idle ──
  uint32_t _lastCmdTime;
  bool _idleTriggered;

  void _checkIdle();

  // ── Layout helpers ──
  const FloorLayout& _getLayout(uint8_t floorIdx);
  const SegmentDef& _getSegment(uint8_t floorIdx, uint8_t segIdx);

  // ── Built-in animation ticks ──
  static bool _animFloorSweepUp(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animFloorSweepDown(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animFloorBounce(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animRoomSweep(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animCrossfade(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animRandom80(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animBreatheAll(BuildingLED& b, uint32_t elapsed, uint16_t step);
  static bool _animFloorChase(BuildingLED& b, uint32_t elapsed, uint16_t step);
};

#endif // BUILDING_LED_H
