/********************************************
 * ESP32 Master Code with Common + 3 Wings
 * 
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
#define LOG_INFO(x) //Serial.println("[INFO] " + String(x))
#define LOG_DEBUG(x) //Serial.println("[DEBUG] " + String(x))
#define LOG_ERROR(x) //Serial.println("[ERROR] " + String(x))
#else
#define LOG_INFO(x)
#define LOG_DEBUG(x)
#define LOG_ERROR(x)
#endif

// ===== Wi-Fi / API =====
const char* ssid        = "Delta_Greenville";
const char* password    = "Delta#786";
const char* baseUrl = "https://admin.modelsofbrainwing.com/";
const char* apiEndpoint = "get_led_control.php";  // e.g. returns {"control":"..."}
const int projectId = 8;                          // Project ID

// ===== I2C Addresses for Wings =====
#define SLAVE_ADDRESS_WING_14 0x08  // Wing A
#define SLAVE_ADDRESS_WING_15 0x09  // Wing B
#define SLAVE_ADDRESS_WING_16 0x0A  // Wing C

// ===== Command IDs (Common + Wing-Specific) =====
// Common commands
#define CMD_TURN_OFF_ALL 0xA0
#define CMD_TURN_ON_ALL 0xA1
#define CMD_ACTIVATE_PATTERNS 0xA2
#define CMD_CONTROL_ALL_TOWERS 0xA3
#define CMD_VIEW_AVAILABLE_FLATS 0xA4
#define CMD_FILTER_1BHK 0xA5
#define CMD_FILTER_2BHK 0xA6
#define CMD_FILTER_3BHK 0xA7

// Wing A (ID=14)
#define CMD_WINGA_2BHK_689_SQFT 0xB0
#define CMD_WINGA_1BHK_458_SQFT 0xB1
#define CMD_WINGA_1BHK_461_SQFT 0xB2

// Wing B (ID=15)
#define CMD_WINGB_2BHK_689_SQFT 0xB3
#define CMD_WINGB_1BHK_458_SQFT 0xB4
#define CMD_WINGB_1BHK_461_SQFT 0xB5

// Wing C (ID=16)
#define CMD_WINGC_2BHK_696_SQFT 0xC0
#define CMD_WINGC_2BHK_731_SQFT 0xC1
#define CMD_WINGC_3BHK_924_SQFT 0xC2
#define CMD_WINGC_3BHK_959_SQFT 0xC3
#define CMD_WINGC_3BHK_1931_SQFT 0xC4
#define CMD_WINGC_3BHK_1972_SQFT 0xC5
#define CMD_WINGC_3BHK_2931_SQFT 0xC6
#define CMD_WINGC_3BHK_2972_SQFT 0xC7

// For Availability
#define CMD_SHOW_AVAIL 0xC8

// ===== Building / Room Constants (for availability chunking) =====
#define NUM_FLOORS 35
#define NUM_ROOMS_PER_FLOOR 8
#define TOTAL_ROOMS (NUM_FLOORS * NUM_ROOMS_PER_FLOOR)

// ===== Data Structure for Each Wing =====
struct WingInfo {
  int wingId;              // e.g. 14, 15, 16
  uint8_t slaveAddress;    // I2C address
  String activeControl;    // e.g. "WingA2BHK689sqft"
  String lastSentControl;  // New field to track the last processed command
  uint8_t roomAvailability[TOTAL_ROOMS];
};

#define NUM_WINGS 3
WingInfo wings[NUM_WINGS];  // We'll store wing A, B, C here

// We'll store the "common" commands (wing_id=0) in a separate global
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
    while (1)
      ;
  }
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(8, OUTPUT);

  // Ensure relays start in a known state (e.g., OFF)
  digitalWrite(10, LOW);
  digitalWrite(9, LOW);
  digitalWrite(8, LOW);
  setupWiFi();
pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  Wire.begin(11, 12);       // Adjust pins if needed
  Wire.setClock(400000UL);  // 400kHz
  LOG_INFO("I2C initialized");

  // Initialize WingInfo
  wings[0] = { 14, SLAVE_ADDRESS_WING_14, "", "", { 0 } };
  wings[1] = { 15, SLAVE_ADDRESS_WING_15, "", "", { 0 } };
  wings[2] = { 16, SLAVE_ADDRESS_WING_16, "", "", { 0 } };
sendCommand(wings[0].slaveAddress,CMD_ACTIVATE_PATTERNS);
  sendCommand(wings[1].slaveAddress,CMD_ACTIVATE_PATTERNS);
  sendCommand(wings[2].slaveAddress,CMD_ACTIVATE_PATTERNS);
  digitalWrite(10, LOW);
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

// void setupWiFi() {
//   WiFiManager wifiManager;
//   // Optional: clear previous credentials to force config portal every boot
//   //wifiManager.resetSettings();

//   // Start configuration portal
//   if (!wifiManager.startConfigPortal("ESP32_ConfigPortal")) {
//     LOG_ERROR("Failed to complete WiFi configuration portal");

//   } else {
//     LOG_INFO("Connected to WiFi! IP: " + WiFi.localIP().toString());
//   }
// }

// ----------------------------------------------------
//             pollAPITask (Wing + Common)
// ----------------------------------------------------
void pollAPITask(void* parameter) {
  int retryCount = 0;
  const int maxRetries = 5;

  while (true) {
    // 1) Poll common commands => wing_id=0
    pollCommonCommands();

    // 2) Poll each actual wing 14,15,16
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo& wing = wings[i];

      if (WiFi.status() != WL_CONNECTED) {
        LOG_ERROR("Wi-Fi not connected; reconnecting...");
        setupWiFi();
        continue;
      }

      HTTPClient http;
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

            // Only update and print if the command has changed
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
//  pollCommonCommands => wing_id=0
// ----------------------------------------------------
void pollCommonCommands() {
  if (WiFi.status() != WL_CONNECTED) return;
  LOG_INFO("Starting Polling Common Commands");
  HTTPClient http;
  String url = String(baseUrl) + apiEndpoint
               + "?project_id=" + String(projectId)
               + "&wing_id=1";  // Here is the magic: get "common" commands
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

        // Relay control based on specific commands
        if(commonControl == "PODIUMLIGHTS") {
          digitalWrite(10, LOW);  // Turn on relay connected to pin 5
          LOG_INFO("Relay on Pin 5 turned ON for PODIUMLIGHTS");
        }
        else if(commonControl == "ROOFTOPLIGHTS") {
          digitalWrite(9, LOW);  // Turn on relay connected to pin 6
          LOG_INFO("Relay on Pin 6 turned ON for ROOFTOPLIGHTS");
        }
        else if(commonControl == "LANDSCAPELIGHTS") {
          digitalWrite(8, LOW);  // Turn on relay connected to pin 7
          LOG_INFO("Relay on Pin 7 turned ON for LANDSCAPELIGHTS");
        }

        LOG_INFO("CommonControl => " + commonControl);
      } else if (doc.containsKey("error")) {
          digitalWrite(8, HIGH);  // Turn on relay connected to pin 7
                    digitalWrite(9, HIGH);  // Turn on relay connected to pin 7
          digitalWrite(10, HIGH);  // Turn on relay connected to pin 7

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
//            ledControlTask => interpret commands
// ----------------------------------------------------
void ledControlTask(void* parameter) {
  while (true) {
    // 1) Handle the "commonControl" => broadcast to all wings
    if (commonControl == "OFF") {
      LOG_INFO("Common => OFF => broadcast CMD_TURN_OFF_ALL to all wings");
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_TURN_OFF_ALL);
      }
    } else if (commonControl == "TurnOnAllLights") {
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_TURN_ON_ALL);
      }
    } else if (commonControl == "ActivatePatterns") {
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_ACTIVATE_PATTERNS);
      }
    } else if (commonControl == "ControlAllTowers") {
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_CONTROL_ALL_TOWERS);
      }
    } else if (commonControl == "ViewAvailableFlats") {
      // We want to fetch availability for *each* wing
      for (int i = 0; i < NUM_WINGS; i++) {
        runPatternAvailableRooms(wings[i]);  // fetch & send chunked data to wing
      }
    } else if (commonControl == "Filter1BHK") {
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_FILTER_1BHK);
      }
    } else if (commonControl == "Filter2BHK") {
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_FILTER_2BHK);
      }
    } else if (commonControl == "Filter3BHK") {
      for (int i = 0; i < NUM_WINGS; i++) {
        sendCommand(wings[i].slaveAddress, CMD_FILTER_3BHK);
      }
    }
    // If it's an unknown common command or empty, do nothing
    // Optionally clear commonControl to prevent re-sending
    // if you want the command to run only once
    commonControl = "";

    // 2) Now handle each wing's specific command
    for (int i = 0; i < NUM_WINGS; i++) {
      WingInfo& wing = wings[i];
            String ctrl = wing.activeControl;

      if (ctrl != "" && ctrl != wing.lastSentControl) {
        if (ctrl == "OFF") {
          sendCommand(wing.slaveAddress, CMD_TURN_OFF_ALL);
        } else if (ctrl == "TurnOnAllLights") {
          sendCommand(wing.slaveAddress, CMD_TURN_ON_ALL);
        } else if (ctrl == "ActivatePatterns") {
          sendCommand(wing.slaveAddress, CMD_ACTIVATE_PATTERNS);
        } else if (ctrl == "ControlAllTowers") {
          sendCommand(wing.slaveAddress, CMD_CONTROL_ALL_TOWERS);
        } else if (ctrl == "ViewAvailableFlats") {
          runPatternAvailableRooms(wing);
        } else if (ctrl == "Filter1BHK") {
          sendCommand(wing.slaveAddress, CMD_FILTER_1BHK);
        } else if (ctrl == "Filter2BHK") {
          sendCommand(wing.slaveAddress, CMD_FILTER_2BHK);
        } else if (ctrl == "Filter3BHK") {
          sendCommand(wing.slaveAddress, CMD_FILTER_3BHK);
        }
        // Wing A
        else if (wing.wingId == 14 && ctrl == "WingA2BHK689sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGA_2BHK_689_SQFT);
        } else if (wing.wingId == 14 && ctrl == "WingA1BHK458sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGA_1BHK_458_SQFT);
        } else if (wing.wingId == 14 && ctrl == "WingA1BHK461sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGA_1BHK_461_SQFT);
        }
        // Wing B
        else if (wing.wingId == 15 && ctrl == "WingB2BHK689sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGB_2BHK_689_SQFT);
        } else if (wing.wingId == 15 && ctrl == "WingB1BHK458sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGB_1BHK_458_SQFT);
        } else if (wing.wingId == 15 && ctrl == "WingB1BHK461sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGB_1BHK_461_SQFT);
        }
        // Wing C
        else if (wing.wingId == 16 && ctrl == "WingC2BHK696sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_2BHK_696_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC2BHK731sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_2BHK_731_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC3BHK924sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_924_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC3BHK959sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_959_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC3BHKType1931sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_1931_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC3BHKType1972sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_1972_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC3BHKType2931sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_2931_SQFT);
        } else if (wing.wingId == 16 && ctrl == "WingC3BHKType2972sqft") {
          sendCommand(wing.slaveAddress, CMD_WINGC_3BHK_2972_SQFT);
        } else {
          // If ctrl is non-empty and not recognized
          if (ctrl != "") {
            //Serial.println("Unknown command for Wing " + String(wing.wingId) + ": " + ctrl);
          }
        }
        // After processing, update the last sent control for this wing
        wing.lastSentControl = ctrl;
      }

      //Serial.println("Wing " + String(wing.wingId) + " => " + ctrl);
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // small delay
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ----------------------------------------------------
//  Send Single Command via I2C
// ----------------------------------------------------
void sendCommand(uint8_t slaveAddress, uint8_t commandID) {
  // if (xSemaphoreTake(serialMutex, (TickType_t)10) == pdTRUE) {
  //   //Serial.print("I2C => 0x");
  //   //Serial.print(commandID, HEX);
  //   //Serial.print(" => Slave 0x");
  //   //Serial.println(slaveAddress, HEX);
  //   xSemaphoreGive(serialMutex);
  // }

  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);
  Wire.write(commandID);
  int res = Wire.endTransmission();
//Serial.println(res);
  delay(10);

  Wire.beginTransmission(slaveAddress);
  Wire.write(0x55);
  res = Wire.endTransmission();
  //Serial.print("I2C RESPONSE --------------------------- ");
  //Serial.println(res);
  //Serial.println("\n\n\n");
}

// ----------------------------------------------------
//  Send Availability Data in Chunks
// ----------------------------------------------------
void sendAvailabilityData(uint8_t slaveAddress, uint8_t* roomAvail, size_t dataSize) {
  const size_t maxChunkSize = 27;
  size_t bytesSent = 0;

  //Serial.println("Sending Availability => Slave 0x" + String(slaveAddress, HEX));

  // Start
  Wire.beginTransmission(slaveAddress);
  Wire.write(0xAA);
  Wire.write(CMD_SHOW_AVAIL);
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

  //Serial.println("Availability data sent => 0x" + String(slaveAddress, HEX));
}

// ----------------------------------------------------
//  runPatternAvailableRooms => fetch JSON, send chunk
// ----------------------------------------------------
void runPatternAvailableRooms(WingInfo& wing) {
  //Serial.println("Fetching availability => Wing " + String(wing.wingId));

  if (WiFi.status() != WL_CONNECTED) {
    //Serial.println("No Wi-Fi => reconnecting...");
    setupWiFi();
    return;
  }

  HTTPClient http;
  String url = String(baseUrl) + "getRoomStatus.php?wing_id=" + String(wing.wingId);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode <= 0) {
    //Serial.println("HTTP error => " + http.errorToString(httpCode));
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    //Serial.println("No room status payload...");
    return;
  }

  // parse JSON
  StaticJsonDocument<16384> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    //Serial.println("JSON parse error => " + String(err.c_str()));
    return;
  }

  // We expect doc to be array-of-arrays
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
