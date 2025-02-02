/********************************************
 * Revised ESP32 Master Code
 * Updated to Handle Latest Controls
 * 
 * Author: Your Name
 * Date:   2025-01-06
 ********************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

// ===== BEGIN: Logging Utility (Optional) =====
// If you don't have or don't use LogUtility.h, you can remove or replace these macros.
#define LOG_LEVEL_INFO
#ifdef LOG_LEVEL_INFO
  #define LOG_INFO(x)   Serial.println("[INFO] " + String(x))
  #define LOG_DEBUG(x)  Serial.println("[DEBUG] " + String(x))
  #define LOG_ERROR(x)  Serial.println("[ERROR] " + String(x))
#else
  #define LOG_INFO(x)
  #define LOG_DEBUG(x)
  #define LOG_ERROR(x)
#endif
// ===== END: Logging Utility (Optional) =====

// ===== Wi-Fi and API Config =====
const char* ssid        = "MyProject";     // Replace with your Wi-Fi SSID
const char* password    = "12345678";      // Replace with your Wi-Fi Password
const char* baseUrl     = "https://admin.modelsofbrainwing.com/";
const char* apiEndpoint = "get_led_control.php"; // The endpoint returning { "control": "..." }
const int   projectId   = 8;               // Your Project ID

// ===== I²C Addresses =====
#define SLAVE_ADDRESS_WING_14  0x08
#define SLAVE_ADDRESS_WING_15  0x09
#define SLAVE_ADDRESS_WING_16  0x0A

// ===== Example Command IDs (Update as needed) =====
// Global/Generic commands
#define CMD_TURN_OFF_ALL         0xA0
#define CMD_TURN_ON_ALL          0xA1
#define CMD_ACTIVATE_PATTERNS    0xA2
#define CMD_CONTROL_ALL_TOWERS   0xA3
#define CMD_VIEW_AVAILABLE_FLATS 0xA4
#define CMD_FILTER_1BHK          0xA5
#define CMD_FILTER_2BHK          0xA6
#define CMD_FILTER_3BHK          0xA7

// Wing A (ID 14) commands
#define CMD_WINGA_2BHK_689_SQFT  0xB0
#define CMD_WINGA_1BHK_458_SQFT  0xB1
#define CMD_WINGA_1BHK_461_SQFT  0xB2

// Wing B (ID 15) commands
#define CMD_WINGB_2BHK_689_SQFT  0xB3
#define CMD_WINGB_1BHK_458_SQFT  0xB4
#define CMD_WINGB_1BHK_461_SQFT  0xB5

// Wing C (ID 16) commands
#define CMD_WINGC_2BHK_696_SQFT      0xC0
#define CMD_WINGC_2BHK_731_SQFT      0xC1
#define CMD_WINGC_3BHK_924_SQFT      0xC2
#define CMD_WINGC_3BHK_959_SQFT      0xC3
#define CMD_WINGC_3BHK_1931_SQFT     0xC4
#define CMD_WINGC_3BHK_1972_SQFT     0xC5
#define CMD_WINGC_3BHK_2931_SQFT     0xC6
#define CMD_WINGC_3BHK_2972_SQFT     0xC7

// For "ViewAvailableFlats" usage
#define CMD_SHOW_AVAIL           0xC8

// ===== Building / Room Constants =====
#define NUM_FLOORS         35
#define NUM_ROOMS_PER_FLOOR 8
#define TOTAL_ROOMS        (NUM_FLOORS * NUM_ROOMS_PER_FLOOR) // 35*8 = 280 (as example)
String commonControl = "";

// ===== Data Structure for Each Wing =====
struct WingInfo {
  int wingId;                 // e.g., 14, 15, 16
  uint8_t slaveAddress;       // I²C address
  String activeControl;       // e.g., "WingA2BHK689sqft"
  uint8_t roomAvailability[TOTAL_ROOMS];
};

// We have 3 wings in total
#define NUM_WINGS 3
WingInfo wings[NUM_WINGS];

// ===== FreeRTOS-Related Objects =====
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t serialMutex;
TaskHandle_t TaskPollAPI;
TaskHandle_t TaskLEDControl;

// ===== Function Declarations =====
void setupWiFi();
void pollAPITask(void* parameter);
void ledControlTask(void* parameter);
void runPatternAvailableRooms(WingInfo& wing);

// I2C Communication Helpers
void sendCommand(uint8_t slaveAddress, uint8_t commandID);
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvailability, size_t dataSize);

// ===== SETUP =====
void setup() {
  LOG_INFO("Entering setup...");

  // Serial init
  Serial.begin(115200);
  delay(1000);
  LOG_INFO("Serial initialized");

  // Create mutexes
  wifiMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();

  if (wifiMutex == NULL || serialMutex == NULL) {
    LOG_ERROR("Failed to create mutexes!");
    while (1) { /* halt */ }
  }
  LOG_INFO("Mutexes created successfully");

  // Connect to Wi-Fi
  setupWiFi();
  LOG_INFO("Wi-Fi setup process started");

  // Initialize I2
  Wire.begin(11,12);       // SDA=11, SCL=12 (adjust for your hardware)
  Wire.setClock(400000UL);  // 400kHz I2C speed
  LOG_INFO("I2C initialized (SDA=11, SCL=12, 400kHz)");

  // Initialize WingInfo objects
  wings[0] = {14, SLAVE_ADDRESS_WING_14, "", {0}};
  wings[1] = {15, SLAVE_ADDRESS_WING_15, "", {0}};
  wings[2] = {16, SLAVE_ADDRESS_WING_16, "", {0}};
  LOG_INFO("Wings initialized");

  // Create tasks
  xTaskCreatePinnedToCore(
    pollAPITask,
    "Poll API Task",
    8192,   // Stack size
    NULL,
    1,      // Priority
    &TaskPollAPI,
    0);     // Run on core 0
  LOG_INFO("Poll API Task created");

  xTaskCreatePinnedToCore(
    ledControlTask,
    "LED Control Task",
    8192,
    NULL,
    1,
    &TaskLEDControl,
    1);     // Run on core 1
  LOG_INFO("LED Control Task created");

  LOG_INFO("Setup complete");
}

