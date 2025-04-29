#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// ----------------------------------------------------------------------------------------
//                          CONFIG / CONSTANTS
// ----------------------------------------------------------------------------------------
#define NUM_FLOORS 16
#define NUM_ROOMS_PER_FLOOR 4
#define NUM_LEDS_PER_ROOM 9
#define TOTAL_LEDS_PER_FLOOR (NUM_ROOMS_PER_FLOOR * NUM_LEDS_PER_ROOM)

// Use the new master’s I2C slave address:
#define I2C_SLAVE_ADDRESS 0x09

// ===== NEW COMMANDS =====
#define CMD_TERRACE                           55  // Example: light a specific zone
#define CMD_TURNOFFLEDS                       65
#define CMD_VERTICALROOMSCHASINGLED           66
#define CMD_ROOMVARIATIONS9TO11FLOOR          67
#define CMD_REFUGEEFLOORS1                    68
#define CMD_VERTICALROOMSFLOORWISEROOM1AND2     69
#define CMD_VERTICALROOMSFLOORWISEROOM3AND4     70
#define CMD_FLOOR3CONTROL                     71
#define CMD_FLOOR45678CONTROL                 72
#define CMD_ALLLIGHT                          73
#define CMD_ROOMVARIATIONS12TO15FLOOR         74
#define CMD_ROOMVARIATIONS16TO18FLOOR         75
#define CMD_REFUGEEFLOORS2                    76
#define CMD_SHOWAVAILABILITY                  78
#define CMD_2BHK                              79
#define CMD_3BHK                              80
#define CMD_PATTERN                           81
#define CMD_TurnOfAllLights                   82

// For example, refugee color defined as an orange tone (you may use this in your functions)
#define REFUGEE_COLOR 0xFFA500

// Some sample color sets used by the patterns
const int NUM_COLORS = 5;
uint8_t softColors[NUM_COLORS][3] = {
  {200, 149, 130},
  {250, 210, 130},
  {122, 211, 255},
  {164, 135, 255},
  {251, 130, 211},
};

// This array is used for BHK-based lighting
const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = { 2,2,3,3 };

// ----------------------------------------------------------------------------------------
//                           GLOBALS / STATE
// ----------------------------------------------------------------------------------------
Adafruit_NeoPixel* ledsFloor[NUM_FLOORS];

volatile bool newCommandReceived = false;
volatile uint8_t currentCommand = 0;
uint8_t lastCommand = 0;

// For ShowAvailability (if using chunked data from API)
uint8_t roomAvailability[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];
size_t totalAvailabilityDataSize = NUM_FLOORS * NUM_ROOMS_PER_FLOOR;
size_t availabilityDataBytesReceived = 0;

// State machine for chunked I2C data
enum ReceiveState {
  WAIT_FOR_START,
  READ_COMMAND_ID,
  READ_DATA_SIZE,
  READ_DATA_CHUNK,
  WAIT_FOR_END
};
ReceiveState receiveState = WAIT_FOR_START;

// Idle logic
#define IDLE_TIMEOUT 60000UL  // adjust as needed (in ms)
unsigned long lastCommandTime = 0;
bool isIdleMode = false;

// ----------------------------------------------------------------------------------------
//                                  SETUP
// ----------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.setClock(400000UL);
  Wire.onReceive(receiveEvent);

  // Initialize each floor’s NeoPixel strip based on your wiring.
  for (int f = 0; f < NUM_FLOORS; f++) {
    int pin = getFloorDataPin(f);
    if (pin != -1) {
      ledsFloor[f] = new Adafruit_NeoPixel(TOTAL_LEDS_PER_FLOOR, pin, NEO_GRB + NEO_KHZ800);
      ledsFloor[f]->begin();
      ledsFloor[f]->setBrightness(255);
      ledsFloor[f]->show();
    } else {
      ledsFloor[f] = NULL;
    }
  }

  memset(roomAvailability, 0, sizeof(roomAvailability));
  Serial.println("Slave ready.");
  turnOffAllLEDs();
}

