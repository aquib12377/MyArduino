/*
 * ═══════════════════════════════════════════════════════════════════════════
 * JP INFRA - ARDUINO MEGA SLAVE (CONFIG.CSV PIN MAPPING)
 * Uses individual pins per floor with lobby exclusion
 * LIBRARY: Adafruit NeoPixel (Memory Optimized)
 *
 * v3.4 CHANGES:
 *   • Per-floor LED start offset (FLOOR_LED_OFFSET[])
 *     - Ground Commercial (F1), First Commercial (F2), Residential 8 (F10):
 *       offset = 0  → rooms start at LED 0
 *     - All other 13 floors:
 *       offset = 4  → first 4 LEDs are physically present but NOT lit,
 *                     room data begins at LED 4
 *   • setSegmentColor() uses the per-floor offset.
 *   • fillGradient() now spreads the gradient across only the 85 room
 *     LEDs (skipping the dead 4-LED zone on offset floors).
 *   • patternRandomFade() and handleCustomLEDs() honour the offset.
 *   • clearFloor() still clears all 89 physical LEDs (safety).
 * NO PRINTF - Arduino Mega Compatible
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * HARDWARE CONFIGURATION:
 * ────────────────────────────────────────────
 * Physical strip per floor: 89 WS2812B LEDs
 *
 * On MOST floors (offset = 4), the layout is:
 *   LEDs 0..3   → unused / dead zone (kept dark)
 *   LEDs 4..18  → Room 1   (15 LEDs)
 *   LEDs 19..32 → Room 2   (14 LEDs)
 *   LEDs 33..47 → Room 3   (15 LEDs)
 *   LEDs 48..61 → Room 4   (14 LEDs)
 *   LEDs 62..75 → Room 5   (14 LEDs)
 *   LEDs 76..88 → Room 6   (13 LEDs)
 *
 * On EXEMPT floors (offset = 0: F1 Ground, F2 First, F10 Res 8):
 *   LEDs 0..14  → Room 1   (15 LEDs)
 *   LEDs 15..28 → Room 2   (14 LEDs)
 *   LEDs 29..43 → Room 3   (15 LEDs)
 *   LEDs 44..57 → Room 4   (14 LEDs)
 *   LEDs 58..71 → Room 5   (14 LEDs)
 *   LEDs 72..84 → Room 6   (13 LEDs)
 *   LEDs 85..88 → unused tail (kept dark)
 *
 * Total controllable room LEDs per floor: 85
 *
 * FLOOR PIN MAPPING:
 * Floor 1  (Ground)     → Pin 13
 * Floor 2  (First)      → Pin 12
 * Floor 3  (Res 1)      → Pin 11
 * Floor 4  (Res 2)      → Pin 10
 * Floor 5  (Res 3)      → Pin 9
 * Floor 6  (Res 4)      → Pin 8
 * Floor 7  (Res 5)      → Pin 7
 * Floor 8  (Res 6)      → Pin 6
 * Floor 9  (Res 7)      → Pin 5
 * Floor 10 (Res 8)      → Pin 4
 * Floor 11 (Res 9)      → Pin 3
 * Floor 12 (Res 10)     → Pin 2
 * Floor 13 (Res 11)     → Pin 14
 * Floor 14 (Res 12)     → Pin 15
 * Floor 15 (Res 13)     → Pin 16
 * Floor 16 (Amenities)  → Pin 17
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <Wire.h>
#include <Adafruit_NeoPixel.h>

/* ══════════════════════ CONFIGURATION ══════════════════════ */

#define I2C_SLAVE_ADDRESS 0x08

// Command codes from ESP32
#define CMD_ALL_ON 0x01
#define CMD_ALL_OFF 0x02
#define CMD_CLASSIC 0x13
#define CMD_FLOOR_ON 0x04
#define CMD_FLOOR_OFF 0x05
#define CMD_ROOM_ON 0x06
#define CMD_ROOM_OFF 0x07
#define CMD_AVAIL_BASE_GREEN 0x10
#define CMD_AVAIL_FLOOR_SET 0x11
#define CMD_CUSTOM_LEDS 0x12
#define CMD_PATTERN 0x03
#define CMD_PING 0x0A

// Status codes
#define STATUS_SOLD 0
#define STATUS_AVAILABLE 1
#define STATUS_BLOCKED 2

/* ══════════════════════ FLOOR & ROOM CONFIGURATION ══════════════════════ */

#define NUM_FLOORS 16
#define SEGMENTS_PER_FLOOR 8  // Total segments including lobbies
#define ACTUAL_ROOMS 6        // Actual controllable rooms (segments 2-7)
#define LEDS_PER_FLOOR 89     // Total physical LEDs per strip

// Floor pin mapping (from config.csv column 5)
const uint8_t FLOOR_PINS[NUM_FLOORS] = {
  13,  // Floor 1  (Ground Commercial)
  12,  // Floor 2  (First Commercial)
  11,  // Floor 3  (Residential 1)
  10,  // Floor 4  (Residential 2)
  9,   // Floor 5  (Residential 3)
  8,   // Floor 6  (Residential 4)
  7,   // Floor 7  (Residential 5)
  6,   // Floor 8  (Residential 6)
  5,   // Floor 9  (Residential 7)
  4,   // Floor 10 (Residential 8)
  3,   // Floor 11 (Residential 9)
  2,   // Floor 12 (Residential 10)
  14,  // Floor 13 (Residential 11)
  15,  // Floor 14 (Residential 12)
  16,  // Floor 15 (Residential 13)
  17   // Floor 16 (Rooftop Amenities)
};

