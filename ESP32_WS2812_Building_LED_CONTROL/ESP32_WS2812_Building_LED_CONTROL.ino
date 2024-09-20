#define BLYNK_TEMPLATE_ID "TMPL3W9OUu6H2"
#define BLYNK_TEMPLATE_NAME "Building Model"
#define BLYNK_AUTH_TOKEN "Y8CJDODQtjk6LDlZhWCKiD0GLDV-Vsp7"

#include <FastLED.h>
#include <BlynkSimpleEsp32.h>  // or BlynkSimpleStream.h for serial connection
#include <WiFi.h>

#define LED_PIN 13
#define NUM_LEDS 144  // Adjusted to include extra LEDs for gaps between floors

#define NUM_FLOORS 3
#define LEDS_PER_ROOM 5  // 4 LEDs per room, 1 LED skipped
#define ROOMS_PER_FLOOR 8
#define LEDS_PER_FLOOR (ROOMS_PER_FLOOR * LEDS_PER_ROOM)  // 40 LEDs per floor including skipped ones
#define FLOOR_GAP 4                                       // 4 LEDs between floors

CRGB leds[NUM_LEDS];

const CRGB colors[10] = {
  CRGB::Red,
  CRGB::Orange,
  CRGB::Yellow,
  CRGB::Green,
  CRGB::Blue,
  CRGB::Indigo,
  CRGB::Violet,
  CRGB::Pink,
  CRGB::White,
  CRGB::Gray
};
int floorIndices[NUM_FLOORS][LEDS_PER_FLOOR];

// Set your WiFi credentials
char ssid[] = "MyProject";
char pass[] = "12345678";

// Variables to track button states
bool runningLEDActive = false;
bool indicateAvailabilityActive = false;
bool bhksLigh1Active = false;
bool bhksLigh2Active = false;
bool bhksLigh3Active = false;

void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  Serial.begin(115200);
  FastLED.clear();
  
  int currentIndex = 0;
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < LEDS_PER_FLOOR; i++) {
      floorIndices[floor][i] = currentIndex;
      currentIndex++;
    }
    currentIndex += FLOOR_GAP;
  }

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    Serial.print("Floor ");
    Serial.print(floor + 1);
    Serial.print(": ");
    for (int i = 0; i < LEDS_PER_FLOOR; i++) {
      Serial.print(floorIndices[floor][i]);
      Serial.print(" ");
    }
    Serial.println();
  }

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void TurnOfAllLEDs() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void controlRoomLED(int floor, int room, bool state, CRGB color) {
  if (floor < 0 || floor >= NUM_FLOORS || room < 0 || room >= ROOMS_PER_FLOOR) {
    Serial.println("Invalid floor or room number!");
    return;
  }
  int startLEDIndex = room * LEDS_PER_ROOM;
  for (int i = 0; i < LEDS_PER_ROOM; i++) {
    int ledIndex = floorIndices[floor][startLEDIndex + i];
    if (i % 5 != 0) {
      leds[ledIndex] = color;
    }
  }
  FastLED.show();
}

void BHKsLIGH(int b) {
  delay(200);
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      if ((room == 0 || room == 1 || room == 2) && b == 1) {
        controlRoomLED(floor, room, true, CRGB::Blue);
        delay(100);
      }
      else if ((room == 3 || room == 4 || room == 5) && b == 2) {
        controlRoomLED(floor, room, true, CRGB::Purple);
        delay(100);
      }
      else if ((room == 6 || room == 7) && b == 3) {
        controlRoomLED(floor, room, true, CRGB::DarkTurquoise);
        delay(100);
      }
    }
  }
}

void IndicateAvailability() {
  delay(200);
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      if (room % 2 == 0) {
        controlRoomLED(floor, room, true, CRGB::White);
      }
      else {
        controlRoomLED(floor, room, true, CRGB::Red);
      }
      delay(100);
    }
  }
}

void runningLED(int numLeds, int delayMs) {
  int ledCount = 1;
  for (int offset = 0; offset < numLeds; offset++) {
    fill_solid(leds, numLeds, CRGB::Black);
    for (int i = 0; i < ledCount; i++) {
      int ledIndex = (offset + i) % numLeds;
      leds[ledIndex-1] = colors[(offset / ledCount) % 7];
      leds[ledIndex-2] = colors[(offset / ledCount) % 7];
      leds[ledIndex-3] = colors[(offset / ledCount) % 8];
      leds[ledIndex-4] = colors[(offset / ledCount) % 8];
      leds[ledIndex-5] = colors[(offset / ledCount) % 9];
      leds[ledIndex-6] = colors[(offset / ledCount) % 9];
      leds[ledIndex] = colors[(offset / ledCount) % 10];
      leds[ledIndex+1] = colors[(offset / ledCount) % 6];
      leds[ledIndex+2] = colors[(offset / ledCount) % 6];
      leds[ledIndex+3] = colors[(offset / ledCount) % 5];
      leds[ledIndex+4] = colors[(offset / ledCount) % 5];
      leds[ledIndex+5] = colors[(offset / ledCount) % 4];
      leds[ledIndex+6] = colors[(offset / ledCount) % 4];
    }
    FastLED.show();
    delay(delayMs);
  }
}

// Blynk button handlers
BLYNK_WRITE(V1) {
int pinValue = param.asInt(); 
if(pinValue == 0)
{
TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;  
  return;
}
TurnOfAllLEDs();
  runningLEDActive = true;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;
}

BLYNK_WRITE(V0) {
  int pinValue = param.asInt(); 
if(pinValue == 0)
{
TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;  
  return;
}
  TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = true;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;
}

BLYNK_WRITE(V2) {
  int pinValue = param.asInt(); 
if(pinValue == 0)
{
TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;  
  return;
}
  TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = true;
  bhksLigh2Active = false;
  bhksLigh3Active = false;
}

BLYNK_WRITE(V3) {
  int pinValue = param.asInt(); 
if(pinValue == 0)
{
TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;  
  return;
}
  TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = true;
  bhksLigh3Active = false;
}

BLYNK_WRITE(V4) {
  int pinValue = param.asInt(); 
if(pinValue == 0)
{
TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = false;  
  return;
}
  TurnOfAllLEDs();
  runningLEDActive = false;
  indicateAvailabilityActive = false;
  bhksLigh1Active = false;
  bhksLigh2Active = false;
  bhksLigh3Active = true;
}

void loop() {
  Blynk.run();

  if (runningLEDActive) {
    runningLED(NUM_LEDS, 80);
  } else if (indicateAvailabilityActive) {
    IndicateAvailability();
  } else if (bhksLigh1Active) {
    BHKsLIGH(1);
  } else if (bhksLigh2Active) {
    BHKsLIGH(2);
  } else if (bhksLigh3Active) {
    BHKsLIGH(3);
  }
}
