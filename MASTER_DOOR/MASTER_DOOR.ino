/****************************************************************
   ESP32 Door-Master  (mixed-channel, ID map)
   ------------------------------------------------------------
   • Sensors transmit : DOOR|<MAC_3B>|CLOSED#N      on SENSOR_CH
   • Router channel   : arbitrary (STA joins it)
   • Firebase tree    : /doors/door1 … door4  (bool flags)
****************************************************************/
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"

/* ───────── project settings ───────── */
#define SENSOR_CH 6
#define ROUTER_SSID "MyProject"
#define ROUTER_PASS "12345678"

#define API_KEY "AIzaSyCAndb2u1X1I8nnXScTy-2dl7bRr5Ms0Y8"
#define DB_URL "https://doormonitoring-92404-default-rtdb.firebaseio.com/"

/* LISTEN / UPLOAD timing (ms) */
#define MAX_LISTEN_MS 500  // ESPNOW window before upload
#define WIFI_UP_TIMEOUT 6000
#define RELAY_HOLD_MS 30000

/* ───────── hardware ───────── */
#define RELAY_PIN 25  // active-LOW

/* ───────── Firebase objects ───────── */
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig cfg;
bool firebaseInit = false;

/* ───────── simple ID → key mapping ─────────
   Fill in the real IDs you see in Serial Monitor.
*/
struct IdMap {
  const char *id;
  const char *key;
};
const IdMap ID_MAP[] = {
  { "2E28B4", "door1" },
  { "328770", "door2" },
  { "EEAA9C", "door3" },
  { "A945C8", "door4" },
};
const size_t MAP_LEN = sizeof(ID_MAP) / sizeof(ID_MAP[0]);

/* helper: convert sensor ID to Firebase key; return empty string if unknown */
String mapId(String id) {
  for (size_t i = 0; i < MAP_LEN; ++i)
    if (id.equalsIgnoreCase(ID_MAP[i].id)) return ID_MAP[i].key;
  return String();  // not found
}

/* ───────── relay helpers ───────── */
bool relayState = false;
uint32_t relayOnSinceMs = 0;
void relaySet(bool on) {
  if (relayState != on) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
    relayState = on;
    Serial.printf("[RELAY] -> %s\n", on ? "ON" : "OFF");
  }
}

/* ───────── tiny event queue (8) ───────── */
struct DoorEvt {
  String payload;
};
bool dequeue(DoorEvt &e);
const uint8_t QSIZE = 8;
DoorEvt q[QSIZE];
volatile uint8_t qHead = 0, qTail = 0;

inline bool qFull() {
  return uint8_t(qHead + 1) % QSIZE == qTail;
}
inline bool qEmpty() {
  return qHead == qTail;
}
void enqueue(const String &p) {
  if (qFull()) {
    Serial.println("[WARN] queue full – dropped");
    return;
  }
  q[qHead].payload = p;
  qHead = (qHead + 1) % QSIZE;
}
bool dequeue(DoorEvt &e) {
  if (qEmpty()) return false;
  e = q[qTail];
  qTail = (qTail + 1) % QSIZE;
  return true;
}

/* ───────── ESP-NOW helpers ───────── */
void startEspNow() {
  WiFi.mode(WIFI_STA);  // idle STA (no AP)
  WiFi.disconnect(true, true);
  esp_wifi_set_channel(SENSOR_CH, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("NOW init FAIL");
    ESP.restart();
  }
  esp_now_register_recv_cb([](const esp_now_recv_info_t *,
                              const uint8_t *data, int len) {
    String pl((const char *)data, len);
    if (pl.startsWith("DOOR|")) enqueue(pl);
  });
  Serial.printf("[NOW ] listening on CH-%d\n", SENSOR_CH);
}

void stopEspNow() {
  esp_now_unregister_recv_cb();
  esp_now_deinit();
}

