#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Wire.h>

// --- WI-FI CONFIGURATION ---
const char* ssid = "Brainwing";
const char* password = "BrainWing";
WebSocketsServer webSocket = WebSocketsServer(81);

// --- I2C & PROJECT CONFIGURATION ---
#define MEGA_I2C_ADDRESS 8
#define NUM_FLOORS 4
#define ROOMS_PER_FLOOR 4
#define LEDS_PER_ROOM 10

// --- I2C COMMAND STRUCTURES (MUST be identical to Mega's) ---
struct __attribute__((packed)) I2C_Initialize_Command {
  uint8_t command_type = 1;
  uint8_t strip_index;
  uint8_t pin;
  uint16_t led_count;
};
struct __attribute__((packed)) I2C_Led_Command {
  uint8_t command_type = 2;
  uint8_t strip_index;
  uint16_t start_led;
  uint16_t led_count;
  uint8_t command;
  uint8_t r, g, b;
  uint8_t brightness;
};

// --- HELPER FUNCTION TO SEND I2C COMMANDS ---
void sendI2cLedCommand(const I2C_Led_Command& cmd) {
  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  Wire.write((uint8_t*)&cmd, sizeof(cmd));
  Wire.endTransmission();
}

// --- COMMAND HANDLERS ---
void handleBulkAvailability(JsonObject data) {
  Serial.println("-> Handling Bulk Availability (Cmd 1)");
  for (JsonPair floor_pair : data) {
    int floor_num = atoi(floor_pair.key().c_str());
    const char* room_states = floor_pair.value().as<const char*>();

    if (floor_num >= 1 && floor_num <= NUM_FLOORS && strlen(room_states) == ROOMS_PER_FLOOR) {
      for (int room_num = 1; room_num <= ROOMS_PER_FLOOR; room_num++) {
        I2C_Led_Command i2c_cmd;
        i2c_cmd.strip_index = floor_num - 1;
        i2c_cmd.start_led = (room_num - 1) * LEDS_PER_ROOM;
        i2c_cmd.led_count = LEDS_PER_ROOM;
        i2c_cmd.brightness = 150;
        i2c_cmd.command = 1; // SET_COLOR

        if (room_states[room_num - 1] == '1') { // Available = Green
          i2c_cmd.r = 0; i2c_cmd.g = 200; i2c_cmd.b = 0;
        } else { // Occupied = Red
          i2c_cmd.r = 200; i2c_cmd.g = 0; i2c_cmd.b = 0;
        }
        sendI2cLedCommand(i2c_cmd);
        delay(5);
      }
    }
  }
}

void handleRoomTrigger(JsonObject data) {
  Serial.println("-> Handling Room Trigger (Cmd 2)");
  int floor_num = data["f"].as<int>();
  int room_num = data["r"].as<int>();

  if (floor_num >= 1 && floor_num <= NUM_FLOORS && room_num >= 1 && room_num <= ROOMS_PER_FLOOR) {
    I2C_Led_Command i2c_cmd;
    i2c_cmd.strip_index = floor_num - 1;
    i2c_cmd.start_led = (room_num - 1) * LEDS_PER_ROOM;
    i2c_cmd.led_count = LEDS_PER_ROOM;
    
    // Flash White
    i2c_cmd.command = 1; i2c_cmd.r = 255; i2c_cmd.g = 255; i2c_cmd.b = 255; i2c_cmd.brightness = 200;
    sendI2cLedCommand(i2c_cmd);
    delay(250);
    
    // Turn Off
    i2c_cmd.command = 3; i2c_cmd.brightness = 0;
    sendI2cLedCommand(i2c_cmd);
  }
}

void handleAnimation(const char* anim_code) {
  Serial.printf("-> Handling Animation (Cmd 3): %s\n", anim_code);
  
  if (strcmp(anim_code, "A1") == 0) { // Rainbow
    for (int i = 0; i < NUM_FLOORS; i++) {
      I2C_Led_Command i2c_cmd;
      i2c_cmd.command = 2; i2c_cmd.strip_index = i; i2c_cmd.start_led = 0;
      i2c_cmd.led_count = ROOMS_PER_FLOOR * LEDS_PER_ROOM; i2c_cmd.brightness = 200;
      sendI2cLedCommand(i2c_cmd);
      delay(5);
    }
  } else if (strcmp(anim_code, "A0") == 0) { // All Off
    for (int i = 0; i < NUM_FLOORS; i++) {
      I2C_Led_Command i2c_cmd;
      i2c_cmd.command = 3; i2c_cmd.strip_index = i; i2c_cmd.start_led = 0;
      i2c_cmd.led_count = ROOMS_PER_FLOOR * LEDS_PER_ROOM; i2c_cmd.brightness = 0;
      sendI2cLedCommand(i2c_cmd);
      delay(5);
    }
  }
}

// --- WEBSOCKET EVENT HANDLER ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    // --- ADDED PRINT STATEMENTS FOR DEBUGGING ---
    Serial.println("\n------------------------------");
    Serial.printf("WebSocket message received: %s\n", payload);
    Serial.println("------------------------------");

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }

    int cmd = doc["cmd"].as<int>();
    
    switch(cmd) {
      case 1:
        handleBulkAvailability(doc["data"].as<JsonObject>());
        break;
      case 2:
        handleRoomTrigger(doc["data"].as<JsonObject>());
        break;
      case 3:
        handleAnimation(doc["data"].as<const char*>());
        break;
      default:
        Serial.println("Unknown command received.");
        break;
    }
  }
}

void initializeMega() {
  const uint8_t mega_pins[NUM_FLOORS] = {2, 3, 4, 5};
  Serial.println("Initializing Mega with project layout via I2C...");
  
  for (int i = 0; i < NUM_FLOORS; i++) {
    I2C_Initialize_Command init_cmd;
    init_cmd.strip_index = i; init_cmd.pin = mega_pins[i];
    init_cmd.led_count = ROOMS_PER_FLOOR * LEDS_PER_ROOM;
    
    Wire.beginTransmission(MEGA_I2C_ADDRESS);
    Wire.write((uint8_t*)&init_cmd, sizeof(init_cmd));
    Wire.endTransmission();
    delay(50);
  }
  Serial.println("Mega initialization complete.");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: ");
  Serial.println(WiFi.localIP());

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("WebSocket server started. Waiting for connections...");
  initializeMega();
}

void loop() {
  webSocket.loop();
}