/* ──────────────────────────────────────────────────────────────────────
 * PER-FLOOR LED START OFFSET  (v3.4)
 * ──────────────────────────────────────────────────────────────────────
 * Most floors have 4 unused LEDs at the physical start of the strip.
 * Set offset = 4 on those floors, 0 on the three exempt floors.
 *
 *   F1  Ground Commercial → 0  (no unused head LEDs)
 *   F2  First Commercial  → 0  (no unused head LEDs)
 *   F10 Residential 8     → 0  (no unused head LEDs)
 *   All other floors      → 4  (skip first 4 LEDs)
 * ─────────────────────────────────────────────────────────────────── */
const uint8_t FLOOR_LED_OFFSET[NUM_FLOORS] = {
  0,   // Floor 1  (Ground Commercial)   - EXEMPT
  0,   // Floor 2  (First Commercial)    - EXEMPT
  4,   // Floor 3  (Residential 1)
  4,   // Floor 4  (Residential 2)
  4,   // Floor 5  (Residential 3)
  4,   // Floor 6  (Residential 4)
  4,   // Floor 7  (Residential 5)
  4,   // Floor 8  (Residential 6)
  4,   // Floor 9  (Residential 7)
  0,   // Floor 10 (Residential 8)       - EXEMPT
  4,   // Floor 11 (Residential 9)
  4,   // Floor 12 (Residential 10)
  4,   // Floor 13 (Residential 11)
  4,   // Floor 14 (Residential 12)
  4,   // Floor 15 (Residential 13)
  4    // Floor 16 (Rooftop Amenities)
};

// LED count per segment. Lobbies are NOT lit (count = 0).
// Rooms 1..6 occupy segments 2..7.
const uint8_t SEGMENT_LED_COUNTS[SEGMENTS_PER_FLOOR] = {
  0,   // Segment 0: Lobby (DON'T LIGHT)
  0,   // Segment 1: Lobby (DON'T LIGHT)
  15,  // Segment 2: Room 1
  14,  // Segment 3: Room 2
  15,  // Segment 4: Room 3
  14,  // Segment 5: Room 4
  14,  // Segment 6: Room 5
  13   // Segment 7: Room 6
};

/* ══════════════════════ ROOM NUMBER MAPPING ══════════════════════ */

// Room Number (1-6) to Segment Index (2-7) Mapping
uint8_t ROOM_TO_SEGMENT_MAP[ACTUAL_ROOMS] = {
  4,  // Room 1 → Segment 4
  3,  // Room 2 → Segment 3
  2,  // Room 3 → Segment 2
  7,  // Room 4 → Segment 7
  6,  // Room 5 → Segment 6
  5   // Room 6 → Segment 5
};

// Starting LED index for each segment (relative, BEFORE applying floor offset)
uint8_t SEGMENT_START_LED[SEGMENTS_PER_FLOOR];

/* ══════════════════════ NEOPIXEL OBJECTS ══════════════════════ */

// Create NeoPixel object for each floor
Adafruit_NeoPixel strips[NUM_FLOORS] = {
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 13, NEO_GRB + NEO_KHZ800),  // Floor 1
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 12, NEO_GRB + NEO_KHZ800),  // Floor 2
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 11, NEO_GRB + NEO_KHZ800),  // Floor 3
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 10, NEO_GRB + NEO_KHZ800),  // Floor 4
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 9, NEO_GRB + NEO_KHZ800),   // Floor 5
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 8, NEO_GRB + NEO_KHZ800),   // Floor 6
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 7, NEO_GRB + NEO_KHZ800),   // Floor 7
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 6, NEO_GRB + NEO_KHZ800),   // Floor 8
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 5, NEO_GRB + NEO_KHZ800),   // Floor 9
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 4, NEO_GRB + NEO_KHZ800),   // Floor 10
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 3, NEO_GRB + NEO_KHZ800),   // Floor 11
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 2, NEO_GRB + NEO_KHZ800),   // Floor 12
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 14, NEO_GRB + NEO_KHZ800),  // Floor 13
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 15, NEO_GRB + NEO_KHZ800),  // Floor 14
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 16, NEO_GRB + NEO_KHZ800),  // Floor 15
  Adafruit_NeoPixel(LEDS_PER_FLOOR, 17, NEO_GRB + NEO_KHZ800)   // Floor 16
};

/* ══════════════════════ COLOR DEFINITIONS ══════════════════════ */

// General LED colors
#define COLOR_WHITE    strips[0].Color(255, 255, 255)  // Floor - Pure White
#define COLOR_CYAN     strips[0].Color(0, 255, 255)    // Room - Cyan

// Availability colors
#define COLOR_AVAILABLE strips[0].Color(0, 255, 50)    // Green
#define COLOR_BLOCKED   strips[0].Color(255, 180, 0)   // Yellow
#define COLOR_SOLD      strips[0].Color(0, 150, 255)   // Blue

// Classic pattern colors
#define COLOR_CLASSIC_START strips[0].Color(100, 0, 200)
#define COLOR_CLASSIC_END   strips[0].Color(0, 100, 255)

// Pattern animation colors
#define COLOR_PATTERN_FLOOR strips[0].Color(255, 255, 255)
#define COLOR_PATTERN_ROOM  strips[0].Color(0, 200, 255)

