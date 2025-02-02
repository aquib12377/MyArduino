#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define NUM_FLOORS 35
#define NUM_ROOMS_PER_FLOOR 8
#define NUM_LEDS_PER_ROOM 4
#define TOTAL_LEDS_PER_FLOOR (NUM_ROOMS_PER_FLOOR * NUM_LEDS_PER_ROOM)

#define I2C_SLAVE_ADDRESS 0x08

#define CMD_TURN_OFF_ALL 0xA0
#define CMD_TURN_ON_ALL 0xA1
#define CMD_ACTIVATE_PATTERNS 0xA2
#define CMD_CONTROL_ALL_TOWERS 0xA3
#define CMD_VIEW_AVAILABLE_FLATS 0xA4
#define CMD_FILTER_1BHK 0xA5
#define CMD_FILTER_2BHK 0xA6
#define CMD_FILTER_3BHK 0xA7

#define CMD_WINGA_2BHK_689_SQFT 0xB0
#define CMD_WINGA_1BHK_458_SQFT 0xB1
#define CMD_WINGA_1BHK_461_SQFT 0xB2

#define CMD_SHOW_AVAIL 0xC8
#define REFUGEE_COLOR 0xFFA500  // Orange in 24-bit hex (R=255,G=165,B=0)

const int NUM_COLORS = 5;
const int FADE_STEPS = 100;   // Number of steps in each transition
const int DELAY_TIME_MS = 2;  // Delay between steps in milliseconds

uint8_t softColors[NUM_COLORS][3] = {
  { 255, 149, 130 },  // Soft color 1
  { 250, 210, 130 },  // Soft color 1
  { 122, 211, 255 },  // Soft color 1
  { 164, 135, 255 },  // Soft color 1
  { 251, 130, 211 },  // Soft color 1
};

const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = {
  2, 2, 1, 1, 2, 2, 3, 3
};

Adafruit_NeoPixel* ledsFloor[NUM_FLOORS];

struct FadeTask {
  int floor;
  int room;
  int brightness;
  int direction;  // +1 for fading in, -1 for fading out
  bool active;
};

#define NUM_TASKS 180
FadeTask tasks[NUM_TASKS];

volatile bool newCommandReceived = false;
volatile uint8_t currentCommand = 0;
uint8_t lastCommand = 0;
uint8_t roomAvailability[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];
size_t totalAvailabilityDataSize = NUM_FLOORS * NUM_ROOMS_PER_FLOOR;
size_t availabilityDataBytesReceived = 0;

enum ReceiveState {
  WAIT_FOR_START,
  READ_COMMAND_ID,
  READ_DATA_SIZE,
  READ_DATA_CHUNK,
  WAIT_FOR_END
};
ReceiveState receiveState = WAIT_FOR_START;

#define IDLE_TIMEOUT (1UL * 60UL * 1000UL)  // 5 minutes in milliseconds

#define NUM_GROUPS 14
#define ROOMS_PER_GROUP 20
#define MAX_FADING_GROUPS 7
#define FADE_STEPS 50
#define FADE_DELAY_MS 10  // delay between fade steps

bool groupIsFading[NUM_GROUPS] = { false };
int groupId[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];



// Flags to track selected groups
bool selectedGroup[NUM_GROUPS] = { false };
struct GroupFade {
  int floor;
  int group;
  uint32_t fromColor;
  uint32_t toColor;
  int currentStep;
  bool active;
};

GroupFade groupFades[MAX_FADING_GROUPS];
unsigned long lastCommandTime = 0;  // Tracks last time we got a command

