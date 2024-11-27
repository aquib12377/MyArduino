#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

// =========================
// ====== CONFIGURATION =====
// =========================

// Wi-Fi Credentials
const char* ssid = "MyProject";     // Replace with your Wi-Fi SSID
const char* password = "12345678";  // Replace with your Wi-Fi Password

const char* baseUrl = "https://admin.modelsofbrainwing.com/";

// API Endpoints
const char* apiEndpoint = "get_led_control.php";  // Endpoint to fetch control commands

// Project and Wing IDs
const int projectId = 8;  // Replace with your actual Project ID
const int wingId = 14;    // Replace with your actual Wing ID

// I²C Configuration
#define SLAVE_ADDRESS 0x08  // I²C address of the Arduino Mega

// Command IDs
#define CMD_TURN_OFF_ALL 0x00
#define CMD_TURN_ON_1BHK1 0x01
#define CMD_TURN_ON_1BHK2 0x02
#define CMD_TURN_ON_2BHK 0x03
#define CMD_SHOW_AVAIL 0x04
#define CMD_RUN_PATTERN 0x05

// Pattern IDs
#define PATTERN_ONE_BY_ONE 0x01
// Add more pattern IDs as needed

// Availability Data Size
#define NUM_FLOORS 34
#define NUM_ROOMS_PER_FLOOR 8
#define TOTAL_ROOMS (NUM_FLOORS * NUM_ROOMS_PER_FLOOR)

uint8_t roomAvailability[TOTAL_ROOMS];  // Room availability array

// =========================
// ====== GLOBALS =========
// =========================

// FreeRTOS Task Handles
TaskHandle_t TaskPollAPI;
TaskHandle_t TaskLEDControl;

// Mutex for Shared Variable
SemaphoreHandle_t controlMutex;

// Mutex for Serial Access
SemaphoreHandle_t serialMutex;

// Shared Variable
String activeControl = "OFF";  // Initialize to OFF

// Pattern Running Flags
bool IsRunningPattern1BHK1 = false;
bool IsRunningPattern1BHK2 = false;
bool IsRunningPattern2BHK = false;
bool IsRunningPatternAvailableRooms = false;
bool IsRunningPatternPatterns = false;

// =========================
// ====== FUNCTION PROTOTYPES ======
// =========================

// Wi-Fi Setup
void setupWiFi();

// API Polling Task
void pollAPITask(void* parameter);

// LED Control Task
void ledControlTask(void* parameter);

// I²C Communication Functions
void sendCommand(uint8_t commandID);
void sendCommand(uint8_t commandID, uint8_t* data, size_t dataSize);
void sendAvailabilityData();

// Reset Pattern Flags
void resetPatternFlags();

// Pattern Functions
void runPatternAvailableRooms();

// =========================
// ====== SETUP FUNCTION ======
// =========================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(230400);  // Increased baud rate
  delay(1000);           // Wait for Serial to initialize

  // Initialize Mutexes
  controlMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();

  if (controlMutex == NULL || serialMutex == NULL) {
    Serial.println("Failed to create mutexes!");
    while (1)
      ;  // Halt execution
  }

  // Connect to Wi-Fi
  setupWiFi();

  // Initialize I²C as Master
  Wire.begin(11, 12);  // SDA and SCL pins (default GPIO 21 and 22)
  Wire.setClock(400000UL);
  // Create FreeRTOS Tasks
  xTaskCreatePinnedToCore(
    pollAPITask,      // Task function
    "Poll API Task",  // Task name
    8192,             // Stack size (words)
    NULL,             // Task input parameter
    1,                // Priority
    &TaskPollAPI,     // Task handle
    0);               // Core (0 or 1)

  xTaskCreatePinnedToCore(
    ledControlTask,      // Task function
    "LED Control Task",  // Task name
    8192,                // Stack size (words)
    NULL,                // Task input parameter
    1,                   // Priority
    &TaskLEDControl,     // Task handle
    1);                  // Core (0 or 1)
}

// =========================
// ====== LOOP FUNCTION ======
// =========================
void loop() {
  // Empty loop as tasks handle functionality
  vTaskDelay(portMAX_DELAY);
}

// =========================
// ====== FUNCTION DEFINITIONS ======
// =========================