// ----------------------------------------------------------------------------------------
//                                   LOOP
// ----------------------------------------------------------------------------------------
void loop() {
  // Check if idle period has elapsed.
  if (!isIdleMode) {
    unsigned long now = millis();
    if ((now - lastCommandTime) >= IDLE_TIMEOUT) {
      turnOffAllLEDs();
      isIdleMode = true;
    }
  }

  // When idle, run an idle pattern.
  if (isIdleMode) {
    patternSoftColorsSmooth();
  }

  // Process any newly received command.
  if (newCommandReceived) {
    noInterrupts();
    newCommandReceived = false;
    uint8_t cmd = currentCommand;
    interrupts();

    // Avoid re-running the same command repeatedly (except for availability).
    if (lastCommand == currentCommand && currentCommand != CMD_SHOWAVAILABILITY) {
      return;
    }

    // Optionally turn off LEDs before a new command (except for availability).
    if (cmd != CMD_SHOWAVAILABILITY) {
      //turnOffAllLEDs();
    }

    lastCommandTime = millis();
    isIdleMode = false;

    // ----------------- Handle the New Commands -----------------
    switch (cmd) {
      case CMD_TERRACE:
        Serial.println("CMD_TERRACE");
        // Example: light a specific zone (here, floor 1 in warm orange)
        //controlFloor(0, 255, 200, 100);
        break;

      case CMD_TURNOFFLEDS:
        Serial.println("CMD_TURNOFFLEDS");
        turnOffAllLEDs();
        break;

      case CMD_VERTICALROOMSCHASINGLED:
        Serial.println("CMD_VERTICALROOMSCHASINGLED");
        patternVerticalRoomsChasing();
        break;

      case CMD_ROOMVARIATIONS9TO11FLOOR:
        Serial.println("CMD_ROOMVARIATIONS9TO11FLOOR");
        patternRoomVariations9To11Floor();
        break;

      case CMD_REFUGEEFLOORS1:
        Serial.println("CMD_REFUGEEFLOORS1");
        turnOnRefugeeFlats1();
        break;

      case CMD_VERTICALROOMSFLOORWISEROOM1AND2:
        Serial.println("CMD_VERTICALROOMSFLOORWISEROOM1AND2");
        patternVerticalRoomsFloorWiseRoom1And2();
        break;

      case CMD_VERTICALROOMSFLOORWISEROOM3AND4:
        Serial.println("CMD_VERTICALROOMSFLOORWISEROOM3AND4");
        patternVerticalRoomsFloorWiseRoom3And4();
        break;

      case CMD_FLOOR3CONTROL:
        Serial.println("CMD_FLOOR3CONTROL");
        patternFloor3Control();
        break;

      case CMD_FLOOR45678CONTROL:
        Serial.println("CMD_FLOOR45678CONTROL");
        patternFloor45678Control();
        break;

      case CMD_ALLLIGHT:
        Serial.println("CMD_ALLLIGHT");
        patternAllLight();
        break;

      case CMD_ROOMVARIATIONS12TO15FLOOR:
        Serial.println("CMD_ROOMVARIATIONS12TO15FLOOR");
        patternRoomVariations12To15Floor();
        break;

      case CMD_ROOMVARIATIONS16TO18FLOOR:
        Serial.println("CMD_ROOMVARIATIONS16TO18FLOOR");
        patternRoomVariations16To18Floor();
        break;

      case CMD_REFUGEEFLOORS2:
        Serial.println("CMD_REFUGEEFLOORS2");
        turnOnRefugeeFlats2();
        break;

      case CMD_SHOWAVAILABILITY:
        Serial.println("CMD_SHOWAVAILABILITY");
        if (availabilityDataBytesReceived >= totalAvailabilityDataSize) {
          showAvailability();
          availabilityDataBytesReceived = 0;
        } else {
          Serial.println("Availability data incomplete.");
        }
        break;

      case CMD_2BHK:
        Serial.println("CMD_2BHK");
        executeTurnOnBHK(2);
        break;

      case CMD_3BHK:
        Serial.println("CMD_3BHK");
        executeTurnOnBHK(3);
        break;

      case CMD_PATTERN:
        Serial.println("CMD_PATTERN");
        patternSoftColorsSmooth();
        break;
      case CMD_TurnOfAllLights:
        Serial.println("CMD_PATTERN");
        turnOffAllLEDs();
        break;

      default:
        Serial.print("Unknown command: 0x");
        Serial.println(cmd, HEX);
        break;
    }

    lastCommand = currentCommand;
  }
}

// ----------------------------------------------------------------------------------------
//           PATTERN & LED CONTROL FUNCTION DEFINITIONS
// ----------------------------------------------------------------------------------------

