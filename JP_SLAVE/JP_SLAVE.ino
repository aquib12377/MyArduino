/*
 * ═══════════════════════════════════════════════════════════════════════════
 * JP INFRA - ARDUINO MEGA SLAVE (CONFIG.CSV PIN MAPPING)
 * Uses individual pins per floor with lobby exclusion
 * CONVERTED TO: Adafruit NeoPixel Library (Memory Optimized)
 * NEW v3.3: Pattern Animations - Floor by Floor, Room by Room, RGB Fade, Random Fade
 * NO PRINTF - Arduino Mega Compatible
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * HARDWARE CONFIGURATION (from config.csv):
 * ────────────────────────────────────────────
 * Each floor has 8 segments on one WS2812B strip:
 * - Segment 0: LOBBY (4 LEDs) - Does NOT light up
 * - Segment 1: LOBBY (13 LEDs) - Does NOT light up
 * - Segment 2: Room 1 (2 LEDs) - Lights up
 * - Segment 3: Room 2 (14 LEDs) - Lights up
 * - Segment 4: Room 3 (15 LEDs) - Lights up
 * - Segment 5: Room 4 (14 LEDs) - Lights up
 * - Segment 6: Room 5 (14 LEDs) - Lights up
 * - Segment 7: Room 6 (13 LEDs) - Lights up
 * 
 * Total LEDs per floor: 89 LEDs (4+13+2+14+15+14+14+13)
 * Controllable rooms: 6 rooms (indices 2-7, which are rooms 1-6)
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
#define CMD_PATTERN 0x03           // NEW: Pattern animations command
#define CMD_PING 0x0A

// Status codes
#define STATUS_SOLD 0
#define STATUS_AVAILABLE 1
#define STATUS_BLOCKED 2

/* ══════════════════════ FLOOR & ROOM CONFIGURATION ══════════════════════ */

#define NUM_FLOORS 16
#define SEGMENTS_PER_FLOOR 8  // Total segments including lobbies
#define ACTUAL_ROOMS 6        // Actual controllable rooms (segments 2-7)
#define LEDS_PER_FLOOR 89     // Total LEDs per strip

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

// LED count per segment (from config.csv room_leds: 4;13;2;14;15;14;14;13)
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

// Starting LED index for each segment
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
#define COLOR_SOLD      strips[0].Color(0, 150, 255)   // Blue - BRIGHTER

// Classic pattern colors
#define COLOR_CLASSIC_START strips[0].Color(100, 0, 200)
#define COLOR_CLASSIC_END   strips[0].Color(0, 100, 255)

// Pattern animation colors
#define COLOR_PATTERN_FLOOR strips[0].Color(255, 255, 255)  // White for floor animation
#define COLOR_PATTERN_ROOM  strips[0].Color(0, 200, 255)    // Bright cyan for room animation

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
  
  Serial.println("\n============================================");
  Serial.println("JP INFRA - Arduino Mega Slave v3.3");
  Serial.println("Library: Adafruit NeoPixel (Memory Optimized)");
  Serial.println("NEW: Pattern Animations");
  Serial.println("============================================\n");
  
  // Initialize I2C as slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  
  Serial.print("[I2C] Slave initialized at address: 0x");
  Serial.println(I2C_SLAVE_ADDRESS, HEX);
  
  // Calculate segment start positions
  initSegmentPositions();
  
  // Validate and print room mapping
  validateAndPrintRoomMapping();
  
  // Initialize NeoPixel strips for each floor
  Serial.println("[LED] Initializing NeoPixel strips for 16 floors...");
  
  for (uint8_t i = 0; i < NUM_FLOORS; i++) {
    strips[i].begin();
    strips[i].setBrightness(200);
    strips[i].show(); // Initialize all pixels to 'off'
  }
  
  Serial.println("[LED] NeoPixel strips initialized");
  Serial.print("  Floors: ");
  Serial.println(NUM_FLOORS);
  Serial.print("  LEDs per floor: ");
  Serial.println(LEDS_PER_FLOOR);
  Serial.print("  Segments per floor: ");
  Serial.print(SEGMENTS_PER_FLOOR);
  Serial.println(" (2 lobbies + 6 rooms)");
  
  // Print segment mapping
  printSegmentMapping();
  
  // Test pattern
  Serial.println("[LED] Running startup test...");
  testPattern();
  
  Serial.println("\n[READY] System ready\n");
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
    Serial.print("[MAP] ERROR: Invalid room number: ");
    Serial.println(roomNumber);
    return 255;
  }
  
  return ROOM_TO_SEGMENT_MAP[arrayIndex];
}

