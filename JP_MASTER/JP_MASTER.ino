/*
 * ════════════════════════════════════════════════════════════════════════════
 * JP INFRA - ESP32 MASTER CONTROLLER
 * Production-Ready with Availability Feature + Custom LED Control
 * ════════════════════════════════════════════════════════════════════════════
 * 
 * ARCHITECTURE:
 * ────────────
 * ESP32 DevKit Master ──[I2C]──> Arduino Mega Slave
 * 
 * Single Building LED Control System with Availability Display
 * 
 * HARDWARE CONNECTIONS:
 * ─────────────────────
 * I2C:
 *   - SDA: GPIO 11
 *   - SCL: GPIO 12
 *   - Slave Address: 0x08
 * 
 * Relays (Active LOW):
 *   - GPIO 5:  Surrounding Lights
 *   - GPIO 6:  Terrace Lights
 * 
 * Physical Buttons (Active LOW with internal pull-up):
 *   - GPIO 47: All Lights ON
 *   - GPIO 48: All Lights OFF
 *   - GPIO 1:  Amenities Toggle (Surrounding Relay)
 *   - GPIO 21: Pattern/Classic Mode
 * 
 * Status LED:
 *   - GPIO 10: Status indicator
 * 
 * ════════════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

/* ══════════════════════ CONFIGURATION ══════════════════════ */

// WiFi credentials
const char* WIFI_SSID = "JP Chembur 71";
const char* WIFI_PASSWORD = "JPinfra@71";

// MQTT Broker configuration
const char* MQTT_BROKER = "mqtt.modelsofbrainwing.com";
const int MQTT_PORT = 8883; 
const char* MQTT_USER = "reactuser";
const char* MQTT_PASSWORD = "scaleModel";
const char* MQTT_CLIENT_ID = "esp32-bridge-jpinfra";

// MQTT Topics - JP Infra namespace
const char* MQTT_PROJECT = "jpinfra";

// I2C Configuration
#define I2C_SDA 11
#define I2C_SCL 12
#define MEGA_I2C_ADDRESS 0x08
#define I2C_TIMEOUT_MS 50
#define I2C_RETRIES 3

// Relay Pins (Active LOW)
#define RELAY_SURROUNDING 5
#define RELAY_TERRACE 6

// Button Pins (Active LOW with pull-up)
#define BTN_ALL_ON 2
#define BTN_ALL_OFF 48
#define BTN_AMENITIES 1 //
#define BTN_PATTERN 47 //All lights

// Status LED
#define STATUS_LED 10

// Command codes for Mega
#define CMD_ALL_ON 0x01
#define CMD_ALL_OFF 0x02
#define CMD_CLASSIC 0x03
#define CMD_FLOOR_ON 0x04
#define CMD_FLOOR_OFF 0x05
#define CMD_ROOM_ON 0x06
#define CMD_ROOM_OFF 0x07
#define CMD_AVAIL_BASE_GREEN 0x10   // Set all residential floors to green
#define CMD_AVAIL_FLOOR_SET 0x11    // Set rooms on a specific floor
#define CMD_CUSTOM_LEDS 0x12        // NEW: Custom LED control (floor, count, R, G, B)
#define CMD_RELAY_TOGGLE 0x09
#define CMD_PING 0x0A

// Availability status codes (matches availability.csv)
#define STATUS_SOLD 0      // Blue
#define STATUS_AVAILABLE 1 // Green
#define STATUS_BLOCKED 2   // Yellow

/* ══════════════════════ TLS CERTIFICATE ══════════════════════ */

static const char ROOT_CA_CERT[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)PEM";

/* ══════════════════════ GLOBAL OBJECTS ══════════════════════ */

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

/* ══════════════════════ STATE VARIABLES ══════════════════════ */

struct SystemState {
  bool surroundingOn;
  bool terraceOn;
  bool megaConnected;
} state = {false, false, false};

struct Button {
  uint8_t pin;
  bool lastReading;
  uint32_t lastDebounceTime;
};

Button buttons[4] = {
  {BTN_ALL_ON,     HIGH, 0},
  {BTN_ALL_OFF,    HIGH, 0},
  {BTN_AMENITIES,  HIGH, 0},
  {BTN_PATTERN,    HIGH, 0}
};

unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 5000; // 5 seconds

/* ══════════════════════ HELPER FUNCTIONS ══════════════════════ */

String getTopic(const char* path) {
  return String(MQTT_PROJECT) + "/" + String(path);
}

uint32_t calculateBackoff(uint8_t attempt) {
  uint32_t base = 750 << min(attempt, (uint8_t)5);
  base = min(base, (uint32_t)15000);
  return base + (esp_random() % (base / 3 + 1));
}

/* ══════════════════════ I2C COMMUNICATION ══════════════════════ */

void sendI2CCommand(byte cmd, byte param1 = 0, byte param2 = 0, byte param3 = 0, byte param4 = 0, byte param5 = 0) {
  Serial.printf("[I2C] Sending command: 0x%02X", cmd);
  
  // Always send params for these commands (even if 0)
  bool alwaysSendParams = (cmd == CMD_CUSTOM_LEDS || cmd == CMD_AVAIL_FLOOR_SET);
  
  if (param1 > 0 || alwaysSendParams) Serial.printf(", p1: %d", param1);
  if (param2 > 0 || alwaysSendParams) Serial.printf(", p2: %d", param2);
  if (param3 > 0 || alwaysSendParams) Serial.printf(", p3: %d", param3);
  if (param4 > 0 || alwaysSendParams) Serial.printf(", p4: %d", param4);
  if (param5 > 0 || alwaysSendParams) Serial.printf(", p5: %d", param5);
  Serial.println();
  
  bool success = false;
  for (uint8_t attempt = 0; attempt < I2C_RETRIES; attempt++) {
    Wire.beginTransmission(MEGA_I2C_ADDRESS);
    Wire.write(cmd);
    if (param1 > 0 || alwaysSendParams) Wire.write(param1);
    if (param2 > 0 || alwaysSendParams) Wire.write(param2);
    if (param3 > 0 || alwaysSendParams) Wire.write(param3);
    if (param4 > 0 || alwaysSendParams) Wire.write(param4);
    if (param5 > 0 || alwaysSendParams) Wire.write(param5);
    
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.println("[I2C] ✅ Command sent successfully");
      state.megaConnected = true;
      success = true;
      break;
    }
    
    if (attempt < I2C_RETRIES - 1) delay(5);
  }
  
  if (!success) {
    Serial.printf("[I2C] ❌ Failed after %d attempts\n", I2C_RETRIES);
    state.megaConnected = false;
  }
  
  delay(10);
}

void testMegaConnection() {
  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  byte error = Wire.endTransmission();
  state.megaConnected = (error == 0);
}

/* ══════════════════════ RELAY CONTROL ══════════════════════ */

void setupRelays() {
  pinMode(RELAY_SURROUNDING, OUTPUT);
  pinMode(RELAY_TERRACE, OUTPUT);
  digitalWrite(RELAY_SURROUNDING, HIGH); // Active LOW - off
  digitalWrite(RELAY_TERRACE, HIGH);
  
  Serial.println("[RELAY] Relay pins initialized");
  Serial.printf("  Surrounding: GPIO%d\n", RELAY_SURROUNDING);
  Serial.printf("  Terrace: GPIO%d\n", RELAY_TERRACE);
}

void setRelay(int relayPin, bool active) {
  digitalWrite(relayPin, active ? LOW : HIGH);  // Active LOW
  
  if (relayPin == RELAY_SURROUNDING) {
    state.surroundingOn = active;
    Serial.printf("[RELAY] Surrounding: %s\n", active ? "ON" : "OFF");
  } else if (relayPin == RELAY_TERRACE) {
    state.terraceOn = active;
    Serial.printf("[RELAY] Terrace: %s\n", active ? "ON" : "OFF");
  }
}

void toggleRelay(int relayPin) {
  if (relayPin == RELAY_SURROUNDING) {
    setRelay(relayPin, !state.surroundingOn);
  } else if (relayPin == RELAY_TERRACE) {
    setRelay(relayPin, !state.terraceOn);
  }
}

/* ══════════════════════ BUTTON HANDLING ══════════════════════ */

