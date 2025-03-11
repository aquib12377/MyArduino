// Slave Code for Arduino Mega

#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define NUM_FLOORS 35
#define NUM_ROOMS_PER_FLOOR 8
#define NUM_LEDS_PER_ROOM 4
#define TOTAL_LEDS_PER_FLOOR (NUM_ROOMS_PER_FLOOR * NUM_LEDS_PER_ROOM)

// Command IDs (updated to match master code)
#define CMD_TURN_OFF_ALL 0xA0  // 0xA0 from master code
#define CMD_TURN_ON_2BHK1 0xC0
#define CMD_TURN_ON_2BHK2 0xC1
#define CMD_TURN_ON_3BHKA1 0xC2
#define CMD_TURN_ON_3BHKA2 0xC3
#define CMD_TURN_ON_3BHKB1 0xC4
#define CMD_TURN_ON_3BHKB2 0xC5
#define CMD_TURN_ON_REFUGEE 0xE6
#define CMD_TURN_ON_3BHKREFUGEE1 0xC6
#define CMD_TURN_ON_3BHKREFUGEE2 0xC7
#define CMD_SHOW_AVAIL 0xC8   // 0xC4 from master code
#define CMD_RUN_PATTERN 0xA2  // 0xD5 from master code
#define CMD_3BHK 0xA7
#define CMD_2BHK 0xA6
#define CMD_1BHK 0xA5
// Pattern IDs
#define PATTERN_ONE_BY_ONE 0x01

// BHK Types Mapping (Define the BHK type for each room)
const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = {
  1, 1, 3, 3, 1, 1 ,1,1 // Example mapping: Rooms 0-1 are 1BHK, 2-3 are 2BHK, 4-7 are 3BHK
};
const int NUM_COLORS = 5;
const int FADE_STEPS = 100;   // Number of steps in each transition
const int DELAY_TIME_MS = 2;  // Delay between steps in milliseconds

struct FadeTask {
  int floor;
  int room;
  int brightness;
  int direction;  // +1 for fading in, -1 for fading out
  bool active;
};
#define IDLE_TIMEOUT (1UL * 60UL * 1000UL)  // 5 minutes in milliseconds

#define NUM_TASKS 180
FadeTask tasks[NUM_TASKS];
uint8_t softColors[NUM_COLORS][3] = {
  { 255, 149, 130 },  // Soft color 1
  { 250, 210, 130 },  // Soft color 1
  { 122, 211, 255 },  // Soft color 1
  { 164, 135, 255 },  // Soft color 1
  { 251, 130, 211 },  // Soft color 1
};
// LED Arrays for Each Floor
Adafruit_NeoPixel* ledsFloor[NUM_FLOORS];

// Control Variables
volatile bool newCommandReceived = false;
volatile uint8_t currentCommand = 0;
uint8_t lastCommand = 0;
// For command data
volatile uint8_t commandData[32];
volatile size_t commandDataSize = 0;

// Availability Data
uint8_t roomAvailability[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];
size_t totalAvailabilityDataSize = NUM_FLOORS * NUM_ROOMS_PER_FLOOR;
size_t availabilityDataBytesReceived = 0;
unsigned long lastCommandTime = 0;  // Tracks last time we got a command

bool isIdleMode = false;
// Packet Parsing Variables
enum ReceiveState {
  WAIT_FOR_START,
  READ_COMMAND_ID,
  READ_DATA_SIZE,
  READ_DATA_CHUNK,
  WAIT_FOR_END
};

ReceiveState receiveState = WAIT_FOR_START;