// Validate room mapping configuration
bool validateRoomMapping() {
  bool valid = true;
  bool segmentUsed[SEGMENTS_PER_FLOOR] = {false};
  
  Serial.println("[MAP] Validating room mapping...");
  
  for (uint8_t i = 0; i < ACTUAL_ROOMS; i++) {
    uint8_t segment = ROOM_TO_SEGMENT_MAP[i];
    
    if (segment < 2 || segment >= SEGMENTS_PER_FLOOR) {
      Serial.print("[MAP] ERROR: Room ");
      Serial.print(i + 1);
      Serial.print(" maps to invalid segment ");
      Serial.println(segment);
      valid = false;
    }
    
    if (segmentUsed[segment]) {
      Serial.print("[MAP] ERROR: Segment ");
      Serial.print(segment);
      Serial.println(" is mapped to multiple rooms!");
      valid = false;
    }
    
    segmentUsed[segment] = true;
  }
  
  if (valid) {
    Serial.println("[MAP] Room mapping validation PASSED");
  } else {
    Serial.println("[MAP] Room mapping validation FAILED!");
    Serial.println("[MAP] Using default mapping as fallback");
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
  Serial.println("\n[MAP] ═══════════════════════════════════════");
  Serial.println("[MAP] ROOM NUMBER MAPPING CONFIGURATION");
  Serial.println("[MAP] ═══════════════════════════════════════");
  Serial.println("[MAP] Room # | Physical Segment | LED Count");
  Serial.println("[MAP] ---------|------------------|----------");
  
  for (uint8_t room = 1; room <= ACTUAL_ROOMS; room++) {
    uint8_t segment = roomToSegment(room);
    uint8_t ledCount = SEGMENT_LED_COUNTS[segment];
    
    Serial.print("[MAP]   ");
    Serial.print(room);
    Serial.print("     |        ");
    Serial.print(segment);
    Serial.print("         |    ");
    Serial.print(ledCount);
    Serial.println(" LEDs");
  }
  
  Serial.println("[MAP] ═══════════════════════════════════════\n");
}

/* ══════════════════════ SEGMENT POSITION CALCULATION ══════════════════════ */

void initSegmentPositions() {
  Serial.println("[MAP] Calculating segment positions...");
  
  uint8_t currentPos = 0;
  for (uint8_t seg = 0; seg < SEGMENTS_PER_FLOOR; seg++) {
    SEGMENT_START_LED[seg] = currentPos;
    currentPos += SEGMENT_LED_COUNTS[seg];
  }
  
  Serial.println("[MAP] Segment positions calculated");
}

void printSegmentMapping() {
  Serial.println("\n[MAP] Physical Segment Layout:");
  Serial.println("Seg | Type   | LEDs | Start | End");
  Serial.println("----|--------|------|-------|----");
  
  for (uint8_t seg = 0; seg < SEGMENTS_PER_FLOOR; seg++) {
    const char* type = (seg <= 1) ? "LOBBY" : "ROOM";
    uint8_t roomNum = (seg <= 1) ? 0 : (seg - 1);
    uint8_t start = SEGMENT_START_LED[seg];
    uint8_t end = start + SEGMENT_LED_COUNTS[seg] - 1;
    
    Serial.print(" ");
    Serial.print(seg);
    Serial.print("  | ");
    Serial.print(type);
    Serial.print(" ");
    Serial.print(roomNum);
    Serial.print(" | ");
    Serial.print(SEGMENT_LED_COUNTS[seg]);
    Serial.print("   | ");
    Serial.print(start);
    Serial.print("    | ");
    Serial.println(end);
  }
  Serial.println();
}

/* ══════════════════════ COMMAND PROCESSING ══════════════════════ */

void processCommand() {
  if (commandLength == 0) return;
  
  byte cmd = commandBuffer[0];
  
  Serial.print("[CMD] Received: 0x");
  Serial.print(cmd, HEX);
  if (commandLength > 1) {
    Serial.print(" Params:");
    for (byte i = 1; i < commandLength; i++) {
      Serial.print(" ");
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
      Serial.println("[CMD] PING received");
      break;
      
    default:
      Serial.print("[CMD] Unknown command: 0x");
      Serial.println(cmd, HEX);
      break;
  }
}

/* ══════════════════════ COMMAND HANDLERS ══════════════════════ */

void handleAllOn() {
  Serial.println("[LED] All ON (White) - Excluding lobbies");
  
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
      setSegmentColor(floor, seg, COLOR_WHITE);
    }
  }
  
  scheduleUpdate();
}

