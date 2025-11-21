#include <Wire.h>
#include <Arduino.h>

#define MEGA_I2C_ADDRESS 8
#define NUM_FLOORS 4
#define LEDS_PER_ROOM 8
#define ROOMS_PER_FLOOR 4
const uint8_t mega_pins[NUM_FLOORS] = {12,11,10,13};

// Memory-check timing (milliseconds)
const unsigned long MEM_CHECK_INTERVAL = 5000UL;  // print every 5s (change as needed)
unsigned long lastMemCheck = 0;

// This struct MUST be identical on the Arduino Mega slave
struct __attribute__((packed)) I2C_Initialize_Command {
  uint8_t command_type = 1;
  uint8_t strip_index;
  uint8_t pin;
  uint16_t led_count;
};

// This struct MUST be identical on the Arduino Mega slave
struct __attribute__((packed)) I2C_Led_Command {
  uint8_t command_type = 2;
  uint8_t strip_index;
  uint16_t start_led;
  uint16_t led_count;
  uint8_t command;
  uint8_t r, g, b;
  uint8_t brightness;
};

void sendLedCommand(uint8_t strip, uint16_t start, uint16_t count, uint8_t cmd, uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  I2C_Led_Command led_cmd;
  led_cmd.strip_index = strip;
  led_cmd.start_led = start;
  led_cmd.led_count = count;
  led_cmd.command = cmd;
  led_cmd.r = r;
  led_cmd.g = g;
  led_cmd.b = b;
  led_cmd.brightness = bright;

  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  Wire.write((uint8_t*)&led_cmd, sizeof(led_cmd));
  int res = Wire.endTransmission();

  Serial.print("Response for Slave: " + String(MEGA_I2C_ADDRESS) + " Response Code: " + String(res) + "\n");
}

void printFreeMemory() {
  // Heap (internal RAM)
  size_t freeHeap = ESP.getFreeHeap();
  // Minimum free heap recorded since boot
  size_t minFreeHeap = ESP.getMinFreeHeap();

  // PSRAM (if present)
  size_t psramSize = ESP.getPsramSize();  // 0 if no PSRAM
  size_t freePsram = 0;
  if (psramSize > 0) {
    // getFreePsram() exists on esp32 Arduino core
    freePsram = ESP.getFreePsram();
    Serial.printf("Free heap: %u bytes (min %u), PSRAM: %u/%u bytes free\n",
                  (unsigned)freeHeap, (unsigned)minFreeHeap, (unsigned)freePsram, (unsigned)psramSize);
  } else {
    Serial.printf("Free heap: %u bytes (min %u), PSRAM: not present\n",
                  (unsigned)freeHeap, (unsigned)minFreeHeap);
  }

  // Optional simple warning threshold (uncomment/change if desired)
  // const size_t WARN_THRESHOLD = 512;
  // if (freeHeap < WARN_THRESHOLD) {
  //   Serial.printf("WARNING: Low free heap (%u bytes)!\n", (unsigned)freeHeap);
  // }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();  // ESP32 as I2C Master
  Serial.println("\nESP32 Standalone Tester Initializing Mega...");

  // Send initialization commands to the Mega
  for (int i = 0; i < NUM_FLOORS; i++) {
    I2C_Initialize_Command init_cmd;
    init_cmd.strip_index = i;
    init_cmd.pin = mega_pins[i];
    init_cmd.led_count = LEDS_PER_ROOM * ROOMS_PER_FLOOR;

    Wire.beginTransmission(MEGA_I2C_ADDRESS);
    Wire.write((uint8_t*)&init_cmd, sizeof(init_cmd));
    Wire.endTransmission();
    Serial.printf("Sent init for Floor %d (Strip %d)\n", i + 1, i);
    delay(50);  // Give slave time to process
  }
  Serial.println("Initialization Complete. Starting Test Cycle.");

  // print initial memory snapshot
  printFreeMemory();
  lastMemCheck = millis();
}

void loop() {
  // Periodic memory check
  unsigned long now = millis();
  if (now - lastMemCheck >= MEM_CHECK_INTERVAL) {
    lastMemCheck = now;
    printFreeMemory();
  }

  Serial.println("TEST: All rooms ON red");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    sendLedCommand(floor, 0, 40, 1, 255, 255, 0, 60*(floor+1));  // Cmd 1: SET_COLOR
    delay(500);
  }
  delay(1000);

  Serial.println("TEST: All rooms OFF");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    sendLedCommand(floor, 0, 40, 3, 0, 0, 0, 0);  // Cmd 3: OFF
    delay(500);
  }
  delay(1000);

  Serial.println("TEST: Cycle through each room one by one");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      uint16_t start_led = room * LEDS_PER_ROOM;
      sendLedCommand(floor, start_led, LEDS_PER_ROOM, 1, 0, 255, 255, 150);  // Cyan
      delay(300);
      sendLedCommand(floor, start_led, LEDS_PER_ROOM, 3, 0, 0, 0, 0);  // OFF
      delay(300);
    }
  }
  delay(2000);

  Serial.println("TEST: Rainbow pattern on Floor 3");
  sendLedCommand(0, 0, 40, 2, 0, 0, 0, 200);  // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
  sendLedCommand(1, 0, 40, 2, 0, 0, 0, 200);  // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
  sendLedCommand(2, 0, 40, 2, 0, 0, 0, 200);  // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
  sendLedCommand(3, 0, 40, 2, 0, 0, 0, 200);  // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);

  // Serial.println("TEST: Floor 2 Green, Floor 4 Magenta");
  // sendLedCommand(1, 0, 40, 1, 0, 255, 0, 150);    // Strip 1 (Floor 2)
  // sendLedCommand(3, 0, 40, 1, 255, 0, 255, 150);  // Strip 3 (Floor 4)
  // delay(2000);
}
