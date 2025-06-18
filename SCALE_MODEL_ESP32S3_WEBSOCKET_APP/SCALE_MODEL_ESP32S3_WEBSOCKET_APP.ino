#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Adafruit_NeoPixel.h>

#define NUM_FLOORS 8
#define LEDS_PER_FLOOR 30
const uint8_t LED_PINS[NUM_FLOORS] = {2, 4, 5, 18, 19, 21, 22, 23};

const char *ssid = "BuildingLEDs";
const char *password = "12345678";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

Adafruit_NeoPixel floors[NUM_FLOORS] = {
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[4], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[5], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[6], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(LEDS_PER_FLOOR, LED_PINS[7], NEO_GRB + NEO_KHZ800)
};

String currentCommand = "{}";
bool debugEnabled = true;

void debugLog(const String &msg) {
  if (debugEnabled) Serial.println("[DEBUG] " + msg);
}

void notifyClients() {
  debugLog("Broadcasting: " + currentCommand);
  webSocket.broadcastTXT(currentCommand);
}

void applyPattern(const String &command) {
  debugLog("Applying: " + command);

  if (command == "wave") {
    for (int i = 0; i < NUM_FLOORS; i++) {
      for (int j = 0; j < LEDS_PER_FLOOR; j++)
        floors[i].setPixelColor(j, floors[i].Color(0, 0, 255));
      floors[i].show();
      delay(100);
    }
  } else if (command == "all_on") {
    for (int i = 0; i < NUM_FLOORS; i++) {
      for (int j = 0; j < LEDS_PER_FLOOR; j++)
        floors[i].setPixelColor(j, floors[i].Color(255, 255, 255));
      floors[i].show();
    }
  } else if (command == "off") {
    for (int i = 0; i < NUM_FLOORS; i++) {
      floors[i].clear();
      floors[i].show();
    }
  } else {
    debugLog("Unknown command: " + command);
  }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_TEXT: {
      String msg = String((char *)payload).substring(0, length);
      debugLog("Received: " + msg);
      currentCommand = msg;
      applyPattern(msg);
      notifyClients();
      break;
    }
    case WStype_CONNECTED:
      debugLog("Client connected");
      break;
    case WStype_DISCONNECTED:
      debugLog("Client disconnected");
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial){};
  debugLog("Initializing NeoPixels...");
  for (int i = 0; i < NUM_FLOORS; i++) floors[i].begin();

  debugLog("Starting WiFi Access Point...");
  WiFi.softAP(ssid, password);
  debugLog("AP started: " + String(ssid));

  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  debugLog("WebSocket server started on port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}