void setupButtons() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
  Serial.println("[BTN] Button pins initialized with pull-up");
  Serial.printf("  All ON:      GPIO%d\n", BTN_ALL_ON);
  Serial.printf("  All OFF:     GPIO%d\n", BTN_ALL_OFF);
  Serial.printf("  Amenities:   GPIO%d\n", BTN_AMENITIES);
  Serial.printf("  Pattern:     GPIO%d\n", BTN_PATTERN);
}

void checkButtons() {
  static uint32_t lastPrint = 0;  // ← ADD THIS
  uint32_t now = millis();
  const uint32_t debounceDelay = 50;  // ← INCREASED
  
  // ← ADD THIS BLOCK
  if (now - lastPrint > 2000) {
    Serial.printf("[BTN] States: 47=%d 48=%d 1=%d 21=%d\n",
                  digitalRead(47), digitalRead(48),
                  digitalRead(1), digitalRead(21));
    lastPrint = now;
  }
  
  for (uint8_t i = 0; i < 4; i++) {
    bool currentReading = digitalRead(buttons[i].pin);
    
    if (currentReading != buttons[i].lastReading) {
      buttons[i].lastDebounceTime = now;
      Serial.printf("[BTN] GPIO%d changed to %d\n", buttons[i].pin, currentReading);  // ← ADD THIS
    }
    
      if (currentReading == LOW && buttons[i].lastReading == HIGH) {
        handleButtonPress(buttons[i].pin);
      
    }
    
    buttons[i].lastReading = currentReading;
  }
}

void handleButtonPress(uint8_t pin) {
  Serial.printf("[BTN] Button pressed on GPIO%d\n", pin);
  
  if (pin == BTN_ALL_ON) {
    sendI2CCommand(CMD_ALL_ON);
  } else if (pin == BTN_ALL_OFF) {
    sendI2CCommand(CMD_ALL_OFF);
  } else if (pin == BTN_AMENITIES) {
    toggleRelay(RELAY_SURROUNDING);
  } else if (pin == BTN_PATTERN) {
    sendI2CCommand(CMD_CLASSIC);
  }
}

/* ══════════════════════ MQTT FUNCTIONS ══════════════════════ */

void sendAck(const char* clientId, const char* status, String message) {
  StaticJsonDocument<256> ackDoc;
  ackDoc["clientId"] = clientId;
  ackDoc["status"] = status;
  ackDoc["message"] = message;
  
  String ackPayload;
  serializeJson(ackDoc, ackPayload);
  
  String ackTopic = getTopic("ui/ack");
  mqttClient.publish(ackTopic.c_str(), ackPayload.c_str(), false);
}

