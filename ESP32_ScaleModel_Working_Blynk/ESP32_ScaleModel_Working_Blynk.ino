#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3W9OUu6H2"
#define BLYNK_TEMPLATE_NAME "Building Model"
#define BLYNK_AUTH_TOKEN "Y8CJDODQtjk6LDlZhWCKiD0GLDV-Vsp7"

#include <FastLED.h>
#include <BlynkSimpleEsp32.h>  // or BlynkSimpleStream.h for serial connection
#include <WiFi.h>

// Blynk token (replace with your actual Blynk token)

// WiFi credentials
char ssid[] = "MyProject";
char pass[] = "12345678";

#define NUM_FLOORS 8
#define ROOMS_PER_FLOOR 4
#define LEDS_PER_ROOM 4
#define NUM_LEDS (NUM_FLOORS * ROOMS_PER_FLOOR * LEDS_PER_ROOM)
CRGB roomColors[ROOMS_PER_FLOOR] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Purple };

CRGB leds[NUM_LEDS];

// LED index map for floors, rooms, and LEDs per room
int ledIndexMap[NUM_FLOORS][ROOMS_PER_FLOOR][LEDS_PER_ROOM] = {
  { { 112, 113, 114, 115 }, { 116, 117, 118, 119 }, { 120, 121, 122, 123 }, { 124, 125, 126, 127 } },
  { { 96, 97, 98, 99 }, { 100, 101, 102, 103 }, { 104, 105, 106, 107 }, { 108, 109, 110, 111 } },
  { { 80, 81, 82, 83 }, { 84, 85, 86, 87 }, { 88, 89, 90, 91 }, { 92, 93, 94, 95 } },
  { { 64, 65, 66, 67 }, { 68, 69, 70, 71 }, { 72, 73, 74, 75 }, { 76, 77, 78, 79 } },
  { { 48, 49, 50, 51 }, { 52, 53, 54, 55 }, { 56, 57, 58, 59 }, { 60, 61, 62, 63 } },
  { { 32, 33, 34, 35 }, { 36, 37, 38, 39 }, { 40, 41, 42, 43 }, { 44, 45, 46, 47 } },
  { { 16, 17, 18, 19 }, { 20, 21, 22, 23 }, { 24, 25, 26, 27 }, { 28, 29, 30, 31 } },
  { { 12, 13, 14, 15 }, { 8, 9, 10, 11 }, { 4, 5, 6, 7 }, { 0, 1, 2, 3 } }
};

void setup() {
  FastLED.addLeds<WS2812, 13, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  Serial.begin(115200);

  // Connect to WiFi and Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Serial.println("Enter command: <floor> <room> <color>");
}
void fadeInRoom(int floor, int room, CRGB targetColor, int steps = 50, int delayMs = 10) {
  for (int i = 0; i <= steps; i++) {
    float ratio = (float)i / steps;
    CRGB currentColor = CRGB(
      targetColor.r * ratio,
      targetColor.g * ratio,
      targetColor.b * ratio);

    // Set the color to the room
    if (floor >= 0 && floor < NUM_FLOORS && room >= 0 && room < ROOMS_PER_FLOOR) {
      for (int led = 0; led < LEDS_PER_ROOM; led++) {
        int ledIndex = ledIndexMap[floor][room][led];
        leds[ledIndex] = currentColor;
      }
      FastLED.show();
      delay(delayMs);
    }
  }
}
void fadeInFloor(int floor, CRGB targetColor, int steps = 50, int delayMs = 10) {
  for (int i = 0; i <= steps; i++) {
    float ratio = (float)i / steps;
    CRGB currentColor = CRGB(
      targetColor.r * ratio,
      targetColor.g * ratio,
      targetColor.b * ratio);

    if (floor >= 0 && floor < NUM_FLOORS) {
      for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
        for (int led = 0; led < LEDS_PER_ROOM; led++) {
          int ledIndex = ledIndexMap[floor][room][led];
          leds[ledIndex] = currentColor;
        }
      }
      FastLED.show();
      delay(delayMs);
    }
  }
}
// Button handlers for Blynk
BLYNK_WRITE(V0) {
  int data = param.asInt();
  if (data == 1) {
    for (int i = 0; i < NUM_FLOORS; i++) {
      int floor = i;
      for (int j = 0; j < ROOMS_PER_FLOOR; j++) {
        controlRoomLEDs(floor, j, random(2) ? CRGB::Red : CRGB::White);
      }
    }
  } else {
    turnOffAllLEDs();
  }
}

