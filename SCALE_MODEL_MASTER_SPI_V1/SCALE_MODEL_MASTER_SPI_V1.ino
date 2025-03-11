/*******************************************************
 * ESP32 Master Code - Optimized SPI Implementation
 *******************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>

// ===== Configuration Constants =====
constexpr uint32_t SERIAL_BAUDRATE = 115200;
constexpr int MAX_RETRIES = 5;
constexpr int INITIAL_BACKOFF_MS = 500;
constexpr uint16_t SPI_CLOCK = 2000000;
constexpr size_t MAX_CMD_LENGTH = 32;
constexpr size_t JSON_DOC_SIZE = 512;
constexpr size_t ROOM_STATUS_DOC_SIZE = 4096;

// ===== Network Configuration =====
const char* SSID = "Vista";
const char* PASSWORD = "Vista@321";
const char* BASE_URL = "https://admin.modelsofbrainwing.com/";
const char* API_ENDPOINT = "get_led_control.php";
constexpr int PROJECT_ID = 9;

// ===== SPI Pins (VSPI) =====
constexpr uint8_t PIN_SPI_SCK = 18;
constexpr uint8_t PIN_SPI_MISO = 19;
constexpr uint8_t PIN_SPI_MOSI = 23;
constexpr uint8_t PIN_SPI_SS = 5;

// ===== Command IDs =====
enum CommandIDs : uint8_t {
  CMD_VERTICALROOMSCHASINGLED = 66,
  CMD_ROOMVARIATIONS9TO11FLOOR,
  CMD_REFUGEEFLOORS1,
  CMD_VERTICALROOMSFLOORWISEROOM1AND2,
  CMD_VERTICALROOMSFLOORWISEROOM3AND4,
  CMD_FLOOR3CONTROL,
  CMD_FLOOR45678CONTROL,
  CMD_ALLLIGHT,
  CMD_ROOMVARIATIONS12TO15FLOOR,
  CMD_ROOMVARIATIONS16TO18FLOOR,
  CMD_REFUGEEFLOORS2,
  CMD_SHOWAVAILABILITY = 78,
  CMD_2BHK,
  CMD_3BHK,
  CMD_PATTERN,
  CMD_TurnOfAllLights
};

// ===== Building Configuration =====
constexpr uint8_t NUM_FLOORS = 16;
constexpr uint8_t NUM_ROOMS_PER_FLOOR = 4;
constexpr uint16_t TOTAL_ROOMS = NUM_FLOORS * NUM_ROOMS_PER_FLOOR;

// ===== Data Structures =====
struct WingInfo {
  int wingId;
  char activeControl[MAX_CMD_LENGTH];
  char lastSentControl[MAX_CMD_LENGTH];
  uint8_t roomAvailability[TOTAL_ROOMS];
};

constexpr uint8_t NUM_WINGS = 1;
WingInfo wings[NUM_WINGS] = {{17, "", "", {}}};  // Initialize wing

// ===== Concurrency Primitives =====
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t wingMutex;

// ===== Function Declarations =====
void setupWiFi();
void pollAPITask(void* parameter);
void ledControlTask(void* parameter);
void sendCommandSPI(uint8_t commandID);
void sendAvailabilityDataSPI(const uint8_t* data, size_t dataSize);
bool fetchRoomStatus(WingInfo& wing);

// ===== Logging Helpers =====
template<typename T>
void logInfo(const T& message) {
  Serial.printf("[INFO] %s\n", String(message).c_str());
}

template<typename T>
void logError(const T& message) {
  Serial.printf("[ERROR] %s\n", String(message).c_str());
}

// ===== Command Lookup Table =====
struct CommandMapping {
  const char* name;
  uint8_t id;
};

const CommandMapping COMMAND_MAP[] = {
  {"VerticalRoomsChasingLed", CMD_VERTICALROOMSCHASINGLED},
  {"RoomVariations9To11Floor", CMD_ROOMVARIATIONS9TO11FLOOR},
  {"RefugeeFloors1", CMD_REFUGEEFLOORS1},
  // Add all other command mappings here
};

uint8_t getCommandId(const String& commandName) {
  for (const auto& mapping : COMMAND_MAP) {
    if (commandName.equals(mapping.name)) {
      return mapping.id;
    }
  }
  return 0;
}

// ===== Setup =====
void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);
  logInfo("Initializing system...");

  // Initialize SPI
  pinMode(PIN_SPI_SS, OUTPUT);
  digitalWrite(PIN_SPI_SS, HIGH);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SS);

  // Initialize mutexes
  wingMutex = xSemaphoreCreateMutex();
  wifiMutex = xSemaphoreCreateMutex();

  // Initialize relays
  constexpr uint8_t RELAY_PINS[] = {8, 9, 10};
  for (auto pin : RELAY_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  setupWiFi();

  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(
    pollAPITask,
    "API Poll",
    4096,
    nullptr,
    1,
    nullptr,
    0
  );

  xTaskCreatePinnedToCore(
    ledControlTask,
    "LED Control",
    4096,
    nullptr,
    1,
    nullptr,
    1
  );

  logInfo("System initialization complete");
}

void loop() {
  vTaskDelete(nullptr);
}

// ===== Wi-Fi Management =====
void setupWiFi() {
  WiFi.begin(SSID, PASSWORD);
  logInfo("Connecting to WiFi...");

  for (int i = 0; i < MAX_RETRIES; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(INITIAL_BACKOFF_MS << i);
  }

  if (WiFi.status() != WL_CONNECTED) {
    logError("WiFi connection failed");
    return;
  }
  logInfo("WiFi connected. IP: " + WiFi.localIP().toString());
}

// ===== API Polling Task =====
void pollAPITask(void* parameter) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      xSemaphoreTake(wifiMutex, portMAX_DELAY);
      setupWiFi();
      xSemaphoreGive(wifiMutex);
    }

    // Poll common commands
    pollCommonCommands();

    // Poll wing-specific commands
    for (auto& wing : wings) {
      HTTPClient http;
      String url = String(BASE_URL) + API_ENDPOINT +
                  "?project_id=" + PROJECT_ID +
                  "&wing_id=" + wing.wingId;

      if (http.begin(url) && http.GET() == HTTP_CODE_OK) {
        DynamicJsonDocument doc(JSON_DOC_SIZE);
        if (deserializeJson(doc, http.getString())) {
          logError("JSON parsing failed");
          continue;
        }

        const char* newCommand = doc["control"];
        if (newCommand && strcmp(newCommand, wing.activeControl) != 0) {
          xSemaphoreTake(wingMutex, portMAX_DELAY);
          strncpy(wing.activeControl, newCommand, MAX_CMD_LENGTH);
          xSemaphoreGive(wingMutex);
          logInfo("New command: " + String(newCommand));
        }
      }
      http.end();
      delay(50);
    }
    delay(500);
  }
}

// ===== Common Commands Handling =====
void pollCommonCommands() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(BASE_URL) + API_ENDPOINT +
              "?project_id=" + PROJECT_ID +
              "&wing_id=1";  // Common commands use wing_id=1

  if (http.begin(url)) {
    if (http.GET() == HTTP_CODE_OK) {
      DynamicJsonDocument doc(JSON_DOC_SIZE);
      if (deserializeJson(doc, http.getString())) {
        logError("Common command JSON error");
        http.end();
        return;
      }

      const char* control = doc["control"];
      if (control && control[0] != '\0') {
        logInfo("Common command received: " + String(control));
        
        // Handle relay controls
        if (strcmp(control, "AllLight") == 0) {
          digitalWrite(10, LOW);  // Activate relay
        } else {
          digitalWrite(10, HIGH); // Deactivate relay
        }
        
        // Add other common command handlers as needed
      }
    }
    http.end();
  }
}

// ===== LED Control Task =====
void ledControlTask(void* parameter) {
  while (true) {
    for (auto& wing : wings) {
      char currentCommand[MAX_CMD_LENGTH];
      
      xSemaphoreTake(wingMutex, portMAX_DELAY);
      strncpy(currentCommand, wing.activeControl, MAX_CMD_LENGTH);
      xSemaphoreGive(wingMutex);

      if (strcmp(currentCommand, wing.lastSentControl) != 0) {
        uint8_t commandId = getCommandId(currentCommand);
        if (commandId != 0) {
          sendCommandSPI(commandId);
          strncpy(wing.lastSentControl, currentCommand, MAX_CMD_LENGTH);
        }

        if (strcmp(currentCommand, "ShowAvailability") == 0) {
          if (fetchRoomStatus(wing)) {
            sendAvailabilityDataSPI(wing.roomAvailability, TOTAL_ROOMS);
          }
        }
      }
    }
    delay(100);
  }
}

// ===== SPI Communication =====
void sendCommandSPI(uint8_t commandID) {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SPI_SS, LOW);
  
  SPI.transfer(0xAA);
  SPI.transfer(commandID);
  SPI.transfer(0x55);
  
  digitalWrite(PIN_SPI_SS, HIGH);
  SPI.endTransaction();
  delayMicroseconds(50);
}

void sendAvailabilityDataSPI(const uint8_t* data, size_t dataSize) {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SPI_SS, LOW);
  
  // Send header
  SPI.transfer16(CMD_SHOWAVAILABILITY);
  SPI.transfer16(dataSize);

  // Send data in chunks
  constexpr size_t CHUNK_SIZE = 32;
  for (size_t i = 0; i < dataSize; i += CHUNK_SIZE) {
    size_t remaining = min(CHUNK_SIZE, dataSize - i);
    SPI.transferBytes(&data[i], nullptr, remaining);
    delayMicroseconds(100);
  }

  digitalWrite(PIN_SPI_SS, HIGH);
  SPI.endTransaction();
}

// ===== Room Status Handling =====
bool fetchRoomStatus(WingInfo& wing) {
  HTTPClient http;
  String url = String(BASE_URL) + "getRoomStatus.php?wing_id=" + wing.wingId;
  
  if (!http.begin(url) || http.GET() != HTTP_CODE_OK) {
    logError("Failed to fetch room status");
    return false;
  }

  DynamicJsonDocument doc(ROOM_STATUS_DOC_SIZE);
  if (deserializeJson(doc, http.getString())) {
    logError("Room status JSON error");
    return false;
  }

  size_t index = 0;
  for (JsonArray floor : doc.as<JsonArray>()) {
    for (JsonVariant room : floor) {
      if (index < TOTAL_ROOMS) {
        wing.roomAvailability[index++] = room.as<uint8_t>();
      }
    }
  }
  return true;
}