/* ══════════════════════ GLOBAL VARIABLES ══════════════════════ */

volatile byte commandBuffer[10];
volatile byte commandLength = 0;
volatile bool newCommand = false;

// Batched update tracking
bool updatePending = false;
unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_BATCH_DELAY = 5;

// Pattern animation control
bool patternRunning = false;
uint8_t currentPattern = 0;

/* ══════════════════════ SETUP ══════════════════════ */

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n============================================"));
  Serial.println(F("JP INFRA - Arduino Mega Slave v3.4"));
  Serial.println(F("Library: Adafruit NeoPixel"));
  Serial.println(F("NEW: Per-floor LED start offset"));
  Serial.println(F("============================================\n"));

  // Initialize I2C as slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  Serial.print(F("[I2C] Slave initialized at address: 0x"));
  Serial.println(I2C_SLAVE_ADDRESS, HEX);

  // Calculate segment start positions (relative to floor's offset)
  initSegmentPositions();

  // Validate and print room mapping
  validateAndPrintRoomMapping();

  // Initialize NeoPixel strips for each floor
  Serial.println(F("[LED] Initializing NeoPixel strips for 16 floors..."));

  for (uint8_t i = 0; i < NUM_FLOORS; i++) {
    strips[i].begin();
    strips[i].setBrightness(200);
    strips[i].show(); // Initialize all pixels to 'off'
  }

  Serial.println(F("[LED] NeoPixel strips initialized"));
  Serial.print(F("  Floors: "));
  Serial.println(NUM_FLOORS);
  Serial.print(F("  LEDs per floor (physical): "));
  Serial.println(LEDS_PER_FLOOR);
  Serial.print(F("  Segments per floor: "));
  Serial.print(SEGMENTS_PER_FLOOR);
  Serial.println(F(" (2 lobbies + 6 rooms)"));

  // Print segment + offset mapping
  printSegmentMapping();
  printFloorOffsetMapping();

  // Test pattern
  Serial.println(F("[LED] Running startup test..."));
  testPattern();

  Serial.println(F("\n[READY] System ready\n"));
}

/* ══════════════════════ MAIN LOOP ══════════════════════ */

void loop() {
  if (newCommand) {
    processCommand();
    newCommand = false;
  }

  // Batched update - only update if pending and enough time passed
  if (updatePending && (millis() - lastUpdateTime >= UPDATE_BATCH_DELAY)) {
    showAllStrips();
    updatePending = false;
  }

  delay(1);
}

bool checkForInterrupt() {
  if (newCommand && commandBuffer[0] != CMD_PATTERN) {
    Serial.println(F("[PATTERN] Interrupted by new command!"));
    return true;
  }
  return false;
}

/* ══════════════════════ I2C EVENT HANDLERS ══════════════════════ */

void receiveEvent(int byteCount) {
  commandLength = 0;
  while (Wire.available() && commandLength < 10) {
    commandBuffer[commandLength++] = Wire.read();
  }
  newCommand = true;
}

void requestEvent() {
  Wire.write(0x01); // ACK
}

/* ══════════════════════ ROOM MAPPING FUNCTIONS ══════════════════════ */

// Convert logical room number (1-6) to physical segment index (2-7)
uint8_t roomToSegment(uint8_t roomNumber) {
  uint8_t arrayIndex = roomNumber - 1;

  if (arrayIndex >= ACTUAL_ROOMS) {
    Serial.print(F("[MAP] ERROR: Invalid room number: "));
    Serial.println(roomNumber);
    return 255;
  }

  return ROOM_TO_SEGMENT_MAP[arrayIndex];
}

// Validate room mapping configuration
bool validateRoomMapping() {
  bool valid = true;
  bool segmentUsed[SEGMENTS_PER_FLOOR] = {false};

  Serial.println(F("[MAP] Validating room mapping..."));

  for (uint8_t i = 0; i < ACTUAL_ROOMS; i++) {
    uint8_t segment = ROOM_TO_SEGMENT_MAP[i];

    if (segment < 2 || segment >= SEGMENTS_PER_FLOOR) {
      Serial.print(F("[MAP] ERROR: Room "));
      Serial.print(i + 1);
      Serial.print(F(" maps to invalid segment "));
      Serial.println(segment);
      valid = false;
    }

    if (segmentUsed[segment]) {
      Serial.print(F("[MAP] ERROR: Segment "));
      Serial.print(segment);
      Serial.println(F(" is mapped to multiple rooms!"));
      valid = false;
    }

    segmentUsed[segment] = true;
  }

  if (valid) {
    Serial.println(F("[MAP] Room mapping validation PASSED"));
  } else {
    Serial.println(F("[MAP] Room mapping validation FAILED!"));
    Serial.println(F("[MAP] Using default mapping as fallback"));
    for (uint8_t i = 0; i < ACTUAL_ROOMS; i++) {
      ROOM_TO_SEGMENT_MAP[i] = i + 2;
    }
  }

  return valid;
}

void validateAndPrintRoomMapping() {
  validateRoomMapping();
  printRoomMapping();
}

