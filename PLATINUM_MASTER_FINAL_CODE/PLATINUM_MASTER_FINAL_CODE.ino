/********************************************
 * ESP32 Master Code (Stripped of Old Commands)
 * and Updated with New Commands
 ********************************************/
#include <WiFiManager.h>  // Add this at the top with other includes
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

// ===== Logging Macros (Optional) =====
#define LOG_LEVEL_INFO
#ifdef LOG_LEVEL_INFO
#define LOG_INFO(x) Serial.println("[INFO] " + String(x))
#define LOG_DEBUG(x) Serial.println("[DEBUG] " + String(x))
#define LOG_ERROR(x) Serial.println("[ERROR] " + String(x))
#else
#define LOG_INFO(x)
#define LOG_DEBUG(x)
#define LOG_ERROR(x)
#endif

// ===== Wi-Fi / API =====
const char* ssid        = "Vista";
const char* password    = "Vista@321";
// Adjust baseUrl / apiEndpoint if needed:
const char* baseUrl     = "https://admin.modelsofbrainwing.com/";
const char* apiEndpoint = "get_led_control.php";

// Adjust projectId if needed (from your new commands table, you may want 17)
const int projectId     = 9;  

// ===== I2C Address(es) =====
// From your table, all new commands appear to use slave address 0x09 (decimal 9).
// If you have only one slave, you can do:
#define SLAVE_ADDRESS 0x09

// ===== New Command IDs =====
// From your list:
//  ID | Name                                  | Wing? | I2C=9 | Project=17
// --------------------------------------------------------------
//  55 | Terrace                               |   1   |   9   |     1  (*Note your table says project=1, if still needed adjust.)
//  65 | TurnOffLeds                           |   0   |   9   |     17
//  66 | VerticalRoomsChasingLed               |   1   |   9   |     17
//  67 | RoomVariations9To18Floor              |   0   |   9   |     17
//  68 | RefugeeFloors                         |   0   |   9   |     17
//  69 | VerticalRoomsFloorWiseRoom1And2       |   0   |   9   |     17
//  70 | VerticalRoomsFloorWiseRoom3And4       |   0   |   9   |     17
//  71 | Floor3Control                         |   0   |   9   |     17
//  72 | Floor45678Control                     |   0   |   9   |     17
//  73 | ShowAvailability                      |   0   |   9   |     17
//  74 | 3BHK                                  |   0   |   9   |     17

// New command definitions based on your table:
#define CMD_VERTICALROOMSCHASINGLED           66
#define CMD_ROOMVARIATIONS9TO11FLOOR          67
#define CMD_REFUGEEFLOORS1                    68
#define CMD_VERTICALROOMSFLOORWISEROOM1AND2     69
#define CMD_VERTICALROOMSFLOORWISEROOM3AND4     70
#define CMD_FLOOR3CONTROL                     71
#define CMD_FLOOR45678CONTROL                 72
#define CMD_ALLLIGHT                          73
#define CMD_ROOMVARIATIONS12TO15FLOOR         74
#define CMD_ROOMVARIATIONS16TO18FLOOR         75
#define CMD_REFUGEEFLOORS2                    76
#define CMD_SHOWAVAILABILITY                  78
#define CMD_2BHK                              79
#define CMD_3BHK                              80
#define CMD_PATTERN                           81
#define CMD_TurnOfAllLights                   82


// ===== Building / Room Constants (only needed if ShowAvailability is used) =====
#define NUM_FLOORS 16
#define NUM_ROOMS_PER_FLOOR 4
#define TOTAL_ROOMS (NUM_FLOORS * NUM_ROOMS_PER_FLOOR)

// ===== Data Structure (if you still have multiple "wings," you can keep or remove) =====
struct WingInfo {
  int wingId;
  uint8_t slaveAddress;
  String activeControl;
  String lastSentControl; 
  uint8_t roomAvailability[TOTAL_ROOMS];
};

#define NUM_WINGS 1  // If you only have 1 wing/device on address 0x09 now
WingInfo wings[NUM_WINGS];

// We'll store "common" commands in a separate global
String commonControl = "";