void handleAllOff() {
  Serial.println("[LED] All OFF");
  
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    clearFloor(floor);
  }
  
  scheduleUpdate();
}

void handleClassic() {
  Serial.println("[LED] Classic gradient pattern");
  
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    fillGradient(floor, COLOR_CLASSIC_START, COLOR_CLASSIC_END);
  }
  
  scheduleUpdate();
}

void handleFloorOn(byte floor) {
  Serial.print("[LED] Floor ON: ");
  Serial.println(floor);
  
  byte floorIndex = floor - 1;
  
  if (floorIndex >= NUM_FLOORS) {
    Serial.println("[LED] Invalid floor number");
    return;
  }
  
  for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
    setSegmentColor(floorIndex, seg, COLOR_WHITE);
  }
  
  scheduleUpdate();
}

void handleFloorOff(byte floor) {
  Serial.print("[LED] Floor OFF: ");
  Serial.println(floor);
  
  byte floorIndex = floor - 1;
  
  if (floorIndex >= NUM_FLOORS) {
    Serial.println("[LED] Invalid floor number");
    return;
  }
  
  clearFloor(floorIndex);
  scheduleUpdate();
}

void handleRoomOn(byte floor, byte room) {
  Serial.print("[LED] Room ON: Floor ");
  Serial.print(floor);
  Serial.print(", Room ");
  Serial.println(room);
  
  byte floorIndex = floor - 1;
  
  if (floorIndex >= NUM_FLOORS) {
    Serial.println("[LED] Invalid floor number");
    return;
  }
  
  byte segmentIndex = roomToSegment(room);
  
  if (segmentIndex == 255 || segmentIndex < 2 || segmentIndex >= SEGMENTS_PER_FLOOR) {
    Serial.print("[LED] Invalid room number (must be 1-6): ");
    Serial.println(room);
    return;
  }
  
  Serial.print("[LED] Mapped: Room ");
  Serial.print(room);
  Serial.print(" -> Segment ");
  Serial.println(segmentIndex);
  
  setSegmentColor(floorIndex, segmentIndex, COLOR_CYAN);
  scheduleUpdate();
}

void handleRoomOff(byte floor, byte room) {
  Serial.print("[LED] Room OFF: Floor ");
  Serial.print(floor);
  Serial.print(", Room ");
  Serial.println(room);
  
  byte floorIndex = floor - 1;
  byte segmentIndex = roomToSegment(room);
  
  if (floorIndex >= NUM_FLOORS || segmentIndex == 255 || 
      segmentIndex < 2 || segmentIndex >= SEGMENTS_PER_FLOOR) {
    Serial.println("[LED] Invalid floor/room number");
    return;
  }
  
  Serial.print("[LED] Mapped: Room ");
  Serial.print(room);
  Serial.print(" -> Segment ");
  Serial.println(segmentIndex);
  
  setSegmentColor(floorIndex, segmentIndex, strips[0].Color(0, 0, 0));
  scheduleUpdate();
}

/* ══════════════════════ AVAILABILITY HANDLERS ══════════════════════ */

