/*******************************************************
   Example: Measure AC Voltage & Current using EmonLib,
   then push Voltage, Current, and Real Power to Firebase
   every 1 minute with a human-readable timestamp (IST).
*******************************************************/

// ========== Includes ==========
// Core libraries
#include <WiFi.h>
#include <WiFiClient.h>

// EmonLib (Download from https://github.com/openenergymonitor/EmonLib)
#include "EmonLib.h"  

// Firebase libraries (by mobizt)
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"

// NTP for time
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h> // for struct tm, strftime, etc.

// ========== Credentials ==========

// Wi-Fi
char ssid[] = "MyProject";
char pass[] = "12345678";

// Firebase
#define API_KEY       "AIzaSyD9YH5-wmsCc-F754d2DjF8KMCHExqyQzM"
#define DATABASE_URL  "https://energymonitoring-d8067-default-rtdb.firebaseio.com/"

// ========== Firebase Objects ==========
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth authFB;

// ========== NTP Objects ==========
// Use pool.ntp.org, offset = 19800 seconds = 5.5 hours for IST
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

// ========== EmonLib Setup ==========
// Create one EmonLib instance that reads BOTH voltage & current
EnergyMonitor emon1;

// ADC pin assignments on ESP32
// Make sure these pins are valid ADC pins on your board
#define PIN_VOLTAGE 35
#define PIN_CURRENT 34

// Calibration values — you must adjust for your setup!
double voltageCal = 234.3;  // e.g., typical for ZMPT101B
double phaseShift = 1.7;    // fine-tune for best power accuracy
double currentCal = 111.1;  // typical for SCT-013-30, etc.

// ========== Logging Interval ==========
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 60000; // 1 minute in ms

// ========== Setup ==========
void setup() {
  Serial.begin(115200);

  // 1) Connect to Wi-Fi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // 2) NTP
  timeClient.begin();

  // 3) Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // debug callback

  // Anonymous sign-up
  if (Firebase.signUp(&config, &authFB, "", "")) {
    Serial.println("Firebase signUp OK");
  } else {
    Serial.println("Firebase signUp failed");
    Serial.println(config.signer.signupError.message.c_str());
  }
  Firebase.begin(&config, &authFB);
  Firebase.reconnectWiFi(true);

  // 4) EmonLib initialization
  // The arguments are: emon1.voltage(analogPin, voltageCalibration, phaseCal);
  //                    emon1.current(analogPin, currentCalibration);
  // You must calibrate these for your sensors/setup!
  emon1.voltage(PIN_VOLTAGE, 83.3, phaseShift);
  emon1.current(PIN_CURRENT, 0.5);

  Serial.println("Setup complete!");
}

// ========== Main Loop ==========
void loop() {
  // Update NTP time
  timeClient.update();

  // Check if it's time to sample & log
  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;

    // 1) Measure voltage/current with EmonLib over multiple half-cycles
    //    (e.g., 20 half-wavelengths, up to 2 seconds max)
    emon1.calcVI(20, 2000);

    // 2) Read RMS voltage, RMS current, and real power
    double vRMS = emon1.Vrms;        // Voltage RMS
    double iRMS = emon1.Irms;        // Current RMS
    double realPower = emon1.realPower; // Active power (Watts)

    // If you want apparent power: emon1.apparentPower;
    // power factor: emon1.powerFactor;

    // 3) Get local time in human-readable format (IST)
    String timeStr = getLocalTimeString();

    // 4) Push data to Firebase
    sendToFirebase(timeStr, (float)vRMS, (float)iRMS, (float)realPower);
  }
}

// ========== Helper: Get local time string ==========
String getLocalTimeString() {
  unsigned long rawTime = timeClient.getEpochTime(); // epoch from NTP
  time_t t = (time_t)rawTime;
  struct tm *timeInfo = localtime(&t);

  // Format: "DD-MM-YYYY HH:MM:SS"
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", timeInfo);
  return String(buffer);
}

// ========== Helper: Send data to Firebase ==========
void sendToFirebase(const String &timeString, float voltage, float current, float power) {
  FirebaseJson json;
  json.set("time", timeString);
  json.set("voltage", voltage);
  json.set("current", current);
  json.set("power", power);

  // We'll push into "/powerLogs"
  String path = "/powerLogs";

  if (Firebase.RTDB.pushJSON(&fbdo, path, &json)) {
    Serial.println("Data pushed to Firebase!");
  } else {
    Serial.print("Firebase push failed: ");
    Serial.println(fbdo.errorReason());
  }
}
