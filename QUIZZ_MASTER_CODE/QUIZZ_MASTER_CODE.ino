/*
  MASTER – Classroom (Polling version)
  ------------------------------------
  - WiFi STA connect -> discover router CH
  - ESP-NOW receive (students broadcast)
  - Firebase_ESP_Client (mobizt) with anonymous auth
  - POLLS /meta every ~1s (no stream)
  - On meta change: subject/bank/currentQ/status/totalStudents -> update local state, fetch correct key
  - On each student answer: write /answers/{qid}/{sid} and update /counts/{qid}

  RTDB paths (ORG/SESS configurable):
    /orgs/{ORG}/sessions/{SESS}/meta
    /orgs/{ORG}/sessions/{SESS}/answers/{qid}/{studentId}
    /orgs/{ORG}/sessions/{SESS}/counts/{qid}
    /orgs/{ORG}/subjects/{subject}/banks/{bank}/questions/{qid}/correct
*/

#include <WiFi.h>
#include "ESP32_NOW.h"
#include <ArduinoJson.h>

// ---- Firebase (mobizt) ----
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ---- Time (SNTP) ----
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct PendingWrite {
  uint16_t qid;
  uint16_t sid;
  uint8_t  ans;
  uint32_t seq;
  uint32_t ts;   // millis() at RX time (optional)
};

QueueHandle_t qWrites = nullptr;

// counts push throttling
volatile bool countsDirty = false;
uint32_t lastCountsPush = 0;
const uint32_t COUNTS_PUSH_EVERY_MS = 400;
// ================== USER CONFIG ==================
static const char* WIFI_SSID = "AAA";
static const char* WIFI_PASS = "Acube123";

#define API_KEY       "AIzaSyDhBSbYp_I34tSnOwbQgL9F-k8_-vhmgAs"
#define DATABASE_URL  "https://quizapp-c2c05-default-rtdb.firebaseio.com/"   // keep trailing slash

static const char* ORG_ID  = "cmh";
static const char* SESS_ID = "2025-11-quiz1";

#define LED_GREEN 2
#define LED_RED   4
static const uint8_t MAX_STUDENTS = 40;

// ================== TYPES ==================
struct __attribute__((packed)) AnswerMsg {
  uint16_t id;   // 1..MAX_STUDENTS
  uint8_t  q;    // unused (master owns)
  uint8_t  ans;  // 1..4
  uint32_t seq;  // de-dup
};

// ================== STATE ==================
static String   g_subjectId = "";
static String   g_bankId    = "";
static uint16_t g_currentQ  = 1;
static String   g_statusStr = "idle";    // "idle" | "running" | "done"
static uint16_t g_totalStudents = 6;
static uint8_t  g_correctKey = 1;

static uint8_t  g_nowChannel = 1;

static uint32_t lastSeqById[ MAX_STUDENTS + 1 ] = {0};
static bool     answeredById[ MAX_STUDENTS + 1 ] = {false};
static uint8_t  countsC[5] = {0,0,0,0,0};
static uint16_t answered=0, correct=0, incorrect=0;

// ================== Firebase objects ==================
FirebaseData   fbdo;     // reads/writes
FirebaseAuth   auth;
FirebaseConfig config;

// ================== Helpers ==================
// POSIX TZ string (IST = UTC+5:30 -> "IST-5:30")
static const char* TZ_STRING = "IST-5:30";
bool syncTimeOnce(uint32_t timeoutMs = 15000) {
  setenv("TZ", TZ_STRING, 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

  struct tm tmnow;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (getLocalTime(&tmnow, 1000)) {
      char buf[32];
      strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", &tmnow);
      Serial.printf("[TIME] OK: %s\n", buf);
      return true;
    }
  }
  Serial.println("[TIME] Failed to sync time");
  return false;
}

