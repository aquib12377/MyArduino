/**
 * @file Building_LED_Controller_I2C.ino
 * @brief Arduino Mega 2560 controlling 11 floors of WS2812B strips.
 *        Adds I²C slave to accept commands from an ESP32-S3 master.
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ================= I2C CONFIG =================
#define MEGA_I2C_ADDR 8

// I2C command bytes (must match ESP32)
enum : uint8_t {
  CMD_OFF             = 0x00,
  CMD_PATTERN_RAINBOW = 0x01,
  CMD_BHK3            = 0x02,
  CMD_BHK4            = 0x03,
  CMD_ALL_ON          = 0x04
};

volatile uint8_t pendingCmd = 0xFF; // 0xFF = none

// I2C receive ISR: capture last byte, handle in loop()
void onI2CRecv(int howMany) {
  while (Wire.available()) {
    pendingCmd = Wire.read();
  }
}

// =============== LED CONFIG ===================

// Pin assignments for each floor's LED strip (11 floors)
const int floorPins[] = { 25, 27, 29, 47, 33, 35, 37, 39, 41, 43, 45 };

// Number of floors and LEDs per floor
const int NUM_FLOORS = 11;
const int LEDS_PER_FLOOR = 170;
const int BRIGHTNESS = 255;

// Array to hold the NeoPixel objects for each floor
Adafruit_NeoPixel floors[NUM_FLOORS];

// LED counts for each room/section within a single floor strip
const int room_led_counts[] = {
  21,  // Room 1
  17,  // Room 2
  16,  // Room 3
  25,  // Room 4
  9,   // Duct 1
  24,  // Room 5
  8,   // Duct 2
  22,  // Room 6
  28   // Room 7
};
const int NUM_ROOMS = sizeof(room_led_counts) / sizeof(room_led_counts[0]);

// Arrays to store the calculated start index and names for each room
int room_start_indices[NUM_ROOMS];
const char* room_names[] = { "Room 1", "Room 2", "Room 3", "Room 4", "Duct 1", "Room 5", "Duct 2", "Room 6", "Room 7" };

// --- Global state for patterns ---
int activePattern = 0;  // 0 = OFF, 3 = Rainbow
unsigned long lastPatternStep = 0;
uint16_t rainbowFirstPixelHue = 0;

// --- Function Prototypes ---
void printMenu();
void clearAllLeds();
void patternOneFloorAtATime();
void patternOneRoomOfEachFloorAtATime();
void updateRainbowFade();
void lightUp3BHKs();
void lightUp4BHKs();
void turnOnAllLights();
void lightUpDuctsPermanently();
uint32_t wheel(byte WheelPos);

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  Serial.println("Building LED Control System Initialized");

  // Calculate room start indices once
  room_start_indices[0] = 0;
  for (int i = 1; i < NUM_ROOMS; i++) {
    room_start_indices[i] = room_start_indices[i - 1] + room_led_counts[i - 1];
  }

  // Initialize all floor strips
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i] = Adafruit_NeoPixel(LEDS_PER_FLOOR, floorPins[i], NEO_GRB + NEO_KHZ800);
    floors[i].begin();
    floors[i].setBrightness(BRIGHTNESS);
    floors[i].clear();
    floors[i].show();
  }

  // ====== I2C Slave init ======
  Wire.begin(MEGA_I2C_ADDR);
  Wire.onReceive(onI2CRecv);
  Serial.println("I2C slave ready @0x8");
testChannelOrder();
}

void loop() {
  // ---------- I2C Command Dispatcher ----------
  if (pendingCmd != 0xFF) {
    uint8_t cmd = pendingCmd;
    pendingCmd = 0xFF;    // consume

    // stop any running pattern
    activePattern = 0;

    switch (cmd) {
      case CMD_OFF:
        clearAllLeds();
        Serial.println("CMD_OFF");
        break;

      case CMD_PATTERN_RAINBOW:
        clearAllLeds();
        activePattern = 3; // Rainbow
        Serial.println("CMD_PATTERN_RAINBOW");
        break;

      case CMD_BHK3:
        clearAllLeds();
        lightUp3BHKs();
        Serial.println("CMD_BHK3");
        break;

      case CMD_BHK4:
        clearAllLeds();
        lightUp4BHKs();
        Serial.println("CMD_BHK4");
        break;

      case CMD_ALL_ON:
        turnOnAllLights();
        Serial.println("CMD_ALL_ON");
        break;

      default:
        // Unknown / ignore
        break;
    }
  }
  // -------------------------------------------

  // ---------- Serial Menu (optional) ----------
  if (Serial.available() > 0) {
    char command = Serial.read();

    // Stop any continuously running patterns and turn off LEDs before starting a new command
    activePattern = 0;
    clearAllLeds();

    switch (command) {
      case '1':
        Serial.println("Executing Pattern: One Floor at a Time");
        patternOneFloorAtATime();
        printMenu();
        break;
      case '2':
        Serial.println("Executing Pattern: One Room of Each Floor at a Time");
        patternOneRoomOfEachFloorAtATime();
        printMenu();
        break;
      case '3':
        Serial.println("Starting Pattern: Rainbow Smooth Fading (Enter '0' to stop)");
        activePattern = 3;
        break;
      case '4':
        Serial.println("Lighting up 3BHKs (Rooms 2 & 3)");
        lightUp3BHKs();
        break;
      case '5':
        Serial.println("Lighting up 4BHKs (Rooms 1, 4, 5, 6, 7)");
        lightUp4BHKs();
        break;
      case '9':
        Serial.println("Turn ON ALL Lights");
        turnOnAllLights();
        break;
      case '0':
        Serial.println("Turning all LEDs OFF");
        printMenu();
        break;
      case 'm':
      case 'M':
        printMenu();
        break;
      default:
        while (Serial.available() > 0) Serial.read();
        break;
    }
  }
  // -------------------------------------------

  // If the rainbow pattern is active, run its update function.
  if (activePattern == 3) {
    if (millis() - lastPatternStep > 1) {  // Control the speed of the rainbow
      updateRainbowFade();
      lastPatternStep = millis();
    }
  }
}

/**
 * @brief Prints the menu of options to the Serial Monitor.
 */
