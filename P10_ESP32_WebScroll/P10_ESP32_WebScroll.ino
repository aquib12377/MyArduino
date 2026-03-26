/*
 * ESP32 Web Controller -> Arduino Nano Serial -> P10 Display
 * ----------------------------------------------------------
 * ESP32:
 *  - Creates WiFi AP
 *  - Hosts web control panel
 *  - Sends text/speed/brightness to Nano over Serial2
 *
 * Serial2 wiring:
 *   ESP32 GPIO17 (TX2) -> Nano D2 (SoftwareSerial RX)
 *   ESP32 GND          -> Nano GND
 */

#include <WiFi.h>
#include <WebServer.h>

// ── WiFi ──────────────────────────────────────────────────
const char* AP_SSID     = "P10-Display";
const char* AP_PASSWORD = "12345678";

// ── Serial to Nano ────────────────────────────────────────
#define NANO_TX 17
#define NANO_RX 16   // not used currently, kept for future

// ── State ─────────────────────────────────────────────────
String g_displayText = "Welcome! Use the web panel to update this message.";
int    g_scrollSpeed = 35;   // ms
int    g_brightness  = 80;   // %

WebServer server(80);

// -----------------------------------------------------------------------------
// HTML UI
// -----------------------------------------------------------------------------
void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent(
    "<!DOCTYPE html>"
    "<html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>P10 Display Control</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#111;color:#f5a623;margin:0;padding:20px}"
    ".wrap{max-width:700px;margin:auto}"
    "h1{text-align:center}"
    ".card{background:#1b1b1b;padding:20px;border:1px solid #333;margin-bottom:16px}"
    "textarea{width:100%;padding:12px;font-size:18px;background:#0f0f0f;color:#fff;border:1px solid #444}"
    "input[type=range]{width:100%}"
    "button{width:100%;padding:16px;background:#f5a623;border:none;font-weight:bold;cursor:pointer}"
    ".label{margin-bottom:8px;display:block;color:#ffbf66}"
    ".row{margin-bottom:12px}"
    ".small{font-size:12px;color:#aaa}"
    "</style></head><body><div class='wrap'>"
    "<h1>P10 Display Control</h1>"
    "<div class='card'>"
    "<div class='row'>"
    "<label class='label'>Message</label>"
    "<textarea id='msg' rows='3' maxlength='200'>Welcome! Use the web panel to update this message.</textarea>"
    "</div>"
    "<div class='row'>"
    "<label class='label'>Speed: <span id='spv'>3</span></label>"
    "<input type='range' id='speed' min='1' max='5' value='3'>"
    "</div>"
    "<div class='row'>"
    "<label class='label'>Brightness: <span id='brv'>80</span>%</label>"
    "<input type='range' id='brightness' min='10' max='100' value='80'>"
    "</div>"
    "<button onclick='sendMsg()'>Send to Display</button>"
    "<p class='small'>Connect to WiFi: <b>P10-Display</b> | IP: <b>192.168.4.1</b></p>"
    "</div>"
    "<script>"
    "const sp=document.getElementById('speed');"
    "const br=document.getElementById('brightness');"
    "sp.oninput=()=>document.getElementById('spv').textContent=sp.value;"
    "br.oninput=()=>document.getElementById('brv').textContent=br.value;"
    "function sendMsg(){"
    " let msg=document.getElementById('msg').value.trim();"
    " if(!msg){alert('Enter a message');return;}"
    " let spMap={1:10,2:20,3:35,4:55,5:80};"
    " let speedMs=spMap[sp.value]||35;"
    " let brightness=br.value;"
    " fetch('/update',{"
    "   method:'POST',"
    "   headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "   body:'text='+encodeURIComponent(msg)+'&speed='+speedMs+'&brightness='+brightness"
    " })"
    " .then(r=>r.text())"
    " .then(x=>alert('Sent to display'))"
    " .catch(e=>alert('Error sending'));"
    "}"
    "</script>"
    "</div></body></html>"
  );
}

// -----------------------------------------------------------------------------
// Send command to Nano
// -----------------------------------------------------------------------------
void sendToNano(const String& text, int speedMs, int brightness) {
  String safeText = text;
  safeText.replace("\r", " ");
  safeText.replace("\n", " ");
  safeText.replace("|", "/");   // avoid breaking parser

  String packet = "TXT=" + safeText + "|SPD=" + String(speedMs) + "|BRT=" + String(brightness);
  Serial.println("[ESP32 -> Nano] " + packet);
  Serial2.println(safeText);
}

void handleUpdate() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  if (server.hasArg("text")) {
    g_displayText = server.arg("text");
  }

  if (server.hasArg("speed")) {
    g_scrollSpeed = constrain(server.arg("speed").toInt(), 5, 200);
  }

  if (server.hasArg("brightness")) {
    g_brightness = constrain(server.arg("brightness").toInt(), 10, 100);
  }

  sendToNano(g_displayText, g_scrollSpeed, g_brightness);
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// -----------------------------------------------------------------------------
// Setup / Loop
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // ESP32 Serial2
  Serial2.begin(9600, SERIAL_8N1, NANO_RX, NANO_TX);

  Serial.println("\n[ESP32] Booting...");
  delay(500);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[WiFi] AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("[HTTP] Server ready");
  Serial.println("[ESP32] Sending initial display data...");
  sendToNano(g_displayText, g_scrollSpeed, g_brightness);
}

void loop() {
  server.handleClient();
}