// Example: Light up a vertical chase across all floors for each room.
void patternVerticalRoomsChasing() {
  uint8_t chaseR = softColors[0][0];
  uint8_t chaseG = softColors[0][1];
  uint8_t chaseB = softColors[0][2];
  const int chaseDelay = 100;

  for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      if (shouldStop()) return;
      if (!ledsFloor[floor] || isRefugee(floor, room)) continue;
      controlFlat(floor, room, chaseR, chaseG, chaseB);
      delay(chaseDelay);
    }
    for (int floor = NUM_FLOORS - 1; floor >= 0; floor--) {
      if (shouldStop()) return;
      if (!ledsFloor[floor] || isRefugee(floor, room)) continue;
      controlFlat(floor, room, chaseR, chaseG, chaseB);
      delay(chaseDelay);
    }
  }
}

// Pattern: Rooms variations for floors 9-11 ("Silver Sea View")
void patternRoomVariations9To11Floor() {
  uint8_t silver[3] =  { 173, 216, 230 };
  for (int f = 0; f < NUM_FLOORS; f++) {
    int floorNum = f + 1;
    if (floorNum >= 7 && floorNum <= 9) {
      for (int r = 2; r < 4; r++) { // Rooms 1 & 2 (index 0 & 1)
        if (isRefugee(f, r)) continue;
        controlFlat(f, r, silver[0], silver[1], silver[2]);
      }
    }
  }
}

// Pattern: Rooms variations for floors 12-15 ("Gold Sea View")
void patternRoomVariations12To15Floor() {
  uint8_t gold[3] =  { 0, 105, 148 };;
  for (int f = 0; f < NUM_FLOORS; f++) {
    int floorNum = f + 1;
    if (floorNum >= 10 && floorNum <= 13) {
      for (int r = 2; r < 4; r++) {
        if (isRefugee(f, r)) continue;
        controlFlat(f, r, gold[0], gold[1], gold[2]);
      }
    }
  }
}

// Pattern: Rooms variations for floors 16-18 ("Platinum Sea View")
void patternRoomVariations16To18Floor() {
  uint8_t platinum[3] = {65, 105, 255};
  for (int f = 0; f < NUM_FLOORS; f++) {
    int floorNum = f + 1;
    if (floorNum >= 14 && floorNum <= 16) {
      for (int r = 2; r < 4; r++) {
        if (isRefugee(f, r)) continue;
        controlFlat(f, r, platinum[0], platinum[1], platinum[2]);
      }
    }
  }
}

// Pattern: Vertical Rooms Floor-Wise Room1 & Room2 (e.g., for 3BHK Jodi)
void patternVerticalRoomsFloorWiseRoom1And2() {
  uint8_t warmYellow[3] = {255, 200, 50};
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    if (shouldStop()) return;
    if (!ledsFloor[floor]) continue;
    for (int r = 0; r < 2; r++) {
      if (isRefugee(floor, r)) continue;
      controlFlat(floor, r, warmYellow[0], warmYellow[1], warmYellow[2]);
    }
    delay(150);
  }
}

// Pattern: Vertical Rooms Floor-Wise Room3 & Room4 (e.g., for 2BHK Jodi)
void patternVerticalRoomsFloorWiseRoom3And4() {
  // Choose a different color (for example, a cool blue)
  uint8_t coolBlue[3] = {144, 238, 144};
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    if (shouldStop()) return;
    if (!ledsFloor[floor]) continue;
    // Rooms 3 & 4 => indices 2 and 3
    for (int r = 2; r < 4; r++) {
      if (isRefugee(floor, r)) continue;
      controlFlat(floor, r, coolBlue[0], coolBlue[1], coolBlue[2]);
    }
    delay(150);
  }
}

// Pattern: Floor 3 Control (for example, light only floor 3 with purple)
void patternFloor3Control() {
  uint8_t purple[3] = {255, 255, 0};
  // Floor 3 in 1-based numbering is index 2.
  controlFloor(0, purple[0], purple[1], purple[2]);
}

// Pattern: Floor 4 to 8 Control (for example, light floors 4 to 8 with white)
void patternFloor45678Control() {
  uint8_t white[3] = {255, 220, 120};
  for (int f = 1; f < 6; f++) {
    controlFloor(f, white[0], white[1], white[2]);
  }
}

// Pattern: All Light – turn every LED on (white)
void patternAllLight() {
  uint8_t white[3] = {255, 255, 255};
  for (int f = 0; f < NUM_FLOORS; f++) {
    controlFloor(f, white[0], white[1], white[2]);
  }
}

// Refugee Floors 1 – example: light specific flats with REFUGEE_COLOR
void turnOnRefugeeFlats1() {
  // Using REFUGEE_COLOR (0xFFA500) which is (255,165,0)
  uint8_t r = 255, g = 165, b = 0;
  // Example: light floor 4 room 2 and floor 11 room 1
  controlFlat(4, 2,46, 170, 67);
}

