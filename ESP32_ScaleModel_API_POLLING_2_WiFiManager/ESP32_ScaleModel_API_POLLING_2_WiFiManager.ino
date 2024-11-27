#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Preferences.h>
#include <WiFiManager.h>  // Ensure WiFiManager is installed

// =========================
// ====== CONFIGURATION =====
// =========================

const char* baseUrl = "https://admin.modelsofbrainwing.com/";  // Base URL for API

// API Endpoints
const char* apiEndpoint = "get_led_control.php";  // Endpoint to fetch control commands

// Project and Wing IDs
const int projectId = 8;   // Replace with your actual Project ID
const int wingId = 14;     // Replace with your actual Wing ID


// I2C Configuration
#define MEGA_I2C_ADDRESS 0x10  // I2C address of Arduino Mega (0x08 to 0x77)

// =========================
// ====== GLOBALS =========
// =========================

// FreeRTOS Task Handles
TaskHandle_t TaskPollAPI;
TaskHandle_t TaskLEDControl;

// Mutex for Shared Variable
SemaphoreHandle_t controlMutex;

// Shared Variable
String activeControl = "OFF";  // Initialize to OFF

// Pattern Running Flags
bool IsRunningPatternSetAllRed = false;
bool IsRunningPatternTurnOffAll = false;
bool IsRunningPatternSetFloorBlue = false;
bool IsRunningPatternTurnOffFloor = false;
// Add more pattern flags as needed

// Preferences for storing Wi-Fi credentials
Preferences preferences;

// WiFiManager instance
WiFiManager wifiManager;

// =========================
// ====== FUNCTION PROTOTYPES ======
// =========================

// Wi-Fi Setup
void setupWiFi();

// API Polling Task
void pollAPITask(void* parameter);

// LED Control Task
void ledControlTask(void* parameter);

// I2C Communication Functions
void sendSetRoomColor(uint8_t floor, uint8_t room, uint8_t red, uint8_t green, uint8_t blue);
void sendTurnOffAllLEDs();
void sendSetFloorColor(uint8_t floor, uint8_t red, uint8_t green, uint8_t blue);
void sendTurnOffFloorLEDs(uint8_t floor);

// Reset Pattern Flags
void resetPatternFlags();

// =========================
// ====== SETUP FUNCTION ======
// =========================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);  // Wait for Serial to initialize

  // Initialize I2C as Master
  Wire.begin();  // Default SDA=21, SCL=22 for ESP32. Change if necessary.
  Wire.setClock(400000);  // Set I2C frequency to 400kHz for faster communication

  // Initialize Mutex
  controlMutex = xSemaphoreCreateMutex();
  if (controlMutex == NULL) {
    Serial.println("Failed to create mutex!");
    while (1);  // Halt execution
  }

  // Initialize Preferences
  preferences.begin("wifi-config", false);  // Namespace "wifi-config", read-write

  // Connect to Wi-Fi
  setupWiFi();

  // Create FreeRTOS Tasks
  xTaskCreatePinnedToCore(
    pollAPITask,          // Task function
    "Poll API Task",     // Task name
    8192,                // Stack size (words)
    NULL,                // Task input parameter
    1,                   // Priority
    &TaskPollAPI,        // Task handle
    0);                  // Core (0 or 1)

  xTaskCreatePinnedToCore(
    ledControlTask,        // Task function
    "LED Control Task",    // Task name
    8192,                  // Stack size (words)
    NULL,                  // Task input parameter
    1,                     // Priority
    &TaskLEDControl,       // Task handle
    1);                    // Core (0 or 1)
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
  // Check if Wi-Fi credentials are stored
  String storedSSID = preferences.getString("ssid", "");
  String storedPassword = preferences.getString("password", "");

  if (storedSSID.length() == 0 || storedPassword.length() == 0) {
    Serial.println("No stored Wi-Fi credentials found.");
    Serial.println("Starting WiFiManager config portal...");

    // Reset saved preferences (optional)
    // preferences.clear();

    // Set up WiFiManager parameters (optional customization)
    // You can add custom parameters here if needed

    // Start config portal with fallback to access point mode
    bool res;
    res = wifiManager.autoConnect("ESP32_Config_AP");
    
    if(!res) {
      Serial.println("Failed to connect and hit timeout");
      // Handle failure (e.g., restart or enter safe mode)
      ESP.restart();
      delay(1000);
    } else {
      Serial.println("Connected via WiFiManager.");
    }

    // After successful connection, save credentials
    storedSSID = WiFi.SSID();
    storedPassword = WiFi.psk();
    preferences.putString("ssid", storedSSID);
    preferences.putString("password", storedPassword);
    Serial.println("Wi-Fi credentials saved to Preferences.");
  } else {
    Serial.println("Found stored Wi-Fi credentials:");
    Serial.print("SSID: ");
    Serial.println(storedSSID);
    // For security reasons, avoid printing the password
  }

  // Attempt to connect using stored credentials
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(storedSSID);

    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());

    int maxRetries = 20;
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to Wi-Fi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to connect to Wi-Fi");
      Serial.println("Restarting ESP32...");
      ESP.restart();
    }
  }
}

