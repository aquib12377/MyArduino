// Arduino Mega Slave Code
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

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

// BHK Types Mapping (Define the BHK type for each room)
const uint8_t bhkTypes[NUM_ROOMS_PER_FLOOR] = {
  1, 1, 2, 2, 3, 3, 3, 3  // Example mapping: Rooms 0-1 are 1BHK, 2-3 are 2BHK, 4-7 are 3BHK
};

// LED Arrays for Each Floor
Adafruit_NeoPixel* ledsFloor[NUM_FLOORS];

// Control Variables
volatile bool newCommandReceived = false;
volatile uint8_t currentCommand = 0;
volatile uint8_t commandData[32];
volatile size_t commandDataSize = 0;

// Availability Data
uint8_t roomAvailability[NUM_FLOORS][NUM_ROOMS_PER_FLOOR];

// Set up the data pins for each floor (replace FastLED with Adafruit_NeoPixel)
void setup() {
  Serial.begin(115200);
  Wire.begin(0x08);
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

  Serial.println("Setup complete");
}

void loop() {
  if (newCommandReceived) {
    newCommandReceived = false;
    Serial.print("Received Command: 0x");
    Serial.println(currentCommand, HEX);

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
        //showAvailability();
        break;
      case CMD_RUN_PATTERN:
        if (commandDataSize >= 1) {
          Serial.print("Executing: CMD_RUN_PATTERN with Pattern ID: ");
          Serial.println(commandData[0]);
          runPattern();
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
        break;
    }
  }
}
void receiveAvailabilityData() {
  static size_t bytesReceived = 0;
  if (commandDataSize >= 2) {
    uint16_t offset = commandData[0] | (commandData[1] << 8);
    size_t dataSize = commandDataSize - 2;
    
    if (offset + dataSize <= sizeof(roomAvailability)) {
      memcpy(((uint8_t*)roomAvailability) + offset, &commandData[2], dataSize);
      bytesReceived += dataSize;
      
      if (bytesReceived >= sizeof(roomAvailability)) {
        Serial.println("All availability data received.");
        bytesReceived = 0;
      }
    } else {
      Serial.println("Data overflow detected in receiveAvailabilityData.");
    }
  } else {
    Serial.println("Invalid data size for CMD_SHOW_AVAIL");
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
  if (howMany < 1) return;

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
    for (int i = 0; i < TOTAL_LEDS_PER_FLOOR; i++) {
      ledsFloor[floor]->setPixelColor(i, ledsFloor[floor]->Color(0, 0, 0));
    }
    ledsFloor[floor]->show();
  }
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
          if(bhkType == 1)
          {
            ledsFloor[floor]->setPixelColor(ledStartIndex + i, ledsFloor[floor]->Color(0, 170, 255));
          }
          else if(bhkType == 2)
          {
           ledsFloor[floor]->setPixelColor(ledStartIndex + i, ledsFloor[floor]->Color(255, 255, 0));
          }
          else if(bhkType == 3)
          {
            ledsFloor[floor]->setPixelColor(ledStartIndex + i, ledsFloor[floor]->Color(255, 0, 255));
          }
          
        }
      }
    }
    ledsFloor[floor]->show();
  }
}

// Function to show availability
void showAvailability() {
  Serial.println("Displaying room availability on LEDs");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < NUM_ROOMS_PER_FLOOR; room++) {
      int ledStartIndex = room * NUM_LEDS_PER_ROOM;
      uint8_t status = roomAvailability[floor][room];
      Serial.println("Status: "+String(status));
      uint32_t color = (status == 1) ? ledsFloor[floor]->Color(255, 0, 0) : ledsFloor[floor]->Color(255, 255, 255);

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
  while (!newCommandReceived) {
    // You can call any of the patterns here based on the specific needs of the pattern.
    runOneByOnePattern();       // Example: Running One By One Pattern
    turnOffAllLEDs();
    runFloorWipePattern();      // Example: Running Floor Wipe Pattern
    turnOffAllLEDs();
    runFloorAlternatePattern(); // Example: Running Floor Alternating Pattern
  }
  
  Serial.println("Pattern execution stopped due to new command.");
}


// Pattern 1: Enhanced One-By-One with Gradient Colors
void runOneByOnePattern() {
  Serial.println("Running Enhanced Pattern: One By One with Gradient Colors");

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
      for (int brightness = 0; brightness <= 255; brightness += 10) {
        // Mix of blue to purple and cyan colors
        ledsFloor[floor]->setPixelColor(led, ledsFloor[floor]->Color(0, 255 - brightness, brightness));
        
      }
      if (newCommandReceived) return;
    }
    ledsFloor[floor]->show();
        delay(15);
    delay(10); // Delay between LEDs on each floor
  }
}

// Pattern 2: Floor Wipe with Gradient Rainbow Transition
void runFloorWipePattern() {
  Serial.println("Running Enhanced Pattern: Floor Wipe with Gradient Rainbow");

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int r = 0; r <= 255; r += 5) {
      for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
        ledsFloor[floor]->setPixelColor(led, ledsFloor[floor]->Color(r, 255 - r, 127));  // Red to orange gradient
      }
      ledsFloor[floor]->show();
      delay(15);
      if (newCommandReceived) return;
    }
    for (int g = 0; g <= 255; g += 5) {
      for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
        ledsFloor[floor]->setPixelColor(led, ledsFloor[floor]->Color(127, g, 255 - g));  // Orange to green gradient
      }
      ledsFloor[floor]->show();
      delay(15);
      if (newCommandReceived) return;
    }
    for (int b = 0; b <= 255; b += 5) {
      for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
        ledsFloor[floor]->setPixelColor(led, ledsFloor[floor]->Color(255 - b, 127, b));  // Green to blue gradient
      }
      ledsFloor[floor]->show();
      delay(15);
      if (newCommandReceived) return;
    }
    delay(300); // Delay before moving to the next floor
  }
}

// Pattern 3: Floor Alternating Color Fade with Gradient Blending
void runFloorAlternatePattern() {
  Serial.println("Running Enhanced Pattern: Floor Alternating Color Fade");

  for (int cycle = 0; cycle < 5; cycle++) {  // Repeat the pattern for 5 cycles
    for (int brightness = 0; brightness <= 255; brightness += 10) {
      for (int floor = 0; floor < NUM_FLOORS; floor++) {
        // Alternating gradient colors between floors
        uint32_t color1 = ledsFloor[floor]->Color(brightness, 255 - brightness, 255);
        uint32_t color2 = ledsFloor[floor]->Color(255, brightness, 255 - brightness);
        uint32_t color = (floor % 2 == 0) ? color1 : color2;

        for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
          ledsFloor[floor]->setPixelColor(led, color);
        }
        ledsFloor[floor]->show();
      }
      delay(15);
      if (newCommandReceived) return;
    }

    for (int brightness = 255; brightness >= 0; brightness -= 10) {
      for (int floor = 0; floor < NUM_FLOORS; floor++) {
        uint32_t color1 = ledsFloor[floor]->Color(brightness, 255 - brightness, 127);
        uint32_t color2 = ledsFloor[floor]->Color(127, brightness, 255 - brightness);
        uint32_t color = (floor % 2 == 0) ? color1 : color2;

        for (int led = 0; led < TOTAL_LEDS_PER_FLOOR; led++) {
          ledsFloor[floor]->setPixelColor(led, color);
        }
        ledsFloor[floor]->show();
      }
      delay(15);
      if (newCommandReceived) return;
    }
  }
}

