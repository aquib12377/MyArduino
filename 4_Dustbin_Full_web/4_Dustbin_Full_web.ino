/*
   ESP32 Smart Garbage Collection – 4 IR Sensors + WebSocket Dashboard + Buzzer

   Buzzer logic:
   - If ANY bin is FULL => buzzer ON
   - If ALL bins are EMPTY => buzzer OFF
   - Also does a short "beep" when a bin becomes FULL (edge-trigger)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ----------------- USER CONFIG -----------------
const char* ssid     = "AAA";
const char* password = "Acube@123";

// IR sensor pins (change to your wiring)
const int IR_PINS[4] = {35,32, 33, 25}; // Bin 1–4

// BUZZER pin (change as per wiring)
const int BUZZER_PIN = 26;   // Example GPIO 27

// Debounce / read interval
const unsigned long READ_INTERVAL_MS = 500;

// Serial print interval (every 2 seconds)
const unsigned long SERIAL_PRINT_INTERVAL_MS = 2000;

// Buzzer tone settings
const int BUZZER_FREQ = 2000;          // Hz (tone pitch)
const unsigned long BEEP_MS = 200;     // short beep duration on "FULL" event

// ----------------- GLOBALS -----------------
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

bool binFull[4] = {false, false, false, false};
unsigned long lastReadTime = 0;
bool lastSentState[4] = {false, false, false, false};

// Serial print timer
unsigned long lastSerialPrintTime = 0;

// Buzzer state machine
bool buzzerOn = false;
unsigned long beepUntil = 0; // millis until which we keep tone for short beep

// ----------------- HTML PAGE -----------------
const char index_html[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Smart Garbage Monitoring</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="min-h-screen bg-slate-900 flex items-center justify-center">
  <div class="max-w-4xl w-full px-4 py-6">
    <h1 class="text-3xl md:text-4xl font-bold text-center text-white mb-2">
      Smart Garbage Collection
    </h1>
    <p class="text-center text-slate-300 mb-6">
      Live status of garbage bins (updates in real-time).
    </p>

    <div id="statusBar" class="text-center text-sm text-slate-300 mb-4">
      Connecting to sensor gateway...
    </div>

    <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
      <!-- Bin cards -->
      <div id="bin1" class="rounded-2xl p-5 bg-slate-800 border border-slate-700 shadow-lg flex flex-col justify-between">
        <div>
          <h2 class="text-xl font-semibold text-slate-100">Bin 1</h2>
          <p class="text-sm text-slate-400 mt-1">Top-mounted IR sensor</p>
        </div>
        <div class="mt-4 flex items-center justify-between">
          <span class="text-sm text-slate-400">Status</span>
          <span class="px-3 py-1 rounded-full text-xs font-semibold" id="bin1Badge">Unknown</span>
        </div>
      </div>

      <div id="bin2" class="rounded-2xl p-5 bg-slate-800 border border-slate-700 shadow-lg flex flex-col justify-between">
        <div>
          <h2 class="text-xl font-semibold text-slate-100">Bin 2</h2>
          <p class="text-sm text-slate-400 mt-1">Top-mounted IR sensor</p>
        </div>
        <div class="mt-4 flex items-center justify-between">
          <span class="text-sm text-slate-400">Status</span>
          <span class="px-3 py-1 rounded-full text-xs font-semibold" id="bin2Badge">Unknown</span>
        </div>
      </div>

      <div id="bin3" class="rounded-2xl p-5 bg-slate-800 border border-slate-700 shadow-lg flex flex-col justify-between">
        <div>
          <h2 class="text-xl font-semibold text-slate-100">Bin 3</h2>
          <p class="text-sm text-slate-400 mt-1">Top-mounted IR sensor</p>
        </div>
        <div class="mt-4 flex items-center justify-between">
          <span class="text-sm text-slate-400">Status</span>
          <span class="px-3 py-1 rounded-full text-xs font-semibold" id="bin3Badge">Unknown</span>
        </div>
      </div>

      <div id="bin4" class="rounded-2xl p-5 bg-slate-800 border border-slate-700 shadow-lg flex flex-col justify-between">
        <div>
          <h2 class="text-xl font-semibold text-slate-100">Bin 4</h2>
          <p class="text-sm text-slate-400 mt-1">Top-mounted IR sensor</p>
        </div>
        <div class="mt-4 flex items-center justify-between">
          <span class="text-sm text-slate-400">Status</span>
          <span class="px-3 py-1 rounded-full text-xs font-semibold" id="bin4Badge">Unknown</span>
        </div>
      </div>
    </div>

    <div class="mt-8 text-center text-xs text-slate-500">
      ESP32 &middot; WebSocket-based live monitoring
    </div>
  </div>

<script>
  let socket;

  function setStatus(text, isError = false) {
    const bar = document.getElementById('statusBar');
    bar.textContent = text;
    bar.className = "text-center text-sm mb-4 " + (isError ? "text-red-400" : "text-emerald-400");
  }

  function setBinState(binIndex, isFull) {
    const card = document.getElementById('bin' + binIndex);
    const badge = document.getElementById('bin' + binIndex + 'Badge');
    if (!card || !badge) return;

    if (isFull) {
      card.className = "rounded-2xl p-5 bg-red-900/40 border border-red-500/70 shadow-lg flex flex-col justify-between";
      badge.textContent = "FULL";
      badge.className = "px-3 py-1 rounded-full text-xs font-semibold bg-red-500 text-white";
    } else {
      card.className = "rounded-2xl p-5 bg-emerald-900/30 border border-emerald-500/70 shadow-lg flex flex-col justify-between";
      badge.textContent = "EMPTY";
      badge.className = "px-3 py-1 rounded-full text-xs font-semibold bg-emerald-500 text-emerald-950";
    }
  }

  function initWebSocket() {
    const gateway = "ws://" + window.location.hostname + ":81/";
    socket = new WebSocket(gateway);

    socket.onopen = () => setStatus("Connected to ESP32 gateway");

    socket.onclose = () => {
      setStatus("Disconnected. Reconnecting...", true);
      setTimeout(initWebSocket, 2000);
    };

    socket.onerror = (error) => {
      console.error("WebSocket Error:", error);
      setStatus("WebSocket error. Check console.", true);
    };

    socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (typeof data.b1 === "boolean") setBinState(1, data.b1);
        if (typeof data.b2 === "boolean") setBinState(2, data.b2);
        if (typeof data.b3 === "boolean") setBinState(3, data.b3);
        if (typeof data.b4 === "boolean") setBinState(4, data.b4);
      } catch (e) {
        console.error("Error parsing message:", e, event.data);
      }
    };
  }

  window.addEventListener('load', () => {
    setStatus("Connecting to ESP32 gateway...");
    initWebSocket();
  });
</script>

</body>
</html>
)HTMLPAGE";

// ----------------- WEBSOCKET HANDLER -----------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WS] Client %u connected from %s\n", num, ip.toString().c_str());

        char msg[80];
        snprintf(msg, sizeof(msg),
                 "{\"b1\":%s,\"b2\":%s,\"b3\":%s,\"b4\":%s}",
                 binFull[0] ? "true" : "false",
                 binFull[1] ? "true" : "false",
                 binFull[2] ? "true" : "false",
                 binFull[3] ? "true" : "false");
        webSocket.sendTXT(num, msg);
      }
      break;

    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected\n", num);
      break;

    case WStype_TEXT:
      Serial.printf("[WS] Received from %u: %s\n", num, payload);
      break;

    default:
      break;
  }
}

// ----------------- HTTP HANDLERS -----------------
void handleRoot() { server.send_P(200, "text/html", index_html); }
void handleNotFound() { server.send(404, "text/plain", "404: Not found"); }

// ----------------- BUZZER HELPERS -----------------
void buzzerStartContinuous() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerOn = true;
}
void buzzerStop() {
  digitalWrite(BUZZER_PIN, LOW);
  buzzerOn = false;
}
void buzzerShortBeep(unsigned long nowMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  beepUntil = nowMs + BEEP_MS;
  buzzerOn = true;
}

// ----------------- SENSOR READING -----------------
void readSensorsAndBroadcast() {
  bool changed = false;
  bool anyFull = false;
  bool becameFullEvent = false;

  int rawIR[4] = {0, 0, 0, 0}; // <-- store raw readings for serial print

  for (int i = 0; i < 4; i++) {
    int val = digitalRead(IR_PINS[i]);
    rawIR[i] = val;

    bool isFull = (val == LOW);

    if (isFull && !binFull[i]) becameFullEvent = true;

    binFull[i] = isFull;
    if (binFull[i]) anyFull = true;

    if (binFull[i] != lastSentState[i]) changed = true;
  }

  // -------- Serial print every 2 seconds (raw + status) --------
  unsigned long nowMs = millis();
  if (nowMs - lastSerialPrintTime >= SERIAL_PRINT_INTERVAL_MS) {
    lastSerialPrintTime = nowMs;

    Serial.printf("[IR] B1=%d(%s)  B2=%d(%s)  B3=%d(%s)  B4=%d(%s)  anyFull=%s  buzzer=%s\n",
                  rawIR[0], binFull[0] ? "FULL" : "EMPTY",
                  rawIR[1], binFull[1] ? "FULL" : "EMPTY",
                  rawIR[2], binFull[2] ? "FULL" : "EMPTY",
                  rawIR[3], binFull[3] ? "FULL" : "EMPTY",
                  anyFull ? "true" : "false",
                  buzzerOn ? "ON" : "OFF");
  }

  // ---------- BUZZER LOGIC ----------
  if (anyFull) {
    if (becameFullEvent) buzzerShortBeep(nowMs);

    // Keep buzzer ON continuously after beep (optional)
    // If you want ONLY short beep, comment next 2 lines.
    if (nowMs > beepUntil) buzzerStartContinuous();
  } else {
    buzzerStop();
    beepUntil = 0;
  }

  if (beepUntil > 0 && nowMs > beepUntil && !anyFull) {
    buzzerStop();
    beepUntil = 0;
  }

  // ---------- WEBSOCKET BROADCAST ----------
  if (changed) {
    for (int i = 0; i < 4; i++) lastSentState[i] = binFull[i];

    char msg[80];
    snprintf(msg, sizeof(msg),
             "{\"b1\":%s,\"b2\":%s,\"b3\":%s,\"b4\":%s}",
             binFull[0] ? "true" : "false",
             binFull[1] ? "true" : "false",
             binFull[2] ? "true" : "false",
             binFull[3] ? "true" : "false");

    Serial.print("[WS] Broadcast: ");
    Serial.println(msg);

    webSocket.broadcastTXT(msg);
  }
}

// ----------------- SETUP & LOOP -----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  for (int i = 0; i < 4; i++) pinMode(IR_PINS[i], INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerStop();

  Serial.println();
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started on port 80");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long now = millis();
  if (now - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = now;
    readSensorsAndBroadcast();
  }
}