// Debugging Variables
volatile uint8_t debugReceivedByte = 0;
volatile bool debugFlag = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(0x0A);  // Slave address
  Wire.setClock(400000UL);
  Wire.onReceive(receiveEvent);

  // Initialize LED strips for each floor
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    int dataPin = getFloorDataPin(floor);
    Serial.println("Floor Pin: " + String(floor) + " | Data Pin: " + String(dataPin));
    if (dataPin != -1) {
      ledsFloor[floor] = new Adafruit_NeoPixel(TOTAL_LEDS_PER_FLOOR, dataPin, NEO_GRB + NEO_KHZ800);
      ledsFloor[floor]->begin();
      ledsFloor[floor]->setBrightness(255);
      ledsFloor[floor]->show();  // Initialize all pixels to 'off'
    }
  }

  // Initialize roomAvailability array
  memset(roomAvailability, 0, sizeof(roomAvailability));

  Serial.println("Setup complete");
  Serial.println("Running Pattern till command received from ESP32");
  initializeFadeTasks();

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
    Serial.println("Running Idle Mode");
    turnOffAllLEDs();
    delay(50);
    patternSoftColorsSmooth();
  }
  // Debugging: Print received bytes outside ISR
  if (debugFlag) {
    noInterrupts();
    uint8_t byte = debugReceivedByte;
    debugFlag = false;
    interrupts();
    Serial.print("Debug Received Byte: 0x");
    Serial.println(byte, HEX);
  }

  if (newCommandReceived) {
    noInterrupts();  // Disable interrupts while processing the command
    newCommandReceived = false;
    uint8_t command = currentCommand;
    interrupts();  // Re-enable interrupts
    //Wire.
    Serial.print("Processing Command: 0x");
    Serial.println(command, HEX);
if (lastCommand == currentCommand && currentCommand != CMD_SHOW_AVAIL) {
      return;
    }
    // Do not turn off LEDs when receiving availability data
    if (command != CMD_SHOW_AVAIL) {
      turnOffAllLEDs();
    }
lastCommandTime = millis();
turnOffAllLEDs();
    switch (command) {
      case CMD_TURN_ON_2BHK1:
        Serial.println("Executing: CMD_TURN_ON_2BHK1");
        executeTurnOn2BHK1();
        break;
      case CMD_TURN_ON_2BHK2:
        Serial.println("Executing: CMD_TURN_ON_2BHK2");
        executeTurnOn2BHK2();
        break;
      case CMD_TURN_ON_3BHKA1:
        Serial.println("Executing: CMD_TURN_ON_3BHKA1");
        executeTurnOn3BHKA1();
        break;
      case CMD_TURN_ON_3BHKA2:
        Serial.println("Executing: CMD_TURN_ON_3BHKA2");
        executeTurnOn3BHKA2();
        break;
      case CMD_TURN_ON_3BHKB1:
        Serial.println("Executing: CMD_TURN_ON_3BHKB1");
        executeTurnOn3BHKB1();
        break;
      case CMD_TURN_ON_3BHKB2:
        Serial.println("Executing: CMD_TURN_ON_3BHKB2");
        executeTurnOn3BHKB2();
        break;
      case CMD_TURN_ON_REFUGEE:
        Serial.println("Executing: CMD_TURN_ON_REFUGEE");
        executeTurnOnRefugee();
        break;
      case CMD_TURN_ON_3BHKREFUGEE1:
        Serial.println("Executing: CMD_TURN_ON_3BHKREFUGEE1");
        executeTurnOn3BHKRefugee1();
        break;
      case CMD_TURN_ON_3BHKREFUGEE2:
        Serial.println("Executing: CMD_TURN_ON_3BHKREFUGEE2");
        executeTurnOn3BHKRefugee2();
        break;
      case CMD_3BHK:
        //Serial.println("Executing: CMD_TURN_ON_3BHKREFUGEE2");
        executeTurnOnBHK(3);
        break;
      case CMD_2BHK:
        //Serial.println("Executing: CMD_TURN_ON_3BHKREFUGEE2");
        executeTurnOnBHK(2);
        break;
      case CMD_1BHK:
        //Serial.println("Executing: CMD_TURN_ON_3BHKREFUGEE2");
        executeTurnOnBHK(1);
        break;
      case CMD_SHOW_AVAIL:
        Serial.println("Executing: CMD_SHOW_AVAIL");
        if (availabilityDataBytesReceived >= totalAvailabilityDataSize) {
          turnOffAllLEDs();
          availabilityDataBytesReceived = 0;
        } else {
          turnOffAllLEDs();
          Serial.println("Availability data not fully received yet.");
        }
        break;
      case CMD_RUN_PATTERN:
        Serial.println("Executing: CMD_RUN_PATTERN");
        patternSoftColorsSmooth();
        break;
      case CMD_TURN_OFF_ALL:
        Serial.println("Executing: CMD_TURN_OFF_ALL");
        turnOffAllLEDs();
        break;
      default:
        Serial.print("Unknown Command Received: 0x");
        Serial.println(command, HEX);
        break;
    }
    lastCommand = currentCommand;
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
    case 8: return 5;
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

// I²C receive event handler
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

// Function to turn off all LEDs

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

// Function to execute turning on BHK types
void executeTurnOnBHK(uint8_t bhkType) {
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (isRefugee(floor, room)) {
        continue;
      }
      if (bhkTypes[room] == bhkType) {
        int ledStartIndex = room * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          uint32_t color;
          if (bhkType == 1) {
            color = ledsFloor[floor]->Color(0, 255, 255);  // Cyan
          } else if (bhkType == 2) {
            color = ledsFloor[floor]->Color(255, 255, 0);  // Yellow
          } else if (bhkType == 3) {
            color = ledsFloor[floor]->Color(255, 0, 255);  // Magenta
          } else {
            color = ledsFloor[floor]->Color(255, 255, 255);  // Default color (White)
          }
          ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
        }
      }
    }
    ledsFloor[floor]->show();
  }
}