// Function to send Set Room Color command
void sendSetRoomColor(uint8_t floor, uint8_t room, uint8_t red, uint8_t green, uint8_t blue) {
  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  Wire.write(0x01);      // Command Type: Set Room Color
  Wire.write(floor);     // Floor Number (0 to 33)
  Wire.write(room);      // Room Number (0 to 7)
  Wire.write(red);       // Red Value (0 to 255)
  Wire.write(green);     // Green Value (0 to 255)
  Wire.write(blue);      // Blue Value (0 to 255)
  Wire.endTransmission();
  
  Serial.printf("Sent Set Room Color: Floor %d, Room %d, Color RGB(%d, %d, %d)\n", floor, room, red, green, blue);
}

// Function to send Turn Off All LEDs command
void sendTurnOffAllLEDs() {
  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  Wire.write(0x02);      // Command Type: Turn Off All LEDs
  Wire.endTransmission();
  
  Serial.println("Sent Turn Off All LEDs command");
}

// Function to send Set Floor Color command
void sendSetFloorColor(uint8_t floor, uint8_t red, uint8_t green, uint8_t blue) {
  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  Wire.write(0x03);      // Command Type: Set Floor Color
  Wire.write(floor);     // Floor Number (0 to 33)
  Wire.write(red);       // Red Value (0 to 255)
  Wire.write(green);     // Green Value (0 to 255)
  Wire.write(blue);      // Blue Value (0 to 255)
  Wire.endTransmission();
  
  Serial.printf("Sent Set Floor Color: Floor %d, Color RGB(%d, %d, %d)\n", floor, red, green, blue);
}

