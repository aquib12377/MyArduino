#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
const char* ssid = "MyProject";     // Replace with your Wi-Fi SSID/NAME
const char* password = "12345678";  // Replace with your Wi-Fi Password
const char* baseUrl = "https://admin.modelsofbrainwing.com/";
const char* apiEndpoint = "get_led_control.php";  // Endpoint to fetch control commands
const int projectId = 8;  // Replace with your actual Project ID
#define SLAVE_ADDRESS_WING_14 0x08  // I²C address for Wing 14
#define SLAVE_ADDRESS_WING_15 0x09  // I²C address for Wing 15
#define SLAVE_ADDRESS_WING_16 0x0A  // I²C address for Wing 16
#define CMD_TURN_OFF_ALL 0xA0  // Command to turn off all LEDs
#define CMD_TURN_ON_1BHK 0xB1  // Command to turn on 1BHK lights
#define CMD_TURN_ON_2BHK 0xB2  // Command to turn on 2BHK lights
#define CMD_TURN_ON_3BHK 0xB3  // Command to turn on 3BHK lights
#define CMD_SHOW_AVAIL 0xC4    // Command to show room availability
#define CMD_RUN_PATTERN 0xD5   // Command to start running LED patterns
#define CMD_TURN_ON_2BHK1 0xE0
#define CMD_TURN_ON_2BHK2 0xE1
#define CMD_TURN_ON_3BHKA1 0xE2
#define CMD_TURN_ON_3BHKA2 0xE3
#define CMD_TURN_ON_3BHKB1 0xE4
#define CMD_TURN_ON_3BHKB2 0xE5
#define CMD_TURN_ON_REFUGEE 0xE6
#define CMD_TURN_ON_3BHKREFUGEE1 0xE7
#define CMD_TURN_ON_3BHKREFUGEE2 0xE8
#define PATTERN_ONE_BY_ONE 0x01
#define NUM_FLOORS 35
#define NUM_ROOMS_PER_FLOOR 8
#define TOTAL_ROOMS (NUM_FLOORS * NUM_ROOMS_PER_FLOOR)
struct WingInfo {
  int wingId;
  uint8_t slaveAddress;
  String activeControl;
  bool IsRunningPattern1BHK;
  bool IsRunningPattern2BHK;
  bool IsRunningPattern3BHK;
  bool IsRunningPatternAvailableRooms;
  bool IsRunningPatternPatterns;

  bool IsRunningPattern2BHK1;
  bool IsRunningPattern2BHK2;
  bool IsRunningPattern3BHKA1;
  bool IsRunningPattern3BHKA2;
  bool IsRunningPattern3BHKB1;
  bool IsRunningPattern3BHKB2;
  bool IsRunningPatternRefugee;
  bool IsRunningPattern3BHKRefugee1;
  bool IsRunningPattern3BHKRefugee2;
  
  uint8_t roomAvailability[TOTAL_ROOMS];
};

#define NUM_WINGS 3
WingInfo wings[NUM_WINGS];

SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t serialMutex;
TaskHandle_t TaskPollAPI;
TaskHandle_t TaskLEDControl;

