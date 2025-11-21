/**
 * @file Building_LED_Controller.ino
 * @author Gemini
 * @brief Controls 11 floors of WS2812B LED strips for a building model.
 * @version 1.0
 * * --- HARDWARE ---
 * Board: Arduino Mega 2560
 * LEDs:  11 x WS2812B Strips (167 LEDs each)
 * * --- WIRING ---
 * - Connect each strip's Data-In pin to the corresponding pin on the Arduino Mega (pins 2-12).
 * - Provide a separate, high-current 5V power supply for the LED strips.
 * - Connect the power supply's Ground to BOTH the Arduino's GND and the strips' GND pins.
 * * --- CONTROL ---
 * Use the Arduino IDE's Serial Monitor (9600 baud) to send commands.
 * A menu of options will be displayed upon startup.
 */

#include <Adafruit_NeoPixel.h>

// --- Configuration ---

// Pin assignments for each floor's LED strip (11 floors)
const int floorPins[] = { 25, 27, 29, 47, 33, 35, 37, 39, 41, 43, 45 };

// Number of floors and LEDs per floor
const int NUM_FLOORS = 11;
const int LEDS_PER_FLOOR = 167;
const int BRIGHTNESS = 255;  // Set brightness (0-255). 50 is a good starting point.

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
  25   // Room 7
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
uint32_t wheel(byte WheelPos);


void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB
  }
  Serial.println("Building LED Control System Initialized");

  // Calculate room start indices. This runs once to map out the strips.
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
    floors[i].show();  // Initialize all strips to 'off'
  }
  turnOnAllLights();
  // delay(10000);
  // lightUpDuctsPermanently();
  // printMenu();
  // clearAllLeds();
  // delay(2000);
  // while (true) {
  //   patternOneFloorAtATime();
  //   delay(2000);
  //   clearAllLeds();

  //   patternOneRoomOfEachFloorAtATime();
  //   delay(2000);
  //   clearAllLeds();
  //   lightUp3BHKs();
  //   delay(2000);
  //   clearAllLeds();
  //   lightUp4BHKs();
  //   delay(2000);
  //   clearAllLeds();
  // }
}