void sendHeartbeat() {
  StaticJsonDocument<512> hbDoc;
  hbDoc["device"] = "esp32-bridge";
  hbDoc["type"] = "heartbeat";
  hbDoc["rssi"] = WiFi.RSSI();
  hbDoc["uptime"] = millis() / 1000;
  hbDoc["megaConnected"] = state.megaConnected;
  
  JsonObject relays = hbDoc.createNestedObject("relays");
  relays["surrounding"] = state.surroundingOn;
  relays["terrace"] = state.terraceOn;
  
  String hbPayload;
  serializeJson(hbDoc, hbPayload);
  
  String hbTopic = getTopic("device/heartbeat");
  mqttClient.publish(hbTopic.c_str(), hbPayload.c_str(), false);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.printf("[MQTT] JSON parse error: %s\n", error.c_str());
    return;
  }
  
  const char* clientId = doc["clientId"] | "unknown";
  const char* type = doc["type"];
  
  if (!type) {
    Serial.println("[CMD] ❌ No 'type' field in command");
    return;
  }
  
  Serial.printf("[CMD] Processing: %s\n", type);
  String typeStr = String(type);
  
  // ===== BASE GREEN COMMAND =====
  if (typeStr == "avail_base_green") {
    Serial.println("[CMD] ⚡ Setting base GREEN for availability mode");
    
    // Send single command to Mega to set all residential floors green
    sendI2CCommand(CMD_AVAIL_BASE_GREEN);
    delay(50);
    
    sendAck(clientId, "ok", "Base green set");
  }
  
  // ===== FLOOR DATA COMMAND =====
  else if (typeStr == "avail_floor_data") {
    int floor = doc["floor"];
    
    Serial.printf("[CMD] ⚡ Processing floor %d availability\n", floor);
    
    // Get blocked rooms array (status=2, Yellow)
    JsonArray blockedArr = doc["blocked"];
    if (!blockedArr.isNull() && blockedArr.size() > 0) {
      Serial.printf("[CMD] Floor %d - %d blocked rooms\n", floor, blockedArr.size());
      
      for (JsonVariant roomVar : blockedArr) {
        int room = roomVar.as<int>();
        sendI2CCommand(CMD_AVAIL_FLOOR_SET, floor, room, 2); // 2 = blocked/yellow
        delay(25); // Slightly longer delay for reliability
      }
    }
    
    // Get sold rooms array (status=0, Blue)
    JsonArray soldArr = doc["sold"];
    if (!soldArr.isNull() && soldArr.size() > 0) {
      Serial.printf("[CMD] Floor %d - %d sold rooms\n", floor, soldArr.size());
      
      for (JsonVariant roomVar : soldArr) {
        int room = roomVar.as<int>();
        sendI2CCommand(CMD_AVAIL_FLOOR_SET, floor, room, 0); // 0 = sold/blue
        delay(25); // Slightly longer delay for reliability
      }
    }
    
    sendAck(clientId, "ok", String("Floor ") + floor + " updated");
  }
  
  // ===== CUSTOM LED CONTROL (NEW) =====
  else if (typeStr == "custom_leds") {
    int floor = doc["floor"];
    int ledCount = doc["count"];
    int r = doc["r"] | 255;
    int g = doc["g"] | 255;
    int b = doc["b"] | 255;
    
    Serial.printf("[CMD] ⚡ Custom LEDs: Floor %d, Count %d, RGB(%d,%d,%d)\n", 
                  floor, ledCount, r, g, b);
    
    sendI2CCommand(CMD_CUSTOM_LEDS, floor, ledCount, r, g, b);
    delay(30);
    
    sendAck(clientId, "ok", String("Custom LEDs set on floor ") + floor);
  }
  
  // ===== LED CONTROL COMMANDS =====
  else if (typeStr == "white_all" || typeStr == "set_all_lights_on") {
    sendI2CCommand(CMD_ALL_ON);
    sendAck(clientId, "ok", "All lights ON");
  }
  else if (typeStr == "set_all_floors_off" || typeStr == "off_all") {
    sendI2CCommand(CMD_ALL_OFF);
    sendAck(clientId, "ok", "All lights OFF");
  }
  else if (typeStr == "classic_all") {
    sendI2CCommand(CMD_CLASSIC);
    sendAck(clientId, "ok", "Classic pattern activated");
  }
  else if (typeStr == "set_floor_color") {
    int floor = doc["floor"];
    sendI2CCommand(CMD_FLOOR_ON, floor);
    sendAck(clientId, "ok", String("Floor ") + floor + " ON");
  }
  else if (typeStr == "set_floor_off") {
    int floor = doc["floor"];
    sendI2CCommand(CMD_FLOOR_OFF, floor);
    sendAck(clientId, "ok", String("Floor ") + floor + " OFF");
  }
  else if (typeStr == "set_room_color") {
    int floor = doc["floor"];
    int room = doc["room"];
    sendI2CCommand(CMD_ROOM_ON, floor, room);
    sendAck(clientId, "ok", String("Room ") + room + " ON");
  }
  else if (typeStr == "set_room_off") {
    int floor = doc["floor"];
    int room = doc["room"];
    sendI2CCommand(CMD_ROOM_OFF, floor, room);
    sendAck(clientId, "ok", String("Room ") + room + " OFF");
  }
  
  // ===== RELAY COMMANDS =====
  else if (typeStr == "relay_toggle") {
    const char* relay = doc["relay"];
    if (strcmp(relay, "surrounding") == 0) {
      toggleRelay(RELAY_SURROUNDING);
      sendAck(clientId, "ok", String("Surrounding: ") + (state.surroundingOn ? "ON" : "OFF"));
    } else if (strcmp(relay, "terrace") == 0) {
      toggleRelay(RELAY_TERRACE);
      sendAck(clientId, "ok", String("Terrace: ") + (state.terraceOn ? "ON" : "OFF"));
    }
  }
  else if (typeStr == "relay_set") {
    const char* relay = doc["relay"];
    bool relayState = doc["state"];
    if (strcmp(relay, "surrounding") == 0) {
      setRelay(RELAY_SURROUNDING, relayState);
      sendAck(clientId, "ok", String("Surrounding: ") + (relayState ? "ON" : "OFF"));
    } else if (strcmp(relay, "terrace") == 0) {
      setRelay(RELAY_TERRACE, relayState);
      sendAck(clientId, "ok", String("Terrace: ") + (relayState ? "ON" : "OFF"));
    }
  }
  
  // ===== PING =====
  else if (typeStr == "ping") {
    sendAck(clientId, "pong", "Device online");
  }
  
  // ===== CAST COMMANDS =====
  else if (typeStr == "cast_request") {
    Serial.println("[CMD] Cast request (handled by TV app)");
    sendAck(clientId, "ok", "Cast request acknowledged");
  }
  else if (typeStr == "cast_release") {
    Serial.println("[CMD] Cast release (handled by TV app)");
    sendAck(clientId, "ok", "Cast released");
  }
  
  else {
    Serial.printf("[CMD] ⚠️  Unknown command type: %s\n", type);
    sendAck(clientId, "error", String("Unknown command: ") + type);
  }
}


