#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <ArduinoJson.h>

// --- Wi-Fi & WebSocket Configuration ---
const char* ssid = "ESP32-Control-Hub";
const char* password = "password123";
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

// --- WebSocket Event Handler ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.printf("[WebSocket] Received: %s\n", payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print("JSON Parsing Failed: ");
      Serial.println(error.c_str());
      return;
    }

    if (doc.containsKey("cmd_type") && doc.containsKey("i2c_address")) {
      int cmd_type = doc["cmd_type"];
      int i2c_address = doc["i2c_address"];

      if (cmd_type == 1 && doc.containsKey("strips")) { // Initialization
        JsonArray strips = doc["strips"];
        for (JsonObject strip_info : strips) {
          I2C_Initialize_Command init_cmd;
          init_cmd.strip_index = strip_info["strip_index"];
          init_cmd.pin = strip_info["pin"];
          init_cmd.led_count = strip_info["led_count"];

          Wire.beginTransmission(i2c_address);
          Wire.write((uint8_t*)&init_cmd, sizeof(init_cmd));
          Wire.endTransmission();
          Serial.printf("Sent init to I2C addr %d for Strip %d\n", i2c_address, init_cmd.strip_index);
          delay(50);
        }
      } else if (cmd_type == 2 && doc.containsKey("led_commands")) { // LED Commands
          JsonArray led_commands = doc["led_commands"];
          for (JsonObject led_info : led_commands) {
            I2C_Led_Command led_cmd;
            led_cmd.strip_index = led_info["strip_index"];
            led_cmd.start_led = led_info["start_led"];
            led_cmd.led_count = led_info["led_count"];
            led_cmd.command = led_info["command"];
            led_cmd.r = led_info["r"];
            led_cmd.g = led_info["g"];
            led_cmd.b = led_info["b"];
            led_cmd.brightness = led_info["brightness"];

            Wire.beginTransmission(i2c_address);
            Wire.write((uint8_t*)&led_cmd, sizeof(led_cmd));
            Wire.endTransmission();
          }
          Serial.printf("Sent %d LED commands to I2C addr %d\n", led_commands.size(), i2c_address);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // ESP32 as I2C Master

  // --- Start Networking ---
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server ready. Waiting for commands...");
}

void loop() {
  webSocket.loop();
}