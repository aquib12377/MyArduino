#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define NUM_FLOORS 16
#define NUM_ROOMS_PER_FLOOR 4
#define NUM_LEDS_PER_ROOM 9
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
#define REFUGEE_COLOR 0xFFA500

const int NUM_COLORS = 5;
const int FADE_STEPS = 100;
const int DELAY_TIME_MS = 2;

uint8_t softColors[NUM_COLORS][3] = {
  { 255, 149, 130 },
  { 250, 210, 130 },
  { 122, 211, 255 },
  { 164, 135, 255 },
  { 251, 130, 211 },
};

const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = {
  2, 2, 3, 3
};

Adafruit_NeoPixel* ledsFloor[NUM_FLOORS];

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

#define IDLE_TIMEOUT (1UL * 1UL * 1000UL)

#define NUM_GROUPS 14
#define ROOMS_PER_GROUP 20
#define MAX_FADING_GROUPS 7
#define FADE_STEPS 50
#define FADE_DELAY_MS 10

unsigned long lastCommandTime = 0;

bool isIdleMode = false;

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
      ledsFloor[f]->show();
    } else {
      ledsFloor[f] = NULL;
    }
  }

  memset(roomAvailability, 0, sizeof(roomAvailability));
  Serial.println("Wing A slave ready.");
  turnOffAllLEDs();
}

void loop() {
  if (!isIdleMode) {
    unsigned long now = millis();
    if ((now - lastCommandTime) >= IDLE_TIMEOUT) {
      turnOffAllLEDs();
      isIdleMode = true;
    }
  }

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


void turnOnRefugeeFlats() {
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (isRefugee(floor, room)) {
        Serial.print(F("Turned ON Refugee Flats:"));
        Serial.print(F("Floor: "));
        Serial.print(floor + 1);
        Serial.print(F(" | Room: "));
        Serial.print(room + 1);
        ledsFloor[floor]->Color(46, 139, 87);
        ledsFloor[floor]->show();
      }
    }
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
    case 4: return 9;  //6
    case 5: return 8;  //8
    case 6: return 7;  //7
    case 7: return 6;  //9
    case 8: return 19;
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
        if (bhkType == 1) color = ledsFloor[f]->Color(softColors[1][0], softColors[1][1], softColors[1][2]);       // cyan
        else if (bhkType == 2) color = ledsFloor[f]->Color(softColors[0][0], softColors[0][1], softColors[0][2]);  // yellow
        else if (bhkType == 3) color = ledsFloor[f]->Color(softColors[2][0], softColors[2][1], softColors[2][2]);  // magenta
        else color = ledsFloor[f]->Color(255, 255, 255);                                                           // default white

        int ledStart = r * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          ledsFloor[f]->setPixelColor(ledStart + i, color);
        }
      }
    }
    ledsFloor[f]->show();
  }
}

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
bool shouldStop() {
  noInterrupts();
  bool stop = newCommandReceived;
  interrupts();
  return stop;
}

void displayAllPatterns() {
  pattern1();
  turnOffAllLEDs();
  pattern2();
  turnOffAllLEDs();
  pattern3();
  turnOffAllLEDs();
  pattern4();
  turnOffAllLEDs();
}

void runBHKCycle() {
  const int repeatCount = 3;
  for (int i = 0; i < repeatCount; ++i) {
    for (int j = 1; j <= 3; ++j) {
      turnOffAllLEDs();
      executeTurnOnBHK(j);
      delay(1000);
    }
    turnOffAllLEDs();
  }
}

void runPattern() {
  while (true) {
    if (shouldStop()) break;
    displayAllPatterns();
    runBHKCycle();
  }
}

// =========== PATTERN 1 ===========
void pattern1() {
  uint32_t color = ledsFloor[0]->Color(255, 255, 255);
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, color);
      ledsFloor[floor]->show();
      delay(5);
    }
  }
}

// =========== PATTERN 2 ===========
void pattern2() {
  uint32_t colorUp = ledsFloor[0]->Color(0, 255, 255);
  uint32_t colorDown = ledsFloor[0]->Color(255, 255, 0);

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, colorUp);
      ledsFloor[floor]->show();
      delay(5);
    }

    for (int i = TOTAL_LEDS_PER_FLOOR - 1; i >= 0; i--) {
      int room = i / NUM_LEDS_PER_ROOM;
      if (isRefugee(floor, room)) {
        continue;
      }
      ledsFloor[floor]->setPixelColor(i, colorDown);
      ledsFloor[floor]->show();
      delay(5);
    }
  }
}

// =========== PATTERN 3 ===========
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
    delay(10);
  }
}

// =========== PATTERN 4 ===========
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

// =========== patternSoftColors ===========
void patternSoftColorsSmooth() {
  const int colors[4][3] = {
    { 173, 216, 230 },  // Lightest Blue
    { 158, 201, 215 },  // Slightly Darker
    { 143, 186, 200 },  // Even Darker
    { 128, 171, 185 }   // Darkest Blue
  };

  while (true) {
    if (shouldStop()) break;

    turnOffAllLEDs();
    executeTurnOnBHK(2);
    delay(500);
    turnOffAllLEDs();
    executeTurnOnBHK(3);
    delay(500);
    turnOffAllLEDs();

    for (int j = 0; j < NUM_ROOMS_PER_FLOOR; j++) {
      for (int i = 0; i < NUM_FLOORS; i++) {
        controlFlat(i, j, colors[j % 4][0], colors[j % 4][1], colors[j % 4][2]);
        delay(80);
      }
    }

    delay(500);

    for (int k = 0; k < NUM_FLOORS + NUM_ROOMS_PER_FLOOR - 1; k++) {
      for (int i = 0; i < NUM_FLOORS; i++) {
        int j = k - i;
        if (j >= 0 && j < NUM_ROOMS_PER_FLOOR) {
          controlFlat(i, j, colors[j % 4][0], colors[j % 4][1], colors[j % 4][2]);
        }
      }
      delay(150);
    }

    delay(500);

    for (int t = 0; t < 20; t++) {
      int i = random(NUM_FLOORS);
      int j = random(NUM_ROOMS_PER_FLOOR);
      controlFlat(i, j, colors[j % 4][0], colors[j % 4][1], colors[j % 4][2]);
      delay(100);
    }

    delay(500);
    turnOffAllLEDs();
  }
}

// =================== NEW CONTROL METHODS ===================
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


void turnOffFlat(int floor, int room) {
  controlFlat(floor, room, 0, 0, 0);
}

void turnOffFloor(int floor) {
  controlFloor(floor, 0, 0, 0);
}

bool isRefugee(int floor, int room) {
  return (floor == 4 && (room == 2)) || (floor == 11 && (room == 1));
}