// ===== LOOP =====
void loop() {
  // Nothing here since we're using FreeRTOS tasks
  vTaskDelay(portMAX_DELAY);
}

// ----------------------------------------------------
//                     Wi-Fi Setup
// ----------------------------------------------------
void setupWiFi() {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    LOG_DEBUG("Connecting to Wi-Fi...");
    xSemaphoreGive(serialMutex);
  }

  WiFi.begin(ssid, password);

  int maxRetries = 20;
  int retries = 0;

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    LOG_DEBUG("Wi-Fi connection attempt " + String(retries + 1) + " of " + String(maxRetries));
    retries++;

    if (retries >= maxRetries) {
      LOG_ERROR("Failed to connect to Wi-Fi after " + String(maxRetries) + " attempts.");
      return; // or reset, or do something else
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG_INFO("Wi-Fi Connected, IP: " + WiFi.localIP().toString());
  }
}

// ----------------------------------------------------
//                   Poll API Task
// ----------------------------------------------------
void pollAPITask(void* parameter) {
  int retryCount = 0;
  const int maxRetries = 5;

  while (true) {
    // Poll each wing for its command
    for (int i = 0; i < NUM_WINGS; i++) {
      pollCommonCommands();
      WingInfo& wing = wings[i];

      // If Wi-Fi is connected, fetch from the API
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(baseUrl) + String(apiEndpoint) 
                   + "?project_id=" + String(projectId)
                   + "&wing_id=" + String(wing.wingId);
        http.begin(url);

        if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
          Serial.println("API URL for Wing " + String(wing.wingId) + ": " + url);
          xSemaphoreGive(serialMutex);
        }

        int httpCode = http.GET();

        if (httpCode > 0) {
          // Successful GET
          String payload = http.getString();

          if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
            Serial.println("Received payload for Wing " + String(wing.wingId) + ": " + payload);
            xSemaphoreGive(serialMutex);
          }

          StaticJsonDocument<512> doc; // Adjust size if needed
          DeserializationError error = deserializeJson(doc, payload);

          if (!error) {
            if (doc.containsKey("control")) {
              String control = doc["control"].as<String>();
              wing.activeControl = control; // Store the new control

              if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
                Serial.println("Wing " + String(wing.wingId) + " active control updated: " + control);
                xSemaphoreGive(serialMutex);
              }

              retryCount = 0;
            }
            else if (doc.containsKey("error")) {
              if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
                Serial.println("API Error for Wing " + String(wing.wingId) + ": " + doc["error"].as<String>());
                xSemaphoreGive(serialMutex);
              }
            }
            else {
              if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
                Serial.println("Unexpected API response for Wing " + String(wing.wingId));
                xSemaphoreGive(serialMutex);
              }
            }
          } else {
            if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
              Serial.println("JSON Parsing Error for Wing " + String(wing.wingId) + ": " + String(error.c_str()));
              xSemaphoreGive(serialMutex);
            }
          }
        } 
        else {
          // HTTP error
          if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
            Serial.println("HTTP GET Error for Wing " + String(wing.wingId) + ": " + http.errorToString(httpCode));
            xSemaphoreGive(serialMutex);
          }
          retryCount++;

          if (retryCount > maxRetries) {
            if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
              Serial.println("Max retries reached. Skipping this poll.");
              xSemaphoreGive(serialMutex);
            }
            retryCount = 0;
          } else {
            int delayTime = pow(2, retryCount) * 1000;  // Exponential backoff
            if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
              Serial.println("Retrying in " + String(delayTime / 1000) + " seconds...");
              xSemaphoreGive(serialMutex);
            }
            vTaskDelay(delayTime / portTICK_PERIOD_MS);
            continue;
          }
        }

        http.end();
      } 
      else {
        // Wi-Fi not connected
        if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
          Serial.println("Wi-Fi not connected. Attempting to reconnect...");
          xSemaphoreGive(serialMutex);
        }
        setupWiFi();
      }

      // Short delay between wing polls
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Delay before next full loop of all wings
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}
// ----------------------------------------------------
//  pollCommonCommands => wing_id=0
// ----------------------------------------------------
void pollCommonCommands() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(baseUrl) + apiEndpoint
             + "?project_id=" + String(projectId)
             + "&wing_id=0";  // Here is the magic: get "common" commands
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      if (doc.containsKey("control")) {
        commonControl = doc["control"].as<String>();
        LOG_INFO("CommonControl => " + commonControl);
      } else if (doc.containsKey("error")) {
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
//                   LED Control Task
// ----------------------------------------------------
void ledControlTask(void* parameter) {
  while (true) {
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo& wing = wings[i];
      String currentControl = wing.activeControl;

      if (currentControl == "OFF") {
        sendCommand(wing.slaveAddress, CMD_TURN_OFF_ALL);
      }
      else if (currentControl == "TurnOnAllLights") {
        sendCommand(wing.slaveAddress, CMD_TURN_ON_ALL);
      }
      else if (currentControl == "ActivatePatterns") {
        sendCommand(wing.slaveAddress, CMD_ACTIVATE_PATTERNS);
      }
      else if (currentControl == "ControlAllTowers") {
        sendCommand(wing.slaveAddress, CMD_CONTROL_ALL_TOWERS);
      }
      else if (currentControl == "ViewAvailableFlats") {
        // This fetches room availability from server & sends chunked data
        runPatternAvailableRooms(wing);
      }
      else if (currentControl == "Filter1BHK") {
        sendCommand(wing.slaveAddress, CMD_FILTER_1BHK);
      }
      else if (currentControl == "Filter2BHK") {
        sendCommand(wing.slaveAddress, CMD_FILTER_2BHK);
      }
      else if (currentControl == "Filter3BHK") {
        sendCommand(wing.slaveAddress, CMD_FILTER_3BHK);
      }

      // ================= Wing A (ID=14) =================
      else if (wing.wingId == 14 && currentControl == "WingA2BHK689sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGA_2BHK_689_SQFT);
      }
      else if (wing.wingId == 14 && currentControl == "WingA1BHK458sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGA_1BHK_458_SQFT);
      }
      else if (wing.wingId == 14 && currentControl == "WingA1BHK461sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGA_1BHK_461_SQFT);
      }
      // (If you still need to handle this older DB row #43)
      else if (wing.wingId == 14 && currentControl == "1 BHK1") {
        // Possibly treat it as Filter1BHK or something similar
        sendCommand(wing.slaveAddress, CMD_FILTER_1BHK);
      }

      // ================= Wing B (ID=15) =================
      else if (wing.wingId == 15 && currentControl == "WingB2BHK689sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGB_2BHK_689_SQFT);
      }
      else if (wing.wingId == 15 && currentControl == "WingB1BHK458sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGB_1BHK_458_SQFT);
      }
      else if (wing.wingId == 15 && currentControl == "WingB1BHK461sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGB_1BHK_461_SQFT);
      }

      // ================= Wing C (ID=16) =================
      else if (wing.wingId == 16 && currentControl == "WingC2BHK696sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_2BHK_696_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC2BHK731sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_2BHK_731_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC3BHK924sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_924_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC3BHK959sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_959_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC3BHKType1931sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_1931_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC3BHKType1972sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_1972_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC3BHKType2931sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_2931_SQFT);
      }
      else if (wing.wingId == 16 && currentControl == "WingC3BHKType2972sqft") {
        sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_2972_SQFT);
      }
      else {
        // Catch any unrecognized commands
        if (currentControl != "") {
          Serial.println("Unknown command for Wing " + String(wing.wingId) + ": " + currentControl);
        }
      }

      // Debug log
      Serial.println("Wing " + String(wing.wingId) + " handled control: " + currentControl);

      // Short delay between each wing
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Delay before repeating
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ----------------------------------------------------
//          I2C: Send Single Command Function
// ----------------------------------------------------
void sendCommand(uint8_t slaveAddress, uint8_t commandID) {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.print("Sending Command ID: 0x");
    Serial.print(commandID, HEX);
    Serial.print(" to Slave Address: 0x");
    Serial.println(slaveAddress, HEX);
    xSemaphoreGive(serialMutex);
  }

  // Start packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);       // Start byte
  Wire.write(commandID);  // Our command
  int res = Wire.endTransmission();
  Serial.println(res);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // End packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);       // End byte
  res = Wire.endTransmission();
  Serial.println(res);
}

// ----------------------------------------------------
//       I2C: Send Chunked Availability Data
// ----------------------------------------------------
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvailability, size_t dataSize) {
  const size_t maxChunkSize = 27;  // Max data in one packet
  size_t bytesSent = 0;

  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Sending Availability Data to Slave: 0x" + String(slaveAddress, HEX));
    xSemaphoreGive(serialMutex);
  }

  // Start of transmission
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);                         // Start byte
  Wire.write(CMD_SHOW_AVAIL);              // Command ID for "show availability"
  Wire.write((uint8_t)(dataSize >> 8));    // High byte of total size
  Wire.write((uint8_t)(dataSize & 0xFF));  // Low byte of total size
  Wire.endTransmission();

  vTaskDelay(2 / portTICK_PERIOD_MS);

  // Send data in chunks
  while (bytesSent < dataSize) {
    size_t chunkSize = min(maxChunkSize, dataSize - bytesSent);

    Wire.beginTransmission(slaveAddress);
    Wire.write(0xAB);  // Data chunk start byte
    Wire.write((uint8_t)(bytesSent >> 8));     // Offset high
    Wire.write((uint8_t)(bytesSent & 0xFF));   // Offset low
    Wire.write((uint8_t)chunkSize);            // Chunk length

    // Payload
    Wire.write(&roomAvailability[bytesSent], chunkSize);

    // Checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < chunkSize; i++) {
      checksum += roomAvailability[bytesSent + i];
    }
    Wire.write(checksum);

    Wire.endTransmission();

    bytesSent += chunkSize;
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }

  // End packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);  // End byte
  Wire.endTransmission();

  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Availability data sent successfully to Slave: 0x" + String(slaveAddress, HEX));
    xSemaphoreGive(serialMutex);
  }
}

