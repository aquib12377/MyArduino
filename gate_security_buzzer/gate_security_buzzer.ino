#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// =========================
// Wi-Fi
// =========================
const char* WIFI_SSID     = "MyProject";
const char* WIFI_PASSWORD = "12345678";

// =========================
// Owner + Camera link
// =========================
const char* OWNER_PHONE    = "+917977845638";
const char* CAM_STREAM_URL = "http://172.23.128.253:81/stream";

// =========================
// Pin mapping for ESP32 38-pin board
// =========================
static const int PIR_PIN    = 34;  // PIR output
static const int SERVO_PIN  = 18;  // Servo signal
static const int GSM_RX_PIN = 16;  // ESP32 RX2  <- SIM900A TX
static const int GSM_TX_PIN = 17;  // ESP32 TX2  -> SIM900A RX
static const int BUZZER_PIN = 18;  // Buzzer signal pin  <-- NEW

// =========================
// Gate servo settings
// =========================
static const int GATE_OPEN_ANGLE   = 90;
static const int GATE_CLOSED_ANGLE = 0;

// =========================
// Motion/SMS timing
// =========================
static const unsigned long MOTION_HOLD_MS  = 15000;
static const unsigned long SMS_COOLDOWN_MS = 60000;

// =========================
// Buzzer settings           <-- NEW
// =========================
static const int  BUZZER_BEEP_COUNT    = 3;     // number of beeps on detection
static const int  BUZZER_BEEP_ON_MS   = 200;   // beep duration  (ms)
static const int  BUZZER_BEEP_OFF_MS  = 200;   // gap between beeps (ms)

HardwareSerial sim900(2);
WebServer server(80);
Servo gateServo;

// Runtime state
bool personDetected         = false;
bool gateOpen               = false;
bool lastPirRaw             = false;
bool smsSentForCurrentEvent = false;

unsigned long lastMotionMs = 0;
unsigned long lastSmsMs    = 0;

// --------------------------------------------------
// Buzzer helper (blocking short beep sequence)     <-- NEW
// Kept short enough (< 1.2 s) to not noticeably
// stall the web server loop.
// --------------------------------------------------
void beepBuzzer(int count, int onMs, int offMs) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < count - 1) delay(offMs);
  }
}

