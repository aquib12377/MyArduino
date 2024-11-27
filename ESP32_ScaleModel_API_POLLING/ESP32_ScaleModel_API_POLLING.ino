#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// =========================
// ====== CONFIGURATION =====
// =========================

// Wi-Fi Credentials
const char* ssid = "MyProject";     // Replace with your Wi-Fi SSID
const char* password = "12345678";  // Replace with your Wi-Fi Password

const char* baseUrl = "https://admin.modelsofbrainwing.com/";  // Corrected base URL

// API Endpoints
const char* apiEndpoint = "get_led_control.php";  // Endpoint to fetch control commands
// Room Status Endpoint will be constructed dynamically using wingId

// Project and Wing IDs
const int projectId = 8;  // Replace with your actual Project ID
const int wingId = 14;     // Replace with your actual Wing ID

// LED Configuration
#define NUM_FLOORS 8
#define ROOMS_PER_FLOOR 4
#define LEDS_PER_ROOM 4
#define NUM_LEDS (NUM_FLOORS * ROOMS_PER_FLOOR * LEDS_PER_ROOM)
#define DATA_PIN 13  // GPIO pin connected to LED strip

CRGB leds[NUM_LEDS];

// LED Index Map for Floors, Rooms, and LEDs per Room
const int ledIndexMap[NUM_FLOORS][ROOMS_PER_FLOOR][LEDS_PER_ROOM] = {
  { { 112, 113, 114, 115 }, { 116, 117, 118, 119 }, { 120, 121, 122, 123 }, { 124, 125, 126, 127 } },
  { { 96, 97, 98, 99 }, { 100, 101, 102, 103 }, { 104, 105, 106, 107 }, { 108, 109, 110, 111 } },
  { { 80, 81, 82, 83 }, { 84, 85, 86, 87 }, { 88, 89, 90, 91 }, { 92, 93, 94, 95 } },
  { { 64, 65, 66, 67 }, { 68, 69, 70, 71 }, { 72, 73, 74, 75 }, { 76, 77, 78, 79 } },
  { { 48, 49, 50, 51 }, { 52, 53, 54, 55 }, { 56, 57, 58, 59 }, { 60, 61, 62, 63 } },
  { { 32, 33, 34, 35 }, { 36, 37, 38, 39 }, { 40, 41, 42, 43 }, { 44, 45, 46, 47 } },
  { { 16, 17, 18, 19 }, { 20, 21, 22, 23 }, { 24, 25, 26, 27 }, { 28, 29, 30, 31 } },
  { { 12, 13, 14, 15 }, { 8, 9, 10, 11 }, { 4, 5, 6, 7 }, { 0, 1, 2, 3 } }
};

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

// LED Control Functions
void controlRoomLEDs(int floor, int room, CRGB color);
void controlFloorLEDs(int floor, CRGB color);
void fadeInRoom(int floor, int room, CRGB targetColor, int steps = 50, int delayMs = 10);
void fadeInFloor(int floor, CRGB targetColor, int steps = 50, int delayMs = 10);
void turnOffAllLEDs();

// LED Patterns
void runPattern1BHK1();
void runPattern1BHK2();
void runPattern2BHK();
void runPatternAvailableRooms();
void runPatternPatterns();

// Reset Pattern Flags
void resetPatternFlags();

// =========================
// ====== SETUP FUNCTION ======
// =========================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);  // Wait for Serial to initialize

  // Initialize LED Strip
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // Initialize Mutex
  controlMutex = xSemaphoreCreateMutex();
  if (controlMutex == NULL) {
    Serial.println("Failed to create mutex!");
    while (1);  // Halt execution
  }

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
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);

  int maxRetries = 20;
  int retries = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
    // Optionally, implement a retry mechanism or enter a safe state
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
      Serial.println("Led Control URL: "+String(url));
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
    vTaskDelay(2000 / portTICK_PERIOD_MS);
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
      runPattern1BHK1();
      IsRunningPattern1BHK1 = true;
    } 
    else if (currentControl == "1 BHK2" && !IsRunningPattern1BHK2) {
      runPattern1BHK2();
      IsRunningPattern1BHK2 = true;
    } 
    else if (currentControl == "2 BHK" && !IsRunningPattern2BHK) {
      runPattern2BHK();
      IsRunningPattern2BHK = true;
    } 
    else if (currentControl == "Available Rooms") {
      runPatternAvailableRooms();
      IsRunningPatternAvailableRooms = true;
    } 
    else if (currentControl == "Patterns") {
      runPatternPatterns();
    } 
    else if (currentControl == "OFF") {
      turnOffAllLEDs();
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Short delay to prevent tight loop
      continue;
    } 
    else {
      // Handle unknown control
      Serial.println("Unknown control received: " + currentControl);
      // Optionally, turn off all LEDs
      // turnOffAllLEDs();
    }

    // Small delay to allow other tasks to run
    vTaskDelay(100 / portTICK_PERIOD_MS);
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
// ====== LED CONTROL FUNCTIONS ======
// =========================