// ----------------------------------------------------
//       Run Pattern for "ViewAvailableFlats"
//       Fetches JSON from server, sends chunked data
// ----------------------------------------------------
void runPatternAvailableRooms(WingInfo& wing) {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Running: ViewAvailableFlats for Wing " + String(wing.wingId));
    xSemaphoreGive(serialMutex);
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Example endpoint: getRoomStatus.php?wing_id=XX
    String roomStatusEndpoint = "getRoomStatus.php?wing_id=" + String(wing.wingId);
    String url = String(baseUrl) + roomStatusEndpoint;
    http.begin(url);

    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println("Room Status Endpoint for Wing " + String(wing.wingId) + ": " + url);
      xSemaphoreGive(serialMutex);
    }

    int httpCode = http.GET();
    String payload = "";

    if (httpCode > 0) {
      payload = http.getString();

      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("Received payload length: " + String(payload.length()));
        // Print snippet of the payload
        Serial.println("Payload snippet: " + payload.substring(0, min((int)payload.length(), 100)));
        xSemaphoreGive(serialMutex);
      }
    } else {
      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("HTTP GET Error: " + String(http.errorToString(httpCode)));
        xSemaphoreGive(serialMutex);
      }
      http.end();
      return; 
    }

    http.end();

    // Parse JSON if we have a payload
    if (payload.length() > 0) {
      StaticJsonDocument<16384> doc;  // Adjust if needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // Expecting a 2D array of floors/rooms
        size_t index = 0;
        for (size_t floor = 0; floor < doc.size(); floor++) {
          JsonArray floorArray = doc[floor];
          for (size_t room = 0; room < floorArray.size(); room++) {
            int status = floorArray[room].as<int>();
            wing.roomAvailability[index++] = (uint8_t)status;
          }
        }

        // Now send the data to the slave
        sendAvailabilityData(wing.slaveAddress, wing.roomAvailability, TOTAL_ROOMS);
      }
      else {
        if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
          Serial.println("JSON Parsing Error: " + String(error.c_str()));
          xSemaphoreGive(serialMutex);
        }
      }
    } else {
      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("No payload received from Room Status API.");
        xSemaphoreGive(serialMutex);
      }
    }
  }
  else {
    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println("Wi-Fi not connected; reattempting Wi-Fi connection...");
      xSemaphoreGive(serialMutex);
    }
    setupWiFi();
  }

  // A small delay to let the data be sent
  vTaskDelay(200 / portTICK_PERIOD_MS);
}
