#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <ArduinoJson.h>

// --- I2C & LED Configuration ---
#define MEGA_I2C_ADDRESS 8
#define NUM_FLOORS 4
#define LEDS_PER_ROOM 8
#define ROOMS_PER_FLOOR 4
const uint8_t mega_pins[NUM_FLOORS] = {12, 11, 10, 13};

// --- Wi-Fi & WebSocket Configuration ---
// *** IMPORTANT: CHANGE THESE TO YOUR RASPBERRY PI'S WI-FI DETAILS ***
const char* ssid = "ScaleModel";         // Your Raspberry Pi's WiFi Name
const char* password = "12345678";  // Your Raspberry Pi's WiFi Password
WebSocketsServer webSocket = WebSocketsServer(81);

// These structs MUST be identical to the ones on the Arduino slave
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


// --- Helper function to send LED commands ---
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

  // Optional: Print the I2C result for debugging
  if (res == 0) {
    Serial.printf("I2C command sent successfully to strip %u.\n", strip);
  } else {
    Serial.printf("I2C Error for strip %u. Response code: %d\n", strip, res);
  }
}

// --- WebSocket Event Handler ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.printf("[WebSocket] Received: %s\n", payload);

    if (length < 2) {
      Serial.println("Ignoring short/invalid message.");
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print("JSON Parsing Failed: ");
      Serial.println(error.c_str());
      return;
    }

    if (!doc.containsKey("cmd")) {
      Serial.println("Received JSON is missing the 'cmd' key.");
      return;
    }

    int commandId = doc["cmd"];

    switch(commandId) {
      case 1: { // "all_available" command
        if (!doc.containsKey("data")) {
          Serial.println("Availability command is missing 'data' object.");
          return;
        }
        Serial.println("Executing detailed 'All Available' command...");
        
        JsonObject data = doc["data"];
        
        for (JsonPair floor_pair : data) {
          int strip_index = atoi(floor_pair.key().c_str()) - 1;
          
          if (strip_index < 0 || strip_index >= NUM_FLOORS) {
            Serial.printf("  -> Invalid floor number %s detected. Skipping.\n", floor_pair.key().c_str());
            continue;
          }
          
          const char* room_states = floor_pair.value().as<const char*>();
          Serial.printf("  -> Processing Floor %d with states '%s'\n", strip_index, room_states);

          for (int room_index = 0; room_index < strlen(room_states); room_index++) {
            uint16_t start_led = room_index * LEDS_PER_ROOM;
            
            if (room_states[room_index] == '1') {
              Serial.printf("    - Sending to Slave: [Strip: %d, StartLED: %d, Count: %d, Color: GREEN]\n", 
                strip_index, start_led, LEDS_PER_ROOM);
              sendLedCommand(strip_index, start_led, LEDS_PER_ROOM, 1, 0, 255, 0, 100);
            } else {
              Serial.printf("    - Sending to Slave: [Strip: %d, StartLED: %d, Count: %d, Color: RED]\n", 
                strip_index, start_led, LEDS_PER_ROOM);
              sendLedCommand(strip_index, start_led, LEDS_PER_ROOM, 1, 255, 0, 0, 100);
            }
          }
        }
        break;
      }
      case 2: { // "room_click" command
        if (!doc.containsKey("data")) {
          Serial.println("Room click command is missing the 'data' object.");
          return;
        }
        JsonObject data = doc["data"];
        int floor = data["f"];
        int room = data["r"];

        uint8_t strip_index = floor - 1; 

        if (strip_index >= NUM_FLOORS) {
            Serial.printf("  -> Invalid floor number %d detected. Skipping.\n", floor);
            return;
        }

        uint16_t start_led = (room - 1) * LEDS_PER_ROOM;
        Serial.printf("Executing 'Room Click' for Floor %d (Strip Index %d), Room %d\n", floor, strip_index, room);
        sendLedCommand(strip_index, start_led, LEDS_PER_ROOM, 1, 0, 0, 255, 200);
        break;
      }
      case 3: { // "play_animation" command
        if (!doc.containsKey("data")) {
          Serial.println("Animation command is missing the 'data' object.");
          return;
        }
        const char* animationName = doc["data"];
        Serial.printf("Executing 'Animation': %s\n", animationName);
        uint8_t mega_command = 0;
        if (strcmp(animationName, "A1") == 0) { mega_command = 2; }
        else if (strcmp(animationName, "A2") == 0) { mega_command = 3; }

        if (mega_command > 0) {
          for (int floor = 0; floor < NUM_FLOORS; floor++) {
            sendLedCommand(floor, 0, LEDS_PER_ROOM * ROOMS_PER_FLOOR, mega_command, 0, 0, 0, 150);
          }
        }
        break;
      }
      case 4: { // "floor_control" command
        if (!doc.containsKey("data")) { return; }
        Serial.println("Data Valid for floors");
        JsonObject data = doc["data"];
        int floor = data["f"];
        uint8_t r = data["r"];
        uint8_t g = data["g"];
        uint8_t b = data["b"];
        
        uint8_t strip_index = floor - 1;
        if (strip_index >= NUM_FLOORS) { return; }
        Serial.println("Index Valid for floors");
        Serial.printf("Executing 'Floor Control' for Floor %d (Strip Index %d)\n", floor, strip_index);
        sendLedCommand(strip_index, 0, LEDS_PER_ROOM * ROOMS_PER_FLOOR, 1, r, g, b, 150);
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // --- Initialize I2C and the Arduino Mega ---
  Wire.begin(11,12); // Use default SDA/SCL pins or specify them e.g., Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("\nESP32 I2C Master: Initializing Arduino Mega Strips...");

  for (int i = 0; i < NUM_FLOORS; i++) {
    I2C_Initialize_Command init_cmd;
    init_cmd.strip_index = i;
    init_cmd.pin = mega_pins[i];
    init_cmd.led_count = LEDS_PER_ROOM * ROOMS_PER_FLOOR;

    Wire.beginTransmission(MEGA_I2C_ADDRESS);
    Wire.write((uint8_t*)&init_cmd, sizeof(init_cmd));
    Wire.endTransmission();
    Serial.printf("Sent init for Strip %d on pin %d\n", i, mega_pins[i]);
    delay(50); // Give slave time to process each init command
  }
  Serial.println("--- Arduino Mega Initialization Complete ---");

  // --- Start Networking ---
  // --- MODIFICATION START ---
  // Connect to the existing Wi-Fi network instead of creating a new one.
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid);

  // Wait for the connection to be established.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // This is the IP you will connect to from your client.
  // --- MODIFICATION END ---

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server ready. Waiting for commands...");
}