// Refugee Floors 2 – alternative configuration
void turnOnRefugeeFlats2() {
  uint8_t r = 255, g = 140, b = 0;
  // Example: light floor 5 room 2 and floor 12 room 1
  controlFlat(11, 1, 46, 170, 67);
}

// ----------------------------------------------------------------------------------------
//           BHK LOGIC (for 1, 2, 3 BHK lighting)
// ----------------------------------------------------------------------------------------
void executeTurnOnBHK(uint8_t bhkType) {
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;
    for (int r = 0; r < NUM_ROOMS_PER_FLOOR; r++) {
      if (isRefugee(f, r)) continue;
      if (bhkTypes[r] == bhkType) {
        uint32_t color;
        if (bhkType == 1) 
          color = ledsFloor[f]->Color(softColors[1][0], softColors[1][1], softColors[1][2]);
        else if (bhkType == 2) 
          color = ledsFloor[f]->Color(softColors[0][0], softColors[0][1], softColors[0][2]);
        else if (bhkType == 3) 
          color = ledsFloor[f]->Color(softColors[2][0], softColors[2][1], softColors[2][2]);
        else  
          color = ledsFloor[f]->Color(255, 255, 255);
          
        int ledStart = r * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          ledsFloor[f]->setPixelColor(ledStart + i, color);
        }
      }
    }
    ledsFloor[f]->show();
  }
}

// ----------------------------------------------------------------------------------------
//                     AVAILABILITY LOGIC
// ----------------------------------------------------------------------------------------
void showAvailability() {
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;
    for (int r = 0; r < NUM_ROOMS_PER_FLOOR; r++) {
      if (isRefugee(f, r)) continue;
      uint8_t stat = roomAvailability[f][r];
      uint32_t color = (stat == 0) ?
         ledsFloor[f]->Color(255, 70, 70) :    // Occupied: Red
         ledsFloor[f]->Color(110, 255, 101);     // Available: Green
      int ledStart = r * NUM_LEDS_PER_ROOM;
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[f]->setPixelColor(ledStart + i, color);
      }
    }
    ledsFloor[f]->show();
  }
}

// ----------------------------------------------------------------------------------------
//             OPTIONAL IDLE PATTERN (Soft Color Cycling)
// ----------------------------------------------------------------------------------------
bool shouldStop() {
  noInterrupts();
  bool stop = newCommandReceived;
  interrupts();
  return stop;
}

// Helper function to fade a single room from off to a target color.
void fadeFlat(int floor, int room, uint8_t targetR, uint8_t targetG, uint8_t targetB, int steps, int delayPerStep) {
  for (int step = 0; step <= steps; step++) {
    // Calculate interpolated color for this step.
    uint8_t newR = (targetR * step) / steps;
    uint8_t newG = (targetG * step) / steps;
    uint8_t newB = (targetB * step) / steps;
    controlFlat(floor, room, newR, newG, newB);
    delay(delayPerStep);
  }
}

// Helper: Returns brightness multiplier based on distance from the active (center) floor.
float getBrightnessFactor(int distance) {
  if (distance == 0)
    return 1.0;   // 100%
  else if (distance == 1)
    return 0.5;   // 50%
  else if (distance == 2)
    return 0.2;   // 20%
  else
    return 0.0;   // off for floors further away
}