// Function to send Turn Off Floor LEDs command
void sendTurnOffFloorLEDs(uint8_t floor) {
  Wire.beginTransmission(MEGA_I2C_ADDRESS);
  Wire.write(0x04);      // Command Type: Turn Off Floor LEDs
  Wire.write(floor);     // Floor Number (0 to 33)
  Wire.endTransmission();
  
  Serial.printf("Sent Turn Off Floor LEDs command: Floor %d\n", floor);
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
      Serial.println("LED Control URL: " + url);
      int httpCode = http.GET();

      if (httpCode > 0) {  // Check for successful request
        String payload = http.getString();
        Serial.println("Received payload: " + payload);

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
                Serial.println("Active control updated: " + activeControl);
              }
              xSemaphoreGive(controlMutex);
            }
          } else if (doc.containsKey("error")) {
            Serial.println("API Error: " + String(doc["error"].as<const char*>()));
          } else {
            Serial.println("Unexpected API response.");
          }

          // Reset retry count on success
          retryCount = 0;
        } else {
          Serial.println("JSON Parsing Error: " + String(error.c_str()));
        }
      } else {
        Serial.println("HTTP GET Error: " + String(http.errorToString(httpCode).c_str()));
        retryCount++;

        if (retryCount > maxRetries) {
          Serial.println("Max retries reached. Skipping this poll.");
          retryCount = 0;
        } else {
          // Exponential Backoff
          int delayTime = pow(2, retryCount) * 1000;  // in milliseconds
          Serial.println("Retrying in " + String(delayTime / 1000) + " seconds...");
          vTaskDelay(delayTime / portTICK_PERIOD_MS);
          continue;  // Retry immediately after delay
        }
      }

      http.end();
    } else {
      Serial.println("Wi-Fi not connected. Attempting to reconnect...");
      setupWiFi();
    }

    // Poll every 5 seconds
    vTaskDelay(5000 / portTICK_PERIOD_MS);  // 5000ms = 5 seconds
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
    if (currentControl == "Set_All_Red" && !IsRunningPatternSetAllRed) {
      // Example: Set all rooms on all floors to Red
      Serial.println("Executing Pattern: Set_All_Red");
      for (uint8_t floor = 0; floor < 34; floor++) {
        for (uint8_t room = 0; room < 8; room++) {
          sendSetRoomColor(floor, room, 255, 0, 0);  // Red
          vTaskDelay(10 / portTICK_PERIOD_MS);      // Short delay between commands
        }
      }
      IsRunningPatternSetAllRed = true;
    }
    else if (currentControl == "Turn_Off_All" && !IsRunningPatternTurnOffAll) {
      // Example: Turn off all LEDs on all floors
      Serial.println("Executing Pattern: Turn_Off_All");
      sendTurnOffAllLEDs();
      IsRunningPatternTurnOffAll = true;
    }
    else if (currentControl.startsWith("Set_Floor_Blue_") && !IsRunningPatternSetFloorBlue) {
      // Example: "Set_Floor_Blue_5" to set floor 5 to blue
      Serial.println("Executing Pattern: Set_Floor_Blue");
      String floorStr = currentControl.substring(strlen("Set_Floor_Blue_"));
      uint8_t targetFloor = floorStr.toInt();
      if (targetFloor < 34) {  // Validate floor number
        sendSetFloorColor(targetFloor, 0, 0, 255);  // Blue
        IsRunningPatternSetFloorBlue = true;
      }
      else {
        Serial.println("Invalid Floor Number in Control: " + currentControl);
      }
    }
    else if (currentControl.startsWith("Turn_Off_Floor_") && !IsRunningPatternTurnOffFloor) {
      // Example: "Turn_Off_Floor_5" to turn off floor 5
      Serial.println("Executing Pattern: Turn_Off_Floor");
      String floorStr = currentControl.substring(strlen("Turn_Off_Floor_"));
      uint8_t targetFloor = floorStr.toInt();
      if (targetFloor < 34) {  // Validate floor number
        sendTurnOffFloorLEDs(targetFloor);
        IsRunningPatternTurnOffFloor = true;
      }
      else {
        Serial.println("Invalid Floor Number in Control: " + currentControl);
      }
    }
    else if (currentControl == "OFF") {
      // Turn off all LEDs on all floors
      Serial.println("Executing Pattern: OFF");
      sendTurnOffAllLEDs();
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Short delay to prevent tight loop
      continue;
    }
    else {
      // Handle unknown control
      Serial.println("Unknown control received: " + currentControl);
      // Optionally, turn off all LEDs or implement a default behavior
      // sendTurnOffAllLEDs();
    }

    // Small delay to allow other tasks to run
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Reset all pattern-running flags
void resetPatternFlags() {
  IsRunningPatternSetAllRed = false;
  IsRunningPatternTurnOffAll = false;
  IsRunningPatternSetFloorBlue = false;
  IsRunningPatternTurnOffFloor = false;
  // Reset additional pattern flags as needed
}