void loop() {
  // Required to process WebSocket events
  webSocket.loop();
}

void TestInSetup()
{
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    sendLedCommand(floor, 0, 40, 1, 255, 255, 0, 60*(floor+1));   // Cmd 1: SET_COLOR
    delay(500);
  }
  delay(1000);

  Serial.println("TEST: All rooms OFF");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    sendLedCommand(floor, 0, 40, 3, 0, 0, 0, 0);   // Cmd 3: OFF
    delay(500);
  }
  delay(1000);

  Serial.println("TEST: Cycle through each room one by one");
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      uint16_t start_led = room * LEDS_PER_ROOM;
      sendLedCommand(floor, start_led, LEDS_PER_ROOM, 1, 0, 255, 255, 150);   // Cyan
      delay(300);
      sendLedCommand(floor, start_led, LEDS_PER_ROOM, 3, 0, 0, 0, 0);   // OFF
      delay(300);
    }
  }
  delay(2000);

  Serial.println("TEST: Rainbow pattern on Floor 3");
  sendLedCommand(0, 0, 40, 2, 0, 0, 0, 200);   // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
  sendLedCommand(1, 0, 40, 2, 0, 0, 0, 200);   // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
  sendLedCommand(2, 0, 40, 2, 0, 0, 0, 200);   // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
  sendLedCommand(3, 0, 40, 2, 0, 0, 0, 200);   // Cmd 2: RAINBOW, strip 2 (Floor 3)
  delay(1000);
}
