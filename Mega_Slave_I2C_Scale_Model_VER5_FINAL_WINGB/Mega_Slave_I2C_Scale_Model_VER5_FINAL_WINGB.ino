// Slave Code for Arduino Mega

#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define NUM_FLOORS 35
#define NUM_ROOMS_PER_FLOOR 8
#define NUM_LEDS_PER_ROOM 4
#define TOTAL_LEDS_PER_FLOOR (NUM_ROOMS_PER_FLOOR * NUM_LEDS_PER_ROOM)

// Command IDs (updated to match master code)
#define CMD_TURN_OFF_ALL       0xA0  // 0xA0 from master code
#define CMD_TURN_ON_1BHK       0xB1  // 0xB1 from master code
#define CMD_TURN_ON_2BHK       0xB2  // 0xB2 from master code
#define CMD_TURN_ON_3BHK       0xB3  // 0xB3 from master code
#define CMD_SHOW_AVAIL         0xC4  // 0xC4 from master code
#define CMD_RUN_PATTERN        0xD5  // 0xD5 from master code

// Pattern IDs
#define PATTERN_ONE_BY_ONE 0x01

// BHK Types Mapping (Define the BHK type for each room)
const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = {
  2, 2, 1, 1, 2, 2, 3, 3  // Example mapping: Rooms 0-1 are 1BHK, 2-3 are 2BHK, 4-7 are 3BHK
};

// LED Arrays for Each Floor
Adafruit_NeoPixel* ledsFloor[NUM_FLOORS];

// Control Variables
volatile bool newCommandReceived = false;
volatile uint8_t currentCommand = 0;

// For command data
volatile uint8_t commandData[32];
volatile size_t commandDataSize = 0;

// Availability Data
uint8_t roomAvailability[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];
size_t totalAvailabilityDataSize = NUM_FLOORS * NUM_ROOMS_PER_FLOOR;
size_t availabilityDataBytesReceived = 0;

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
  Wire.begin(0x09); // Slave address
  Wire.setClock(400000UL);
  Wire.onReceive(receiveEvent);

  // Initialize LED strips for each floor
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    int dataPin = getFloorDataPin(floor);
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

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->Color(255, 255, 255));
    }
    ledsFloor[floor]->show();
  }
}

void loop() {
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
    noInterrupts(); // Disable interrupts while processing the command
    newCommandReceived = false;
    uint8_t command = currentCommand;
    interrupts(); // Re-enable interrupts

    Serial.print("Processing Command: 0x");
    Serial.println(command, HEX);

    // Do not turn off LEDs when receiving availability data
    if (command != CMD_SHOW_AVAIL) {
      turnOffAllLEDs();
    }

    switch (command) {
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
        // The availability data is processed in receiveEvent()
        // Once all data is received, update LEDs
        if (availabilityDataBytesReceived >= totalAvailabilityDataSize) {
          showAvailability();
          availabilityDataBytesReceived = 0; // Reset for next transmission
        } else {
          Serial.println("Availability data not fully received yet.");
        }
        break;
      case CMD_RUN_PATTERN:
        Serial.println("Executing: CMD_RUN_PATTERN");
        runPattern();
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
  }
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
    default: return -1;  // Invalid floor
  }
}