// ===== FreeRTOS / Concurrency =====
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t serialMutex;
TaskHandle_t TaskPollAPI;
TaskHandle_t TaskLEDControl;

// ===== Function Declarations =====
void setupWiFi();
void pollAPITask(void* parameter);
void pollCommonCommands();
void ledControlTask(void* parameter);

// I2C Helpers
void sendCommand(uint8_t slaveAddress, uint8_t commandID);
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvailability, size_t dataSize);
void runPatternAvailableRooms(WingInfo& wing);

void setup() {
  LOG_INFO("Entering setup...");
  Serial.begin(115200);
  delay(1000);

  wifiMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();

  if (!wifiMutex || !serialMutex) {
    LOG_ERROR("Failed to create mutexes!");
    while (1);
  }

  // Example pins for relays
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(8, OUTPUT);

  // Ensure relays start in a known (OFF) state
  digitalWrite(10, LOW);
  digitalWrite(9, LOW);
  digitalWrite(8, LOW);

  setupWiFi();

  // I2C init
  Wire.begin(11, 12);       // Adjust pins if needed
  Wire.setClock(400000UL);  // 400kHz
  LOG_INFO("I2C initialized");

  // If only 1 device:
  wings[0] = { 
    17,                // wingId (if used) 
    SLAVE_ADDRESS,    // i2c address 0x09
    "",               // activeControl
    "",               // lastSentControl
    { 0 }             // roomAvailability
  };

  // Create tasks
  xTaskCreatePinnedToCore(
    pollAPITask,
    "Poll API Task",
    8192,
    NULL,
    1,
    &TaskPollAPI,
    0);

  xTaskCreatePinnedToCore(
    ledControlTask,
    "LED Control Task",
    8192,
    NULL,
    1,
    &TaskLEDControl,
    1);

  LOG_INFO("Setup complete");
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}

// ----------------------------------------------------
//                Wi-Fi Setup
// ----------------------------------------------------
void setupWiFi() {
  WiFi.begin(ssid, password);
  LOG_INFO("Connecting to Wi-Fi...");

  int maxRetries = 20, retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    retries++;
    LOG_DEBUG("Wi-Fi attempt " + String(retries));
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG_INFO("Wi-Fi Connected: IP " + WiFi.localIP().toString());
  } else {
    LOG_ERROR("Failed to connect to Wi-Fi.");
  }
}