// Control Specific Room LEDs
void controlRoomLEDs(int floor, int room, CRGB color) {
  if (floor < 0 || floor >= NUM_FLOORS || room < 0 || room >= ROOMS_PER_FLOOR) {
    Serial.println("Invalid floor or room number in controlRoomLEDs.");
    return;
  }

  for (int led = 0; led < LEDS_PER_ROOM; led++) {
    int ledIndex = ledIndexMap[floor][room][led];
    if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
      leds[ledIndex] = color;
    } else {
      Serial.printf("Invalid LED index: %d for floor %d, room %d, LED %d\n", ledIndex, floor, room, led);
    }
  }
}

// Control Entire Floor LEDs
void controlFloorLEDs(int floor, CRGB color) {
  if (floor < 0 || floor >= NUM_FLOORS) {
    Serial.println("Invalid floor number in controlFloorLEDs.");
    return;
  }

  for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
    for (int led = 0; led < LEDS_PER_ROOM; led++) {
      int ledIndex = ledIndexMap[floor][room][led];
      if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
        leds[ledIndex] = color;
      } else {
        Serial.printf("Invalid LED index: %d for floor %d, room %d, LED %d\n", ledIndex, floor, room, led);
      }
    }
  }
}

// Fade In a Specific Room
void fadeInRoom(int floor, int room, CRGB targetColor, int steps, int delayMs) {
  if (floor < 0 || floor >= NUM_FLOORS || room < 0 || room >= ROOMS_PER_FLOOR) {
    Serial.println("Invalid floor or room number in fadeInRoom.");
    return;
  }

  for (int i = 0; i <= steps; i++) {
    float ratio = (float)i / steps;
    CRGB currentColor = CRGB(
      targetColor.r * ratio,
      targetColor.g * ratio,
      targetColor.b * ratio);

    for (int led = 0; led < LEDS_PER_ROOM; led++) {
      int ledIndex = ledIndexMap[floor][room][led];
      if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
        leds[ledIndex] = currentColor;
      }
    }

    FastLED.show();
    vTaskDelay(delayMs / portTICK_PERIOD_MS);
  }
}

// Fade In an Entire Floor
void fadeInFloor(int floor, CRGB targetColor, int steps, int delayMs) {
  if (floor < 0 || floor >= NUM_FLOORS) {
    Serial.println("Invalid floor number in fadeInFloor.");
    return;
  }

  for (int i = 0; i <= steps; i++) {
    float ratio = (float)i / steps;
    CRGB currentColor = CRGB(
      targetColor.r * ratio,
      targetColor.g * ratio,
      targetColor.b * ratio);

    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      for (int led = 0; led < LEDS_PER_ROOM; led++) {
        int ledIndex = ledIndexMap[floor][room][led];
        if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
          leds[ledIndex] = currentColor;
        }
      }
    }

    FastLED.show();
    vTaskDelay(delayMs / portTICK_PERIOD_MS);
  }
}

// Turn Off All LEDs
void turnOffAllLEDs() {
  FastLED.clear();
  FastLED.show();
}

// =========================
// ====== LED PATTERN FUNCTIONS ======
// =========================