// I²C receive event handler
void receiveEvent(int howMany) {
  while (Wire.available()) {
    uint8_t receivedByte = Wire.read();
    debugReceivedByte = receivedByte;  // Store for debugging outside ISR
    debugFlag = true;

    switch (receiveState) {
      case WAIT_FOR_START:
        if (receivedByte == 0xAA) {
          receiveState = READ_COMMAND_ID;
          // Avoid Serial.print in ISR
        } else {
          // Ignore any other bytes
        }
        break;

      case READ_COMMAND_ID:
        currentCommand = receivedByte;
        if (currentCommand == CMD_SHOW_AVAIL) {
          receiveState = READ_DATA_SIZE;
        } else {
          // For other commands, we don't expect data
          newCommandReceived = true;
          receiveState = WAIT_FOR_END;
        }
        break;

      case READ_DATA_SIZE:
        static uint8_t dataSizeBytes[2];
        static uint8_t dataSizeIndex = 0;

        dataSizeBytes[dataSizeIndex++] = receivedByte;

        if (dataSizeIndex >= 2) {
          uint16_t totalDataSize = (dataSizeBytes[0] << 8) | dataSizeBytes[1];
          dataSizeIndex = 0;
          availabilityDataBytesReceived = 0;
          receiveState = READ_DATA_CHUNK;
        }
        break;

      case READ_DATA_CHUNK:
        if (receivedByte == 0xAB) {
          // Start of a data chunk
          static uint8_t headerBytes[3];
          static uint8_t headerIndex = 0;

          // Read offset (2 bytes) and chunk size (1 byte)
          while (Wire.available() && headerIndex < 3) {
            headerBytes[headerIndex++] = Wire.read();
          }

          if (headerIndex >= 3) {
            uint16_t offset = (headerBytes[0] << 8) | headerBytes[1];
            uint8_t chunkSize = headerBytes[2];

            // Now read data
            uint8_t dataBuffer[32]; // Adjust size if necessary
            uint8_t dataIndex = 0;

            while (Wire.available() && dataIndex < chunkSize) {
              dataBuffer[dataIndex++] = Wire.read();
            }

            // Read checksum
            uint8_t checksum = 0;
            if (Wire.available()) {
              checksum = Wire.read();
            } else {
              // Error: checksum not received
              receiveState = WAIT_FOR_START;
              break;
            }

            // Verify checksum
            uint8_t calculatedChecksum = 0;
            for (uint8_t i = 0; i < dataIndex; i++) {
              calculatedChecksum += dataBuffer[i];
            }

            if (checksum == calculatedChecksum) {
              // Check if data fits in our array
              if (offset + chunkSize <= totalAvailabilityDataSize) {
                // Copy data to roomAvailability
                memcpy(((uint8_t*)roomAvailability) + offset, dataBuffer, dataIndex);
                availabilityDataBytesReceived += dataIndex;
              } else {
                // Data chunk exceeds array size
              }
            } else {
              // Checksum mismatch
            }

            headerIndex = 0; // Reset header index for next chunk
          } else {
            // Not enough data yet, wait for next receiveEvent call
            return;
          }
        } else if (receivedByte == 0x55) {
          // End of transmission
          receiveState = WAIT_FOR_START;
          // If all data received, process it
          if (availabilityDataBytesReceived >= totalAvailabilityDataSize) {
            newCommandReceived = true;
          } else {
            // Data not fully received
          }
        } else {
          // Unexpected byte
          receiveState = WAIT_FOR_START;
        }
        break;

      case WAIT_FOR_END:
        if (receivedByte == 0x55) {
          receiveState = WAIT_FOR_START;
        } else {
          // Waiting for end byte
        }
        break;

      default:
        receiveState = WAIT_FOR_START;
        break;
    }
  }
}

// Function to turn off all LEDs
void turnOffAllLEDs() {
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->Color(0, 0, 0));
    }
    ledsFloor[floor]->show();
  }
}

// Function to execute turning on BHK types
void executeTurnOnBHK(uint8_t bhkType) {
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      if (bhkTypes[room] == bhkType) {
        int ledStartIndex = room * NUM_LEDS_PER_ROOM;
        for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
          uint32_t color;
          if (bhkType == 1) {
            color = ledsFloor[floor]->Color(0, 255, 255); // Cyan
          } else if (bhkType == 2) {
            color = ledsFloor[floor]->Color(255, 255, 0); // Yellow
          } else if (bhkType == 3) {
            color = ledsFloor[floor]->Color(255, 0, 255); // Magenta
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

// Function to show availability
void showAvailability() {
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      int ledStartIndex = room * NUM_LEDS_PER_ROOM;
      uint8_t status = roomAvailability[floor][room];
      uint32_t color = (status == 0) ? ledsFloor[floor]->Color(255, 0, 0) : ledsFloor[floor]->Color(255, 255, 255);
      // Red for occupied, Green for available

      // Set each LED in the room to the designated color
      for (int i = 0; i < NUM_LEDS_PER_ROOM; i++) {
        ledsFloor[floor]->setPixelColor(ledStartIndex + i, color);
      }
    }
    ledsFloor[floor]->show();  // Only call show() once per floor to avoid excessive updates
  }
}

// Function to run patterns
void runPattern() {
  Serial.println("Running Continuous Pattern");

  // This loop will keep the pattern running until a new command is received
  while (true) {
    noInterrupts(); // Disable interrupts to read shared variable safely
    bool commandReceived = newCommandReceived;
    interrupts(); // Re-enable interrupts

    if (commandReceived) {
      break; // Exit the pattern loop if a new command is received
    }

    executeTurnOnBHK(1);
    delay(2000);
    turnOffAllLEDs();
    executeTurnOnBHK(2);
    delay(2000);
    turnOffAllLEDs();
    executeTurnOnBHK(3);
    delay(2000);
    turnOffAllLEDs();
    // You can call any of the patterns here based on the specific needs of the pattern.
    runOneByOnePattern();       // Example: Running One By One Pattern
    turnOffAllLEDs();
    runRunningFadingRainbowPattern(200);      // Example: Running Floor Wipe Pattern
    turnOffAllLEDs();
    //runFloorAlternatePattern(); // Example: Running Floor Alternating Pattern
    turnOffAllLEDs();
    gradientRainbow(10);
    turnOffAllLEDs();
    fastRainbowCycle(5);
    turnOffAllLEDs();

  }

  Serial.println("Pattern execution stopped due to new command.");
}

// Pattern 1: One-By-One Lighting
void runOneByOnePattern() {
  Serial.println("Running Pattern: One By One");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
      ledsFloor[floor]->setPixelColor(led, ledsFloor[floor]->Color(115, 180, 250)); // Blue
      ledsFloor[floor]->show();
      delay(2);
    }
    
    delay(10);
  }
}

