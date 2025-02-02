/*
  Ration System Code - Ration Card Types
  --------------------------------------
  - Fingerprint identifies the user
  - We fetch user data (including rationCardType) from Firebase
  - Press '1' => Dispense Rice
  - Press '2' => Dispense Oil
  - Amount depends on rationCardType: 
      Yellow => Rice=1kg, Oil=1L
      White  => Rice=0.5kg, Oil=0.5L
      Orange => Rice=5kg, Oil=5L
*/

// =======================================
// 1) Include Libraries
// =======================================
#include <Arduino.h>
#include <WiFi.h>                   // For ESP32
#include <Firebase_ESP_Client.h>    // Firebase client
#include "addons/RTDBHelper.h"      // For Firebase debugging
#include "addons/TokenHelper.h"     // For token status
#include <LiquidCrystal_I2C.h>      // For 16x4 I2C LCD
#include <Keypad.h>                 // For 4x3 matrix keypad
#include <HardwareSerial.h>         // For Fingerprint sensor
#include <ESP32Servo.h>             // For servo
// Fingerprint library - example Adafruit R307 library
#include <Adafruit_Fingerprint.h>   // Example library; adapt if using a different one

// =======================================
// 2) Wi-Fi and Firebase Credentials
// =======================================
#define WIFI_SSID       "MyProject"
#define WIFI_PASSWORD   "12345678"

#define API_KEY         "AIzaSyAhflssFOujbkt-pCM07tz-E26Q0YZent8"
#define DATABASE_URL    "https://rationsystem-11dee-default-rtdb.firebaseio.com/"

// =======================================
// 3) Pin Definitions (Adjust to match your wiring)
// =======================================
#define RELAY_PIN       26
#define FLOW_SENSOR_PIN 27
#define SERVO_PIN       14

// Fingerprint sensor pins (R307)
#define FP_SERIAL_RX    16
#define FP_SERIAL_TX    17

// I2C pins for LCD (if needed)
#define I2C_SDA         21
#define I2C_SCL         22

// Keypad pins (example)
#define ROW1 32
#define ROW2 33
#define ROW3 25
#define ROW4 4
#define COL1 5
#define COL2 18
#define COL3 19

// =======================================
// 4) Globals & Objects
// =======================================

// Firebase objects
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// Flow sensor variables
volatile long flowPulses = 0;
float calibrationFactor = 450; // Example pulses per liter (MUST be calibrated!)

bool signupOK = false;

// Fingerprint Sensor Serial
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial1);

// Servo
Servo riceServo;

// LCD (address can vary, e.g., 0x3F, 0x27, etc.)
LiquidCrystal_I2C lcd(0x27, 16, 4);

// Keypad (4x3)
const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {ROW1, ROW2, ROW3, ROW4};
byte colPins[COLS] = {COL1, COL2, COL3};

// Create the Keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// userID <--> fingerIndex mapping in DB. In a real scenario, you'd fetch from DB.
int user1_fingerIndex = 1; 
int user2_fingerIndex = 2; 
int user3_fingerIndex = 3; // If you have 3 fingerprints

// =======================================
// Interrupt for flow sensor
// =======================================
void IRAM_ATTR flowCounter() {
  flowPulses++;
}

// =======================================
// Setup
// =======================================
void setup() {
  Serial.begin(115200);
  // Initialize fingerprint sensor's hardware serial
  Serial1.begin(57600, SERIAL_8N1, FP_SERIAL_RX, FP_SERIAL_TX);

  // 1) Initialize Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 2) Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  // Sign up for token (anonymous)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
    signupOK = true;
  } else {
    Serial.printf("SignUp Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // 3) Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ration System");

  // 4) Relay, Flow Sensor Setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay OFF
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowCounter, RISING);

  // 5) Servo Setup
  riceServo.attach(SERVO_PIN);
  riceServo.write(0); // start closed

  // 6) Fingerprint Sensor
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
  }

  // Small delay to stabilize
  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("Init Complete");
}

// =======================================
// Helper Functions
// =======================================

// Print a message on the LCD + Serial
void showOnLCD(const String &line1, const String &line2, 
               const String &line3="", const String &line4="") 
{
  Serial.println(line1);
  Serial.println(line2);
  Serial.println(line3);
  Serial.println(line4);

  lcd.clear();
  lcd.setCursor(0,0); lcd.print(line1);
  lcd.setCursor(0,1); lcd.print(line2);
  lcd.setCursor(0,2); lcd.print(line3);
  lcd.setCursor(0,3); lcd.print(line4);
}

// Get current inventory from Firebase
void readInventory(float &oil, float &rice) {
  if (Firebase.RTDB.getFloat(&fbdo, "/inventory/oil")) {
    oil = fbdo.floatData();
  }
  if (Firebase.RTDB.getFloat(&fbdo, "/inventory/rice")) {
    rice = fbdo.floatData();
  }
}

// Write updated inventory to Firebase
void updateInventory(float oil, float rice) {
  Firebase.RTDB.setFloat(&fbdo, "/inventory/oil", oil);
  Firebase.RTDB.setFloat(&fbdo, "/inventory/rice", rice);
}

// Get user's usage data from Firebase
bool getUserData(const String &userPath, float &oilTaken, float &riceTaken, String &rationType) 
{
  // e.g. /users/user1/oilTaken
  if (!Firebase.RTDB.getFloat(&fbdo, userPath + "/oilTaken")) {
    return false;
  }
  oilTaken = fbdo.floatData();

  if (!Firebase.RTDB.getFloat(&fbdo, userPath + "/riceTaken")) {
    return false;
  }
  riceTaken = fbdo.floatData();

  // read rationCardType
  if (!Firebase.RTDB.getString(&fbdo, userPath + "/rationCardType")) {
    return false;
  }
  rationType = fbdo.stringData(); // e.g., "Yellow", "White", "Orange"

  return true;
}