void printRoomMapping() {
  Serial.println(F("\n[MAP] ═══════════════════════════════════════"));
  Serial.println(F("[MAP] ROOM NUMBER MAPPING CONFIGURATION"));
  Serial.println(F("[MAP] ═══════════════════════════════════════"));
  Serial.println(F("[MAP] Room # | Physical Segment | LED Count"));
  Serial.println(F("[MAP] ---------|------------------|----------"));

  for (uint8_t room = 1; room <= ACTUAL_ROOMS; room++) {
    uint8_t segment = roomToSegment(room);
    uint8_t ledCount = SEGMENT_LED_COUNTS[segment];

    Serial.print(F("[MAP]   "));
    Serial.print(room);
    Serial.print(F("     |        "));
    Serial.print(segment);
    Serial.print(F("         |    "));
    Serial.print(ledCount);
    Serial.println(F(" LEDs"));
  }

  Serial.println(F("[MAP] ═══════════════════════════════════════\n"));
}

void printFloorOffsetMapping() {
  Serial.println(F("[MAP] ═══════════════════════════════════════"));
  Serial.println(F("[MAP] PER-FLOOR LED START OFFSET"));
  Serial.println(F("[MAP] ═══════════════════════════════════════"));
  Serial.println(F("[MAP] Floor | Pin | Offset | Rooms occupy"));
  Serial.println(F("[MAP] ------|-----|--------|--------------"));

  for (uint8_t f = 0; f < NUM_FLOORS; f++) {
    uint8_t off = FLOOR_LED_OFFSET[f];
    Serial.print(F("[MAP]   "));
    if (f + 1 < 10) Serial.print(' ');
    Serial.print(f + 1);
    Serial.print(F("  |  "));
    if (FLOOR_PINS[f] < 10) Serial.print(' ');
    Serial.print(FLOOR_PINS[f]);
    Serial.print(F(" |   "));
    Serial.print(off);
    Serial.print(F("    | LEDs "));
    Serial.print(off);
    Serial.print(F(".."));
    Serial.println(off + 84);  // 85 room LEDs total
  }
  Serial.println(F("[MAP] ═══════════════════════════════════════\n"));
}

/* ══════════════════════ SEGMENT POSITION CALCULATION ══════════════════════ */

void initSegmentPositions() {
  Serial.println(F("[MAP] Calculating segment positions (relative)..."));

  uint8_t currentPos = 0;
  for (uint8_t seg = 0; seg < SEGMENTS_PER_FLOOR; seg++) {
    SEGMENT_START_LED[seg] = currentPos;
    currentPos += SEGMENT_LED_COUNTS[seg];
  }

  Serial.println(F("[MAP] Segment positions calculated"));
}

void printSegmentMapping() {
  Serial.println(F("\n[MAP] Physical Segment Layout (relative, offset added per floor):"));
  Serial.println(F("Seg | Type   | LEDs | RelStart | RelEnd"));
  Serial.println(F("----|--------|------|----------|-------"));

  for (uint8_t seg = 0; seg < SEGMENTS_PER_FLOOR; seg++) {
    const char* type = (seg <= 1) ? "LOBBY" : "ROOM";
    uint8_t roomNum = (seg <= 1) ? 0 : (seg - 1);
    uint8_t start = SEGMENT_START_LED[seg];
    uint8_t end = (SEGMENT_LED_COUNTS[seg] == 0) ? start : (start + SEGMENT_LED_COUNTS[seg] - 1);

    Serial.print(F(" "));
    Serial.print(seg);
    Serial.print(F("  | "));
    Serial.print(type);
    Serial.print(F(" "));
    Serial.print(roomNum);
    Serial.print(F(" | "));
    Serial.print(SEGMENT_LED_COUNTS[seg]);
    Serial.print(F("   | "));
    Serial.print(start);
    Serial.print(F("        | "));
    Serial.println(end);
  }
  Serial.println();
}

/* ══════════════════════ COMMAND PROCESSING ══════════════════════ */

void processCommand() {
  if (commandLength == 0) return;

  byte cmd = commandBuffer[0];

  Serial.print(F("[CMD] Received: 0x"));
  Serial.print(cmd, HEX);
  if (commandLength > 1) {
    Serial.print(F(" Params:"));
    for (byte i = 1; i < commandLength; i++) {
      Serial.print(F(" "));
      Serial.print(commandBuffer[i]);
    }
  }
  Serial.println();

  switch(cmd) {
    case CMD_ALL_ON:
      handleAllOn();
      break;

    case CMD_ALL_OFF:
      handleAllOff();
      break;

    case CMD_CLASSIC:
      handleClassic();
      break;

    case CMD_FLOOR_ON:
      if (commandLength >= 2) {
        handleFloorOn(commandBuffer[1]);
      }
      break;

    case CMD_FLOOR_OFF:
      if (commandLength >= 2) {
        handleFloorOff(commandBuffer[1]);
      }
      break;

    case CMD_ROOM_ON:
      if (commandLength >= 3) {
        handleRoomOn(commandBuffer[1], commandBuffer[2]);
      }
      break;

    case CMD_ROOM_OFF:
      if (commandLength >= 3) {
        handleRoomOff(commandBuffer[1], commandBuffer[2]);
      }
      break;

    case CMD_AVAIL_BASE_GREEN:
      handleAvailBaseGreen();
      break;

    case CMD_AVAIL_FLOOR_SET:
      if (commandLength >= 4) {
        handleAvailFloorSet(commandBuffer[1], commandBuffer[2], commandBuffer[3]);
      }
      break;

    case CMD_CUSTOM_LEDS:
      if (commandLength >= 6) {
        handleCustomLEDs(commandBuffer[1], commandBuffer[2],
                         commandBuffer[3], commandBuffer[4], commandBuffer[5]);
      }
      break;

    case CMD_PATTERN:
      handlePatternCommand();
      break;

    case CMD_PING:
      Serial.println(F("[CMD] PING received"));
      break;

    default:
      Serial.print(F("[CMD] Unknown command: 0x"));
      Serial.println(cmd, HEX);
      break;
  }
}

