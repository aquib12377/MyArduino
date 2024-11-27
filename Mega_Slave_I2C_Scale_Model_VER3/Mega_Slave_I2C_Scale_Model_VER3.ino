// Arduino Mega Slave Code
#include <Wire.h>
#include <FastLED.h>

#define NUM_FLOORS 34
#define NUM_ROOMS_PER_FLOOR 8
#define NUM_LEDS_PER_ROOM 4
#define TOTAL_LEDS_PER_FLOOR (NUM_ROOMS_PER_FLOOR * NUM_LEDS_PER_ROOM)

// Command IDs
#define CMD_TURN_ON_1BHK   0x01
#define CMD_TURN_ON_2BHK   0x02
#define CMD_TURN_ON_3BHK   0x03
#define CMD_SHOW_AVAIL     0x04
#define CMD_RUN_PATTERN    0x05
#define CMD_TURN_OFF_ALL   0x00

// Pattern IDs
#define PATTERN_ONE_BY_ONE 0x01
// Add more pattern IDs as needed

// BHK Types Mapping (Define the BHK type for each room)
const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = {
  1, 1, 2, 2, 3, 3, 3, 3  // Example mapping: Rooms 0-1 are 1BHK, 2-3 are 2BHK, 4-7 are 3BHK
};

// LED Arrays for Each Floor
CRGB ledsFloor[NUM_FLOORS][TOTAL_LEDS_PER_FLOOR];

// Control Variables
volatile bool newCommandReceived = false;
volatile uint8_t currentCommand = 0;
volatile uint8_t commandData[32];  // Adjust size as needed
volatile size_t commandDataSize = 0;

// Availability Data
uint8_t roomAvailability[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];  // 0: Available, 1: Booked

void setup() {
  Serial.begin(115200);  // Initialize Serial communication
  Wire.begin(0x08);      // Initialize I²C as slave with address 0x08
  Wire.setClock(400000UL);
  Wire.onReceive(receiveEvent);  // Register receive event handler

  // Initialize LED strips for each floor
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    int dataPin = getFloorDataPin(floor);
    if (dataPin != -1) {
      FastLED.addLeds<WS2812B, GRB>(ledsFloor[floor], TOTAL_LEDS_PER_FLOOR).setCorrection(TypicalLEDStrip);
      FastLED.setBrightness(255);
    }
  }

  Serial.println("Setup complete");
}

void loop() {
  if (newCommandReceived) {
    newCommandReceived = false;

    Serial.print("Received Command: 0x");
    Serial.println(currentCommand, HEX);

    // Turn off all LEDs before executing new command
    turnOffAllLEDs();

    switch (currentCommand) {
      case CMD_TURN_ON_1BHK:
        Serial.println("Executing: CMD_TURN_ON_1BHK");
        executeTurnOnBHK(1);
        break;
      case CMD_TURN_ON_2BHK:
        Serial.println("Executing: CMD_TURN_ON_2BHK");
        executeTurnOnBHK(2);
        break;
      case CMD_TURN_ON_3BHK:
        Serial.println("Executing: CMD_TURN_ON_3BHK");
        executeTurnOnBHK(3);
        break;
      case CMD_SHOW_AVAIL:
        Serial.println("Executing: CMD_SHOW_AVAIL");
        receiveAvailabilityData();
        showAvailability();
        break;
      case CMD_RUN_PATTERN:
        if (commandDataSize >= 1) {
          Serial.print("Executing: CMD_RUN_PATTERN with Pattern ID: ");
          Serial.println(commandData[0]);
          runPattern(commandData[0]);
        } else {
          Serial.println("CMD_RUN_PATTERN received but no Pattern ID provided");
        }
        break;
      case CMD_TURN_OFF_ALL:
        Serial.println("Executing: CMD_TURN_OFF_ALL");
        turnOffAllLEDs();
        break;
      default:
        Serial.print("Unknown Command Received: 0x");
        Serial.println(currentCommand, HEX);
        // Unknown command
        break;
    }
  }

  // Implement any ongoing patterns if needed
}

