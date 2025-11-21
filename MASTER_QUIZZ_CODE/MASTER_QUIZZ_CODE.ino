/*
  MASTER ESP32 – Connect to Wi-Fi (STA), ESP32_NOW receiver, Minimal HTTP UI
  ---------------------------------------------------------------------------
  - Connects to an existing 2.4 GHz Wi-Fi network (no SoftAP)
  - Derives the Wi-Fi channel from the associated AP and uses it for ESP-NOW
  - Receives student answers via ESP32_NOW broadcast (ESP32_NOW wrapper)
  - Minimal UI at http://<router-assigned-ip>/ with Start/Next/Reset + live counts
  - Question bank in LittleFS (/qbank.json); default 10 History Qs on first boot
*/

#include <WiFi.h>
#include <esp_mac.h>
#include "ESP32_NOW.h"

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char* FB_HOST   = "YOUR_PROJECT_ID-default-rtdb.asia-southeast1.firebasedatabase.app";
static const char* FB_BASE   = "https://"  /* keep */ ;
static const char* ORG_ID    = "cmh";          // change as needed
static const char* SESS_ID   = "2025-11-quiz1";// generate per run
static const char* BANK_ID   = "default";      // a question bank id
static const char* FB_AUTH   = "";             // demo: "" if rules open; else "?auth=TOKEN"

// ---------------- CONFIG: set your Wi-Fi credentials ----------------
static const char* WIFI_SSID = "AAA";
static const char* WIFI_PASS = "Acube123";

// ---------------- Pins / constants ----------------
#define LED_GREEN 35
#define LED_RED   26

static const uint8_t  NUM_STUDENTS = 6;

// ---------------- Data structures ----------------
struct __attribute__((packed)) AnswerMsg {
  uint16_t id;
  uint8_t  q;      // ignored by master; we own currentQ
  uint8_t  ans;    // 1..4
  uint32_t seq;
};

struct Counters {
  uint16_t qid      = 1;
  uint16_t c[5]     = {0,0,0,0,0};   // index 1..4
  uint16_t total    = NUM_STUDENTS;
  uint16_t answered = 0;
  uint16_t correct  = 0;
  uint16_t incorrect= 0;
} counts;

struct Question {
  uint16_t id;
  String text;
  String o1,o2,o3,o4;
  uint8_t correct; // 1..4
};

// ---------------- Globals ----------------
AsyncWebServer server(80);

std::vector<Question> qbank;
uint16_t currentQ   = 1;
uint8_t  correctKey = 1;
bool     started    = false;

static uint32_t lastSeqById[8] = {0};  // 1..6
static bool     answeredById[8] = {false};
static uint8_t  lastAnsById[8]  = {0};

String fbUrl(const String& path) {
  String url = FB_BASE + String(FB_HOST) + path + ".json";
  if (strlen(FB_AUTH)) url += "?auth=" + String(FB_AUTH);
  return url;
}

bool fbPut(const String& path, const String& json) {
  WiFiClientSecure client;
  client.setInsecure(); // or set root CA for stricter TLS
  HTTPClient http;
  String url = fbUrl(path);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(json);
  http.end();
  return code >= 200 && code < 300;
}

bool fbPatch(const String& path, const String& json) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = fbUrl(path);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest("PATCH", (uint8_t*)json.c_str(), json.length());
  http.end();
  return code >= 200 && code < 300;
}

void fbInitSession(uint16_t totalStudents, uint16_t firstQ) {
  // meta
  StaticJsonDocument<256> doc;
  doc["bankId"]        = BANK_ID;
  doc["startedAt"]     = (uint64_t)millis();  // you can use epoch if you have NTP
  doc["status"]        = "running";
  doc["currentQ"]      = firstQ;
  doc["totalStudents"] = totalStudents;
  String body; serializeJson(doc, body);
  fbPut("/orgs/"+String(ORG_ID)+"/sessions/"+String(SESS_ID)+"/meta", body);

  // zero the first counts
  StaticJsonDocument<128> c;
  c["c1"]=0; c["c2"]=0; c["c3"]=0; c["c4"]=0;
  c["answered"]=0; c["correct"]=0; c["incorrect"]=0;
  String cbody; serializeJson(c, cbody);
  fbPut("/orgs/"+String(ORG_ID)+"/sessions/"+String(SESS_ID)+"/counts/"+String(firstQ), cbody);
}