void setupWiFi();
void pollAPITask(void* parameter);
void ledControlTask(void* parameter);
void sendCommand(uint8_t slaveAddress, uint8_t commandID);
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvailability, size_t dataSize);
void runPatternAvailableRooms(WingInfo& wing);
void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for Serial to initialize

  wifiMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();

  if (wifiMutex == NULL || serialMutex == NULL) {
    Serial.println(F("Failed to create mutexes!"));
    while (1);
  }

  setupWiFi();
  Wire.begin(11, 12);       // SDA and SCL pins for ESP32 (adjust if necessary)
  Wire.setClock(400000UL);  // Set I²C clock speed to 400kHz

  // Initialize wings
  wings[0] = {14, SLAVE_ADDRESS_WING_14, "OFF", false, false, false, false, false, false, false, false, false, false, false, false, false, false, {0}};
  wings[1] = {15, SLAVE_ADDRESS_WING_15, "OFF", false, false, false, false, false, false, false, false, false, false, false, false, false, false, {0}};
  wings[2] = {16, SLAVE_ADDRESS_WING_16, "OFF", false, false, false, false, false, false, false, false, false, false, false, false, false, false, {0}};

  // // Print all values of each wing
  // for (int i = 0; i < NUM_WINGS; i++) {
  //   WingInfo& wing = wings[i];
  //   Serial.println("Wing Information:");
  //   Serial.print("Wing ID: "); Serial.println(wing.wingId);
  //   Serial.print("Slave Address: 0x"); Serial.println(wing.slaveAddress, HEX);
  //   Serial.print("Active Control: "); Serial.println(wing.activeControl);
  //   Serial.print("IsRunningPattern1BHK: "); Serial.println(wing.IsRunningPattern1BHK);
  //   Serial.print("IsRunningPattern2BHK: "); Serial.println(wing.IsRunningPattern2BHK);
  //   Serial.print("IsRunningPattern3BHK: "); Serial.println(wing.IsRunningPattern3BHK);
  //   Serial.print("IsRunningPatternAvailableRooms: "); Serial.println(wing.IsRunningPatternAvailableRooms);
  //   Serial.print("IsRunningPatternPatterns: "); Serial.println(wing.IsRunningPatternPatterns);
  //   Serial.print("IsRunningPattern2BHK1: "); Serial.println(wing.IsRunningPattern2BHK1);
  //   Serial.print("IsRunningPattern2BHK2: "); Serial.println(wing.IsRunningPattern2BHK2);
  //   Serial.print("IsRunningPattern3BHKA1: "); Serial.println(wing.IsRunningPattern3BHKA1);
  //   Serial.print("IsRunningPattern3BHKA2: "); Serial.println(wing.IsRunningPattern3BHKA2);
  //   Serial.print("IsRunningPattern3BHKB1: "); Serial.println(wing.IsRunningPattern3BHKB1);
  //   Serial.print("IsRunningPattern3BHKB2: "); Serial.println(wing.IsRunningPattern3BHKB2);
  //   Serial.print("IsRunningPatternRefugee: "); Serial.println(wing.IsRunningPatternRefugee);
  //   Serial.print("IsRunningPattern3BHKRefugee1: "); Serial.println(wing.IsRunningPattern3BHKRefugee1);
  //   Serial.print("IsRunningPattern3BHKRefugee2: "); Serial.println(wing.IsRunningPattern3BHKRefugee2);
  //   Serial.println("------------------------");
  // }

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


void loop() {
  vTaskDelay(portMAX_DELAY);
}

void setupWiFi() {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.print(F("Connecting to Wi-Fi"));
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
        Serial.println(F("\nFailed to connect to Wi-Fi"));
        xSemaphoreGive(serialMutex);
      }
      return;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println(F("\nConnected to Wi-Fi"));
      Serial.print(F("IP Address: "));
      Serial.println(WiFi.localIP());
      xSemaphoreGive(serialMutex);
    }
  }
}