// Firebase path helpers
String pathMeta()           { return "/orgs/" + String(ORG_ID) + "/sessions/" + String(SESS_ID) + "/meta"; }
String pathAnswers(uint16_t q){ return "/orgs/" + String(ORG_ID) + "/sessions/" + String(SESS_ID) + "/answers/" + String(q); }
String pathCounts (uint16_t q){ return "/orgs/" + String(ORG_ID) + "/sessions/" + String(SESS_ID) + "/counts/"  + String(q); }
String pathCorrect(uint16_t q){ return "/orgs/" + String(ORG_ID) + "/subjects/" + g_subjectId + "/banks/" + g_bankId + "/questions/" + String(q) + "/correct"; }

// Small JSON getters (mobizt style)
bool jgetStr(FirebaseJson &j, const char* p, String &out){
  FirebaseJsonData d; if (j.get(d, p) && d.success) { out = d.to<String>(); return true; } return false;
}
bool jgetInt(FirebaseJson &j, const char* p, int &out){
  FirebaseJsonData d; if (j.get(d, p) && d.success) { out = d.to<int>(); return true; } return false;
}

// Reset tallies for a question
void resetTalliesFor(uint16_t qid){
  memset(countsC, 0, sizeof(countsC));
  memset(answeredById, 0, sizeof(answeredById));
  memset(lastSeqById, 0, sizeof(lastSeqById));
  answered = correct = incorrect = 0;
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);

  // fetch correct key
  if (g_subjectId.length() && g_bankId.length()) {
    if (Firebase.RTDB.getInt(&fbdo, pathCorrect(qid).c_str())) {
      int v = fbdo.intData();
      if (v >= 1 && v <= 4) g_correctKey = (uint8_t)v;
    } else {
      Serial.printf("[FB] getInt(correct) err: %s\n", fbdo.errorReason().c_str());
    }
  }
  Serial.printf("[TALLY] Reset Q%u, correct=%u\n", qid, g_correctKey);
}

// Write one student's answer
void writeAnswer(uint16_t qid, uint16_t sid, uint8_t ans, uint32_t seq){
  FirebaseJson json;
  json.set("ans", ans);
  json.set("seq", seq);
  json.set("ts",  (uint64_t)millis()); // swap to epoch if you sync NTP and want real time
  String p = pathAnswers(qid) + "/" + String(sid);
  if (!Firebase.RTDB.setJSON(&fbdo, p.c_str(), &json)) {
    Serial.printf("[FB] setJSON(answers) err: %s\n", fbdo.errorReason().c_str());
  }
}

// Write counts node
void writeCounts(uint16_t qid){
  FirebaseJson json;
  json.set("c1", countsC[1]);
  json.set("c2", countsC[2]);
  json.set("c3", countsC[3]);
  json.set("c4", countsC[4]);
  json.set("answered",  answered);
  json.set("correct",   correct);
  json.set("incorrect", incorrect);

  const String p = pathCounts(qid);
  if (!Firebase.RTDB.updateNode(&fbdo, p.c_str(), &json)) {
    if (!Firebase.RTDB.setJSON(&fbdo, p.c_str(), &json)) {
      Serial.printf("[FB] update/set(counts) err: %s\n", fbdo.errorReason().c_str());
    }
  }
}

// Polling meta (call from loop)
uint32_t lastMetaPoll = 0;
const uint32_t META_POLL_MS = 1000;  // 1s
void pollMeta(){
  if (millis() - lastMetaPoll < META_POLL_MS) return;
  lastMetaPoll = millis();
  if (!Firebase.ready()) return;

  FirebaseJson meta;
  if (!Firebase.RTDB.getJSON(&fbdo, pathMeta().c_str(), &meta)) {
    Serial.printf("[FB] getJSON(meta) err: %s\n", fbdo.errorReason().c_str());
    return;
  }

  String sId = g_subjectId, bId = g_bankId, stat = g_statusStr;
  int cq = (int)g_currentQ, ts = (int)g_totalStudents;

  jgetStr(meta, "/subjectId", sId);
  jgetStr(meta, "/bankId",    bId);
  jgetStr(meta, "/status",    stat);
  jgetInt(meta, "/currentQ",  cq);
  jgetInt(meta, "/totalStudents", ts);

  bool changedBank = (sId != g_subjectId) || (bId != g_bankId);
  bool changedQ    = (cq != (int)g_currentQ);
  bool changedSt   = (stat != g_statusStr);

  g_subjectId = sId;
  g_bankId    = bId;
  g_totalStudents = (uint16_t)ts;

  if (changedBank) {
    Serial.printf("[META] Subject=%s Bank=%s\n", g_subjectId.c_str(), g_bankId.c_str());
  }
  if (changedQ) {
    g_currentQ = (uint16_t)cq;
    resetTalliesFor(g_currentQ);
  }
  if (changedSt) {
    g_statusStr = stat;
    Serial.printf("[META] Status=%s\n", g_statusStr.c_str());
  }
}