void fbSetCurrentQ(uint16_t qid) {
  StaticJsonDocument<64> d; d["currentQ"]=qid;
  String b; serializeJson(d, b);
  fbPatch("/orgs/"+String(ORG_ID)+"/sessions/"+String(SESS_ID)+"/meta", b);

  StaticJsonDocument<128> c;
  c["c1"]=0; c["c2"]=0; c["c3"]=0; c["c4"]=0;
  c["answered"]=0; c["correct"]=0; c["incorrect"]=0;
  String cb; serializeJson(c, cb);
  fbPut("/orgs/"+String(ORG_ID)+"/sessions/"+String(SESS_ID)+"/counts/"+String(qid), cb);
}

void fbWriteAnswer(uint16_t qid, uint16_t studentId, uint8_t ans, uint32_t seq, bool isCorrect, 
                   uint16_t c1, uint16_t c2, uint16_t c3, uint16_t c4, 
                   uint16_t answered, uint16_t correct, uint16_t incorrect) {
  // answers/{qid}/{studentId}
  StaticJsonDocument<192> a;
  a["ans"]=ans; a["seq"]=seq; a["ts"]=(uint64_t)millis();
  String ab; serializeJson(a, ab);
  fbPut("/orgs/"+String(ORG_ID)+"/sessions/"+String(SESS_ID)+"/answers/"+String(qid)+"/"+String(studentId), ab);

  // counts/{qid} (PATCH only the fields you changed)
  StaticJsonDocument<192> c;
  c["c1"]=c1; c["c2"]=c2; c["c3"]=c3; c["c4"]=c4;
  c["answered"]=answered; c["correct"]=correct; c["incorrect"]=incorrect;
  String cb; serializeJson(c, cb);
  fbPatch("/orgs/"+String(ORG_ID)+"/sessions/"+String(SESS_ID)+"/counts/"+String(qid), cb);
}


// ---------------- Helpers ----------------
void setLEDsWaiting(bool waiting) {
  digitalWrite(LED_RED,   waiting ? HIGH : LOW);
  digitalWrite(LED_GREEN, waiting ? LOW  : HIGH);
}

bool loadQBank() {
  if (!LittleFS.exists("/qbank.json")) {
    // create default 10 history questions
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.createNestedArray("q");
    for (int i=1;i<=10;i++) {
      JsonObject o = arr.createNestedObject();
      o["id"]=i;
      o["text"]=String("History Q")+i;
      o["o1"]="Option A"; o["o2"]="Option B"; o["o3"]="Option C"; o["o4"]="Option D";
      o["correct"]= (i%4)+1;
    }
    File f = LittleFS.open("/qbank.json","w");
    if (!f) return false;
    serializeJsonPretty(doc, f); f.close();
  }

  File f = LittleFS.open("/qbank.json","r");
  if (!f) return false;
  StaticJsonDocument<8192> doc;
  auto e = deserializeJson(doc, f); f.close();
  if (e) return false;

  qbank.clear();
  for (JsonObject o : doc["q"].as<JsonArray>()) {
    Question q;
    q.id = o["id"].as<uint16_t>();
    q.text = o["text"].as<String>();
    q.o1 = o["o1"].as<String>();
    q.o2 = o["o2"].as<String>();
    q.o3 = o["o3"].as<String>();
    q.o4 = o["o4"].as<String>();
    q.correct = o["correct"].as<uint8_t>();
    qbank.push_back(q);
  }
  if (!qbank.empty()) {
    currentQ = qbank.front().id;
    for (auto &q: qbank) if (q.id==currentQ) { correctKey=q.correct; break; }
  }
  return true;
}

