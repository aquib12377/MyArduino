/*************************************************************
   ESP32-CAM Firmware to Receive RFID UID Over Serial2
   And Update Firebase With Entry/Exit + Photo
*************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
#include "esp_camera.h"
#include <base64.h>                // Simple Base64 encoding (part of some Arduino cores or libraries)
// ------------------- User Config -------------------

// 1) Wi-Fi Credentials
#define WIFI_SSID     "MyProject"
#define WIFI_PASSWORD "12345678"

// 2) Firebase Credentials
#define API_KEY         "AIzaSyBUjHE2I_o7mVHYEZiMJ_AMQS7-dQ0SwtE"
#define DATABASE_URL    "https://smartparkingdata.firebaseio.com/" 

// 3) Optional NTP Settings
#define GMT_OFFSET_SEC     0
#define DAYLIGHT_OFFSET_SEC 0
#define NTP_SERVER         "pool.ntp.org"

// 4) Serial2 Pins (for receiving RFID from Gate Controller)
#define RXD2 14
#define TXD2 15  // not necessarily used if only receiving

// ------------------- Camera Pins (AI Thinker) -------------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ------------------- Firebase Objects -------------------
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;
bool signupOK = false;

// ------------------------------------------------
//  Function: initCamera()
// ------------------------------------------------
bool initCamera() {
  camera_config_t camera_config;
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.ledc_timer   = LEDC_TIMER_0;
  camera_config.pin_d0       = Y2_GPIO_NUM;
  camera_config.pin_d1       = Y3_GPIO_NUM;
  camera_config.pin_d2       = Y4_GPIO_NUM;
  camera_config.pin_d3       = Y5_GPIO_NUM;
  camera_config.pin_d4       = Y6_GPIO_NUM;
  camera_config.pin_d5       = Y7_GPIO_NUM;
  camera_config.pin_d6       = Y8_GPIO_NUM;
  camera_config.pin_d7       = Y9_GPIO_NUM;
  camera_config.pin_xclk     = XCLK_GPIO_NUM;
  camera_config.pin_pclk     = PCLK_GPIO_NUM;
  camera_config.pin_vsync    = VSYNC_GPIO_NUM;
  camera_config.pin_href     = HREF_GPIO_NUM;
  camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
  camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
  camera_config.pin_pwdn     = PWDN_GPIO_NUM;
  camera_config.pin_reset    = RESET_GPIO_NUM;
  camera_config.xclk_freq_hz = 20000000;
  camera_config.pixel_format = PIXFORMAT_JPEG;  // Use JPEG for better images
  camera_config.frame_size   = FRAMESIZE_QVGA;  // Adjust resolution if needed
  camera_config.jpeg_quality = 12;             // 0-63 (lower = better)
  camera_config.fb_count     = 1;
  camera_config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  camera_config.fb_location  = CAMERA_FB_IN_PSRAM;

  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  return true;
}

// ------------------------------------------------
//  Function: capturePhotoBase64()
// ------------------------------------------------
String capturePhotoBase64() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "";
  }

  base64::
  // Convert to Base64
  String base64Image = base64::encode((uint8_t *)fb->buf, fb->len);

  // Return the buffer
  esp_camera_fb_return(fb);

  return base64Image;
}

// ------------------------------------------------
//  Function: getCurrentDateTimeString()
// ------------------------------------------------
String getCurrentDateTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get local time");
    return "UnknownTime";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ------------------------------------------------
//  Function: processRFID(rfidUID)
//  Handles the logic to create/update the DB record
// ------------------------------------------------
void processRFID(const String& rfidUID) {
  Serial.println("Processing RFID: " + rfidUID);

  // 1) Generate the record path in Firebase
  //    For example: /parkingRecords/ABC123UID/
  String recordPath = "/parkingRecords/" + rfidUID;
  String entryTimePath = recordPath + "/entryTime";
  String exitTimePath  = recordPath + "/exitTime";
  String imagePath     = recordPath + "/imageBase64";

  // 2) Get the current time
  String currentTime = getCurrentDateTimeString();

  // 3) Check if there's an existing exitTime
  //    - If it doesn't exist or is empty => that means user is 'inside' => set exitTime
  //    - If it exists and is not empty => new entry
  if (Firebase.RTDB.getString(&fbdo, exitTimePath)) {
    // getString succeeded
    String existingExitTime = fbdo.to<String>();

    if (existingExitTime.length() == 2) {
      // Means entryTime was set previously, but exitTime is blank => second scan => set exitTime
      Serial.println("Found open record (no exitTime). Setting exitTime now...");

      // Optionally capture a new photo for the exit
      String base64Image = capturePhotoBase64();

      if (!Firebase.RTDB.setString(&fbdo, exitTimePath, currentTime)) {
        Serial.println("Failed to set exitTime: " + fbdo.errorReason());
      }
      

      Serial.println("Exit time updated for UID: " + rfidUID);
    }
    else {
      // exitTime is already set => treat as new entry
      Serial.println("Record already has exitTime. Creating a NEW entry for this UID...");

      // Capture new photo for new entry
      String base64Image = capturePhotoBase64();

      // Write new entryTime & clear exitTime
      if (!Firebase.RTDB.setString(&fbdo, entryTimePath, currentTime)) {
        Serial.println("Failed to set entryTime: " + fbdo.errorReason());
      }
      if (!Firebase.RTDB.setString(&fbdo, exitTimePath, "NA")) {
        Serial.println("Failed to clear exitTime: " + fbdo.errorReason());
      }
      if (!Firebase.RTDB.setString(&fbdo, imagePath, base64Image)) {
        Serial.println("Failed to set imageBase64: " + fbdo.errorReason());
      }
      Serial.println("New entry created for UID: " + rfidUID);
    }
  }
  else {
    // getString failed => likely no record exists => first scan => new entry
    Serial.println("No existing record found. Creating a new entry...");

    // Capture photo
    String base64Image = capturePhotoBase64();

    if (!Firebase.RTDB.setString(&fbdo, entryTimePath, currentTime)) {
      Serial.println("Failed to set entryTime: " + fbdo.errorReason());
    }
    // exitTime blank for now
    if (!Firebase.RTDB.setString(&fbdo, exitTimePath, "NA")) {
      Serial.println("Failed to set exitTime: " + fbdo.errorReason());
    }
    if (!Firebase.RTDB.setString(&fbdo, imagePath, base64Image)) {
      Serial.println("Failed to set imageBase64: " + fbdo.errorReason());
    }
    Serial.println("Entry created for UID: " + rfidUID);
  }
}

// ------------------------------------------------
//  SETUP
// ------------------------------------------------
void setup() {
  Serial.begin(115200);  // For debugging via USB-Serial

  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera init failed. Check connections!");
    while (true) { delay(1000); }
  }
  Serial.println("Camera init OK");

  // Connect Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi connected!");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  // Sync time via NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("Syncing time with NTP...");
  for(int i = 0; i < 10; i++){
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to sync with NTP, continuing anyway...");
  } else {
    Serial.println("NTP time sync successful!");
  }

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // For debug

  // Sign up with an anonymous account
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
    signupOK = true;
  } else {
    Serial.printf("SignUp Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize Serial2 for communication with Gate Controller
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Setup complete. Waiting for RFID UID via Serial2...");
}

// ------------------------------------------------
//  LOOP
// ------------------------------------------------
void loop() {
  // Check if there's any incoming data on Serial2
  if (Serial2.available() > 0) {
    // Read the incoming RFID UID string (until newline)
    String rfidUID = Serial2.readStringUntil('\n');
    rfidUID.trim();
    if (rfidUID.length() > 0) {
      Serial.println("Received UID: " + rfidUID);
      // Process that UID
      processRFID(rfidUID);
    }
  }

  // Small delay to avoid flooding
  delay(100);
}