void executeTurnOn2BHK1() {
  for (int floor = 0; floor <= 10; floor++) {  // Floors 1 to 11
    for (int roomIndex = 0; roomIndex < NUM_ROOMS_PER_FLOOR; roomIndex++) {
      if (roomIndex == 1 || roomIndex == 2 || roomIndex == 4 || roomIndex == 5) {
        int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          if (isRefugee(floor, roomIndex)) {
            continue;
          }
          uint32_t color = ledsFloor[floor]->Color(255, 255, 0);  // Yellow
          ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
        }
      }
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_2BHK2
void executeTurnOn2BHK2() {
  for (int floor = 11; floor <= 33; floor++) {  // Floors 12 to 34
    for (int roomIndex = 0; roomIndex < NUM_ROOMS_PER_FLOOR; roomIndex++) {
      if (roomIndex == 1 || roomIndex == 2 || roomIndex == 4 || roomIndex == 5) {
        int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          if (isRefugee(floor, roomIndex)) {
            continue;
          }
          uint32_t color = ledsFloor[floor]->Color(255, 255, 0);  // Yellow
          ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
        }
      }
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_3BHKA1
void executeTurnOn3BHKA1() {
  for (int floor = 0; floor <= 10; floor++) {  // Floors 1 to 11
    int roomIndex = 0;                         // Room Index 1
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
      if (isRefugee(floor, i)) {
        continue;
      }
      uint32_t color = ledsFloor[floor]->Color(255, 0, 255);  // Magenta
      ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_3BHKA2
void executeTurnOn3BHKA2() {
  for (int floor = 11; floor <= 33; floor++) {  // Floors 12 to 34
    int roomIndex = 0;                          // Room Index 1
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
      if (isRefugee(floor, i)) {
        continue;
      }
      uint32_t color = ledsFloor[floor]->Color(255, 0, 255);  // Magenta
      ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_3BHKB1
void executeTurnOn3BHKB1() {
  for (int floor = 0; floor <= 10; floor++) {  // Floors 1 to 11
    int roomIndex = 3;                         // Room Index 4
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
      if (isRefugee(floor, i)) {
        continue;
      }
      uint32_t color = ledsFloor[floor]->Color(255, 0, 255);  // Magenta
      ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_3BHKB2
void executeTurnOn3BHKB2() {
  for (int floor = 11; floor <= 33; floor++) {  // Floors 12 to 34
    int roomIndex = 3;                          // Room Index 4
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
      if (isRefugee(floor, i)) {
        continue;
      }
      uint32_t color = ledsFloor[floor]->Color(255, 0, 255);  // Magenta
      ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_REFUGEE
void executeTurnOnRefugee() {
  int refugeeFloors[] = { 2, 7, 12, 17, 22, 27, 32 };  // Floor indices corresponding to Floors 3,8,13,18,23,28,33
  int numRefugeeFloors = sizeof(refugeeFloors) / sizeof(int);
  int roomIndex = 5;  // Room Index 6
  for (int i = 0; i < numRefugeeFloors; i++) {
    int floor = refugeeFloors[i];
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int j = 0; j < NUM_LEDS_PER_ROOM; j++) {
      uint32_t color = ledsFloor[floor]->Color(0, 255, 0);  // Green
      ledsFloor[floor]->setPixelColor(ledStartIndex + j, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_3BHKREFUGEE1
void executeTurnOn3BHKRefugee1() {
  int refugeeFloors[] = { 2, 7 };  // Floor indices between 1st and 11th floor (Floors 3 and 8)
  int numRefugeeFloors = sizeof(refugeeFloors) / sizeof(int);
  int roomIndex = 4;  // Room Index 5
  for (int i = 0; i < numRefugeeFloors; i++) {
    int floor = refugeeFloors[i];
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int j = 0; j < NUM_LEDS_PER_ROOM; j++) {
      uint32_t color = ledsFloor[floor]->Color(0, 0, 255);  // Blue
      ledsFloor[floor]->setPixelColor(ledStartIndex + j, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute CMD_TURN_ON_3BHKREFUGEE2
void executeTurnOn3BHKRefugee2() {
  int refugeeFloors[] = { 12, 17, 22, 27, 32 };  // Floor indices above 11th floor
  int numRefugeeFloors = sizeof(refugeeFloors) / sizeof(int);
  int roomIndex = 4;  // Room Index 5
  for (int i = 0; i < numRefugeeFloors; i++) {
    int floor = refugeeFloors[i];
    int ledStartIndex = roomIndex * NUM_LEDS_PER_ROOM;
    for (int j = 0; j < NUM_LEDS_PER_ROOM; j++) {
      uint32_t color = ledsFloor[floor]->Color(0, 0, 255);  // Blue
      ledsFloor[floor]->setPixelColor(ledStartIndex + j, color);
    }
    ledsFloor[floor]->show();
  }
}

// Function to show availability
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


void patternSoftColorsSmooth() {
  while (true) {
    noInterrupts();
    bool stop = newCommandReceived;
    interrupts();
    if (stop) break;

    uint32_t goldenColor = 0;
    for (int f = 0; f < NUM_FLOORS; f++) {
        goldenColor = ledsFloor[f]->Color(255, 215, 0);
      }
    
//Serial.println("Idle mode 1");
    // Update each fade task
    for (int t = 0; t < NUM_TASKS; t++) {
      if (!tasks[t].active) continue;

      int floor = tasks[t].floor;
      if (!ledsFloor[floor]) continue;  // skip if floor LED strip is not available

      Adafruit_NeoPixel* strip = ledsFloor[floor];
      int room = tasks[t].room;
      int startIndex = room * NUM_LEDS_PER_ROOM;
//Serial.println("Idle mode 2");
      // Adjust brightness for this task
      tasks[t].brightness += tasks[t].direction;
      if (tasks[t].brightness >= 255) {
        tasks[t].brightness = 255;
        tasks[t].direction = -5;  // start fading out once fully bright
      } else if (tasks[t].brightness <= 0) {
        tasks[t].brightness = 0;
        tasks[t].direction = 5;  // switch back to fading in
//Serial.println("Idle mode 3");
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
//===========================================================================================//
void initializeFadeTasks() {
  for (int t = 0; t < NUM_TASKS; ++t) {
    tasks[t].floor = random(0, NUM_FLOORS);
    tasks[t].room = random(0, NUM_ROOMS_PER_FLOOR);
    tasks[t].brightness = 0;
    tasks[t].direction = 1;  // start with fade-in
    tasks[t].active = true;
  }
}


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
  //  int refugeeFloors[] = {2,7,12,17,22,27,32}; // Floor indices corresponding to Floors 3,8,13,18,23,28,33
  return (room == 4 && (floor == 3 || floor == 8 || floor == 13 || floor == 18 || floor == 22 || floor == 27));
}