// Update user's usage data in Firebase
void updateUserData(const String &userPath, float oilTaken, float riceTaken) {
  Firebase.RTDB.setFloat(&fbdo, userPath + "/oilTaken", oilTaken);
  Firebase.RTDB.setFloat(&fbdo, userPath + "/riceTaken", riceTaken);
}

// Determine how much to dispense based on ration card type
void getDispenseAmounts(const String &cardType, float &riceAmount, float &oilAmount) {
  if (cardType == "Yellow") {
    riceAmount = 1.0;
    oilAmount  = 1.0;
  } 
  else if (cardType == "White") {
    riceAmount = 0.5;
    oilAmount  = 0.5;
  }
  else if (cardType == "Orange") {
    riceAmount = 5.0;
    oilAmount  = 5.0;
  }
  else {
    // default if unknown type
    riceAmount = 1.0;
    oilAmount  = 1.0;
  }
}

// Scan fingerprint, returns userID or -1 if fail
int scanFingerprint() {
  int result = finger.getImage();
  if (result != FINGERPRINT_OK) return -1;

  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) return -1;

  result = finger.fingerFastSearch();
  if (result != FINGERPRINT_OK) return -1;

  int fingerID = finger.fingerID; // The ID stored in sensor memory
  return fingerID;
}

// Dispense Oil using Relay + Flow Sensor
void dispenseOil(float volumeToDispense) {
  flowPulses = 0; // reset pulse count

  // Turn ON pump
  digitalWrite(RELAY_PIN, HIGH);

  float dispensedLiters = 0.0;
  // Keep pumping until we reach the desired volume
  while (dispensedLiters < volumeToDispense) {
    dispensedLiters = flowPulses / calibrationFactor;
    // Optional small delay
    delay(5);
  }

  // Turn OFF pump
  digitalWrite(RELAY_PIN, LOW);
}

// Dispense Rice using servo door (time-based)
void dispenseRice(float kgToDispense) {
  /*
    You must calibrate how many seconds your servo flap needs to be open
    for "kgToDispense" kg. For simplicity, let's assume 1 kg = 3 seconds.
    For 0.5 kg, that might be 1.5 seconds, etc.
  */
  float secondsPerKg = 3.0;  // example
  float dispenseTime = secondsPerKg * kgToDispense;

  // Open servo
  riceServo.write(90); 
  delay((unsigned long)(dispenseTime * 1000));
  // Close servo
  riceServo.write(0);
}

// =======================================
// Main Loop
// =======================================
void loop() {
  // Prompt to scan fingerprint
  showOnLCD("Scan Fingerprint", "Waiting...");
  int scannedID = scanFingerprint();

  if (scannedID == -1) {
    // No fingerprint match or none scanned
    delay(1000); 
    return;
  }
  // Finger recognized
  showOnLCD("Fingerprint Found!", "ID: " + String(scannedID));
  delay(1000);

  // Identify user path in DB
  String userPath;
  if (scannedID == 1) userPath = "/users/user1";
  else if (scannedID == 2) userPath = "/users/user2";
  else if (scannedID == 3) userPath = "/users/user3";
  else {
    showOnLCD("User not found", "In DB!");
    delay(2000);
    return;
  }

  // Read user data: oilTaken, riceTaken, rationCardType
  float userOilTaken, userRiceTaken;
  String rationType;
  if (!getUserData(userPath, userOilTaken, userRiceTaken, rationType)) {
    showOnLCD("DB Error", "Could not read user!");
    delay(2000);
    return;
  }

  // Determine how much Rice/Oil to dispense for this ration card
  float rationRice, rationOil;
  getDispenseAmounts(rationType, rationRice, rationOil);

  // Now prompt user: '1' => Rice, '2' => Oil
  showOnLCD("Press '1' => Rice", "Press '2' => Oil");
  while (true) {
    char selection = keypad.getKey();
    if (!selection) {
      delay(100);
      continue; // no key pressed
    }

    if (selection == '1') {
      // Rice selected
      showOnLCD("Rice selected", "Dispensing...");
      // 1) Read inventory
      float currentOil, currentRice;
      readInventory(currentOil, currentRice);

      if (currentRice < rationRice) {
        showOnLCD("Not Enough Rice!", "");
        delay(2000);
        break;
      }

      // 2) Dispense Rice
      dispenseRice(rationRice);

      // 3) Update inventory
      currentRice -= rationRice;
      updateInventory(currentOil, currentRice);

      // 4) Update user stats
      userRiceTaken += rationRice;
      updateUserData(userPath, userOilTaken, userRiceTaken);

      showOnLCD("Rice Dispensed!", "", "Remaining Rice: " + String(currentRice));
      delay(3000);
      break; // exit while
    }
    else if (selection == '2') {
      // Oil selected
      showOnLCD("Oil selected", "Dispensing...");
      // 1) Read inventory
      float currentOil, currentRice;
      readInventory(currentOil, currentRice);

      if (currentOil < rationOil) {
        showOnLCD("Not Enough Oil!", "");
        delay(2000);
        break;
      }

      // 2) Dispense Oil
      dispenseOil(rationOil);

      // 3) Update inventory
      currentOil -= rationOil;
      updateInventory(currentOil, currentRice);

      // 4) Update user stats
      userOilTaken += rationOil;
      updateUserData(userPath, userOilTaken, userRiceTaken);

      showOnLCD("Oil Dispensed!", "", "Remaining Oil: " + String(currentOil));
      delay(3000);
      break; // exit while
    }
    // If pressed other key, ignore or add error message
  }
}
