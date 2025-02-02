/*************************************************************
   ESP32-CAM Parking System (No pushJSON)
   - Wait for HIGH pulse on pin 4
   - Capture image, convert to Base64
   - Add new parking record in Firebase with unique key
*************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"     // For Firebase Realtime DB
#include "addons/TokenHelper.h"    // For token status
#include "esp_camera.h"
#include <base64.h>                // Simple Base64 encoding (part of some Arduino cores or libraries)

// ------------- YOUR CREDENTIALS HERE ---------------

// 1) Wi-Fi Credentials
#define WIFI_SSID       "MyProject"
#define WIFI_PASSWORD   "12345678"

// 2) Firebase Credentials
#define API_KEY         "AIzaSyBUjHE2I_o7mVHYEZiMJ_AMQS7-dQ0SwtE"
#define DATABASE_URL    "https://smartparkingdata.firebaseio.com/" 

// 3) Optional: Set the time zone for NTP
#define GMT_OFFSET_SEC     0
#define DAYLIGHT_OFFSET_SEC 0
#define NTP_SERVER         "pool.ntp.org"

// ------------- CAMERA PIN CONFIG for AI Thinker  -------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ------------- TRIGGER PIN -------------
#define TRIGGER_PIN       4   // We wait for a HIGH pulse on Pin 4

// ------------- Firebase Objects -------------
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;
bool signupOK = false;

// ------------- VEHICLE DATA -------------
const char* vehicleNames[6] = {
  "Toyota", "Honda", "Ford", "Hyundai", "Nissan", "Chevrolet"
};

const char* vehiclePlates[6] = {
  "MH12AB1234", 
  "DL9CQ1234", 
  "KA05MH1234", 
  "UP32HP1234", 
  "AP16BK1234", 
  "TN10CM1234"
};

// ------------------------------------------------
//    SETUP CAMERA
// ------------------------------------------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;

  // Set frame size & quality (adjust as needed)
  config.frame_size   = FRAMESIZE_QVGA; // e.g. QVGA, VGA, SVGA, ...
  config.jpeg_quality = 12;            // 0-63 (lower = better quality)
  config.fb_count     = 1;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\r\n", err);
    return false;
  }
  return true;
}

// ------------------------------------------------
//    CAPTURE IMAGE AND RETURN BASE64 STRING
// ------------------------------------------------
String capturePhotoBase64() {
  camera_fb_t *fb = esp_camera_fb_get(); // Take a picture
  if (!fb) {
    Serial.println("Camera capture failed");
    return "";
  }

  // Convert to Base64
  String base64Image = base64::encode((uint8_t *)fb->buf, fb->len);

  // Return frame buffer back to driver
  esp_camera_fb_return(fb);

  return base64Image;
}

// ------------------------------------------------
//    GET CURRENT TIME (as string)
// ------------------------------------------------
String getCurrentDateTimeString() {
  // Make sure time is already set via NTP
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "UnknownTime";
  }
  // Format: e.g. "2025-01-15 10:25:30"
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ------------------------------------------------
//    SETUP
// ------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1) Initialize Camera
  if (!initCamera()) {
    Serial.println("Camera init failed. Check connections!");
    while(true) { delay(1000); }
  }
  Serial.println("Camera init OK!");

  // 2) Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 3) Init and sync time (NTP)
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

  // 4) Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // For debugging

  // Sign up for token (anonymous)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
    signupOK = true;
  } else {
    Serial.printf("SignUp Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // 5) Set pin mode for TRIGGER_PIN
  pinMode(TRIGGER_PIN, INPUT);

  Serial.println("Setup complete. Waiting for trigger on pin 4...");
}

// ------------------------------------------------
//    LOOP
// ------------------------------------------------
void loop() {
  // Check if TRIGGER_PIN is HIGH
  if (digitalRead(TRIGGER_PIN) == HIGH) {
    Serial.println("Trigger received! Capturing photo...");

    // 1) Capture photo in Base64
    String base64Image = capturePhotoBase64();
    if (base64Image.length() == 0) {
      Serial.println("Failed to capture image or convert to Base64.");
      delay(1000);
      return;
    }

    // 2) Choose random Vehicle Name & Plate
    int randomIndexName   = random(0, 6);
    int randomIndexPlate  = random(0, 6);
    String vehicleName    = vehicleNames[randomIndexName];
    String vehiclePlate   = vehiclePlates[randomIndexPlate];

    // 3) Get current time
    String currentTime = getCurrentDateTimeString();

    // 4) Create a unique key for this record
    //    For example: "parking_12345678"
    String dbPath = "/parkingRecords";
    String uniqueKey = "parking_" + String(millis());
    String recordPath = dbPath + "/" + uniqueKey;

    // Now set each field
    bool success = true;

    if(!Firebase.RTDB.setString(&fbdo, recordPath + "/vehicleName", vehicleName)) {
      Serial.println("Failed to set vehicleName: " + fbdo.errorReason());
      success = false;
    }
    if(!Firebase.RTDB.setString(&fbdo, recordPath + "/vehicleNumber", vehiclePlate)) {
      Serial.println("Failed to set vehicleNumber: " + fbdo.errorReason());
      success = false;
    }
    if(!Firebase.RTDB.setString(&fbdo, recordPath + "/entryTime", currentTime)) {
      Serial.println("Failed to set entryTime: " + fbdo.errorReason());
      success = false;
    }
    if(!Firebase.RTDB.setString(&fbdo, recordPath + "/imageBase64", base64Image)) {
      Serial.println("Failed to set imageBase64: " + fbdo.errorReason());
      success = false;
    }

    if (success) {
      Serial.println("Parking record added at: " + recordPath);
    } else {
      Serial.println("Some fields failed to save. Check error messages.");
    }

    // Debounce/wait for next trigger
    delay(2000);
  }

  // Small delay to avoid busy loop
  delay(100);
}
