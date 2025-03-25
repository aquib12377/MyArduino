/***************************************************************************************
 * Example ESP32 Code for RFID Check-In / Check-Out in Firebase and SOS SMS via SIM80L
 ***************************************************************************************/

#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

// If using Firebase-ESP-Client
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
// =========== Replace with your actual WiFi credentials ============
#define WIFI_SSID "MyProject"
#define WIFI_PASSWORD "12345678"

// =========== Replace with your Firebase project credentials =======
// For Firebase-ESP-Client:
#define API_KEY "AIzaSyAcJAk2HWXjUd17D_LQqGsYaEIiVvdAH9Y"
#define DATABASE_URL "https://logistic-management-77548-default-rtdb.firebaseio.com/"  // e.g. "https://my-project.firebaseio.com"

// =========== GSM / SIM80L settings ============
#define SOS_PHONE_NUMBER "+919359951778"  // The phone number to send SMS
// The TX and RX pins used for hardware serial with SIM80L
#define GSM_TX_PIN 17
#define GSM_RX_PIN 16

// =========== RFID settings ============
#define RST_PIN 4    // Configurable, check your wiring
#define SS_PIN  5    // Configurable, check your wiring

// =========== SOS button pin ============
#define SOS_BUTTON_PIN 13  // Adjust to your wiring

// Create instances
MFRC522 rfid(SS_PIN, RST_PIN);
HardwareSerial simSerial(1); // Use UART1 for GSM, or whichever is free

// Firebase objects (if using Firebase-ESP-Client)
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// We'll keep track of who is currently "in" using a simple map of <String UID, bool isCheckedIn>
#include <map>
std::map<String, bool> checkInMap;

/**
 * Helper function to get the current timestamp string
 * Format: "YYYY-MM-DD HH:MM:SS"
 */
String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return String("TimeError");
  }
  char timeStringBuff[25];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

/**
 * Connect to Wi-Fi
 */
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected! IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * Initialize Firebase
 * If using a different library, adjust accordingly
 */
void initFirebase() {
  // For Firebase-ESP-Client
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Sign up options if needed
  auth.user.email = "";
  auth.user.password = "";

  // or use custom token auth
  // auth.token.uid = "user_id";
  
  // Initialize the library
  Firebase.begin(&config, &auth);
  // Optional: set the time to synchronize with NTP for accurate timestamps
  Firebase.setFloatDigits(2);
  
  // Wait for Firebase to be ready
  Serial.println("Firebase initialized.");
}

/**
 * Post Check-In or Check-Out event to Firebase
 */
void postEventToFirebase(const String& uid, const String& status) {
  // e.g. push to path "RFID_Logs/uid/timestamp"
  // or use a push node for auto ID
  String path = "/RFID_Logs/" + uid;
  
  String timestampStr = getTimestamp();
  
  // Example JSON data
  FirebaseJson json;
  json.set("status", status);
  json.set("timestamp", timestampStr);
  
  // push to the path
  bool pushPath = Firebase.RTDB.pushJSON(&fbdo, path, &json);
  
  if (fbdo.httpCode() == 200) {
    Serial.println("Data pushed successfully to: " + pushPath);
  } else {
    Serial.print("Push failed, reason: ");
    Serial.println(fbdo.errorReason());
  }
}

/**
 * Send an SOS SMS with a hard-coded location
 */
void sendSOS() {
  Serial.println("SOS button pressed! Sending SMS...");
  
  // Hardcode your location - "Nwo6" or something else
  String location = "https://maps.app.goo.gl/sZ9T8NYrHXW4jdDy6";  
  String message = "SOS! Please help! Location: " + location;
  
  // For many GSM modules, you can send SMS via AT commands:
  simSerial.println("AT");            // Wake up the module
  delay(1000);
  simSerial.println("AT+CMGF=1");     // Set SMS text mode
  delay(1000);
  // Phone number must be in international format with + sign
  simSerial.print("AT+CMGS=\"");
  simSerial.print(SOS_PHONE_NUMBER);
  simSerial.println("\"");
  delay(1000);
  simSerial.print(message);
  // Ctrl+Z (ASCII 26) to send SMS
  simSerial.write(26);
  delay(3000);
  
  Serial.println("SOS SMS command sent to the SIM80L module.");
}

void setup() {
  Serial.begin(115200);        // Debug serial
  SPI.begin();                 // Initialize SPI bus for RFID
  rfid.PCD_Init();            // Init MFRC522
  
  // Connect to Wi-Fi
  connectToWiFi();
  
  // Init and sync time (optional but recommended for correct timestamps)
  configTime(0, 0, "pool.ntp.org");  // Using NTP
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // debug callback

  // Anonymous sign-up
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signUp OK");
  } else {
    Serial.println("Firebase signUp failed");
    Serial.println(config.signer.signupError.message.c_str());
  }
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  
  // Initialize SIM80L serial
  simSerial.begin(9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  
  // Setup SOS button
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("Setup complete. Ready to read RFID and handle SOS.");
}

void loop() {
  // 1. Check for SOS button press
  if (digitalRead(SOS_BUTTON_PIN) == LOW) {
    // Debounce check (simple)
    delay(50);
    if (digitalRead(SOS_BUTTON_PIN) == LOW) {
      sendSOS();
      // Wait until button is released to avoid spamming
      while (digitalRead(SOS_BUTTON_PIN) == LOW) {
        delay(50);
      }
    }
  }
  
  // 2. Check for RFID card
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }
  
  // Get the UID as a string
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase(); // optional to keep consistent formatting
  
  Serial.println("RFID Detected. UID: " + uidStr);
  
  // Toggle check-in / check-out logic
  bool isCurrentlyCheckedIn = false;
  if (checkInMap.find(uidStr) != checkInMap.end()) {
    isCurrentlyCheckedIn = checkInMap[uidStr];
  }
  
  if (!isCurrentlyCheckedIn) {
    // Perform Check-In
    Serial.println("Performing Check-IN for " + uidStr);
    checkInMap[uidStr] = true;
    postEventToFirebase(uidStr, "Check-In");
  } else {
    // Perform Check-Out
    Serial.println("Performing Check-OUT for " + uidStr);
    checkInMap[uidStr] = false;
    postEventToFirebase(uidStr, "Check-Out");
  }
  
  // Halt PICC so it doesn't read the same card repeatedly
  rfid.PICC_HaltA();
}