/* ══════════════════════ COMMAND HANDLERS ══════════════════════ */

void handleAllOn() {
  Serial.println(F("[LED] All ON (White) - Excluding lobbies"));

  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    // Ensure dead-zone LEDs (offset region) stay dark
    clearFloor(floor);
    for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
      setSegmentColor(floor, seg, COLOR_WHITE);
    }
  }

  scheduleUpdate();
}

void handleAllOff() {
  Serial.println(F("[LED] All OFF"));

  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    clearFloor(floor);
  }

  scheduleUpdate();
}

void handleClassic() {
  Serial.println(F("[LED] Classic gradient pattern"));

  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    fillGradient(floor, COLOR_CLASSIC_START, COLOR_CLASSIC_END);
  }

  scheduleUpdate();
}

void handleFloorOn(byte floor) {
  Serial.print(F("[LED] Floor ON: "));
  Serial.println(floor);

  byte floorIndex = floor - 1;

  if (floorIndex >= NUM_FLOORS) {
    Serial.println(F("[LED] Invalid floor number"));
    return;
  }

  clearFloor(floorIndex);  // keep dead-zone dark
  for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
    setSegmentColor(floorIndex, seg, COLOR_WHITE);
  }

  scheduleUpdate();
}

void handleFloorOff(byte floor) {
  Serial.print(F("[LED] Floor OFF: "));
  Serial.println(floor);

  byte floorIndex = floor - 1;

  if (floorIndex >= NUM_FLOORS) {
    Serial.println(F("[LED] Invalid floor number"));
    return;
  }

  clearFloor(floorIndex);
  scheduleUpdate();
}

void handleRoomOn(byte floor, byte room) {
  Serial.print(F("[LED] Room ON: Floor "));
  Serial.print(floor);
  Serial.print(F(", Room "));
  Serial.println(room);

  byte floorIndex = floor - 1;

  if (floorIndex >= NUM_FLOORS) {
    Serial.println(F("[LED] Invalid floor number"));
    return;
  }

  byte segmentIndex = roomToSegment(room);

  if (segmentIndex == 255 || segmentIndex < 2 || segmentIndex >= SEGMENTS_PER_FLOOR) {
    Serial.print(F("[LED] Invalid room number (must be 1-6): "));
    Serial.println(room);
    return;
  }

  Serial.print(F("[LED] Mapped: Room "));
  Serial.print(room);
  Serial.print(F(" -> Segment "));
  Serial.print(segmentIndex);
  Serial.print(F("  (floor offset = "));
  Serial.print(FLOOR_LED_OFFSET[floorIndex]);
  Serial.println(F(")"));

  setSegmentColor(floorIndex, segmentIndex, COLOR_CYAN);
  scheduleUpdate();
}

void handleRoomOff(byte floor, byte room) {
  Serial.print(F("[LED] Room OFF: Floor "));
  Serial.print(floor);
  Serial.print(F(", Room "));
  Serial.println(room);

  byte floorIndex = floor - 1;
  byte segmentIndex = roomToSegment(room);

  if (floorIndex >= NUM_FLOORS || segmentIndex == 255 ||
      segmentIndex < 2 || segmentIndex >= SEGMENTS_PER_FLOOR) {
    Serial.println(F("[LED] Invalid floor/room number"));
    return;
  }

  Serial.print(F("[LED] Mapped: Room "));
  Serial.print(room);
  Serial.print(F(" -> Segment "));
  Serial.println(segmentIndex);

  setSegmentColor(floorIndex, segmentIndex, strips[0].Color(0, 0, 0));
  scheduleUpdate();
}

/* ══════════════════════ AVAILABILITY HANDLERS ══════════════════════ */

void handleAvailBaseGreen() {
  Serial.println(F("[AVAIL] Setting base GREEN for availability mode"));

  // Turn OFF commercial floors (1-2)
  clearFloor(0);
  clearFloor(1);

  // Turn GREEN residential floors (3-15) - only rooms, not lobbies
  for (uint8_t floor = 2; floor < 15; floor++) {
    clearFloor(floor);  // ensure dead-zone dark
    for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
      setSegmentColor(floor, seg, COLOR_AVAILABLE);
    }
  }

  // Turn OFF amenities floor (16)
  clearFloor(15);

  // IMMEDIATE update for base green
  showAllStrips();
  Serial.println(F("[AVAIL] Base green set complete"));
}

