/*******************************************************
   Multiple Dustbins + Ultrasonic + GSM + GPS + Firebase
   with LED Indicators Restored
*******************************************************/

#include <Arduino.h>
#include <WiFi.h>

// ========== Firebase Libraries (by mobizt) ==========
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"

// ========== TinyGPS++ for GPS ==========
#include <TinyGPS++.h>

// ========== GSM Module on Serial2 (RX=16, TX=17) ==========
#include <HardwareSerial.h>

// ------------------------------------------------------------------
// Replace these with your actual WiFi and Firebase credentials
// ------------------------------------------------------------------
const char* ssid = "MyProject";
const char* password = "12345678";

#define API_KEY       "AIzaSyCqBmV25ovNLB-7SSDyc08d_oBG4YmC54s"
#define DATABASE_URL  "https://garbagesystem-4a8e1-default-rtdb.firebaseio.com/"

// ------------------------------------------------------------------
// GSM & GPS Setup
// ------------------------------------------------------------------
HardwareSerial gsmSerial(2);  // Serial2 for GSM
HardwareSerial gpsSerial(1);  // Serial1 for GPS
TinyGPSPlus gps;

// Phone number for SMS alerts (include country code)
String phoneNumber = "+917977845638";

// ------------------------------------------------------------------
// Firebase Objects
// ------------------------------------------------------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ------------------------------------------------------------------
// LED PIN DEFINITIONS
// ------------------------------------------------------------------
const int redLED    = 32;
const int yellowLED = 33;
const int greenLED  = 25;

// ------------------------------------------------------------------
// Structure to hold dustbin data
// ------------------------------------------------------------------
struct Dustbin {
  int id;            // Unique dustbin number
  int trigPin;       // Ultrasonic trigger pin
  int echoPin;       // Ultrasonic echo pin
  float latitude;    // Current lat (could be static or from GPS)
  float longitude;   // Current lon (could be static or from GPS)
  int fullness;      // 0-100%
  bool smsSent;      // Flag to avoid repeat SMS
};

// ------------------------------------------------------------------
// Example array of multiple dustbins
// ------------------------------------------------------------------
Dustbin dustbins[] = {
  {1, 18, 19, 18.961734, 72.816222, 0, false},
  // If you have more dustbins, add them here:
  // {2, 21, 22, <lat>, <lon>, 0, false},
};
const int NUM_DUSTBINS = sizeof(dustbins) / sizeof(dustbins[0]);

// ------------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------------
float measureDistance(int trigPin, int echoPin);
void sendSMS(const String& message, const String& phoneNumber);
void updateSerial();
void sendToFirebase();
void updateDustbinData();
void updateLEDs(int fullness);

// ------------------------------------------------------------------
// SETUP
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Multiple Dustbins + Firebase Demo (with LEDs)");

  // ========== WiFi ==========
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());

  // ========== Firebase Setup ==========
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // optional debug

  // Sign up (anonymous)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase anonymous sign-up successful");
  } else {
    Serial.println("Firebase sign-up failed.");
    Serial.println(config.signer.signupError.message.c_str());
  }
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ========== GSM Setup (Serial2) ==========
  gsmSerial.begin(9600, SERIAL_8N1, 16, 17);
  delay(1000);
  Serial.println("Initializing GSM module...");
  gsmSerial.println("AT");

  // ========== GPS Setup (Serial1) ==========
  // Adjust the RX pin to your hardware wiring (default is RX=4 for many boards)
  gpsSerial.begin(9600, SERIAL_8N1, 4, -1);
  delay(1000);

  // ========== Ultrasonic Pins ==========
  for (int i = 0; i < NUM_DUSTBINS; i++) {
    pinMode(dustbins[i].trigPin, OUTPUT);
    pinMode(dustbins[i].echoPin, INPUT);
  }

  // ========== LED Pins ==========
  pinMode(redLED,    OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(greenLED,  OUTPUT);
  digitalWrite(redLED,    LOW);
  digitalWrite(yellowLED, LOW);
  digitalWrite(greenLED,  LOW);
}

// ------------------------------------------------------------------
// LOOP
// ------------------------------------------------------------------
void loop() {
  // 1) Continuously read from GPS
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
  }

  // 2) Measure each dustbin's fullness & update their GPS location
  updateDustbinData();

  // 3) Check if any dustbin is >= 95% full => send SMS if not yet sent
  for (int i = 0; i < NUM_DUSTBINS; i++) {
    if (dustbins[i].fullness >= 80 && !dustbins[i].smsSent) {
      // Build the message
      String latStr = String(dustbins[i].latitude, 6);
      String lonStr = String(dustbins[i].longitude, 6);
      String message = "Dustbin #" + String(dustbins[i].id) +
                 " is FULL (" + String(dustbins[i].fullness) + "%).\nLocation: https://maps.google.com/?q=" +
                 latStr + "," + lonStr +
                 "\nFor more details, visit: https://red-tilly-5.tiiny.site/";

      sendSMS(message, phoneNumber);
      dustbins[i].smsSent = true;
    } 
    else if (dustbins[i].fullness < 90) {
      // Reset SMS flag when dustbin level drops
      dustbins[i].smsSent = false;
    }
  }

  // 4) Send updated dustbin data to Firebase
  sendToFirebase();

  // 5) Periodically send "AT" to keep GSM awake
  static unsigned long lastATTime = 0;
  if (millis() - lastATTime > 10000) {
    gsmSerial.println("AT");
    lastATTime = millis();
    updateSerial();
  }

  // Delay to control sensor reading frequency
  delay(2000);
}