void patternSoftColorsSmooth() {
  const int colors[4][3] = {
    {255, 255, 130},
    {255, 255, 130},
    {255, 255, 130},
    {255, 255, 130}
  };

  // Total fade time per floor in milliseconds and number of steps.
  int totalFadeTime = 250;
  int fadeSteps = 10;
  int stepDelay = totalFadeTime / fadeSteps;
    int fadeDelay = 250;

  while (true) {
    if (shouldStop()) break;
    
    turnOffAllLEDs();
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      // Move the active center from floor 0 to NUM_FLOORS-1
      for (int center = 0; center < NUM_FLOORS; center++) {
        for (int floor = 0; floor < NUM_FLOORS; floor++) {
          // Compute brightness based on the distance from the current center.
          float factor = getBrightnessFactor(abs(floor - center));
          // Scale the base color for this room by the factor.
          uint8_t scaledR = (uint8_t)(colors[room % 4][0] * factor);
          uint8_t scaledG = (uint8_t)(colors[room % 4][1] * factor);
          uint8_t scaledB = (uint8_t)(colors[room % 4][2] * factor);
          controlFlat(floor, room, scaledR, scaledG, scaledB);
        }
        delay(fadeDelay);
        if (shouldStop()) break;
      }
      if (shouldStop()) break;
    }
    turnOffAllLEDs();
    // Pattern 1: Fade in each floor for each room (using the room's color)
    for (int j = 0; j < NUM_ROOMS_PER_FLOOR; j++) {
      for (int i = 0; i < NUM_FLOORS; i++) {
        fadeFlat(i, j, colors[j % 4][0], colors[j % 4][1], colors[j % 4][2], fadeSteps, stepDelay);
        if (shouldStop()) break;
      }
      if (shouldStop()) break;
    }
    delay(500);
    if (shouldStop()) break;
    turnOffAllLEDs();
    // Pattern 2: Diagonal fade pattern across floors and rooms.
    for (int k = 0; k < NUM_FLOORS + NUM_ROOMS_PER_FLOOR - 1; k++) {
      for (int i = 0; i < NUM_FLOORS; i++) {
        int j = k - i;
        if (j >= 0 && j < NUM_ROOMS_PER_FLOOR) {
          fadeFlat(i, j, colors[j % 4][0], colors[j % 4][1], colors[j % 4][2], fadeSteps, stepDelay);
        }
      }
      delay(totalFadeTime);
      if (shouldStop()) break;
    }
    delay(500);
    turnOffAllLEDs();
    if (shouldStop()) break;

    // Pattern 3: Random sparkle with a fade-in effect.
    for (int t = 0; t < 20; t++) {
      int i = random(NUM_FLOORS);
      int j = random(NUM_ROOMS_PER_FLOOR);
      fadeFlat(i, j, colors[j % 4][0], colors[j % 4][1], colors[j % 4][2], fadeSteps, stepDelay);
      if (shouldStop()) break;
    }
    delay(500);
    turnOffAllLEDs();
  }
}


// ----------------------------------------------------------------------------------------
//                    I2C RECEIVE EVENT HANDLER
// ----------------------------------------------------------------------------------------
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
        if (currentCommand == CMD_SHOWAVAILABILITY) {
          receiveState = READ_DATA_SIZE;
        } else {
          newCommandReceived = true;
          receiveState = WAIT_FOR_END;
        }
        break;
      case READ_DATA_SIZE: {
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
          if (hIdx < 3) return;
          uint16_t offset = (header[0] << 8) | header[1];
          uint8_t chunkSize = header[2];
          hIdx = 0;
          uint8_t dataBuf[64];
          uint8_t dIdx = 0;
          while (Wire.available() && dIdx < chunkSize) {
            dataBuf[dIdx++] = Wire.read();
          }
          if (dIdx < chunkSize) return;
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

// ----------------------------------------------------------------------------------------
//                     LED / FLOOR HELPER METHODS
// ----------------------------------------------------------------------------------------
int getFloorDataPin(int floor) {
  switch (floor) {
    case 0:  return 13;
    case 1:  return 12;
    case 2:  return 11;
    case 3:  return 10;
    case 4:  return 9;
    case 5:  return 8;
    case 6:  return 7;
    case 7:  return 6;
    case 8:  return 19;
    case 9:  return 4;
    case 10: return 3;
    case 11: return 2;
    case 12: return 14;
    case 13: return 15;
    case 14: return 16;
    case 15: return 18;
    default: return -1;
  }
}

void turnOffAllLEDs() {
  for (int f = 0; f < NUM_FLOORS; f++) {
    if (!ledsFloor[f]) continue;
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (isRefugee(f, room)) continue;
      int ledStartIndex = room * NUM_LEDS_PER_ROOM;
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[f]->setPixelColor(ledStartIndex + i, 0);
      }
    }
    ledsFloor[f]->show();
  }
}

void controlFlat(int floor, int room, uint8_t r, uint8_t g, uint8_t b) {
  if (floor < 0 || floor >= NUM_FLOORS) return;
  if (room < 0 || room >= NUM_ROOMS_PER_FLOOR) return;
  if (!ledsFloor[floor]) return;
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
  for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
    if (isRefugee(floor, room)) continue;
    int ledStart = room * NUM_LEDS_PER_ROOM;
    for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
      ledsFloor[floor]->setPixelColor(ledStart + i, ledsFloor[floor]->Color(r, g, b));
    }
  }
  ledsFloor[floor]->show();
}

bool isRefugee(int floor, int room) {
  // Example: mark specific flats as "refugee" (adjust as needed)
  return false;
  //return (floor == 4 && room == 2) || (floor == 11 && room == 1);
}