// Pattern 3: Floor Alternating
void runFloorAlternatePattern() {
  Serial.println("Running Pattern: Floor Alternating");
  for (int cycle = 0; cycle < 5; cycle++) {
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      uint32_t color = (floor % 2 == 0) ? ledsFloor[floor]->Color(255, 120, 50) : ledsFloor[floor]->Color(120, 80, 255);
      for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
        ledsFloor[floor]->setPixelColor(led, color);
      }
      ledsFloor[floor]->show();
    }
    delay(300);

    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      uint32_t color = (floor % 2 == 0) ? ledsFloor[floor]->Color(0, 0, 255) : ledsFloor[floor]->Color(255, 0, 0);
      for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
        ledsFloor[floor]->setPixelColor(led, color);
      }
      ledsFloor[floor]->show();
    }
    delay(300);
  }
}

// Fading Rainbow Pattern
// Floor-Based Fading Rainbow Pattern
// Running LED Fading Rainbow Pattern
// Running LED Fading Rainbow Pattern with Finite Cycles
void runRunningFadingRainbowPattern(int numCycles) {
  Serial.println("Running Pattern: Running Fading Rainbow");

  uint16_t baseHue = 0; // Initial hue to start the rainbow

  for (int cycle = 0; cycle < numCycles; cycle++) {
    Serial.println("Cycle: "+String(cycle));
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      // Calculate the hue for each floor to create a running effect across floors
      uint16_t floorHue = (baseHue + (floor * 3000)) % 65536; // Adjust 3000 for speed and spread

      // Set the entire floor to the same color based on the calculated hue
      uint32_t color = ledsFloor[floor]->gamma32(ledsFloor[floor]->ColorHSV(floorHue));
      for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
        ledsFloor[floor]->setPixelColor(led, color);
      }

      // Display the color on the floor
      ledsFloor[floor]->show();
    }

    // Gradually shift the base hue to create a moving effect
    baseHue += 256; // Controls the speed of the color movement
    delay(50); // Adjust for smooth fading effect
  }
  
  // Turn off all LEDs after completing the cycles
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    ledsFloor[floor]->clear();
    ledsFloor[floor]->show();
  }

  Serial.println("Completed Running Fading Rainbow Pattern.");
}
// Gradient Rainbow Pattern: Smooth transition across LEDs on each floor
void gradientRainbow(uint8_t wait) {
  Serial.println("Running Gradient Rainbow Pattern");

  for (int j = 0; j < 256; j++) { // Cycle through each color step by step
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
        // Use a varying hue for each LED to create a gradient effect
        uint16_t hue = (i * 65536 / TOTAL_LEDS_PER_FLOOR + j * 256) % 65536;
        ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->gamma32(ledsFloor[floor]->ColorHSV(hue)));
      }
      ledsFloor[floor]->show(); // Update each floor's LED strip
    }
    delay(wait); // Control the speed of the transition
  }
}

// Fast Rainbow Cycle: Quickly moves rainbow colors across LEDs on each floor
void fastRainbowCycle(uint8_t wait) {
  Serial.println("Running Fast Rainbow Cycle");

  for (int j = 0; j < 256 * 3; j++) { // 3 full cycles of all colors on the wheel
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
        // Each LED gets a color from the rainbow based on position and the cycle
        uint16_t hue = (i * 65536 / TOTAL_LEDS_PER_FLOOR + j * 256) % 65536;
        ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->gamma32(ledsFloor[floor]->ColorHSV(hue)));
      }
      ledsFloor[floor]->show(); // Update each floor's LED strip
    }
    delay(wait); // Adjust speed of color cycling
  }
}