// ================== ESP-NOW ==================
class StudentPeer : public ESP_NOW_Peer {
public:
  StudentPeer(const uint8_t* mac, uint8_t ch)
  : ESP_NOW_Peer(mac, ch, WIFI_IF_STA, nullptr) {}
  bool add_peer(){ return add(); }

  void onReceive(const uint8_t* data, size_t len, bool broadcast) override {
    if (len != sizeof(AnswerMsg)) {
      Serial.printf("[RX] " MACSTR " invalid=%u\n", MAC2STR(addr()), (unsigned)len);
      return;
    }
    const AnswerMsg* m = reinterpret_cast<const AnswerMsg*>(data);
    Serial.printf("[RX] " MACSTR " id=%u ans=%u seq=%lu %s\n",
                  MAC2STR(addr()), m->id, m->ans, (unsigned long)m->seq,
                  broadcast ? "BCAST":"UNIC");

    if (g_statusStr != "running") return;
    if (m->id < 1 || m->id > MAX_STUDENTS) return;

    // de-dup
    if (lastSeqById[m->id] == m->seq) return;
    lastSeqById[m->id] = m->seq;

    if (!answeredById[m->id]) {
      answeredById[m->id] = true;
      if (m->ans >= 1 && m->ans <= 4) {
        countsC[m->ans]++;
        if (m->ans == g_correctKey) correct++; else incorrect++;
      }
      answered++;
      if (answered >= g_totalStudents) { digitalWrite(LED_RED, LOW); digitalWrite(LED_GREEN, HIGH); }
    }
    countsDirty = true;
PendingWrite pw{ g_currentQ, m->id, m->ans, m->seq, millis() };
xQueueSend(qWrites, &pw, 0);
  }
};

std::vector<StudentPeer> g_students;

#include "esp_wifi_types.h"
static void onNewPeer(const esp_now_recv_info_t* info, const uint8_t* data, int len, void* arg) {
  if (!info) return;
  const wifi_pkt_rx_ctrl_t* rxc = info->rx_ctrl;
  uint8_t rxCh = rxc ? rxc->channel : g_nowChannel;
  Serial.printf("[NEW] peer " MACSTR " rxCh=%u -> reg on CH=%u\n",
                MAC2STR(info->src_addr), rxCh, g_nowChannel);

  g_students.emplace_back(info->src_addr, g_nowChannel);
  if (!g_students.back().add_peer()) {
    Serial.println("[NEW] add_peer FAILED");
    g_students.pop_back();
  }
}
bool doWriteAnswer(const PendingWrite& pw){
  FirebaseJson json;
  json.set("ans", pw.ans);
  json.set("seq", pw.seq);
  json.set("ts",  (uint64_t)pw.ts);

  String p = pathAnswers(pw.qid) + "/" + String(pw.sid);
  for (int i=0;i<3;i++){
    if (Firebase.RTDB.setJSON(&fbdo, p.c_str(), &json)) return true;
    Serial.printf("[FB] setJSON(answers) try%d err: %s\n", i+1, fbdo.errorReason().c_str());
    delay(150*(i+1));
  }
  return false;
}