// Pattern for "1 BHK1"
void runPattern1BHK1() {
  Serial.println("Running Pattern: 1 BHK1");
  turnOffAllLEDs();

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    controlRoomLEDs(floor, 0, CRGB::Red);  // Assuming Room 0 is 1 BHK1
    controlRoomLEDs(floor, 1, CRGB::Red);  // Assuming Room 1 is 1 BHK1
  }

  FastLED.show();
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Pattern for "1 BHK2"
void runPattern1BHK2() {
  Serial.println("Running Pattern: 1 BHK2");
  turnOffAllLEDs();

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    controlRoomLEDs(floor, 2, CRGB::Green);  // Assuming Room 2 is 1 BHK2
  }

  FastLED.show();
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Pattern for "2 BHK"
void runPattern2BHK() {
  Serial.println("Running Pattern: 2 BHK");
  turnOffAllLEDs();

  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    controlRoomLEDs(floor, 3, CRGB::Blue);  // Assuming Room 3 is 2 BHK
  }

  FastLED.show();
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Pattern for "Available Rooms"
void runPatternAvailableRooms() {
  Serial.println("Running Pattern: Available Rooms");

  // Check Wi-Fi connection
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Dynamically construct the room status endpoint based on wingId
    String roomStatusEndpoint = "getRoomStatus.php?wing_id=" + String(wingId);
    String url = String(baseUrl) + roomStatusEndpoint;
    http.begin(url);
    Serial.println("Room Status Endpoint: " + url);

    int httpCode = http.GET();
    String payload = "";
    if (httpCode > 0) {  // Check for successful request
      payload = http.getString();
      Serial.println("Received payload: " + payload);
    } else {
      Serial.println("HTTP GET Error: " + String(http.errorToString(httpCode).c_str()));
    }

    http.end();

    // Parse JSON payload
    if (payload.length() > 0) {
      StaticJsonDocument<4096> doc;  // Adjust size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // Assuming the JSON structure matches the 3D array
        // Example JSON:
        // [
        //   [
        //     [1, 1, 1, 1],
        //     [0, 0, 0, 0],
        //     ...
        //   ],
        //   ...
        // ]

        for (int floor = 0; floor < NUM_FLOORS; floor++) {
          for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
            for (int led = 0; led < LEDS_PER_ROOM; led++) {
              if (doc.size() > floor && doc[floor].size() > room && doc[floor][room].size() > led) {
                int status = doc[floor][room][led].as<int>();
                CRGB color = (status == 1) ? CRGB::White : CRGB::Red;  // Define colors based on status
                leds[ledIndexMap[floor][room][led]] = color;
              }
            }
          }
        }

        FastLED.show();
      } else {
        Serial.println("JSON Parsing Error: " + String(error.c_str()));
      }
    } else {
      Serial.println("No payload received from Room Status API.");
      // Optionally, handle no data scenario
    }

  } else {
    Serial.println("WiFi not connected. Trying to re-connect....");
    setupWiFi();
  }

  vTaskDelay(500 / portTICK_PERIOD_MS);
}

// Pattern for "Patterns"
void runPatternPatterns() {
  Serial.println("Running Pattern: Patterns");
  if(activeControl != "Patterns")
    return;
  // Example Pattern: Cascade Colors Across Floors
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    controlFloorLEDs(floor, CRGB::Crimson);
    FastLED.show();
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
if(activeControl != "Patterns")
    return;
  for (int floor = NUM_FLOORS - 1; floor >= 0; floor--) {
    for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
      CRGB color = CHSV(room * (255 / ROOMS_PER_FLOOR), 255, 255);
      controlRoomLEDs(floor, room, color);
    }
    FastLED.show();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  // Fade In Floors with Different Colors
  turnOffAllLEDs();  // Ensure all LEDs are off first
  if(activeControl != "Patterns")
    return;
  for (int floor = 0; floor < NUM_FLOORS; floor++) {
    uint8_t hue = floor * (255 / NUM_FLOORS);  // Distributes hues evenly across the floors
    CRGB floorColor;
    floorColor.setHSV(hue, 255, 255);       // Full saturation and brightness
    fadeInFloor(floor, floorColor, 50, 5);  // 50 steps, 5ms delay for smooth fade
    vTaskDelay(50 / portTICK_PERIOD_MS);   // Delay between floors for a cascading effect
  }

  // Fade In Rooms with Random Colors
  turnOffAllLEDs();  // Ensure all LEDs are off first
  if(activeControl != "Patterns")
    return;
  for (int room = 0; room < ROOMS_PER_FLOOR; room++) {
    Serial.printf("Controlling Room %d\n", room + 1);
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
      CRGB color = (random(2)) ? CRGB::LightGoldenrodYellow : CRGB::Cyan;
      fadeInRoom(floor, room, color, 10, 5);  // 10 steps, 5ms delay
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
if(activeControl != "Patterns")
    return;
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

// =========================
// ====== END OF CODE ======
// =========================