// ----------------------------------------------------
//             pollAPITask (Wing + Common)
// ----------------------------------------------------
void pollAPITask(void* parameter) {
  int retryCount = 0;
  const int maxRetries = 5;

  while (true) {
    // 1) Poll "common" commands (wing_id=0 or 1 as you require)
    pollCommonCommands();

    // 2) Poll each wing/device
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo& wing = wings[i];

      if (WiFi.status() != WL_CONNECTED) {
        LOG_ERROR("Wi-Fi not connected; reconnecting...");
        setupWiFi();
        continue;
      }

      HTTPClient http;
      // E.g. GET /get_led_control.php?project_id=17&wing_id=1
      String url = String(baseUrl) + apiEndpoint
                   + "?project_id=" + String(projectId)
                   + "&wing_id=" + String(wing.wingId);

      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          if (doc.containsKey("control")) {
            String newCommand = doc["control"].as<String>();

            // Only update if changed
            if (newCommand != wing.activeControl) {
              wing.activeControl = newCommand;
              LOG_INFO("Wing " + String(wing.wingId) + " => " + wing.activeControl);
            }
            retryCount = 0;
          } else if (doc.containsKey("error")) {
            LOG_ERROR("Wing " + String(wing.wingId) + " => " + doc["error"].as<String>());
          } else {
            LOG_ERROR("Wing " + String(wing.wingId) + " => Unexpected response");
          }
        } else {
          LOG_ERROR("JSON parse error (Wing " + String(wing.wingId) + "): " + String(error.c_str()));
        }
      } else {
        LOG_ERROR("HTTP GET error (Wing " + String(wing.wingId) + "): " + http.errorToString(httpCode));
        retryCount++;
        if (retryCount > maxRetries) {
          LOG_ERROR("Max retries. Skipping poll for now.");
          retryCount = 0;
        } else {
          int backoff = pow(2, retryCount) * 1000;
          vTaskDelay(backoff / portTICK_PERIOD_MS);
          http.end();
          continue;
        }
      }
      http.end();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Delay before next cycle
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// ----------------------------------------------------
//  pollCommonCommands => e.g. wing_id=0 (adjust if needed)
// ----------------------------------------------------
void pollCommonCommands() {
  if (WiFi.status() != WL_CONNECTED) return;
  LOG_INFO("Starting Polling Common Commands");
  HTTPClient http;

  // If your "common" is using wing_id=0 (some tables say so), do that here:
  String url = String(baseUrl) + apiEndpoint
               + "?project_id=" + String(projectId)
               + "&wing_id=1";  
  http.begin(url);
  LOG_INFO("COMMON COMMANDS API URL => " + url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    LOG_INFO("Common Commands => " + payload);

    if (!error) {
      if (doc.containsKey("control")) {
        commonControl = doc["control"].as<String>();

        // Relay control example (adjust as you wish):
        if(commonControl == "AllLight") {
          digitalWrite(10, LOW);
          LOG_INFO("Relay on Pin 10 turned ON");
        }
        else{
          digitalWrite(10, HIGH);
          LOG_INFO("Relay on Pin 10 turned OFF");
        }
        // etc.

        LOG_INFO("CommonControl => " + commonControl);
      } else if (doc.containsKey("error")) {
        // Possibly turn all relays off
        digitalWrite(8, HIGH);
        digitalWrite(9, HIGH);
        digitalWrite(10, HIGH);

        LOG_ERROR("Common => " + doc["error"].as<String>());
      }
    } else {
      LOG_ERROR("JSON parse error (common): " + String(error.c_str()));
    }
  } else {
    LOG_ERROR("HTTP error (common): " + http.errorToString(httpCode));
  }
  http.end();
}

// ----------------------------------------------------
//            ledControlTask => interpret new commands
// ----------------------------------------------------
void ledControlTask(void* parameter) {
  while (true) {
    // Process common command (if any) – for example, if the common control is "AllLight"
    // Process wing-specific commands.
    // Loop through all wings; in this example, we assume one wing.
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo &wing = wings[i];
      String ctrl = wing.activeControl;

      // Process new command only if changed
      if (ctrl != "" && ctrl != wing.lastSentControl) {
        if (ctrl == "VerticalRoomsChasingLed") {
          sendCommand(wing.slaveAddress, CMD_VERTICALROOMSCHASINGLED);
        }
        else if (ctrl == "RoomVariations9To11Floor") {
          sendCommand(wing.slaveAddress, CMD_ROOMVARIATIONS9TO11FLOOR);
        }
        else if (ctrl == "RefugeeFloors1") {
          sendCommand(wing.slaveAddress, CMD_REFUGEEFLOORS1);
        }
        else if (ctrl == "VerticalRoomsFloorWiseRoom1And2") {
          sendCommand(wing.slaveAddress, CMD_VERTICALROOMSFLOORWISEROOM1AND2);
        }
        else if (ctrl == "VerticalRoomsFloorWiseRoom3And4") {
          sendCommand(wing.slaveAddress, CMD_VERTICALROOMSFLOORWISEROOM3AND4);
        }
        else if (ctrl == "Floor3Control") {
          sendCommand(wing.slaveAddress, CMD_FLOOR3CONTROL);
        }
        else if (ctrl == "Floor45678Control") {
          sendCommand(wing.slaveAddress, CMD_FLOOR45678CONTROL);
        }
        else if (ctrl == "RoomVariations12To15Floor") {
          sendCommand(wing.slaveAddress, CMD_ROOMVARIATIONS12TO15FLOOR);
        }
        else if (ctrl == "RoomVariations16To18Floor") {
          sendCommand(wing.slaveAddress, CMD_ROOMVARIATIONS16TO18FLOOR);
        }
        else if (ctrl == "RefugeeFloors2") {
          sendCommand(wing.slaveAddress, CMD_REFUGEEFLOORS2);
        }
        else if (ctrl == "ShowAvailability") {
          runPatternAvailableRooms(wing);  // fetch & send availability data
        }
        else if (ctrl == "2BHK") {
          sendCommand(wing.slaveAddress, CMD_2BHK);
        }
        else if (ctrl == "3BHK") {
          sendCommand(wing.slaveAddress, CMD_3BHK);
        }
        else if (ctrl == "Pattern") {
          sendCommand(wing.slaveAddress, CMD_PATTERN);
        }
        else if (ctrl == "TurnOfAllLights") {
          sendCommand(wing.slaveAddress, CMD_TurnOfAllLights);
        }
        else {
          Serial.println("Unknown command for Wing " + String(wing.wingId) + ": " + ctrl);
        }
        // Update last sent control
        wing.lastSentControl = ctrl;
      }
      Serial.println("Wing " + String(wing.wingId) + " => " + ctrl);
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    // Small delay between cycles
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}


void sendCommand(uint8_t slaveAddress, uint8_t commandID) {
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);          // Start byte
  Wire.write(commandID);     // The actual command
  int res = Wire.endTransmission();
  Serial.println(res);
  delay(10);

  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);          // End byte
  res = Wire.endTransmission();
  Serial.println(res);
}