void handleAvailFloorSet(byte floor, byte room, byte status) {
  uint32_t color;
  const char* statusName;

  switch(status) {
    case STATUS_SOLD:
      color = COLOR_SOLD;
      statusName = "SOLD (Blue)";
      break;
    case STATUS_AVAILABLE:
      color = COLOR_AVAILABLE;
      statusName = "AVAILABLE (Green)";
      break;
    case STATUS_BLOCKED:
      color = COLOR_BLOCKED;
      statusName = "BLOCKED (Yellow)";
      break;
    default:
      Serial.print(F("[AVAIL] Unknown status: "));
      Serial.println(status);
      return;
  }

  Serial.print(F("[AVAIL] "));
  Serial.print(statusName);
  Serial.print(F(" - Floor "));
  Serial.print(floor);
  Serial.print(F(", Room "));
  Serial.println(room);

  byte floorIndex = floor - 1;
  byte segmentIndex = roomToSegment(room);

  if (floorIndex >= NUM_FLOORS || segmentIndex == 255 ||
      segmentIndex < 2 || segmentIndex >= SEGMENTS_PER_FLOOR) {
    Serial.println(F("[AVAIL] Invalid floor/room number"));
    return;
  }

  Serial.print(F("[AVAIL] Mapped: Room "));
  Serial.print(room);
  Serial.print(F(" -> Segment "));
  Serial.println(segmentIndex);

  setSegmentColor(floorIndex, segmentIndex, color);
  scheduleUpdate();
}

/* ══════════════════════ CUSTOM LED CONTROL ══════════════════════ */

/*
 * CMD_CUSTOM_LEDS paints `ledCount` LEDs starting at the floor's room area
 * (i.e. at FLOOR_LED_OFFSET[floor]). Remaining LEDs on that floor (including
 * the dead-zone head LEDs on offset floors) are cleared.
 */
void handleCustomLEDs(byte floor, byte ledCount, byte r, byte g, byte b) {
  Serial.print(F("[CUSTOM] Floor "));
  Serial.print(floor);
  Serial.print(F(", Count: "));
  Serial.print(ledCount);
  Serial.print(F(", RGB("));
  Serial.print(r);
  Serial.print(F(","));
  Serial.print(g);
  Serial.print(F(","));
  Serial.print(b);
  Serial.println(F(")"));

  byte floorIndex = floor - 1;

  if (floorIndex >= NUM_FLOORS) {
    Serial.println(F("[CUSTOM] Invalid floor number"));
    return;
  }

  uint8_t offset = FLOOR_LED_OFFSET[floorIndex];
  uint8_t maxRoomLEDs = LEDS_PER_FLOOR - offset;       // room area capacity
  uint8_t actualCount = (ledCount > maxRoomLEDs) ? maxRoomLEDs : ledCount;

  uint32_t customColor = strips[0].Color(r, g, b);

  // Clear entire strip first (handles dead zone + tail)
  for (uint16_t i = 0; i < LEDS_PER_FLOOR; i++) {
    strips[floorIndex].setPixelColor(i, 0);
  }

  // Paint requested LEDs in the room area
  for (uint8_t i = 0; i < actualCount; i++) {
    strips[floorIndex].setPixelColor(offset + i, customColor);
  }

  scheduleUpdate();
  Serial.print(F("[CUSTOM] Painted "));
  Serial.print(actualCount);
  Serial.print(F(" LEDs starting at index "));
  Serial.println(offset);
}

/* ══════════════════════ PATTERN ANIMATION HANDLERS ══════════════════════ */

void handlePatternCommand() {
  Serial.println(F("\n[PATTERN] Starting pattern sequence..."));
  Serial.println(F("[PATTERN] ═══════════════════════════════════"));

  // Pattern 1: Floor by Floor
  Serial.println(F("[PATTERN] 1/4: Floor by Floor Animation"));
  patternFloorByFloor();
  delay(500);
  if (checkForInterrupt()) return;

  // Pattern 2: Room by Room
  Serial.println(F("[PATTERN] 2/4: Room by Room Animation"));
  patternRoomByRoom();
  delay(500);
  if (checkForInterrupt()) return;

  // Pattern 3: RGB Smooth Fading
  Serial.println(F("[PATTERN] 3/4: RGB Smooth Fading"));
  patternRGBFade();
  delay(500);
  if (checkForInterrupt()) return;

  // Pattern 4: Random LED Fading (80% of LEDs)
  Serial.println(F("[PATTERN] 4/4: Random LED Fading (80%)"));
  patternRandomFade();
  delay(500);
  if (checkForInterrupt()) return;

  // Clear all after patterns
  Serial.println(F("[PATTERN] Clearing all LEDs"));
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    clearFloor(floor);
  }
  showAllStrips();

  Serial.println(F("[PATTERN] ═══════════════════════════════════"));
  Serial.println(F("[PATTERN] Pattern sequence complete!\n"));
}

/* ══════════════════════ PATTERN 1: FLOOR BY FLOOR ══════════════════════ */

void patternFloorByFloor() {
  const uint16_t FLOOR_HOLD_TIME = 300;
  const uint8_t FADE_STEPS = 20;
  const uint8_t FADE_DELAY = 10;

  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    Serial.print(F("[PATTERN] Lighting Floor "));
    Serial.println(floor + 1);
    if (checkForInterrupt()) return;

    // Fade IN
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = (step * 255) / FADE_STEPS;
      uint32_t color = strips[0].Color(brightness, brightness, brightness);
      clearFloor(floor); // ensure dead-zone stays dark on each frame
      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        setSegmentColor(floor, seg, color);
      }
      strips[floor].show();
      delay(FADE_DELAY);
    }

    // Hold
    delay(FLOOR_HOLD_TIME);

    // Fade OUT
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = 255 - ((step * 255) / FADE_STEPS);
      uint32_t color = strips[0].Color(brightness, brightness, brightness);
      clearFloor(floor);
      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        setSegmentColor(floor, seg, color);
      }
      strips[floor].show();
      delay(FADE_DELAY);
    }

    clearFloor(floor);
    strips[floor].show();
  }
}