void pollAPITask(void* parameter) {
  int retryCount = 0;
  const int maxRetries = 5;

  while (true) {
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo& wing = wings[i];

      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        Serial.println("Wing ID: "+String(wing.wingId)+" ProjectId: "+String(projectId));
        String url = String(baseUrl) + String(apiEndpoint) + "?project_id=" + String(projectId) + "&wing_id=" + String(wing.wingId);
        http.begin(url);

        if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
          Serial.println("API URL for Wing " + String(wing.wingId) + ": " + url);
          xSemaphoreGive(serialMutex);
        }

        int httpCode = http.GET();

        if (httpCode > 0) {  // Check for successful request
          String payload = http.getString();

          if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
            Serial.println("Received payload for Wing " + String(wing.wingId) + ": " + payload);
            xSemaphoreGive(serialMutex);
          }

          StaticJsonDocument<200> doc;
          DeserializationError error = deserializeJson(doc, payload);

          if (!error) {
            if (doc.containsKey("control")) {
              String control = doc["control"].as<String>();

              wing.activeControl = control;

              if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
                Serial.println("Wing " + String(wing.wingId) + " active control updated: " + wing.activeControl);
                xSemaphoreGive(serialMutex);
              }

              retryCount = 0;
            } else if (doc.containsKey("error")) {
              if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
                Serial.println("API Error for Wing " + String(wing.wingId) + ": " + String(doc["error"].as<const char*>()));
                xSemaphoreGive(serialMutex);
              }
            } else {
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
        } else {
          if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
            Serial.println("HTTP GET Error for Wing " + String(wing.wingId) + ": " + String(http.errorToString(httpCode).c_str()));
            xSemaphoreGive(serialMutex);
          }
          retryCount++;

          if (retryCount > maxRetries) {
            if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
              Serial.println(F("Max retries reached. Skipping this poll."));
              xSemaphoreGive(serialMutex);
            }
            retryCount = 0;
          } else {
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
          Serial.println(F("Wi-Fi not connected. Attempting to reconnect..."));
          xSemaphoreGive(serialMutex);
        }
        setupWiFi();
      }

      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void ledControlTask(void* parameter) {
  while (true) {
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo& wing = wings[i];
      String currentControl = wing.activeControl;

      if (wing.wingId == 16) {
        if (currentControl == "2BHK1" && !wing.IsRunningPattern2BHK1) {
          // Reset other patterns
          wing.IsRunningPattern2BHK1 = true;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;

          sendCommand(wing.slaveAddress, CMD_TURN_ON_2BHK1);
        } else if (currentControl == "2BHK2" && !wing.IsRunningPattern2BHK2) {
          // Reset other patterns
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = true;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;

          sendCommand(wing.slaveAddress, CMD_TURN_ON_2BHK2);
        } else if (currentControl == "3BHKA1" && !wing.IsRunningPattern3BHKA1) {
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = true;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;

          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHKA1);
        } else if (currentControl == "3BHKA2" && !wing.IsRunningPattern3BHKA2) {
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = true;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          // Reset other patterns
          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHKA2);
        } else if (currentControl == "3BHKB1" && !wing.IsRunningPattern3BHKB1) {
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = true;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          // Reset other patterns
          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHKB1);
        } else if (currentControl == "3BHKB2" && !wing.IsRunningPattern3BHKB2) {
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = true;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          // Reset other patterns
          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHKB2);
        } else if (currentControl == "REFUGEE" && !wing.IsRunningPatternRefugee) {
          wing.IsRunningPatternRefugee = true;
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          // Reset other patterns
          sendCommand(wing.slaveAddress, CMD_TURN_ON_REFUGEE);
        } else if (currentControl == "3BHKREFUGEE1" && !wing.IsRunningPattern3BHKRefugee1) {
          wing.IsRunningPattern3BHKRefugee1 = true;
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          // Reset other patterns
          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHKREFUGEE1);
        } else if (currentControl == "3BHKREFUGEE2" && !wing.IsRunningPattern3BHKRefugee2) {
          wing.IsRunningPattern3BHKRefugee2 = true;
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          // Reset other patterns
          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHKREFUGEE2);
        } else if (currentControl == "OFF") {
          // Reset all patterns
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;

          sendCommand(wing.slaveAddress, CMD_TURN_OFF_ALL);
        } else if (currentControl == "Available Rooms") {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = true;
          wing.IsRunningPatternPatterns = false;
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          runPatternAvailableRooms(wing);  // Updated to pass the wing
        } else if (currentControl == "Patterns" && !wing.IsRunningPatternPatterns) {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = true;
          wing.IsRunningPattern2BHK1 = false;
          wing.IsRunningPattern2BHK2 = false;
          wing.IsRunningPattern3BHKA1 = false;
          wing.IsRunningPattern3BHKA2 = false;
          wing.IsRunningPattern3BHKB1 = false;
          wing.IsRunningPattern3BHKB2 = false;
          wing.IsRunningPatternRefugee = false;
          wing.IsRunningPattern3BHKRefugee1 = false;
          wing.IsRunningPattern3BHKRefugee2 = false;
          sendCommand(wing.slaveAddress, CMD_RUN_PATTERN);
        } else {
          Serial.println("Unknown Command for Wing C: ID > 16 | "+currentControl);
        }
      } else {
        if (currentControl == "1 BHK1" && !wing.IsRunningPattern1BHK) {
          wing.IsRunningPattern1BHK = true;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          sendCommand(wing.slaveAddress, CMD_TURN_ON_1BHK);
        } else if (currentControl == "1 BHK2" && !wing.IsRunningPattern2BHK) {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = true;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          sendCommand(wing.slaveAddress, CMD_TURN_ON_2BHK);
        } else if (currentControl == "2 BHK" && !wing.IsRunningPattern3BHK) {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = true;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          sendCommand(wing.slaveAddress, CMD_TURN_ON_3BHK);
        } else if (currentControl == "Available Rooms") {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = true;
          wing.IsRunningPatternPatterns = false;
          runPatternAvailableRooms(wing);  // Updated to pass the wing
        } else if (currentControl == "Patterns" && !wing.IsRunningPatternPatterns) {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = true;
          sendCommand(wing.slaveAddress, CMD_RUN_PATTERN);
        } else if (currentControl == "OFF") {
          wing.IsRunningPattern1BHK = false;
          wing.IsRunningPattern2BHK = false;
          wing.IsRunningPattern3BHK = false;
          wing.IsRunningPatternAvailableRooms = false;
          wing.IsRunningPatternPatterns = false;
          sendCommand(wing.slaveAddress, CMD_TURN_OFF_ALL);
        } else {
          Serial.println("Unknown Command for Wing C: ID > "+String(wing.wingId)+" | "+currentControl);
          
        }
      }

      Serial.println("Active Command for Wing: "+String(wing.wingId)+" | "+currentControl);
      
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// =========================
// ====== I²C COMMUNICATION FUNCTIONS ======
// =========================