bool isIdleMode = false;
void initializeFadeTasks() {
  for (int t = 0; t < NUM_TASKS; ++t) {
    tasks[t].floor = random(0, NUM_FLOORS);
    tasks[t].room = random(0, NUM_ROOMS_PER_FLOOR);
    tasks[t].brightness = 0;
    tasks[t].direction = 1;  // start with fade-in
    tasks[t].active = true;
  }
}
void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.setClock(400000UL);
  Wire.onReceive(receiveEvent);

  for (int f = 0; f < NUM_FLOORS; f++) {
    int pin = getFloorDataPin(f);
    if (pin != -1) {
      ledsFloor[f] = new Adafruit_NeoPixel(TOTAL_LEDS_PER_FLOOR, pin, NEO_GRB + NEO_KHZ800);
      ledsFloor[f]->begin();
      ledsFloor[f]->setBrightness(255);
      ledsFloor[f]->show();  // all off
    } else {
      ledsFloor[f] = NULL;  // invalid pin => no strip
    }
  }
  // for (int floor = 0; floor < NUM_FLOORS; floor++) {
  //   if (!ledsFloor[floor]) continue;
  //   for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
  //     if (isRefugee(floor, room)) {
  //       int ledStartIndex = room * NUM_LEDS_PER_ROOM;
  //       for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
  //         ledsFloor[floor]->setPixelColor(ledStartIndex + i,255,0,255);
  //       }
  //     }
  //   }
  //   ledsFloor[floor]->show();
  // }
  initializeFadeTasks();
  memset(roomAvailability, 0, sizeof(roomAvailability));
  Serial.println("Wing A slave ready.");
  // turnOnAllLEDs();
  // delay(1000);
  // turnOffAllLEDs();

  //Refugee for Wing B
  // controlFlat(3, 2, 255, 0, 0);
  // controlFlat(8, 2, 255, 0, 0);
  // controlFlat(13, 2, 255, 0, 0);
  // controlFlat(18, 2, 255, 0, 0);
  // controlFlat(22, 2, 255, 0, 0);
  // controlFlat(27, 2, 255, 0, 0);
  // controlFlat(32, 2, 255, 0, 0);

  // for (int i = 0; i < 8; i++) {
  // controlFlat(0, i, 255, 0, 0);
  // delay(1000);
  // }
  turnOffAllLEDs();
  // while(true)
  // {
  //   for (int floor = 0 ; floor <= NUM_FLOORS; floor++) {
  //     controlFloor(floor, 255, 149, 130);
  //     delay(500);
  //   }
  //   turnOffAllLEDs();
  //   delay(500);
  // }
}

void loop() {
  if (!isIdleMode) {
    unsigned long now = millis();
    if ((now - lastCommandTime) >= IDLE_TIMEOUT) {
      // 5 minutes have passed with no new commands => enter idle
      turnOffAllLEDs();
      isIdleMode = true;
    }
  }

  // (B) If we are idle, run an idle pattern
  if (isIdleMode) {
    patternSoftColorsSmooth();
  }
  if (newCommandReceived) {
    noInterrupts();
    newCommandReceived = false;
    uint8_t cmd = currentCommand;
    interrupts();
    if (lastCommand == currentCommand && currentCommand != CMD_SHOW_AVAIL) {
      return;
    }
    if (cmd != CMD_SHOW_AVAIL) {
      turnOffAllLEDs();
    }
    lastCommandTime = millis();

    // (2) If we are in idle mode, exit it
    isIdleMode = false;
    switch (cmd) {

      case CMD_TURN_OFF_ALL:
        Serial.println("CMD_TURN_OFF_ALL");
        turnOffAllLEDs();
        break;
      case CMD_TURN_ON_ALL:
        Serial.println("CMD_TURN_ON_ALL");
        turnOnAllLEDs();
        break;
      case CMD_ACTIVATE_PATTERNS:
        Serial.println("CMD_ACTIVATE_PATTERNS");
        runPattern();
        break;
      case CMD_CONTROL_ALL_TOWERS:
        Serial.println("CMD_CONTROL_ALL_TOWERS");
        turnOnAllLEDs();
        break;
      case CMD_VIEW_AVAILABLE_FLATS:
        Serial.println("CMD_VIEW_AVAILABLE_FLATS");
        showAvailability();
        break;
      case CMD_FILTER_1BHK:
        Serial.println("CMD_FILTER_1BHK");
        executeTurnOnBHK(1);
        break;
      case CMD_FILTER_2BHK:
        Serial.println("CMD_FILTER_2BHK");
        executeTurnOnBHK(2);
        break;
      case CMD_FILTER_3BHK:
        Serial.println("CMD_FILTER_3BHK");
        executeTurnOnBHK(3);
        break;
      case CMD_WINGA_2BHK_689_SQFT:
        Serial.println("CMD_WINGA_2BHK_689_SQFT");
        executeTurnOnBHK(2);
        break;
      case CMD_WINGA_1BHK_458_SQFT:
        Serial.println("CMD_WINGA_1BHK_458_SQFT");
        executeTurnOnBHK(1);
        break;
      case CMD_WINGA_1BHK_461_SQFT:
        Serial.println("CMD_WINGA_1BHK_461_SQFT");
        executeTurnOnBHK(3);
        break;
      case CMD_SHOW_AVAIL:
        Serial.println("CMD_SHOW_AVAIL");
        if (availabilityDataBytesReceived >= totalAvailabilityDataSize) {
          showAvailability();
          availabilityDataBytesReceived = 0;
        } else {
          Serial.println("Availability data incomplete.");
        }
        break;
      default:
        Serial.print("Unknown command: 0x");
        Serial.println(cmd, HEX);
        break;
    }
    lastCommand = currentCommand;
  }
}