void handleAvailBaseGreen() {
  Serial.println("[AVAIL] Setting base GREEN for availability mode");
  
  // Turn OFF commercial floors (1-2)
  clearFloor(0);
  clearFloor(1);
  
  // Turn GREEN residential floors (3-15) - only rooms, not lobbies
  for (uint8_t floor = 2; floor < 15; floor++) {
    for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
      setSegmentColor(floor, seg, COLOR_AVAILABLE);
    }
  }
  
  // Turn OFF amenities floor (16)
  clearFloor(15);
  
  // IMMEDIATE update for base green
  showAllStrips();
  Serial.println("[AVAIL] Base green set complete");
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
      Serial.print("[AVAIL] Unknown status: ");
      Serial.println(status);
      return;
  }
  
  Serial.print("[AVAIL] ");
  Serial.print(statusName);
  Serial.print(" - Floor ");
  Serial.print(floor);
  Serial.print(", Room ");
  Serial.println(room);
  
  byte floorIndex = floor - 1;
  byte segmentIndex = roomToSegment(room);
  
  if (floorIndex >= NUM_FLOORS || segmentIndex == 255 || 
      segmentIndex < 2 || segmentIndex >= SEGMENTS_PER_FLOOR) {
    Serial.println("[AVAIL] Invalid floor/room number");
    return;
  }
  
  Serial.print("[AVAIL] Mapped: Room ");
  Serial.print(room);
  Serial.print(" -> Segment ");
  Serial.println(segmentIndex);
  
  setSegmentColor(floorIndex, segmentIndex, color);
  scheduleUpdate();
}

/* ══════════════════════ CUSTOM LED CONTROL ══════════════════════ */

void handleCustomLEDs(byte floor, byte ledCount, byte r, byte g, byte b) {
  Serial.print("[CUSTOM] Floor ");
  Serial.print(floor);
  Serial.print(", Count: ");
  Serial.print(ledCount);
  Serial.print(", RGB(");
  Serial.print(r);
  Serial.print(",");
  Serial.print(g);
  Serial.print(",");
  Serial.print(b);
  Serial.println(")");
  
  byte floorIndex = floor - 1;
  
  if (floorIndex >= NUM_FLOORS) {
    Serial.println("[CUSTOM] Invalid floor number");
    return;
  }
  
  byte actualCount = min(ledCount, LEDS_PER_FLOOR);
  uint32_t customColor = strips[0].Color(r, g, b);
  
  for (uint8_t i = 0; i < actualCount; i++) {
    strips[floorIndex].setPixelColor(i, customColor);
  }
  
  for (uint8_t i = actualCount; i < LEDS_PER_FLOOR; i++) {
    strips[floorIndex].setPixelColor(i, 0);
  }
  
  scheduleUpdate();
  Serial.println("[CUSTOM] Custom LEDs set");
}

/* ══════════════════════ PATTERN ANIMATION HANDLERS ══════════════════════ */

void handlePatternCommand() {
  Serial.println("\n[PATTERN] Starting pattern sequence...");
  Serial.println("[PATTERN] ═══════════════════════════════════");
  
  // Pattern 1: Floor by Floor
  Serial.println("[PATTERN] 1/4: Floor by Floor Animation");
  patternFloorByFloor();
  delay(500);
  
  // Pattern 2: Room by Room
  Serial.println("[PATTERN] 2/4: Room by Room Animation");
  patternRoomByRoom();
  delay(500);
  
  // Pattern 3: RGB Smooth Fading
  Serial.println("[PATTERN] 3/4: RGB Smooth Fading");
  patternRGBFade();
  delay(500);
  
  // Pattern 4: Random LED Fading (80% of LEDs)
  Serial.println("[PATTERN] 4/4: Random LED Fading (80%)");
  patternRandomFade();
  delay(500);
  
  // Clear all after patterns
  Serial.println("[PATTERN] Clearing all LEDs");
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    clearFloor(floor);
  }
  showAllStrips();
  
  Serial.println("[PATTERN] ═══════════════════════════════════");
  Serial.println("[PATTERN] Pattern sequence complete!\n");
}

/* ══════════════════════ PATTERN 1: FLOOR BY FLOOR ══════════════════════ */