// Function to get the data pin for each floor
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
    case 8:  return 5;
    case 9:  return 4;
    case 10: return 3;
    case 11: return 2;
    case 12: return 14;
    case 13: return 15;
    case 14: return 16;
    case 15: return 18;
    case 16: return 19;
    case 17: return 53;
    case 18: return 51;
    case 19: return 49;
    case 20: return 47;
    case 21: return 45;
    case 22: return 43;
    case 23: return 41;
    case 24: return 39;
    case 25: return 37;
    case 26: return 35;
    case 27: return 33;
    case 28: return 31;
    case 29: return 29;
    case 30: return 27;
    case 31: return 25;
    case 32: return 23;
    case 33: return 69;  // A15 corresponds to digital pin 69
    default: return -1;  // Invalid floor
  }
}

// I²C receive event handler
void receiveEvent(int howMany) {
  if (howMany < 1) return;  // At least command ID is needed

  currentCommand = Wire.read();
  commandDataSize = howMany - 1;

  Serial.print("receiveEvent: Command 0x");
  Serial.print(currentCommand, HEX);
  Serial.print(", Data Size: ");
  Serial.println(commandDataSize);

  for (size_t i = 0; i < commandDataSize; i++) {
    if (Wire.available()) {
      commandData[i] = Wire.read();
      Serial.print("Received Data[");
      Serial.print(i);
      Serial.print("]: ");
      Serial.println(commandData[i]);
    }
  }

  newCommandReceived = true;
}

// Function to turn off all LEDs
void turnOffAllLEDs() {
  Serial.println("Turning off all LEDs");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    fill_solid(ledsFloor[floor], TOTAL_LEDS_PER_FLOOR, CRGB::Black);
  }
  FastLED.show();
}

// Function to execute turning on BHK types
void executeTurnOnBHK(uint8_t bhkType) {
  Serial.print("Executing Turn On for BHK Type: ");
  Serial.println(bhkType);
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (bhkTypes[room] == bhkType) {
        int ledStartIndex = room * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          ledsFloor[floor][ledStartIndex + i] = CRGB::White;
        }
      }
    }
    FastLED.show();
  }
}

// Function to receive availability data
void receiveAvailabilityData() {
  // Assuming data is being received in chunks with offset
  static size_t bytesReceived = 0;

  if (commandDataSize >= 2) {
    uint16_t offset = commandData[0] | (commandData[1] << 8);
    size_t dataSize = commandDataSize - 2;

    Serial.print("Receiving availability data: Offset=");
    Serial.print(offset);
    Serial.print(", Data Size=");
    Serial.println(dataSize);

    memcpy(((uint8_t*)roomAvailability) + offset, &commandData[2], dataSize);
    bytesReceived += dataSize;

    Serial.print("Total Bytes Received: ");
    Serial.println(bytesReceived);

    if (bytesReceived >= sizeof(roomAvailability)) {
      Serial.println("All availability data received.");
      bytesReceived = 0;  // Reset for next full data reception
    }
  } else {
    Serial.println("Invalid data size for CMD_SHOW_AVAIL");
  }
}

// Function to show availability
void showAvailability() {
  Serial.println("Displaying room availability on LEDs");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      int ledStartIndex = room * NUM_LEDS_PER_ROOM;
      uint8_t status = roomAvailability[floor][room];
      CRGB color = (status == 1) ? CRGB::Red : CRGB::White;
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[floor][ledStartIndex + i] = color;
      }
    }
    FastLED.show();
  }
}

// Function to run patterns
void runPattern(uint8_t patternID) {
  Serial.print("Running Pattern ID: ");
  Serial.println(patternID);
  switch (patternID) {
    case PATTERN_ONE_BY_ONE:
      runOneByOnePattern();
      break;
    // Add cases for other patterns
    default:
      Serial.println("Unknown Pattern ID");
      break;
  }
}

// Example pattern: Turn on LEDs one by one on each floor
void runOneByOnePattern() {
  Serial.println("Running Pattern: One By One");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
      ledsFloor[floor][led] = CRGB::Blue;
      FastLED.show();
      delay(50);  // Adjust speed as needed

      // Check if a new command has been received
      if (newCommandReceived) {
        Serial.println("New command received during pattern execution. Exiting pattern.");
        return;  // Exit pattern
      }

      // Turn off the LED after displaying it
      ledsFloor[floor][led] = CRGB::Black;
    }
  }
}