void receiveEvent(int howMany) {
  while (Wire.available()) {
    uint8_t b = Wire.read();

    switch (receiveState) {

      case WAIT_FOR_START:
        if (b == 0xAA) {
          receiveState = READ_COMMAND_ID;
        }
        break;

      case READ_COMMAND_ID:
        currentCommand = b;
        if (currentCommand == CMD_SHOW_AVAIL) {
          receiveState = READ_DATA_SIZE;
        } else {
          newCommandReceived = true;
          receiveState = WAIT_FOR_END;
        }
        break;
      case READ_DATA_SIZE:
        {
          static uint8_t sizeBuf[2];
          static uint8_t idx = 0;
          sizeBuf[idx++] = b;

          if (idx == 2) {
            uint16_t totalDataSize = (sizeBuf[0] << 8) | sizeBuf[1];
            idx = 0;
            availabilityDataBytesReceived = 0;
            receiveState = READ_DATA_CHUNK;
          }
        }
        break;
      case READ_DATA_CHUNK:
        if (b == 0xAB) {
          static uint8_t header[3];
          static uint8_t hIdx = 0;
          while (Wire.available() && hIdx < 3) {
            header[hIdx++] = Wire.read();
          }
          if (hIdx < 3) {
            return;
          }

          uint16_t offset = (header[0] << 8) | header[1];
          uint8_t chunkSize = header[2];
          hIdx = 0;
          uint8_t dataBuf[64];
          uint8_t dIdx = 0;

          while (Wire.available() && dIdx < chunkSize) {
            dataBuf[dIdx++] = Wire.read();
          }
          if (dIdx < chunkSize) return;  // incomplete
          uint8_t chksum = 0;
          if (Wire.available()) {
            chksum = Wire.read();
          } else {
            receiveState = WAIT_FOR_START;
            return;
          }
          uint8_t calcSum = 0;
          for (uint8_t i = 0; i < dIdx; i++) {
            calcSum += dataBuf[i];
          }

          if (chksum == calcSum) {
            if (offset + chunkSize <= totalAvailabilityDataSize) {
              memcpy(((uint8_t*)roomAvailability) + offset, dataBuf, dIdx);
              availabilityDataBytesReceived += dIdx;
            } else {
              Serial.println("Data chunk exceeds array bounds!");
            }
          } else {
            Serial.println("Checksum mismatch!");
          }
        } else if (b == 0x55) {
          receiveState = WAIT_FOR_START;
          if (availabilityDataBytesReceived >= totalAvailabilityDataSize) {
            newCommandReceived = true;
          }
        } else {
          receiveState = WAIT_FOR_START;
        }
        break;

      case WAIT_FOR_END:
        if (b == 0x55) {
          receiveState = WAIT_FOR_START;
        }
        break;
    }
  }
}