void patternFloorByFloor() {
  const uint16_t FLOOR_HOLD_TIME = 300;  // Time each floor stays lit (ms)
  const uint8_t FADE_STEPS = 20;         // Steps for fade in/out
  const uint8_t FADE_DELAY = 10;         // Delay between fade steps (ms)
  
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    Serial.print("[PATTERN] Lighting Floor ");
    Serial.println(floor + 1);
    
    // Fade IN
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = (step * 255) / FADE_STEPS;
      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        uint32_t color = strips[0].Color(brightness, brightness, brightness);
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
      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        uint32_t color = strips[0].Color(brightness, brightness, brightness);
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
  const uint16_t ROOM_HOLD_TIME = 250;   // Time each room stays lit (ms)
  const uint8_t FADE_STEPS = 15;         // Steps for fade
  const uint8_t FADE_DELAY = 8;          // Delay between fade steps (ms)
  
  for (uint8_t room = 1; room <= ACTUAL_ROOMS; room++) {
    Serial.print("[PATTERN] Lighting Room ");
    Serial.println(room);
    
    uint8_t segment = roomToSegment(room);
    
    if (segment == 255) continue;
    
    // Fade IN all floors for this room
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = (step * 255) / FADE_STEPS;
      uint32_t color = strips[0].Color(0, brightness, brightness);  // Cyan fade
      
      for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
        setSegmentColor(floor, segment, color);
      }
      showAllStrips();
      delay(FADE_DELAY);
    }
    
    // Hold
    delay(ROOM_HOLD_TIME);
    
    // Fade OUT
    for (uint8_t step = 0; step <= FADE_STEPS; step++) {
      uint8_t brightness = 255 - ((step * 255) / FADE_STEPS);
      uint32_t color = strips[0].Color(0, brightness, brightness);  // Cyan fade
      
      for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
        setSegmentColor(floor, segment, color);
      }
      showAllStrips();
      delay(FADE_DELAY);
    }
    
    // Clear all floors for this room
    for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
      setSegmentColor(floor, segment, 0);
    }
    showAllStrips();
  }
}

/* ══════════════════════ PATTERN 3: RGB SMOOTH FADING ══════════════════════ */