bool doWriteCounts(uint16_t qid){
  FirebaseJson json;
  json.set("c1", countsC[1]); json.set("c2", countsC[2]);
  json.set("c3", countsC[3]); json.set("c4", countsC[4]);
  json.set("answered", answered);
  json.set("correct", correct);
  json.set("incorrect", incorrect);

  String p = pathCounts(qid);
  for (int i=0;i<2;i++){
    if (Firebase.RTDB.updateNode(&fbdo, p.c_str(), &json)) return true;
    // fallback to set on 2nd try
    if (Firebase.RTDB.setJSON(&fbdo, p.c_str(), &json)) return true;
    Serial.printf("[FB] upsert(counts) try%d err: %s\n", i+1, fbdo.errorReason().c_str());
    delay(120*(i+1));
  }
  return false;
}

// ================== SETUP / LOOP ==================
void setup(){
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== MASTER – Firebase (Polling) ===");

  // Wi-Fi first
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
    if (millis() - t0 > 20000) { Serial.println("\n[WiFi] Timeout, reboot"); delay(500); ESP.restart(); }
  }
  g_nowChannel = WiFi.channel();
  Serial.printf("\n[WiFi] OK IP=%s CH=%u RSSI=%d\n",
                WiFi.localIP().toString().c_str(), g_nowChannel, WiFi.RSSI());

  // Time (TLS needs correct clock)
  syncTimeOnce(15000);

  // ESP-NOW
  if (!ESP_NOW.begin()) { Serial.println("[ESPNOW] begin FAILED"); delay(1000); ESP.restart(); }
  ESP_NOW.onNewPeer(onNewPeer, nullptr);
  Serial.printf("[ESPNOW] ready on CH=%u\n", g_nowChannel);

  // Firebase
  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;      // trailing '/'
  config.token_status_callback = tokenStatusCallback;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(8192);
  config.timeout.serverResponse = 15000;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("[FB] signUp OK");
  } else {
    Serial.printf("[FB] signUp FAIL: %s\n", config.signer.signupError.message.c_str());
  }
  Firebase.begin(&config, &auth);

  // Initial meta read (non-fatal if fails; poll will retry)
  FirebaseJson meta;
  if (Firebase.RTDB.getJSON(&fbdo, pathMeta().c_str(), &meta)) {
    String sId, bId, stat; int cq=(int)g_currentQ, ts=(int)g_totalStudents;
    jgetStr(meta, "/subjectId", sId);
    jgetStr(meta, "/bankId",    bId);
    jgetStr(meta, "/status",    stat);
    jgetInt(meta, "/currentQ",  cq);
    jgetInt(meta, "/totalStudents", ts);
    g_subjectId = sId; g_bankId = bId; g_statusStr = stat;
    g_currentQ  = (uint16_t)cq; g_totalStudents = (uint16_t)ts;

    Serial.printf("[META] Subject=%s Bank=%s Status=%s Q=%u Students=%u\n",
                  g_subjectId.c_str(), g_bankId.c_str(), g_statusStr.c_str(),
                  g_currentQ, g_totalStudents);
  } else {
    Serial.printf("[FB] initial meta err: %s\n", fbdo.errorReason().c_str());
  }

  resetTalliesFor(g_currentQ);
  qWrites = xQueueCreate(64, sizeof(PendingWrite));  // room for bursts
if (!qWrites) { Serial.println("[Q] create FAILED"); while(1) delay(1000); }

}

void loop(){
  // Poll meta every 1s
  pollMeta();
  if (Firebase.ready()) {
  PendingWrite pw;
  int processed = 0;
  while (processed < 4 && xQueueReceive(qWrites, &pw, 0) == pdTRUE) {
    doWriteAnswer(pw);
    processed++;
  }

  // push counts at most every ~400ms when changed
  uint32_t now = millis();
  if (countsDirty && (now - lastCountsPush >= COUNTS_PUSH_EVERY_MS)) {
    doWriteCounts(g_currentQ);
    countsDirty = false;
    lastCountsPush = now;
  }
}
  // Event-driven ESP-NOW RX handled by StudentPeer::onReceive
}