int getFloorDataPin(int floor) {
  switch (floor) {
    case 0: return 13;
    case 1: return 12;
    case 2: return 11;
    case 3: return 10;
        case 4: return 6;//6
    case 5: return 8;//8
    case 6: return 7;//7
    case 7: return 9;//9
    case 8: return 8;
    case 9: return 4;
    case 10: return 3;
    case 11: return 2;
    case 12: return 14;
    case 13: return 15;
    case 14: return 16;
    case 15: return 18;
    case 16: return 19;
    case 17: return 23;
    case 18: return 25;
    case 19: return 27;
    case 20: return 29;
    case 21: return 31;
    case 22: return 33;
    case 23: return 35;
    case 24: return 37;
    case 25: return 39;
    case 26: return 41;
    case 27: return 43;
    case 28: return 45;
    case 29: return 47;
    case 30: return 49;
    case 31: return 51;
    case 32: return 53;
    case 33: return A15;
    case 34: return A14;
    default: return -1;
  }
}

void turnOffAllLEDs() {
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;

    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (isRefugee(f, room)) {
        continue;
      }
      int ledStartIndex = room * NUM_LEDS_PER_ROOM;
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[f]->setPixelColor(ledStartIndex + i, 0);
      }
    }
    ledsFloor[f]->show();
  }
}

void turnOnAllLEDs() {
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (isRefugee(f, room)) {
        continue;
      }
      int ledStartIndex = room * NUM_LEDS_PER_ROOM;
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[f]->setPixelColor(ledStartIndex + i, softColors[0][0], softColors[0][1], softColors[0][2]);
      }
    }
    ledsFloor[f]->show();
  }
}

void executeTurnOnBHK(uint8_t bhkType) {
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;
    for (int r = 0; r < NUM_ROOMS_PER_FLOOR; r++) {
      if (isRefugee(f, r))
        continue;
      if (bhkTypes[r] == bhkType) {
        uint32_t color;
        if (bhkType == 1) color = ledsFloor[f]->Color(0, 255, 255);       // cyan
        else if (bhkType == 2) color = ledsFloor[f]->Color(255, 255, 0);  // yellow
        else if (bhkType == 3) color = ledsFloor[f]->Color(255, 100, 100);  // magenta
        else color = ledsFloor[f]->Color(255, 255, 255);                  // default white

        int ledStart = r * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          ledsFloor[f]->setPixelColor(ledStart + i, color);
        }
      }
    }
    ledsFloor[f]->show();
  }
}

// Show availability data (0 => Red / 1 => Green, etc.)
void showAvailability() {
  //=================================================================================
  //    Uncommnent for testing available room lights with dummy random data
  //=================================================================================
  //randomSeed(analogRead(0));  // Or any other method of seeding
  // // Fill the array with random 0s and 1s
  // for (int floor = 0; floor < NUM_FLOORS; floor++) {
  //   for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
  //     roomAvailability[floor][room] = random(0, 2);
  //     // random(0,2) generates 0 or 1
  //   }
  // }
  //=================================================================================
  //    Uncommnent for testing available room lights with dummy random data
  //=================================================================================
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;
    for (int r = 0; r < NUM_ROOMS_PER_FLOOR; r++) {
      if (isRefugee(f, r))
        continue;
      uint8_t stat = roomAvailability[f][r];
      uint32_t color = (stat == 0)
                         ? ledsFloor[f]->Color(255, 70, 70)     // occupied => red
                         : ledsFloor[f]->Color(110, 255, 101);  // available => green
      int ledStart = r * NUM_LEDS_PER_ROOM;
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[f]->setPixelColor(ledStart + i, color);
      }
    }
    ledsFloor[f]->show();
  }
}

// ======================= OPTIONAL PATTERN =======================
void runPattern() {
  // Example: cycle 1BHK -> 2BHK -> 3BHK -> all -> repeat
  // until a new command interrupts
  while (true) {
    noInterrupts();
    bool stop = newCommandReceived;
    interrupts();
    if (stop) break;

    pattern1();
    turnOffAllLEDs();
    pattern2();
    turnOffAllLEDs();
    pattern3();
    turnOffAllLEDs();
    pattern4();
    turnOffAllLEDs();
    //pattern5Smooth();
    int o = 0;
    uint32_t whiteColor = ledsFloor[0]->Color(255, 255, 0);

    while (++o < 3) {
      turnOffAllLEDs();
      executeTurnOnBHK(1);
      delay(1000);
      turnOffAllLEDs();
      executeTurnOnBHK(2);
      delay(1000);
      turnOffAllLEDs();
      executeTurnOnBHK(3);
      delay(1000);
      turnOffAllLEDs();
    }
  }
}