BLYNK_WRITE(V1) {
  int data = param.asInt();

  if (data == 1) {
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      for (int _room = 0; _room < ROOMS_PER_FLOOR; _room++) {
        controlRoomLEDs(floor, _room, CHSV(_room * (255 / ROOMS_PER_FLOOR), 255, 255));
        delay(100);
      }
    }
    turnOffAllLEDs();  // Turn off all LEDs
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      controlFloorLEDs(floor, CRGB::SeaGreen);  // Choose a color for demo
      delay(200);                           // Delay for 500 ms between floors
    }
    turnOffAllLEDs();  // Turn off all LEDs first
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      uint8_t hue = floor * (255 / NUM_FLOORS);  // Distributes hues evenly across the floors
      CRGB floorColor;
      floorColor.setHSV(hue, 255, 255);        // Full saturation and brightness
      fadeInFloor(floor, floorColor, 50, 10);  // 50 steps, 10ms delay for smooth fade
      delay(200);                              // Delay between floors for a cascading effect
    }
    turnOffAllLEDs();  // Turn off all LEDs first
    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      Serial.printf("Controlling Room %d\n", room + 1);
      for (int floor = 0; floor < NUM_FLOORS; floor++) {
        fadeInRoom(floor, room, random(2) ? CRGB::MediumPurple : CRGB::PaleVioletRed, 10, 5);  // Adjust steps and delay as needed
      }
      delay(200);
    }
  } else {
    turnOffAllLEDs();
  }
}

BLYNK_WRITE(V2) {
  int data = param.asInt();
  if (data == 1) {
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      controlRoomLEDs(floor, 1, CRGB::Yellow);  // Room 0 on each floor
      controlRoomLEDs(floor, 3, CRGB::Yellow);  // Room 1 on each floor
    }
  } else {
    turnOffAllLEDs();
  }
}

BLYNK_WRITE(V3) {
  int data = param.asInt();
  if (data == 1) {
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      controlRoomLEDs(floor, 1, CRGB::Cyan);  // Room 2 on each floor
    }
  } else {
    turnOffAllLEDs();
  }
}

BLYNK_WRITE(V4) {
  int data = param.asInt();
  if (data == 1) {
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      controlRoomLEDs(floor, 0, CRGB::Purple);  // Room 3 on each floor
    }
  } else {
    turnOffAllLEDs();
  }
}

void loop() {
  Blynk.run();
  FastLED.show();
}

// Function to control room LEDs
void controlRoomLEDs(int floor, int room, CRGB color) {
  //turnOffAllLEDs();
  for (int led = 0; led < LEDS_PER_ROOM; led++) {
    int ledIndex = ledIndexMap[floor][room][led];
    leds[ledIndex] = color;
  }
  FastLED.show();
}

// Function to control floor LEDs
void controlFloorLEDs(int floor, CRGB color) {
  for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
    for (int led = 0; led < LEDS_PER_ROOM; led++) {
      int ledIndex = ledIndexMap[floor][room][led];
      leds[ledIndex] = color;
    }
  }
  FastLED.show();
}
// void fadeInRoom(int floor, int room, CRGB targetColor, int steps = 50, int delayMs = 10) {
//   for(int i = 0; i <= steps; i++) {
//     float ratio = (float)i / steps;
//     CRGB currentColor = CRGB(
//       targetColor.r * ratio,
//       targetColor.g * ratio,
//       targetColor.b * ratio
//     );

//     // Set the color to the room
//     if (floor >= 0 && floor < NUM_FLOORS && room >= 0 && room < ROOMS_PER_FLOOR) {
//       for(int led = 0; led < LEDS_PER_ROOM; led++) {
//         int ledIndex = ledIndexMap[floor][room][led];
//         leds[ledIndex] = currentColor;
//       }
//       FastLED.show();
//       delay(delayMs);
//     }
//   }
// }
void turnOffAllLEDs() {
  FastLED.clear();
  FastLED.show();
}