void resetForQuestion(uint16_t qid) {
  currentQ = qid;
  memset(answeredById, 0, sizeof(answeredById));
  memset(lastAnsById,  0, sizeof(lastAnsById));
  memset(counts.c,     0, sizeof(counts.c));
  counts.qid      = qid;
  counts.total    = NUM_STUDENTS;
  counts.answered = 0;
  // find correct key
  for (auto &q : qbank) if (q.id == qid) { correctKey = q.correct; break; }
  counts.correct  = 0;
  counts.incorrect= 0;
  setLEDsWaiting(true);
}

void onStudentAnswer(uint16_t id, uint8_t ans, uint32_t seq) {
  if (!started) return;
  if (id < 1 || id > NUM_STUDENTS) return;
  if (seq == lastSeqById[id]) return;        // de-dup
  lastSeqById[id] = seq;

  if (!answeredById[id]) {
    answeredById[id] = true;
    if (ans>=1 && ans<=4) {
      counts.c[ans]++;
      lastAnsById[id] = ans;
      if (ans == correctKey) counts.correct++;
      else counts.incorrect++;
    }
    counts.answered++;
  }
  if (counts.answered == NUM_STUDENTS) {
    setLEDsWaiting(false);  // all answered -> GREEN
  }
}

// ---------------- ESP32_NOW integration ----------------
// Accept first broadcast from unknown MAC, then route packets to onReceive

class StudentPeer : public ESP_NOW_Peer {
public:
  // Channel will be the Wi-Fi channel we’re connected on (see setup)
  StudentPeer(const uint8_t *mac, uint8_t channel)
    : ESP_NOW_Peer(mac, channel, WIFI_IF_STA, nullptr) {}
  bool add_peer() { return add(); }

  void onReceive(const uint8_t* data, size_t len, bool broadcast) override {
    if (len != sizeof(AnswerMsg)) {
      Serial.printf("[RX] from " MACSTR " invalid size=%u\n", MAC2STR(addr()), (unsigned)len);
      return;
    }
    const AnswerMsg* m = reinterpret_cast<const AnswerMsg*>(data);
    Serial.printf("[RX] from " MACSTR " id=%u ans=%u seq=%lu %s\n",
                  MAC2STR(addr()), m->id, m->ans, (unsigned long)m->seq,
                  broadcast ? "(BCAST)" : "(UNICAST)");
    onStudentAnswer(m->id, m->ans, m->seq);
  }
};

std::vector<StudentPeer> students;
static uint8_t g_nowChannel = 1; // set after Wi-Fi connects

static void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
  if (!info) return;
  // We only expect broadcast from unknown students
  if (memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, 6) != 0) return;
  Serial.printf("[NEW] unknown " MACSTR " on ch=%u -> registering\n",
                MAC2STR(info->src_addr), info->rx_ctrl->channel);

  students.emplace_back(info->src_addr, g_nowChannel);
  if (!students.back().add_peer()) {
    Serial.println("[NEW] add_peer FAILED");
    students.pop_back();
  }
}