// =========== PATTERN 1 ===========
// Lights up each LED on each floor with white, one-by-one
void pattern1() {
  uint32_t color = ledsFloor[0]->Color(255, 255, 255);  // white
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      // Calculate the room from LED index
      int room = i / NUM_LEDS_PER_ROOM;
      // If refugee, skip
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, color);
      ledsFloor[floor]->show();
      delay(2);  // Adjust speed
    }
  }
}

// =========== PATTERN 2 ===========
// Moves "up" then "down" each floor, coloring LEDs
void pattern2() {
  // Start color
  uint32_t colorUp = ledsFloor[0]->Color(0, 255, 255);    // e.g., cyan
  uint32_t colorDown = ledsFloor[0]->Color(255, 255, 0);  // e.g., yellow

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    // "Up" the floor
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, colorUp);
      ledsFloor[floor]->show();
      delay(2);
    }

    // "Down" the floor
    for (int i = TOTAL_LEDS_PER_FLOOR - 1; i >= 0; i--) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, colorDown);
      ledsFloor[floor]->show();
      delay(2);
    }
  }
}

// =========== PATTERN 3 ===========
// Horizontal RGB fading (Magenta->Cyan->LightCoral->Turquoise) across each floor
void pattern3() {
  const int numColors = 4;
  uint8_t colors[numColors][3] = {
    { 255, 0, 255 },    // Magenta
    { 0, 255, 255 },    // Cyan
    { 240, 128, 128 },  // LightCoral
    { 64, 224, 208 }    // Turquoise
  };

  int segmentLength = TOTAL_LEDS_PER_FLOOR / (numColors - 1);

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }

      int segment = i / segmentLength;
      if (segment >= numColors - 1) segment = numColors - 2;
      float localT = (float)(i - segment * segmentLength) / (float)segmentLength;

      uint8_t r = (uint8_t)((1 - localT) * colors[segment][0] + localT * colors[segment + 1][0]);
      uint8_t g = (uint8_t)((1 - localT) * colors[segment][1] + localT * colors[segment + 1][1]);
      uint8_t b = (uint8_t)((1 - localT) * colors[segment][2] + localT * colors[segment + 1][2]);

      ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->Color(r, g, b));
    }
    ledsFloor[floor]->show();
    delay(100);
  }
}

// =========== PATTERN 4 ===========
// Vertical RGB fading across floors (Red->Green->Yellow->Blue->Orange)
void pattern4() {
  const int numColors = 5;
  uint8_t verticalColors[numColors][3] = {
    { 255, 0, 0 },    // Red
    { 0, 255, 0 },    // Green
    { 255, 255, 0 },  // Yellow
    { 0, 0, 255 },    // Blue
    { 255, 165, 0 }   // Orange
  };

  int segmentLength = NUM_FLOORS / (numColors - 1);

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    int segment = floor / segmentLength;
    if (segment >= numColors - 1) segment = numColors - 2;

    float localT = (float)(floor - segment * segmentLength) / (float)segmentLength;
    uint8_t r = (uint8_t)((1 - localT) * verticalColors[segment][0] + localT * verticalColors[segment + 1][0]);
    uint8_t g = (uint8_t)((1 - localT) * verticalColors[segment][1] + localT * verticalColors[segment + 1][1]);
    uint8_t b = (uint8_t)((1 - localT) * verticalColors[segment][2] + localT * verticalColors[segment + 1][2]);

    // Color the entire floor, except refugee flats
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->Color(r, g, b));
    }
    ledsFloor[floor]->show();
    delay(100);
  }
}

// =========== PATTERN 5 ===========