/* ───────── Firebase helper ───────── */
void ensureFirebaseBegun() {
  if (firebaseInit) return;
  cfg.api_key = API_KEY;
  cfg.database_url = DB_URL;
  cfg.token_status_callback = tokenStatusCallback;
  Firebase.reconnectWiFi(true);

  if (Firebase.signUp(&cfg, &auth, "", "")) {
    Serial.println("[FB  ] sign-up OK");
  } else {
    Serial.printf("[FB  ] sign-up FAIL: %s\n",
                  cfg.signer.signupError.message.c_str());
  }
  Firebase.begin(&cfg, &auth);
  firebaseInit = true;
}

/* ───────── radio phases ───────── */
enum Phase { LISTEN,
             WIFI_CONNECT,
             UPLOAD };
Phase phase = LISTEN;
uint32_t phaseStart = 0;

/* ───────── setup ───────── */
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  startEspNow();

  phaseStart = millis();
}

/* ───────── main loop ───────── */
uint32_t lastDiag = 0;

void loop() {
  uint32_t now = millis();

  /* — LISTEN — */
  if (phase == LISTEN) {
    if (relayState && now - relayOnSinceMs >= RELAY_HOLD_MS) {
      relaySet(false);
      Serial.println("[TIMEOUT] RELAY OFF");
    }
    static uint32_t firstEvt = 0;
    if (!qEmpty()) {
      if (!firstEvt) firstEvt = now;
      if (now - firstEvt >= MAX_LISTEN_MS) {
        stopEspNow();
        WiFi.mode(WIFI_STA);
        WiFi.begin(ROUTER_SSID, ROUTER_PASS);
        phase = WIFI_CONNECT;
        phaseStart = now;
        Serial.println("[WIFI] connecting …");
      }
    } else {
      firstEvt = 0;
    }
  }

  /* — WIFI_CONNECT — */
  else if (phase == WIFI_CONNECT) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] OK  CH=%d  IP=%s\n",
                    WiFi.channel(), WiFi.localIP().toString().c_str());
      ensureFirebaseBegun();
      phase = UPLOAD;
    } else if (now - phaseStart >= WIFI_UP_TIMEOUT) {
      Serial.println("[WIFI] timeout, back to LISTEN");
      WiFi.disconnect(true, true);
      startEspNow();
      phase = LISTEN;
    }
  }

  /* — UPLOAD — */
  else if (phase == UPLOAD && Firebase.ready()) {
    DoorEvt e;
    while (dequeue(e)) {
      Serial.printf("[PROC] %s\n", e.payload.c_str());

      int p1 = e.payload.indexOf('|');
      int p2 = e.payload.indexOf('|', p1 + 1);
      if (p1 < 0 || p2 < 0) {
        Serial.println("[ERR ] bad format");
        continue;
      }

      String id = e.payload.substring(p1 + 1, p2);  // e.g. 2E28B4
      String dbKey = mapId(id);
      if (dbKey.isEmpty()) {
        Serial.printf("[WARN] unknown ID %s – ignored\n", id.c_str());
        continue;
      }

      String path = "/doors/" + dbKey;
      bool flag = false;
      if (Firebase.RTDB.getBool(&fbdo, path)) {
        flag = fbdo.boolData();
        Serial.printf("[FB  ] %s is %s\n", path.c_str(), flag ? "OPEN" : "CLOSED");
        
        if(!flag)
        {
          relaySet(true);
          relayOnSinceMs = millis();
        }
      } else {
        Serial.printf("[FB  ] Failed to read %s: %s\n", path.c_str(), fbdo.errorReason().c_str());
      }
    }

    if (qEmpty()) {
      phase = LISTEN;  // change phase **before** disconnecting
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      delay(50);
      startEspNow();
      phaseStart = now;
    }
  }

  /* — diagnostics — */
  if (now - lastDiag >= 1000) {
    lastDiag = now;
    Serial.printf("[DIAG] %s  Heap=%u  Q=%u/%u  Phase=%d\n",
                  (phase == LISTEN ? "LISTEN" : phase == WIFI_CONNECT ? "WIFI_CONNECT"
                                                                      : "UPLOAD"),
                  ESP.getFreeHeap(),
                  (qHead + QSIZE - qTail) % QSIZE, QSIZE - 1, phase);
  }
  delay(5);
}