bool connectMQTT() {
  String willTopic = getTopic("ui/status");
  String clientID = String("jpinfra-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  
  Serial.print("[MQTT] Connecting... ");
  
  if (!mqttClient.connect(clientID.c_str(), MQTT_USER, MQTT_PASSWORD, 
                          willTopic.c_str(), 0, true, "offline")) {
    Serial.printf("FAILED (state: %d)\n", mqttClient.state());
    return false;
  }
  
  Serial.println("SUCCESS");
  mqttClient.publish(willTopic.c_str(), "online", true);
  
  String cmdTopic = getTopic("ui/cmd");
  mqttClient.subscribe(cmdTopic.c_str(), 0);
  Serial.printf("[MQTT] Subscribed to: %s\n", cmdTopic.c_str());
  
  sendHeartbeat();
  return true;
}

/* ══════════════════════ SETUP ══════════════════════ */

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔════════════════════════════════════════════╗");
  Serial.println("║  JP INFRA - ESP32 Master v2.2             ║");
  Serial.println("║  With Availability + Custom LED Control   ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  setupRelays();
  setupButtons();
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeout(I2C_TIMEOUT_MS);
  Serial.println("[I2C] I2C Master initialized");
  Serial.printf("  SDA: GPIO%d, SCL: GPIO%d\n", I2C_SDA, I2C_SCL);
  Serial.printf("  Mega Address: 0x%02X\n", MEGA_I2C_ADDRESS);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected | IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n[WIFI] FAILED - Will retry in loop");
  }
  
  espClient.setCACert(ROOT_CA_CERT);
  espClient.setTimeout(3000);
  mqttClient.setSocketTimeout(3);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(25);
  mqttClient.setBufferSize(2048);
  
  if (WiFi.status() == WL_CONNECTED) {
    connectMQTT();
  }
  
  testMegaConnection();
  digitalWrite(STATUS_LED, HIGH);
  Serial.println("\n[READY] System ready\n");
}

/* ══════════════════════ MAIN LOOP ══════════════════════ */

void loop() {
  static uint8_t wifiAttempts = 0, mqttAttempts = 0;
  static uint32_t nextWifiRetry = 0, nextMqttRetry = 0;
  uint32_t now = millis();
  
  checkButtons();
  
  if (WiFi.status() != WL_CONNECTED) {
    if (now >= nextWifiRetry) {
      Serial.println("[WIFI] Reconnecting...");
      WiFi.disconnect(true);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      nextWifiRetry = now + calculateBackoff(wifiAttempts++);
    }
  } else {
    wifiAttempts = 0;
  }
  
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    if (now >= nextMqttRetry) {
      bool success = connectMQTT();
      mqttAttempts = success ? 0 : mqttAttempts + 1;
      nextMqttRetry = now + (success ? 1000 : calculateBackoff(mqttAttempts));
    }
  } else if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    if (mqttClient.connected()) {
      sendHeartbeat();
    }
    lastHeartbeat = millis();
  }
}