// =========== patternSoftColors ===========
// Example fade pattern across multiple colors
void patternSoftColorsSmooth() {
  while (true) {
    noInterrupts();
    bool stop = newCommandReceived;
    interrupts();
    if (stop) break;

    uint32_t goldenColor = 0;
    for (int f = 0; f < NUM_FLOORS; f++) {
      if (ledsFloor[f]) {
        goldenColor = ledsFloor[f]->Color(255, 215, 0);
        break;
      }
    }

    // Update each fade task
    for (int t = 0; t < NUM_TASKS; t++) {
      if (!tasks[t].active) continue;

      int floor = tasks[t].floor;
      if (!ledsFloor[floor]) continue;  // skip if floor LED strip is not available

      Adafruit_NeoPixel* strip = ledsFloor[floor];
      int room = tasks[t].room;
      int startIndex = room * NUM_LEDS_PER_ROOM;

      // Adjust brightness for this task
      tasks[t].brightness += tasks[t].direction;
      if (tasks[t].brightness >= 255) {
        tasks[t].brightness = 255;
        tasks[t].direction = -5;  // start fading out once fully bright
      } else if (tasks[t].brightness <= 0) {
        tasks[t].brightness = 0;
        tasks[t].direction = 5;  // switch back to fading in

        // Optionally assign a new random floor/room when a cycle completes
        tasks[t].floor = random(0, NUM_FLOORS);
        tasks[t].room = random(0, NUM_ROOMS_PER_FLOOR);
      }

      // Scale the golden color by the current brightness
      uint8_t r = (uint8_t)(255 * tasks[t].brightness / 255);  // Golden red component
      uint8_t g = (uint8_t)(255 * tasks[t].brightness / 255);  // Golden green component
      uint8_t b = (uint8_t)(100 * tasks[t].brightness / 255);  // Golden blue component

      uint32_t scaledColor = strip->Color(r, g, b);

      // Apply the scaled color to all LEDs in the room
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        strip->setPixelColor(startIndex + i, scaledColor);
      }
    }

    // Update all LED strips after processing tasks
    for (int f = 0; f < NUM_FLOORS; f++) {
      if (ledsFloor[f]) {
        ledsFloor[f]->show();
      }
    }

    delay(1);  // Short pause to control update speed; adjust as necessary
  }
}




// =================== NEW CONTROL METHODS ===================

void controlFlat(int floor, int room, uint8_t r, uint8_t g, uint8_t b) {
  if (floor < 0 || floor >= NUM_FLOORS) return;
  if (room < 0 || room >= NUM_ROOMS_PER_FLOOR) return;
  if (!ledsFloor[floor]) return;

  // Skip changing color for refugee flats
  if (isRefugee(floor, room)) return;

  int ledStart = room * NUM_LEDS_PER_ROOM;
  for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
    ledsFloor[floor]->setPixelColor(ledStart + i, ledsFloor[floor]->Color(r, g, b));
  }
  ledsFloor[floor]->show();
}


void controlFloor(int floor, uint8_t r, uint8_t g, uint8_t b) {
  if (floor < 0 || floor >= NUM_FLOORS) return;
  if (!ledsFloor[floor]) return;

  // Instead of setting all LEDs directly, iterate room by room
  for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
    // Skip refugee rooms
    if (isRefugee(floor, room)) continue;

    int ledStart = room * NUM_LEDS_PER_ROOM;
    for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
      ledsFloor[floor]->setPixelColor(ledStart + i, ledsFloor[floor]->Color(r, g, b));
    }
  }
  ledsFloor[floor]->show();
}


void turnOffFlat(int floor, int room) {
  controlFlat(floor, room, 0, 0, 0);
}

void turnOffFloor(int floor) {
  controlFloor(floor, 0, 0, 0);
}

bool isRefugee(int floor, int room) {
  // Adjust if your indexing is zero-based: e.g. floor=14 if you say "15th floor"
  // Here we assume floor=15 is literally the 16th floor in zero-based indexing.
  return (floor == 15 && (room == 2 || room == 3|| room == 4|| room == 5|| room == 6));
}