// ---------------- Minimal UI (single file) ----------------
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>Classroom Quiz (Minimal)</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:20px;background:#0b1020;color:#e7ebff}
.btn{padding:10px 14px;border-radius:10px;border:0;background:#2f61ff;color:#fff;font-weight:700;cursor:pointer;margin-right:8px}
.card{background:#141b36;border:1px solid #1a2352;border-radius:14px;padding:14px;margin-top:12px}
h1{font-size:18px;margin:0 0 8px 0}
.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px}
.stat{background:#0e1533;border:1px solid #24306a;border-radius:12px;padding:10px;text-align:center}
.big{font-size:22px;font-weight:800}
.p{color:#aab2d8}
</style>
<h1>Classroom Quiz</h1>
<button class=btn onclick="cmd('start')">Start</button>
<button class=btn onclick="cmd('next')">Next</button>
<button class=btn onclick="cmd('reset')">Reset</button>

<div class=card>
  <div class=p>Question <b id=qid>1</b></div>
  <div id=qtext style="font-size:20px;font-weight:700;margin:6px 0 8px">—</div>
  <div class=grid>
    <div class=stat><div class=big id=c1>0</div><div class=p>Option 1</div></div>
    <div class=stat><div class=big id=c2>0</div><div class=p>Option 2</div></div>
    <div class=stat><div class=big id=c3>0</div><div class=p>Option 3</div></div>
    <div class=stat><div class=big id=c4>0</div><div class=p>Option 4</div></div>
  </div>
  <div class=p style="margin-top:8px">Answered: <b id=ans>0</b>/<b id=tot>0</b> • Correct option: <b id=key>1</b></div>
</div>

<script>
async function cmd(action){
  await fetch(`/cmd?action=${action}`);
  await refresh();
}
async function refresh(){
  const r = await fetch('/state'); const s = await r.json();
  document.getElementById('qid').textContent = s.qid;
  document.getElementById('qtext').textContent = s.qtext || '';
  document.getElementById('c1').textContent = s.counts[1]||0;
  document.getElementById('c2').textContent = s.counts[2]||0;
  document.getElementById('c3').textContent = s.counts[3]||0;
  document.getElementById('c4').textContent = s.counts[4]||0;
  document.getElementById('ans').textContent = s.answered;
  document.getElementById('tot').textContent = s.total;
  document.getElementById('key').textContent = s.correctKey;
}
setInterval(refresh, 1000);
refresh();
</script>
)HTML";

// ---------------- HTTP handlers ----------------
void handleState(AsyncWebServerRequest* req) {
  StaticJsonDocument<1024> doc;
  doc["started"]    = started;
  doc["qid"]        = currentQ;
  doc["correctKey"] = correctKey;
  doc["answered"]   = counts.answered;
  doc["total"]      = counts.total;
  JsonArray arr     = doc.createNestedArray("counts");
  arr.add(0); // pad index 0
  for (int i=1;i<=4;i++) arr.add(counts.c[i]);
  for (auto &q: qbank) if (q.id == currentQ) {
    doc["qtext"] = q.text;
    break;
  }
  String s; serializeJson(doc, s);
  req->send(200, "application/json", s);
}

void handleCmd(AsyncWebServerRequest* req) {
  if (!req->hasParam("action")) { req->send(400, "text/plain", "missing action"); return; }
  String a = req->getParam("action")->value();
  if (a == "start") {
    started = true;
    if (!qbank.empty()) resetForQuestion(qbank.front().id);
  } else if (a == "next") {
    if (!qbank.empty()) {
      int idx=-1; for (int i=0;i<(int)qbank.size(); i++) if (qbank[i].id==currentQ) { idx=i; break; }
      if (idx!=-1 && idx+1<(int)qbank.size()) resetForQuestion(qbank[idx+1].id);
    }
  } else if (a == "reset") {
    started = false;
    resetForQuestion(currentQ);
  }
  req->send(200, "text/plain", "ok");
}

// ---------------- Setup / Loop ----------------
void setup() {
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  setLEDsWaiting(true);

  Serial.begin(115200);
  delay(150);

  // Filesystem + QBank
  if (!LittleFS.begin(true)) Serial.println("[FS] LittleFS mount FAILED");
  loadQBank();

  // --- Connect to existing Wi-Fi (STA) ---
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
    if (millis() - t0 > 15000) {   // 15s timeout
      Serial.println("\n[WiFi] Connect timeout. Rebooting...");
      delay(1000); ESP.restart();
    }
  }
  Serial.printf("\n[WiFi] Connected. IP=%s  CH=%d  RSSI=%d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());

  // --- ESP32_NOW wrapper init on the associated channel ---
  g_nowChannel = WiFi.channel();  // students must transmit on this channel
  if (!ESP_NOW.begin()) {
    Serial.println("[ESPNOW] begin FAILED -> rebooting"); delay(2000); ESP.restart();
  }
  ESP_NOW.onNewPeer(onNewPeer, nullptr);
  Serial.printf("[ESPNOW] ready on channel %u (listening for broadcasts)\n", g_nowChannel);

  // --- Minimal HTTP UI (served on router IP) ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/state", HTTP_GET, handleState);
  server.on("/cmd",   HTTP_GET, handleCmd);
  server.begin();
  Serial.printf("[HTTP] server started at http://%s\n", WiFi.localIP().toString().c_str());

  resetForQuestion(currentQ);
}

void loop() {
  // event-driven; UI polls /state every 1s
}