void patternRGBFade() {
  const uint16_t CYCLE_DURATION = 6000;  // Total cycle time (ms)
  const uint8_t FRAME_DELAY = 20;        // Delay between frames (ms)
  const uint16_t TOTAL_FRAMES = CYCLE_DURATION / FRAME_DELAY;
  
  Serial.println("[PATTERN] Starting RGB smooth fade cycle");
  
  for (uint16_t frame = 0; frame < TOTAL_FRAMES; frame++) {
    // Calculate RGB values using smooth sine wave
    float angle = (frame * 2.0 * PI) / TOTAL_FRAMES;
    
    uint8_t r = (sin(angle) * 127.5) + 127.5;
    uint8_t g = (sin(angle + (2.0 * PI / 3.0)) * 127.5) + 127.5;
    uint8_t b = (sin(angle + (4.0 * PI / 3.0)) * 127.5) + 127.5;
    
    uint32_t color = strips[0].Color(r, g, b);
    
    // Apply to all rooms on all floors
    for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
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
  const uint16_t ANIMATION_DURATION = 8000;  // Total animation time (ms)
  const uint8_t FRAME_DELAY = 30;            // Delay between frames (ms)
  const uint16_t TOTAL_FRAMES = ANIMATION_DURATION / FRAME_DELAY;
  const uint8_t ACTIVE_PERCENT = 80;         // 80% of LEDs active
  
  // Calculate total controllable LEDs
  uint16_t totalRoomLEDs = 0;
  for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
    totalRoomLEDs += SEGMENT_LED_COUNTS[seg];
  }
  totalRoomLEDs *= NUM_FLOORS;
  
  uint16_t activeLEDs = (totalRoomLEDs * ACTIVE_PERCENT) / 100;
  
  Serial.print("[PATTERN] Random fade: ");
  Serial.print(activeLEDs);
  Serial.print(" / ");
  Serial.print(totalRoomLEDs);
  Serial.println(" LEDs");
  
  // Create array to track which LEDs are active
  bool* ledActive = (bool*)malloc(totalRoomLEDs * sizeof(bool));
  if (ledActive == NULL) {
    Serial.println("[PATTERN] Memory allocation failed!");
    return;
  }
  
  // Initialize random LED selection
  for (uint16_t i = 0; i < totalRoomLEDs; i++) {
    ledActive[i] = false;
  }
  
  // Select random LEDs to be active
  randomSeed(analogRead(A0));
  for (uint16_t i = 0; i < activeLEDs; i++) {
    uint16_t ledIndex;
    do {
      ledIndex = random(totalRoomLEDs);
    } while (ledActive[ledIndex]);
    ledActive[ledIndex] = true;
  }
  
  // Animate random LEDs with individual sine waves
  for (uint16_t frame = 0; frame < TOTAL_FRAMES; frame++) {
    uint16_t ledCounter = 0;
    
    for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
      for (uint8_t seg = 2; seg < SEGMENTS_PER_FLOOR; seg++) {
        uint8_t startLed = SEGMENT_START_LED[seg];
        uint8_t count = SEGMENT_LED_COUNTS[seg];
        
        for (uint8_t i = 0; i < count; i++) {
          if (ledActive[ledCounter]) {
            // Each LED has its own phase offset for variety
            float phase = (ledCounter % 360) * (PI / 180.0);
            float angle = ((frame * 2.0 * PI) / TOTAL_FRAMES) + phase;
            
            // Smooth brightness fade
            uint8_t brightness = (sin(angle) * 127.5) + 127.5;
            
            // Color varies slightly per LED
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

void setSegmentColor(uint8_t floorIndex, uint8_t segmentIndex, uint32_t color) {
  if (floorIndex >= NUM_FLOORS || segmentIndex >= SEGMENTS_PER_FLOOR) {
    return;
  }
  
  uint8_t startLed = SEGMENT_START_LED[segmentIndex];
  uint8_t count = SEGMENT_LED_COUNTS[segmentIndex];
  
  for (uint8_t i = 0; i < count; i++) {
    strips[floorIndex].setPixelColor(startLed + i, color);
  }
}

void clearFloor(uint8_t floorIndex) {
  if (floorIndex >= NUM_FLOORS) return;
  
  for (uint16_t i = 0; i < LEDS_PER_FLOOR; i++) {
    strips[floorIndex].setPixelColor(i, 0);
  }
}

void fillGradient(uint8_t floorIndex, uint32_t startColor, uint32_t endColor) {
  if (floorIndex >= NUM_FLOORS) return;
  
  // Extract RGB components from start color
  uint8_t startR = (startColor >> 16) & 0xFF;
  uint8_t startG = (startColor >> 8) & 0xFF;
  uint8_t startB = startColor & 0xFF;
  
  // Extract RGB components from end color
  uint8_t endR = (endColor >> 16) & 0xFF;
  uint8_t endG = (endColor >> 8) & 0xFF;
  uint8_t endB = endColor & 0xFF;
  
  for (uint16_t i = 0; i < LEDS_PER_FLOOR; i++) {
    // Calculate interpolated color
    uint8_t r = map(i, 0, LEDS_PER_FLOOR - 1, startR, endR);
    uint8_t g = map(i, 0, LEDS_PER_FLOOR - 1, startG, endG);
    uint8_t b = map(i, 0, LEDS_PER_FLOOR - 1, startB, endB);
    
    strips[floorIndex].setPixelColor(i, strips[0].Color(r, g, b));
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
  Serial.println("[TEST] Testing each floor with room mapping...");
  
  uint32_t testColors[6] = {
    strips[0].Color(255, 0, 0),    // Red
    strips[0].Color(0, 255, 0),    // Green
    strips[0].Color(0, 0, 255),    // Blue
    strips[0].Color(255, 255, 0),  // Yellow
    strips[0].Color(255, 0, 255),  // Magenta
    strips[0].Color(0, 255, 255)   // Cyan
  };
  
  for (uint8_t floor = 0; floor < NUM_FLOORS; floor++) {
    Serial.print("[TEST] Floor ");
    Serial.print(floor + 1);
    Serial.print(" (Pin ");
    Serial.print(FLOOR_PINS[floor]);
    Serial.println(")");
    
    for (uint8_t room = 1; room <= ACTUAL_ROOMS; room++) {
      uint8_t seg = roomToSegment(room);
      
      Serial.print("[TEST]   Room ");
      Serial.print(room);
      Serial.print(" -> Segment ");
      Serial.println(seg);
      
      setSegmentColor(floor, seg, testColors[room - 1]);
    }
    
    strips[floor].show();
    delay(20);
    
    clearFloor(floor);
    strips[floor].show();
    delay(20);
  }
  
  Serial.println("[TEST] Test complete");
}