/* ══════════════════════ PATTERN 2: ROOM BY ROOM ══════════════════════ */

void patternRoomByRoom() {
  const uint16_t ROOM_HOLD_TIME = 250;
  const uint8_t FADE_STEPS = 15;
  const uint8_t FADE_DELAY = 8;

  for (uint8_t room = 1; room <= ACTUAL_ROOMS; room++) {
    Serial.print(F("[PATTERN] Lighting Room "));
    Serial.println(room);
    if (checkForInterrupt()) return;

    uint8_t segment = roomToSegment(room);
    if (segment == 255) continue;

    // Fade IN all floors for this room
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = (step * 255) / FADE_STEPS;
      uint32_t color = strips[0].Color(0, brightness, brightness);

      for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
        setSegmentColor(floor, segment, color);
      }
      showAllStrips();
      delay(FADE_DELAY);
    }

    delay(ROOM_HOLD_TIME);

    // Fade OUT
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = 255 - ((step * 255) / FADE_STEPS);
      uint32_t color = strips[0].Color(0, brightness, brightness);

      for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
        setSegmentColor(floor, segment, color);
      }
      showAllStrips();
      delay(FADE_DELAY);
    }

    // Clear room across all floors
    for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
      setSegmentColor(floor, segment, 0);
    }
    showAllStrips();
  }
}

/* ══════════════════════ PATTERN 3: RGB SMOOTH FADING ══════════════════════ */

void patternRGBFade() {
  const uint16_t CYCLE_DURATION = 6000;
  const uint8_t FRAME_DELAY = 20;
  const uint16_t TOTAL_FRAMES = CYCLE_DURATION / FRAME_DELAY;

  Serial.println(F("[PATTERN] Starting RGB smooth fade cycle"));

  for (uint16_t frame = 0; frame < TOTAL_FRAMES; frame++) {
    float angle = (frame * 2.0 * PI) / TOTAL_FRAMES;
    if (checkForInterrupt()) return;

    uint8_t r = (sin(angle) * 127.5) + 127.5;
    uint8_t g = (sin(angle + (2.0 * PI / 3.0)) * 127.5) + 127.5;
    uint8_t b = (sin(angle + (4.0 * PI / 3.0)) * 127.5) + 127.5;

    uint32_t color = strips[0].Color(r, g, b);

    for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
      if (checkForInterrupt()) return;
      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        setSegmentColor(floor, seg, color);
      }
    }

    showAllStrips();
    delay(FRAME_DELAY);
  }
}

/* ══════════════════════ PATTERN 4: RANDOM LED FADING ══════════════════════ */

void patternRandomFade() {
  const uint16_t ANIMATION_DURATION = 8000;
  const uint8_t FRAME_DELAY = 30;
  const uint16_t TOTAL_FRAMES = ANIMATION_DURATION / FRAME_DELAY;
  const uint8_t ACTIVE_PERCENT = 80;

  // Total controllable LEDs (rooms only)
  uint16_t totalRoomLEDs = 0;
  for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
    totalRoomLEDs += SEGMENT_LED_COUNTS[seg];
  }
  totalRoomLEDs *= NUM_FLOORS;
  if (checkForInterrupt()) return;

  uint16_t activeLEDs = (totalRoomLEDs * ACTIVE_PERCENT) / 100;

  Serial.print(F("[PATTERN] Random fade: "));
  Serial.print(activeLEDs);
  Serial.print(F(" / "));
  Serial.print(totalRoomLEDs);
  Serial.println(F(" LEDs"));

  // Array to track active LEDs
  bool* ledActive = (bool*)malloc(totalRoomLEDs * sizeof(bool));
  if (ledActive == NULL) {
    Serial.println(F("[PATTERN] Memory allocation failed!"));
    return;
  }
  if (checkForInterrupt()) { free(ledActive); return; }

  for (uint16_t i = 0; i < totalRoomLEDs; i++) {
    ledActive[i] = false;
  }

  randomSeed(analogRead(A0));
  for (uint16_t i = 0; i < activeLEDs; i++) {
    uint16_t ledIndex;
    do {
      ledIndex = random(totalRoomLEDs);
    } while (ledActive[ledIndex]);
    ledActive[ledIndex] = true;
  }

  // Animate
  for (uint16_t frame = 0; frame < TOTAL_FRAMES; frame++) {
    uint16_t ledCounter = 0;

    for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
      uint8_t offset = FLOOR_LED_OFFSET[floor];

      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        uint8_t startLed = SEGMENT_START_LED[seg] + offset;  // <-- v3.4 offset
        uint8_t count = SEGMENT_LED_COUNTS[seg];
        if (checkForInterrupt()) { free(ledActive); return; }

        for (uint8_t i = 0; i < count; i++) {
          if (ledActive[ledCounter]) {
            float phase = (ledCounter % 360) * (PI / 180.0);
            float angle = ((frame * 2.0 * PI) / TOTAL_FRAMES) + phase;
            uint8_t brightness = (sin(angle) * 127.5) + 127.5;

            uint8_t r = brightness;
            uint8_t g = (brightness * 2) / 3;
            uint8_t b = brightness / 2;

            uint32_t color = strips[0].Color(r, g, b);
            strips[floor].setPixelColor(startLed + i, color);
          } else {
            strips[floor].setPixelColor(startLed + i, 0);
          }
          ledCounter++;
        }
      }
    }

    showAllStrips();
    delay(FRAME_DELAY);
  }

  free(ledActive);
}