// ------------------------------------------------------------------
// UPDATE DUSTBIN DATA: measure fullness & optional GPS
// ------------------------------------------------------------------
void updateDustbinData() {
  // If using a single GPS for all bins, read once and apply to each
  float currentLat  = gps.location.isValid() ? gps.location.lat() : 18.961734; // fallback
  float currentLong = gps.location.isValid() ? gps.location.lng() : 72.816222; // fallback

  for (int i = 0; i < NUM_DUSTBINS; i++) {
    // 1) Measure distance via ultrasonic
    float distance = measureDistance(dustbins[i].trigPin, dustbins[i].echoPin);
    // Clip to 30 cm max => 0% full
    if (distance > 30) distance = 30;

    // fullness: 0 cm => 100%, 30 cm => 0%
    int fullness = (int)(((30 - distance) / 30.0) * 100);
    dustbins[i].fullness = fullness;

    // 2) Update GPS for each dustbin (if single device, all share same coords)
    dustbins[i].latitude  = currentLat;
    dustbins[i].longitude = currentLong;

    // 3) Print debug info
    Serial.print("Dustbin #");
    Serial.print(dustbins[i].id);
    Serial.print(": distance=");
    Serial.print(distance);
    Serial.print(" cm, fullness=");
    Serial.print(fullness);
    Serial.println("%");

    // 4) Update LEDs based on this dustbin's fullness
    //    (If you want a single dustbin to control the LEDs, 
    //     then just call updateLEDs() once for that dustbin.)
    updateLEDs(fullness);
  }
}

// ------------------------------------------------------------------
// LED CONTROL: Update the LED indicators based on fullness
// ------------------------------------------------------------------
void updateLEDs(int fullness) {
  if (fullness < 30) {
    // < 50% => Green ON
    digitalWrite(greenLED, HIGH);
    digitalWrite(yellowLED, LOW);
    digitalWrite(redLED, LOW);
  } 
  else if (fullness >= 30 && fullness < 70) {
    // 50% to 79% => Yellow ON
    digitalWrite(greenLED, LOW);
    digitalWrite(yellowLED, HIGH);
    digitalWrite(redLED, LOW);
  } 
  else {
    // 80% or more => Red ON
    digitalWrite(greenLED, LOW);
    digitalWrite(yellowLED, LOW);
    digitalWrite(redLED, HIGH);
  }
}

// ------------------------------------------------------------------
// MEASURE DISTANCE via Ultrasonic sensor
// ------------------------------------------------------------------
float measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Timeout 30000 µs to avoid hanging if no echo
  long duration = pulseIn(echoPin, HIGH, 30000);
  float distance = duration * 0.0343f / 2; // Speed of sound ~0.0343 cm/µs
  return distance;
}

// ------------------------------------------------------------------
// SEND UPDATED DUSTBIN DATA TO FIREBASE
// ------------------------------------------------------------------
void sendToFirebase() {
  // For each dustbin, store JSON data under /dustbins/<id>
  for (int i = 0; i < NUM_DUSTBINS; i++) {
    FirebaseJson json;
    json.set("fullness",  dustbins[i].fullness);
    json.set("latitude",  dustbins[i].latitude);
    json.set("longitude", dustbins[i].longitude);

    String path = "/dustbins/" + String(dustbins[i].id);
    if (Firebase.RTDB.setJSON(&fbdo, path, &json)) {
      Serial.print("Firebase updated for Dustbin #");
      Serial.println(dustbins[i].id);
    } else {
      Serial.print("Firebase ERROR for Dustbin #");
      Serial.print(dustbins[i].id);
      Serial.print(": ");
      Serial.println(fbdo.errorReason());
    }
  }
}

// ------------------------------------------------------------------
// SEND SMS VIA GSM
// ------------------------------------------------------------------
void sendSMS(const String& message, const String& phoneNumber) {
  Serial.println("Configuring GSM module for SMS...");
  gsmSerial.println("AT+CMGF=1");  // Set SMS mode = text
  delay(1000);
  updateSerial();

  Serial.println("Sending SMS...");
  gsmSerial.println("AT+CMGS=\"" + phoneNumber + "\"");
  delay(1000);
  updateSerial();

  gsmSerial.print(message);
  delay(100);
  updateSerial();

  // Ctrl+Z => ASCII 26
  gsmSerial.write(26);
  delay(1000);
  updateSerial();

  Serial.println("SMS sent.");
}

// ------------------------------------------------------------------
// FORWARD DATA BETWEEN ESP32 SERIAL AND GSM SERIAL2 (debug tool)
// ------------------------------------------------------------------
void updateSerial() {
  delay(100);
  while (Serial.available()) {
    gsmSerial.write(Serial.read());
  }
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());
  }
}