void loop() {
  // Check if a command has been sent via Serial Monitor
  if (Serial.available() > 0) {
    char command = Serial.read();

    // Stop any continuously running patterns and turn off LEDs before starting a new command
    activePattern = 0;
    clearAllLeds();

    switch (command) {
      case '1':
        Serial.println("Executing Pattern: One Floor at a Time");
        patternOneFloorAtATime();
        printMenu();  // Show menu again after pattern completes
        break;
      case '2':
        Serial.println("Executing Pattern: One Room of Each Floor at a Time");
        patternOneRoomOfEachFloorAtATime();
        printMenu();  // Show menu again after pattern completes
        break;
      case '3':
        Serial.println("Starting Pattern: Rainbow Smooth Fading (Enter '0' to stop)");
        activePattern = 3;  // Set Rainbow pattern as active
        break;
      case '4':
        Serial.println("Lighting up 3BHKs (Rooms 2 & 3)");
        lightUp3BHKs();
        break;
      case '5':
        Serial.println("Lighting up 4BHKs (Rooms 1, 4, 5, 6, 7)");
        lightUp4BHKs();
        break;
      case '0':
        Serial.println("Turning all LEDs OFF");
        // The clearAllLeds() call above already handled this.
        printMenu();
        break;
      case 'm':
      case 'M':
        printMenu();
        break;
      default:
        // Ignore invalid commands, but clear any leftover input
        while (Serial.available() > 0) Serial.read();
        break;
    }
  }

  // If the rainbow pattern is active, run its update function.
  // This is non-blocking to keep the system responsive.
  if (activePattern == 3) {
    if (millis() - lastPatternStep > 20) {  // Control the speed of the rainbow
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
 * @brief Operation 1.1: Lights up one entire floor at a time, cycling through all floors.
 */
void patternOneFloorAtATime() {
  uint32_t color = floors[0].Color(0, 150, 150);  // Teal color
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i].fill(color);
    floors[i].show();
    delay(500);
    floors[i].clear();
    floors[i].show();
  }
}

void turnOnAllLights() {
  uint32_t color = floors[0].Color(255, 255, 255); // White color
  for (int i = 0; i < NUM_FLOORS; i++) {
    floors[i].fill(color);
    floors[i].show();
  }
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
 * @brief Operation 1.3: Update function for the smooth rainbow pattern. Called repeatedly from loop().
 */
void updateRainbowFade() {
  rainbowFirstPixelHue += 256;  // Increment hue for animation

  for (int f = 0; f < NUM_FLOORS; f++) {
    for (int i = 0; i < LEDS_PER_FLOOR; i++) {
      // Create a complex hue that varies with pixel position and floor for a 3D effect
      int pixelHue = rainbowFirstPixelHue + (i * 65536L / 90) + (f * 65536L / (NUM_FLOORS * 2));
      floors[f].setPixelColor(i, wheel((pixelHue >> 8) & 255));
    }
    floors[f].show();
  }
}

/**
 * @brief Operation 3.1: Lights up Rooms 2 & 3 on all floors, with ducts on.
 */
void lightUp3BHKs() {
  uint32_t color = floors[0].Color(200, 200, 0);        // Yellow color
  uint32_t ductColor = floors[0].Color(200, 200, 200);  // White color for ducts

  // Room 2 is at array index 1, Room 3 is at array index 2
  int room2_start = room_start_indices[1];
  int room2_count = room_led_counts[1];
  int room3_start = room_start_indices[2];
  int room3_count = room_led_counts[2];

  // Duct 1 is at index 4, Duct 2 is at index 6
  int duct1_start = room_start_indices[4];
  int duct1_count = room_led_counts[4];
  int duct2_start = room_start_indices[6];
  int duct2_count = room_led_counts[6];

  for (int f = 0; f < NUM_FLOORS; f++) {
    // --- FIX ---
    // First, clear the entire floor to start fresh
    floors[f].clear();

    // Use setPixelColor for each section to avoid overwriting
    for (int i = 0; i < room2_count; i++) {
      floors[f].setPixelColor(room2_start + i, color);
    }
    for (int i = 0; i < room3_count; i++) {
      floors[f].setPixelColor(room3_start + i, color);
    }
    for (int i = 0; i < duct1_count; i++) {
      floors[f].setPixelColor(duct1_start + i, ductColor);
    }
    for (int i = 0; i < duct2_count; i++) {
      floors[f].setPixelColor(duct2_start + i, ductColor);
    }

    // Now, update the physical strip once all colors are set in memory
    floors[f].show();
  }
}

/**
 * @brief Operation 3.2: Lights up Rooms 1, 4, 5, 6, & 7 on all floors, with ducts on.
 */
void lightUp4BHKs() {
  uint32_t color = floors[0].Color(0, 200, 200);        // Cyan color
  uint32_t ductColor = floors[0].Color(200, 200, 200);  // White color for ducts

  // Room indices in our arrays: 0, 3, 5, 7, 8
  const int rooms_to_light[] = { 0, 3, 5, 7, 8 };
  int num_rooms_to_light = sizeof(rooms_to_light) / sizeof(rooms_to_light[0]);

  // Duct 1 is at index 4, Duct 2 is at index 6
  int duct1_start = room_start_indices[4];
  int duct1_count = room_led_counts[4];
  int duct2_start = room_start_indices[6];
  int duct2_count = room_led_counts[6];

  for (int f = 0; f < NUM_FLOORS; f++) {
    // --- FIX ---
    // First, clear the entire floor
    floors[f].clear();

    // Light up the specified rooms
    for (int i = 0; i < num_rooms_to_light; i++) {
      int room_index = rooms_to_light[i];
      int start = room_start_indices[room_index];
      int count = room_led_counts[room_index];
      for (int j = 0; j < count; j++) {
        floors[f].setPixelColor(start + j, color);
      }
    }

    // Light up the ducts
    for (int i = 0; i < duct1_count; i++) {
      floors[f].setPixelColor(duct1_start + i, ductColor);
    }
    for (int i = 0; i < duct2_count; i++) {
      floors[f].setPixelColor(duct2_start + i, ductColor);
    }

    // Update the physical strip once
    floors[f].show();
  }
}

/**
 * @brief Sets the duct LEDs on all floors to white. Called once from setup.
 */
void lightUpDuctsPermanently() {
  uint32_t ductColor = floors[0].Color(200, 200, 200);  // White color

  // Duct 1 is at index 4, Duct 2 is at index 6
  int duct1_start = room_start_indices[4];
  int duct1_count = room_led_counts[4];
  int duct2_start = room_start_indices[6];
  int duct2_count = room_led_counts[6];

  for (int f = 0; f < NUM_FLOORS; f++) {
    // Light up the ducts using loops and setPixelColor
    for (int i = 0; i < duct1_count; i++) {
      floors[f].setPixelColor(duct1_start + i, ductColor);
    }
    for (int i = 0; i < duct2_count; i++) {
      floors[f].setPixelColor(duct2_start + i, ductColor);
    }
    floors[f].show();  // Update the strip
  }
}

/**
 * @brief Helper function to generate rainbow colors.
 * @param WheelPos A value from 0 to 255.
 * @return A 32-bit packed color value.
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