// Wi-Fi Setup
void setupWiFi() {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.print("Connecting to Wi-Fi");
    xSemaphoreGive(serialMutex);
  }

  WiFi.begin(ssid, password);

  int maxRetries = 20;
  int retries = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.print(".");
      xSemaphoreGive(serialMutex);
    }
    retries++;
    if (retries >= maxRetries) {
      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("\nFailed to connect to Wi-Fi");
        xSemaphoreGive(serialMutex);
      }
      // Optionally, implement a retry mechanism or enter a safe state
      return;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println("\nConnected to Wi-Fi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      xSemaphoreGive(serialMutex);
    }
  }
}

// API Polling Task
void pollAPITask(void* parameter) {
  int retryCount = 0;
  const int maxRetries = 5;

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String(baseUrl) + String(apiEndpoint) + "?project_id=" + String(projectId) + "&wing_id=" + String(wingId);
      http.begin(url);

      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("\nLED Control URL: " + String(url));
        xSemaphoreGive(serialMutex);
      }

      int httpCode = http.GET();

      if (httpCode > 0) {  // Check for successful request
        String payload = http.getString();

        if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
          Serial.println("Received payload length: " + String(payload.length()));
          Serial.println("Payload snippet: " + payload.substring(0, 100));  // Print first 100 characters
          xSemaphoreGive(serialMutex);
        }

        // Parse JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          if (doc.containsKey("control")) {
            String control = doc["control"].as<String>();

            // Acquire mutex before updating shared variable
            if (xSemaphoreTake(controlMutex, (TickType_t)10) == pdTRUE) {
              if (control != activeControl) {
                activeControl = control;
                if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
                  Serial.println("Active control updated: " + activeControl);
                  xSemaphoreGive(serialMutex);
                }
              }
              xSemaphoreGive(controlMutex);
            }
          } else if (doc.containsKey("error")) {
            if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
              Serial.println("API Error: " + String(doc["error"].as<const char*>()));
              xSemaphoreGive(serialMutex);
            }
          } else {
            if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
              Serial.println("Unexpected API response.");
              xSemaphoreGive(serialMutex);
            }
          }

          // Reset retry count on success
          retryCount = 0;
        } else {
          if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
            Serial.println("JSON Parsing Error: " + String(error.c_str()));
            xSemaphoreGive(serialMutex);
          }
        }
      } else {
        if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
          Serial.println("HTTP GET Error: " + String(http.errorToString(httpCode).c_str()));
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
          // Exponential Backoff
          int delayTime = pow(2, retryCount) * 1000;  // in milliseconds
          if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
            Serial.println("Retrying in " + String(delayTime / 1000) + " seconds...");
            xSemaphoreGive(serialMutex);
          }
          vTaskDelay(delayTime / portTICK_PERIOD_MS);
          continue;  // Retry immediately after delay
        }
      }

      http.end();
    } else {
      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("Wi-Fi not connected. Attempting to reconnect...");
        xSemaphoreGive(serialMutex);
      }
      setupWiFi();
    }

    // Poll every 5 seconds
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// LED Control Task
void ledControlTask(void* parameter) {
  while (true) {
    String currentControl = "OFF";  // Default to OFF

    // Acquire mutex before reading shared variable
    if (xSemaphoreTake(controlMutex, (TickType_t)10) == pdTRUE) {
      currentControl = activeControl;
      xSemaphoreGive(controlMutex);
    }

    // Reset pattern flags when control changes
    static String previousControl = "";
    if (currentControl != previousControl) {
      resetPatternFlags();
      previousControl = currentControl;
    }

    // Determine action based on currentControl
    if (currentControl == "1 BHK1" && !IsRunningPattern1BHK1) {
      sendCommand(CMD_TURN_ON_1BHK1);
      IsRunningPattern1BHK1 = true;
    } else if (currentControl == "1 BHK2" && !IsRunningPattern1BHK2) {
      sendCommand(CMD_TURN_ON_1BHK2);
      IsRunningPattern1BHK2 = true;
    } else if (currentControl == "2 BHK" && !IsRunningPattern2BHK) {
      sendCommand(CMD_TURN_ON_2BHK);
      IsRunningPattern2BHK = true;
    } else if (currentControl == "Available Rooms" && !IsRunningPatternAvailableRooms) {
      runPatternAvailableRooms();
      IsRunningPatternAvailableRooms = true;
    } else if (currentControl == "Patterns" && !IsRunningPatternPatterns) {
      uint8_t patternID = PATTERN_ONE_BY_ONE;  // Define your pattern ID
      sendCommand(CMD_RUN_PATTERN, &patternID, 1);
      IsRunningPatternPatterns = true;
    } else if (currentControl == "OFF") {
      sendCommand(CMD_TURN_OFF_ALL);
      // vTaskDelay(100 / portTICK_PERIOD_MS);  // Short delay to prevent tight loop
      // continue;
    } else {
      // Handle unknown control (optional)
      // if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      //   Serial.println("Unknown control received: " + currentControl);
      //   xSemaphoreGive(serialMutex);
      // }
    }

    // Small delay to allow other tasks to run
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Reset all pattern-running flags
void resetPatternFlags() {
  IsRunningPattern1BHK1 = false;
  IsRunningPattern1BHK2 = false;
  IsRunningPattern2BHK = false;
  IsRunningPatternAvailableRooms = false;
  IsRunningPatternPatterns = false;
}

// =========================
// ====== I²C COMMUNICATION FUNCTIONS ======
// =========================

// Overloaded function for commands without data
void sendCommand(uint8_t commandID) {
  Serial.print("Sending Command: ");
  Serial.println(commandID);
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(commandID);
  Wire.endTransmission();
}

// Overloaded function for commands with data
void sendCommand(uint8_t commandID, uint8_t* data, size_t dataSize) {
  Serial.print("Sending Command: ");
  Serial.println(commandID);
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(commandID);
  if (data != nullptr && dataSize > 0) {
    Wire.write(data, dataSize);
  }
  Wire.endTransmission();
}

// Function to send availability data over I²C
void sendAvailabilityData() {
  const size_t dataSize = TOTAL_ROOMS;
  size_t bytesSent = 0;
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(CMD_SHOW_AVAIL);
  //Wire.write(roomAvailability);
  Wire.endTransmission();
  for (int i = 0; i < TOTAL_ROOMS; i++) {
    Wire.beginTransmission(SLAVE_ADDRESS);
    Wire.write(roomAvailability[i]);
    
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Short delay between transmissions
  }
  uint8_t result = Wire.endTransmission();
    if (result != 0) {
      Serial.print("I2C Transmission Error: ");
      Serial.println(result);               // Print error code
      vTaskDelay(50 / portTICK_PERIOD_MS);  // Slightly longer delay before retrying
      return;                             // Retry sending this chunk
    }
  
}

// =========================
// ====== PATTERN FUNCTIONS ======
// =========================

// Pattern for "Available Rooms"
void runPatternAvailableRooms() {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Running Pattern: Available Rooms");
    xSemaphoreGive(serialMutex);
  }

  // Check Wi-Fi connection
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Dynamically construct the room status endpoint based on wingId
    String roomStatusEndpoint = "getRoomStatus.php?wing_id=" + String(wingId);
    String url = String(baseUrl) + roomStatusEndpoint;
    http.begin(url);

    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println("Room Status Endpoint: " + url);
      xSemaphoreGive(serialMutex);
    }

    int httpCode = http.GET();
    String payload = "";
    if (httpCode > 0) {  // Check for successful request
      payload = http.getString();

      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("Received payload length: " + String(payload.length()));
        Serial.println("Payload snippet: " + payload);  // First 100 characters
        xSemaphoreGive(serialMutex);
      }
    } else {
      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("HTTP GET Error: " + String(http.errorToString(httpCode).c_str()));
        xSemaphoreGive(serialMutex);
      }
      http.end();
      return;
    }

    http.end();

    // Parse JSON payload
    if (payload.length() > 0) {
      StaticJsonDocument<8192> doc;  // Adjust size as needed
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        // Assuming the JSON structure is a 2D array: [[floor0_rooms], [floor1_rooms], ...]
        size_t index = 0;  // Index in roomAvailability array

        for (size_t floor = 0; floor < doc.size(); floor++) {
          JsonArray floorArray = doc[floor];
          for (size_t room = 0; room < floorArray.size(); room++) {
            int status = floorArray[room].as<int>();
            roomAvailability[index++] = (uint8_t)status;  // Store directly in the roomAvailability array
            Serial.print(status);
            Serial.print(" ");
          }
          Serial.println();
        }

        // Now send the roomAvailability data over I²C
        sendAvailabilityData();

      } else {
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
      // Optionally, handle no data scenario
    }

  } else {
    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println("Wi-Fi not connected. Trying to reconnect...");
      xSemaphoreGive(serialMutex);
    }
    setupWiFi();
  }

  vTaskDelay(500 / portTICK_PERIOD_MS);
}


// =========================
// ====== END OF CODE ======
// =========================