void printMenu() {
  Serial.println("\n----- LED Control Menu -----");
  Serial.println("Select an operation:");
  Serial.println("  1 - Pattern: One Floor at a Time");
  Serial.println("  2 - Pattern: One Room of Each Floor at a Time");
  Serial.println("  3 - Pattern: Rainbow Smooth Fading");
  Serial.println("  4 - BHKs: Light up 3BHKs (Rooms 2 & 3)");
  Serial.println("  5 - BHKs: Light up 4BHKs (Rooms 1, 4, 5, 6, 7)");
  Serial.println("  9 - Turn ON ALL Lights");
  Serial.println("  0 - Turn All LEDs OFF");
  Serial.println("  M - Show this Menu");
  Serial.println("----------------------------");
}

/**
 * @brief Turns all LEDs on all floors off.
 */
void clearAllLeds() {
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i].clear();
    floors[i].show();
  }
}

/**
 * @brief Turn ON all LEDs (white).
 */
void turnOnAllLights() {
  // Try lower brightness for a power sanity check
  for (int i = 0; i < NUM_FLOORS; i++) {
    // OPTIONAL: temporarily lower brightness to rule out power issues
    floors[i].setBrightness(100); // or try 80–120 for testing
    floors[i].fill(floors[i].Color(255, 255, 255));
    floors[i].show();
    delay(20);
  }
}
void testChannelOrder() {
  Serial.println("Testing channel order: R, then G, then B on all floors...");
  uint32_t R = floors[0].Color(255, 0, 0);
  uint32_t G = floors[0].Color(0, 255, 0);
  uint32_t B = floors[0].Color(0, 0, 255);

  for (int f = 0; f < NUM_FLOORS; f++) {
    floors[f].clear(); floors[f].fill(R); floors[f].show(); delay(800);
    floors[f].clear(); floors[f].fill(G); floors[f].show(); delay(800);
    floors[f].clear(); floors[f].fill(B); floors[f].show(); delay(800);
    floors[f].clear(); floors[f].show();  delay(50);
  }
  Serial.println("If colors appear out of order, change NEO_GRB to NEO_RGB (or other) for all floors.");
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
  for (int i = NUM_FLOORS - 1; i >=0 ; i--) {
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
 * @brief Rainbow smooth fading update. Call repeatedly when activePattern==3.
 */
void updateRainbowFade() {
  rainbowFirstPixelHue += 256;  // Increment hue for animation

  for (int f = 0; f < NUM_FLOORS; f++) {
    for (int i = 0; i < LEDS_PER_FLOOR; i++) {
      int pixelHue = rainbowFirstPixelHue + (i * 65536L / 90) + (f * 65536L / (NUM_FLOORS * 2));
      floors[f].setPixelColor(i, wheel((pixelHue >> 8) & 255));
    }
    floors[f].show();
  }
}

/**
 * @brief Lights up Rooms 2 & 3 on all floors, with ducts on.
 */
void lightUp3BHKs() {
  uint32_t color = floors[0].Color(255, 255, 0);        // Yellow
  uint32_t ductColor = floors[0].Color(255, 255, 0);  // White (ducts)

  // Room 2 -> index 1, Room 3 -> index 2
  int room2_start = room_start_indices[1];
  int room2_count = room_led_counts[1];
  int room3_start = room_start_indices[2];
  int room3_count = room_led_counts[2];

  // Duct 1 -> index 4, Duct 2 -> index 6
  int duct1_start = room_start_indices[4];
  int duct1_count = room_led_counts[4];
  int duct2_start = room_start_indices[6];
  int duct2_count = room_led_counts[6];

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
  uint32_t color = floors[0].Color(0, 255, 255);        // Cyan
  uint32_t ductColor = floors[0].Color(0, 255, 255);  // White (ducts)

  const int rooms_to_light[] = { 0, 3, 5, 7, 8 }; // indices
  int num_rooms_to_light = sizeof(rooms_to_light) / sizeof(rooms_to_light[0]);

  int duct1_start = room_start_indices[4];
  int duct1_count = room_led_counts[4];
  int duct2_start = room_start_indices[6];
  int duct2_count = room_led_counts[6];

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
void lightUpDuctsPermanently() {
  uint32_t ductColor = floors[0].Color(255, 255, 255);  // White

  int duct1_start = room_start_indices[4];
  int duct1_count = room_led_counts[4];
  int duct2_start = room_start_indices[6];
  int duct2_count = room_led_counts[6];

  for (int f = 0; f < NUM_FLOORS; f++) {
    for (int i = 0; i < duct1_count; i++) floors[f].setPixelColor(duct1_start + i, ductColor);
    for (int i = 0; i < duct2_count; i++) floors[f].setPixelColor(duct2_start + i, ductColor);
    floors[f].show();
  }
}

/**
 * @brief Helper function to generate rainbow colors.
 */
uint32_t wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return floors[0].Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return floors[0].Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return floors[0].Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