// ----------------------------------------------------
//  Send Availability Data in Chunks (Only if you need it)
// ----------------------------------------------------
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvail, size_t dataSize) {
  const size_t maxChunkSize = 27;
  size_t bytesSent = 0;

  Serial.println("Sending Availability => Slave 0x" + String(slaveAddress, HEX));

  // Start
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);
  Wire.write(CMD_SHOWAVAILABILITY);
  Wire.write((uint8_t)(dataSize >> 8));
  Wire.write((uint8_t)(dataSize & 0xFF));
  Wire.endTransmission();
  vTaskDelay(2 / portTICK_PERIOD_MS);

  // Chunks
  while (bytesSent < dataSize) {
    size_t chunkSize = min(maxChunkSize, dataSize - bytesSent);

    Wire.beginTransmission(slaveAddress);
    Wire.write(0xAB);
    Wire.write((uint8_t)(bytesSent >> 8));
    Wire.write((uint8_t)(bytesSent & 0xFF));
    Wire.write((uint8_t)chunkSize);

    // payload
    Wire.write(&roomAvail[bytesSent], chunkSize);

    // checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < chunkSize; i++) {
      checksum += roomAvail[bytesSent + i];
    }
    Wire.write(checksum);

    Wire.endTransmission();
    bytesSent += chunkSize;
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }

  // End
  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);
  Wire.endTransmission();

  Serial.println("Availability data sent => 0x" + String(slaveAddress, HEX));
}

// ----------------------------------------------------
//  runPatternAvailableRooms => fetch & send chunk
// ----------------------------------------------------
void runPatternAvailableRooms(WingInfo& wing) {
  Serial.println("Fetching availability => Wing " + String(wing.wingId));

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No Wi-Fi => reconnecting...");
    setupWiFi();
    return;
  }

  // Example endpoint if you have a getRoomStatus for availability
  HTTPClient http;
  String url = String(baseUrl) + "getRoomStatus.php?wing_id=" + String(wing.wingId);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode <= 0) {
    Serial.println("HTTP error => " + http.errorToString(httpCode));
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("No room status payload...");
    return;
  }

  // parse JSON
  StaticJsonDocument<16384> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("JSON parse error => " + String(err.c_str()));
    return;
  }

  size_t index = 0;
  for (size_t floor = 0; floor < doc.size(); floor++) {
    JsonArray arr = doc[floor];
    for (size_t room = 0; room < arr.size(); room++) {
      int status = arr[room].as<int>();
      wing.roomAvailability[index++] = (uint8_t)status;
    }
  }

  // Now send chunked data
  sendAvailabilityData(wing.slaveAddress, wing.roomAvailability, TOTAL_ROOMS);
  vTaskDelay(200 / portTICK_PERIOD_MS);
}