// --------------------------------------------------
// HTML page
// --------------------------------------------------
String htmlPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Gate Security</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f3f4f6;
      margin: 0;
      padding: 20px;
      color: #111827;
    }
    .card {
      max-width: 700px;
      margin: auto;
      background: #ffffff;
      border-radius: 14px;
      padding: 20px;
      box-shadow: 0 6px 18px rgba(0,0,0,0.10);
    }
    h1 { margin-top: 0; font-size: 24px; }
    .status { font-size: 20px; font-weight: bold; margin: 12px 0; }
    .detected { color: #b91c1c; }
    .clear    { color: #065f46; }
    .gate     { margin: 10px 0 20px 0; font-size: 18px; }
    button {
      border: none; padding: 12px 18px; border-radius: 10px;
      cursor: pointer; font-size: 16px; margin-right: 10px; margin-bottom: 10px;
    }
    .openBtn  { background: #16a34a; color: white; }
    .closeBtn { background: #dc2626; color: white; }
    a { color: #2563eb; text-decoration: none; font-weight: bold; }
    img {
      width: 100%; max-width: 640px; border-radius: 12px;
      border: 1px solid #d1d5db; margin-top: 14px; background: #000;
    }
    .small { color: #6b7280; font-size: 14px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>ESP32 Gate Security Panel</h1>

    <div id="personStatus" class="status">Checking status...</div>
    <div id="gateStatus"   class="gate">Gate: --</div>

    <button class="openBtn"  onclick="openGate()">Open Gate</button>
    <button class="closeBtn" onclick="closeGate()">Close Gate</button>

    <p style="margin-top:20px;">
      <a href="CAM_URL" target="_blank">Open ESP32-CAM Live Stream</a>
    </p>
    <div class="small">Preview:</div>
    <img src="CAM_URL" alt="ESP32-CAM Stream">
    <p class="small">This page refreshes motion status automatically.</p>
  </div>

<script>
async function refreshStatus() {
  try {
    const response = await fetch('/status');
    const data = await response.json();
    const person = document.getElementById('personStatus');
    const gate   = document.getElementById('gateStatus');
    if (data.personDetected) {
      person.textContent = 'Person Detected';
      person.className   = 'status detected';
    } else {
      person.textContent = 'No Person Detected';
      person.className   = 'status clear';
    }
    gate.textContent = 'Gate: ' + data.gate;
  } catch (e) {
    document.getElementById('personStatus').textContent = 'Status unavailable';
  }
}
async function openGate()  { await fetch('/open');  refreshStatus(); }
async function closeGate() { await fetch('/close'); refreshStatus(); }
refreshStatus();
setInterval(refreshStatus, 1000);
</script>
</body>
</html>
)rawliteral";

  page.replace("CAM_URL", String(CAM_STREAM_URL));
  return page;
}

// --------------------------------------------------
// Gate helpers
// --------------------------------------------------
void openGate() {
  gateServo.write(GATE_OPEN_ANGLE);
  gateOpen = true;
  Serial.println("Gate opened");
}

void closeGate() {
  gateServo.write(GATE_CLOSED_ANGLE);
  gateOpen = false;
  Serial.println("Gate closed");
}

// --------------------------------------------------
// GSM helpers
// --------------------------------------------------
bool waitForAny(const String& token1, const String& token2, uint32_t timeoutMs) {
  String response;
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (sim900.available()) {
      char c = (char)sim900.read();
      Serial.write(c);
      response += c;
      if ((token1.length() && response.indexOf(token1) >= 0) ||
          (token2.length() && response.indexOf(token2) >= 0)) return true;
      if (response.indexOf("ERROR") >= 0 ||
          response.indexOf("+CMS ERROR") >= 0 ||
          response.indexOf("+CME ERROR") >= 0) return false;
    }
  }
  return false;
}

bool sendAT(const String& cmd, const String& expected = "OK", uint32_t timeoutMs = 3000) {
  while (sim900.available()) sim900.read();
  Serial.print("SIM900 >> "); Serial.println(cmd);
  sim900.println(cmd);
  return waitForAny(expected, "", timeoutMs);
}

bool initSIM900() {
  bool ready = false;
  for (int i = 0; i < 5; i++) {
    if (sendAT("AT")) { ready = true; break; }
    delay(1000);
  }
  if (!ready) { Serial.println("SIM900A not responding"); return false; }
  sendAT("ATE0");
  sendAT("AT+CMGF=1");
  sendAT("AT+CSCS=\"GSM\"");
  Serial.println("SIM900A initialized");
  return true;
}

bool sendSMS(const String& phone, const String& text) {
  sendAT("AT+CMGF=1");
  sendAT("AT+CSCS=\"GSM\"");
  while (sim900.available()) sim900.read();
  String cmd = "AT+CMGS=\"" + phone + "\"";
  Serial.print("SIM900 >> "); Serial.println(cmd);
  sim900.println(cmd);
  if (!waitForAny(">", "", 5000)) { Serial.println("No SMS prompt received"); return false; }
  Serial.println("SIM900 >> [SMS BODY]");
  sim900.print(text);
  sim900.write(26);
  bool ok = waitForAny("+CMGS", "OK", 20000);
  Serial.println(ok ? "SMS sent successfully" : "SMS failed");
  return ok;
}

// --------------------------------------------------
// Web handlers
// --------------------------------------------------
void handleRoot()   { server.send(200, "text/html", htmlPage()); }
void handleOpen()   { openGate();  server.send(200, "text/plain", "OPENED"); }
void handleClose()  { closeGate(); server.send(200, "text/plain", "CLOSED"); }

void handleStatus() {
  String json = "{";
  json += "\"personDetected\":";
  json += (personDetected ? "true" : "false");
  json += ",\"gate\":\"";
  json += (gateOpen ? "OPEN" : "CLOSED");
  json += "\",\"camUrl\":\"";
  json += CAM_STREAM_URL;
  json += "\"}";
  server.send(200, "application/json", json);
}

// --------------------------------------------------
// Motion + Buzzer logic
// --------------------------------------------------
void processPir() {
  bool pirRaw = (digitalRead(PIR_PIN) == HIGH);

  if (pirRaw) lastMotionMs = millis();

  bool currentDetected = (millis() - lastMotionMs) < MOTION_HOLD_MS;

  if (currentDetected != personDetected) {
    personDetected = currentDetected;
    Serial.println(personDetected ? "Person detected" : "No person detected");
  }

  // Rising edge: new motion event
  if (pirRaw && !lastPirRaw) {
    Serial.println("Motion edge detected");

    // ---- Buzzer alert ----                               <-- NEW
    beepBuzzer(BUZZER_BEEP_COUNT, BUZZER_BEEP_ON_MS, BUZZER_BEEP_OFF_MS);

    // ---- SMS alert ----
    if (!smsSentForCurrentEvent &&
        (lastSmsMs == 0 || millis() - lastSmsMs > SMS_COOLDOWN_MS)) {
      String smsText = "Alert: Person detected near gate.\nLive stream:\n";
      smsText += CAM_STREAM_URL;
      bool sent = sendSMS(OWNER_PHONE, smsText);
      if (sent) {
        lastSmsMs = millis();
        smsSentForCurrentEvent = true;
      }
    }
  }

  // Reset per-event SMS flag after motion fully clears
  if (!currentDetected) smsSentForCurrentEvent = false;

  lastPirRaw = pirRaw;
}

// --------------------------------------------------
// Setup / Loop
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIR_PIN,    INPUT);
  pinMode(BUZZER_PIN, OUTPUT);          // <-- NEW
  digitalWrite(BUZZER_PIN, LOW);        // <-- NEW: ensure buzzer is off at boot

  // Servo
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2400);
  closeGate();

  // GSM UART2
  sim900.begin(9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  delay(1000);
  initSIM900();

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Web routes
  server.on("/",       handleRoot);
  server.on("/status", handleStatus);
  server.on("/open",   handleOpen);
  server.on("/close",  handleClose);
  server.begin();

  Serial.println("Web server started");

  // Boot confirmation beep (1 short beep)  <-- NEW
  beepBuzzer(1, 100, 0);
}

void loop() {
  server.handleClient();
  processPir();
  delay(50);
}
