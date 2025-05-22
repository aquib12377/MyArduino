/*
 * Master node – ESP32-WROOM-32
 * • ESP-NOW door events from four slaves
 * • Firebase JSON  /doorsData/doors/1…4/{auth,state}
 * • Relay triggers ONLY when auth == 0
 *   (no time-window logic any more)
 * Tested on Arduino-ESP32 core 3.2.0 + Firebase-ESP-Client 4.4.17
 */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
#include "door_common.h"

/* ------------ credentials ------------ */
#define WIFI_SSID  "MyProject"
#define WIFI_PASS  "12345678"
#define API_KEY    "AIzaSyCAndb2u1X1I8nnXScTy-2dl7bRr5Ms0Y8"
#define DB_URL     "https://doormonitoring-92404-default-rtdb.firebaseio.com/"

/* ------------ hardware ------------ */
constexpr int  RELAY_PIN       = 25;   // active-LOW relay
constexpr long RELAY_PULSE_MS  = 5000; // siren length
constexpr long POLL_MS         = 5000; // poll auth every 5 s

/* ------------ Firebase objects ------------ */
FirebaseData   fbGet;
FirebaseData   fbPush;
FirebaseAuth   auth;
FirebaseConfig config;

/* ------------ local state ------------ */
bool  doorAuth[4]    = {false,false,false,false};  // true → auth==1
bool  relayOn        = false;
unsigned long relayOffAt = 0;
unsigned long lastPoll   = 0;

/* ------------ helpers ------------ */
String pathAuth(uint8_t id)  { return "/doorsData/doors/" + String(id) + "/auth";  }
String pathState(uint8_t id) { return "/doorsData/doors/" + String(id) + "/state"; }

void seedDoorsDocument()
{
  if (Firebase.RTDB.getJSON(&fbGet, "/doorsData")) return;   // already there

  FirebaseJson j;
  for (int i = 1; i <= 4; ++i) {
    j.set("doors/[" + String(i) + "]/auth",  0);
    j.set("doors/[" + String(i) + "]/state", 0);
  }
  Firebase.RTDB.setJSON(&fbPush, "/doorsData", &j);
}

void writeAuth(uint8_t door, int value)
{
  Firebase.RTDB.setInt(&fbPush, pathAuth(door), value);
}

/* ------------ ESP-NOW callback ------------ */
void onDoorMsg(const esp_now_recv_info_t*, const uint8_t* data, int len)
{
  if (len != sizeof(DoorMsg)) return;
  DoorMsg msg;  memcpy(&msg, data, len);

  const uint8_t door = msg.doorId + 1;             // 1-based for Firebase
  Serial.printf("[ESP-NOW] Door %u → %s\n",
                door, msg.state ? "OPEN" : "CLOSE");

  if (msg.state == CMD_OPEN) {
    if (doorAuth[msg.doorId]) {                     // authorised?
      Serial.printf("[AUTH] Door %u accepted ✓\n", door);
      doorAuth[msg.doorId] = false;                 // consume
      writeAuth(door, 0);                           // reset in DB
    } else {
      Serial.printf("[ALARM] Door %u unauthorised!\n", door);
      digitalWrite(RELAY_PIN, LOW);
      relayOn    = true;
      relayOffAt = millis() + RELAY_PULSE_MS;
    }
  }
  Firebase.RTDB.setInt(&fbPush, pathState(door), msg.state);
}

/* ------------ poll auth every 5 s ------------ */
void pollAuth()
{
  for (uint8_t i = 1; i <= 4; ++i) {
    yield();                                        // keep watchdog happy

    if (Firebase.RTDB.getInt(&fbGet, pathAuth(i))) {
      doorAuth[i-1] = fbGet.intData() != 0;         // store as bool
    }
    else if (fbGet.errorReason() == "path not exist") {
      Firebase.RTDB.setInt(&fbPush, pathAuth(i), 0);
      Firebase.RTDB.setInt(&fbPush, pathState(i), 0);
      doorAuth[i-1] = false;
    }
  }
}

/* ------------ setup ------------ */
void setup()
{
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);  digitalWrite(RELAY_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  config.api_key      = API_KEY;
  config.database_url = DB_URL;
  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  seedDoorsDocument();

  esp_now_init();
  esp_now_register_recv_cb(onDoorMsg);

  Serial.println("Master ready ✔");
}

/* ------------ loop ------------ */
void loop()
{
  if (relayOn && millis() > relayOffAt) {
    digitalWrite(RELAY_PIN, HIGH);
    relayOn = false;
  }

  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    if (Firebase.ready()) pollAuth();
  }
}