/* ══════════════════════ LED HELPER FUNCTIONS ══════════════════════ */

/*
 * setSegmentColor — paints all LEDs of a given segment on a given floor.
 * Applies the floor's per-floor LED start offset (v3.4) so that the dead
 * 4-LED zone at the strip head is skipped on non-exempt floors.
 */
void setSegmentColor(uint8_t floorIndex, uint8_t segmentIndex, uint32_t color) {
  if (floorIndex >= NUM_FLOORS || segmentIndex >= SEGMENTS_PER_FLOOR) {
    return;
  }

  uint8_t offset = FLOOR_LED_OFFSET[floorIndex];
  uint16_t startLed = (uint16_t)SEGMENT_START_LED[segmentIndex] + offset;
  uint8_t count = SEGMENT_LED_COUNTS[segmentIndex];

  // Safety: don't write past the physical strip length
  for (uint8_t i = 0; i < count; i++) {
    uint16_t pixel = startLed + i;
    if (pixel >= LEDS_PER_FLOOR) break;
    strips[floorIndex].setPixelColor(pixel, color);
  }
}

/*
 * clearFloor — turns OFF every physical LED on the floor (including the
 * dead-zone head LEDs and any tail LEDs beyond the room area). Safe to
 * call before any "set" operation to guarantee no stale pixels.
 */
void clearFloor(uint8_t floorIndex) {
  if (floorIndex >= NUM_FLOORS) return;

  for (uint16_t i = 0; i < LEDS_PER_FLOOR; i++) {
    strips[floorIndex].setPixelColor(i, 0);
  }
}

/*
 * fillGradient — draws a smooth gradient across ONLY the 85 room LEDs of
 * the floor, in segment order (2..7). Dead-zone and tail LEDs are cleared.
 */
void fillGradient(uint8_t floorIndex, uint32_t startColor, uint32_t endColor) {
  if (floorIndex >= NUM_FLOORS) return;

  // Clear entire strip first so dead-zone / tail stay dark
  clearFloor(floorIndex);

  uint8_t startR = (startColor >> 16) & 0xFF;
  uint8_t startG = (startColor >> 8) & 0xFF;
  uint8_t startB = startColor & 0xFF;
  uint8_t endR = (endColor >> 16) & 0xFF;
  uint8_t endG = (endColor >> 8) & 0xFF;
  uint8_t endB = endColor & 0xFF;

  // Total room LEDs (85 across rooms 1..6)
  uint16_t totalRoomLEDs = 0;
  for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
    totalRoomLEDs += SEGMENT_LED_COUNTS[seg];
  }
  if (totalRoomLEDs == 0) return;

  uint8_t offset = FLOOR_LED_OFFSET[floorIndex];
  uint16_t ledCounter = 0;

  for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
    uint16_t startLed = (uint16_t)SEGMENT_START_LED[seg] + offset;
    uint8_t count = SEGMENT_LED_COUNTS[seg];

    for (uint8_t i = 0; i < count; i++) {
      uint8_t r = map(ledCounter, 0, totalRoomLEDs - 1, startR, endR);
      uint8_t g = map(ledCounter, 0, totalRoomLEDs - 1, startG, endG);
      uint8_t b = map(ledCounter, 0, totalRoomLEDs - 1, startB, endB);
      uint16_t pixel = startLed + i;
      if (pixel < LEDS_PER_FLOOR) {
        strips[floorIndex].setPixelColor(pixel, strips[0].Color(r, g, b));
      }
      ledCounter++;
    }
  }
}

void showAllStrips() {
  for (uint8_t i = 0; i < NUM_FLOORS; i++) {
    strips[i].show();
  }
}

void scheduleUpdate() {
  updatePending = true;
  lastUpdateTime = millis();
}

/* ══════════════════════ TEST PATTERN ══════════════════════ */

void testPattern() {
  Serial.println(F("[TEST] Testing each floor with room mapping..."));

  uint32_t testColors[6] = {
    strips[0].Color(255, 0, 0),    // Red
    strips[0].Color(0, 255, 0),    // Green
    strips[0].Color(0, 0, 255),    // Blue
    strips[0].Color(255, 255, 0),  // Yellow
    strips[0].Color(255, 0, 255),  // Magenta
    strips[0].Color(0, 255, 255)   // Cyan
  };

  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    Serial.print(F("[TEST] Floor "));
    Serial.print(floor + 1);
    Serial.print(F(" (Pin "));
    Serial.print(FLOOR_PINS[floor]);
    Serial.print(F(", Offset "));
    Serial.print(FLOOR_LED_OFFSET[floor]);
    Serial.println(F(")"));

    clearFloor(floor);  // explicit dead-zone clear

    for (uint8_t room = 1; room <= ACTUAL_ROOMS; room++) {
      uint8_t seg = roomToSegment(room);

      Serial.print(F("[TEST]   Room "));
      Serial.print(room);
      Serial.print(F(" -> Segment "));
      Serial.println(seg);

      setSegmentColor(floor, seg, testColors[room - 1]);
    }

    strips[floor].show();
    delay(20);

    clearFloor(floor);
    strips[floor].show();
    delay(20);
  }

  Serial.println(F("[TEST] Test complete"));
}