// Function to send commands to a specific slave
void sendCommand(uint8_t slaveAddress, uint8_t commandID) {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.print("Sending Command ID: 0x");
    Serial.print(commandID, HEX);
    Serial.print(" to Slave Address: 0x");
    Serial.println(slaveAddress, HEX);
    xSemaphoreGive(serialMutex);
  }

  // Send Start of Transmission Packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);       // Start byte
  Wire.write(commandID);  // Unique Command ID
  Wire.endTransmission();

  vTaskDelay(10 / portTICK_PERIOD_MS);  // Short delay

  // Send End of Transmission Packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);  // End byte
  Wire.endTransmission();
}

// Function to send availability data over I²C
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvailability, size_t dataSize) {
  const size_t maxChunkSize = 27;  // Maximum data payload per chunk
  size_t bytesSent = 0;

  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Sending Availability Data to Slave Address: 0x" + String(slaveAddress, HEX));
    xSemaphoreGive(serialMutex);
  }

  // Send Start of Transmission Packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);                        // Start byte
  Wire.write(CMD_SHOW_AVAIL);              // Command ID
  Wire.write((uint8_t)(dataSize >> 8));    // Total data size high byte
  Wire.write((uint8_t)(dataSize & 0xFF));  // Total data size low byte
  Wire.endTransmission();

  vTaskDelay(2 / portTICK_PERIOD_MS);  // Short delay

  // Now send data in chunks
  while (bytesSent < dataSize) {
    size_t chunkSize = min((size_t)maxChunkSize, dataSize - bytesSent);

    Wire.beginTransmission(slaveAddress);
    Wire.write(0xAB);                         // Data chunk start byte
    Wire.write((uint8_t)(bytesSent >> 8));    // Offset high byte
    Wire.write((uint8_t)(bytesSent & 0xFF));  // Offset low byte
    Wire.write((uint8_t)chunkSize);           // Data length in this chunk

    // Send data payload
    Wire.write(&roomAvailability[bytesSent], chunkSize);

    // Calculate and send checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < chunkSize; i++) {
      checksum += roomAvailability[bytesSent + i];
    }
    Wire.write(checksum);

    Wire.endTransmission();

    bytesSent += chunkSize;
    vTaskDelay(2 / portTICK_PERIOD_MS);  // Short delay between chunks
  }

  // Send End of Transmission Packet
  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);  // End byte
  Wire.endTransmission();

  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Availability data sent successfully to Slave Address: 0x" + String(slaveAddress, HEX));
    xSemaphoreGive(serialMutex);
  }
}

// =========================
// ====== PATTERN FUNCTIONS ======
// =========================

// Pattern for "Available Rooms"
void runPatternAvailableRooms(WingInfo& wing) {
  if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
    Serial.println("Running Pattern: Available Rooms for Wing " + String(wing.wingId));
    xSemaphoreGive(serialMutex);
  }

  // Check Wi-Fi connection
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Dynamically construct the room status endpoint based on wingId
    String roomStatusEndpoint = "getRoomStatus.php?wing_id=" + String(wing.wingId);
    String url = String(baseUrl) + roomStatusEndpoint;
    http.begin(url);

    if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
      Serial.println("Room Status Endpoint for Wing " + String(wing.wingId) + ": " + url);
      xSemaphoreGive(serialMutex);
    }

    int httpCode = http.GET();
    String payload = "";
    if (httpCode > 0) {  // Check for successful request
      payload = http.getString();

      if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
        Serial.println("Received payload length: " + String(payload.length()));
        Serial.println("Payload snippet: " + payload.substring(0, 100));  // First 100 characters
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
      StaticJsonDocument<16384> doc;  // Adjust size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // Assuming the JSON structure is a 2D array: [[floor0_rooms], [floor1_rooms], ...]
        size_t index = 0;  // Index in roomAvailability array

        for (size_t floor = 0; floor < doc.size(); floor++) {
          JsonArray floorArray = doc[floor];
          for (size_t room = 0; room < floorArray.size(); room++) {
            int status = floorArray[room].as<int>();
            wing.roomAvailability[index++] = (uint8_t)status;  // Store directly in the roomAvailability array
          }
        }

        // Now send the roomAvailability data over I²C
        sendAvailabilityData(wing.slaveAddress, wing.roomAvailability, TOTAL_ROOMS);

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

  vTaskDelay(200 / portTICK_PERIOD_MS);
}
