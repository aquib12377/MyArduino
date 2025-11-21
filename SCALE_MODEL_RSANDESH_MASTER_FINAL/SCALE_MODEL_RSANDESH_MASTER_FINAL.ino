// ESP32-S3: WebServer + I2C master + Relay control for Building Lights
// Buttons: Patterns (rainbow on Mega), OFF, 3 BHK, 4 BHK, Surround ON/OFF (relay on ESP32), All lights (Mega + relay)
//
// Wiring (suggested):
//   I2C: ESP32-S3 SDA=GPIO8, SCL=GPIO9  -> level shifter -> Mega SDA(20), SCL(21)
//   Relay IN: ESP32-S3 GPIO4 (active HIGH assumed)
//   Common GND across ESP32, Mega, PSU(s)
//   MEGA I2C slave address: 0x42
//
// Mega must implement the matching I2C command bytes and actions.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// ===== USER CONFIG =====
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";

// I2C to Mega
#define I2C_SDA   11
#define I2C_SCL   12
#define I2C_FREQ  100000
#define MEGA_ADDR 8

// Relay (Surround Lights)
#define PIN_RELAY 10   // active HIGH

// I2C command bytes (must match Mega)
enum : uint8_t {
  CMD_OFF             = 0x00,
  CMD_PATTERN_RAINBOW = 0x01,
  CMD_BHK3            = 0x02,
  CMD_BHK4            = 0x03,
  CMD_ALL_ON          = 0x04
};

WebServer server(80);

// --- Minimal responsive UI ---
String htmlPage() {
  return String(F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Building Lights</title>"
    "<style>"
      "body{font-family:system-ui,Segoe UI,Roboto,Arial;display:flex;min-height:100vh;"
      "align-items:center;justify-content:center;background:#0b1220;color:#e6edf3;margin:0}"
      ".card{background:#0f172a;padding:24px;border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,.35);"
      "width:min(480px,92vw)}"
      ".title{margin:0 0 16px;font-size:22px}"
      ".grid{display:grid;gap:12px;grid-template-columns:repeat(2,1fr)}"
      ".btn{padding:14px 16px;border:0;border-radius:12px;font-weight:600;cursor:pointer;background:#1f2937;"
      "color:#e6edf3;transition:transform .08s ease,opacity .2s}"
      ".btn:hover{opacity:.9}.btn:active{transform:scale(.98)}"
      ".accent{background:#2563eb}.warn{background:#ef4444}.ok{background:#10b981}.alt{background:#7c3aed}"
      ".full{grid-column:1/-1}"
      "#stat{margin-top:14px;font-size:12px;color:#9ca3af}"
    "</style></head><body><div class='card'>"
      "<h1 class='title'>Building Lights</h1>"
      "<div class='grid'>"
        "<button class='btn accent' onclick=send('pattern')>Patterns</button>"
        "<button class='btn warn' onclick=send('off')>OFF</button>"
        "<button class='btn ok' onclick=send('3bhk')>3 BHK</button>"
        "<button class='btn ok' onclick=send('4bhk')>4 BHK</button>"
        "<button class='btn alt' onclick=send('surround_on')>Surround Lights ON</button>"
        "<button class='btn' onclick=send('surround_off')>Surround Lights OFF</button>"
        "<button class='btn full accent' onclick=send('all')>All lights</button>"
      "</div>"
      "<div id='stat'>Ready</div>"
      "<script>"
        "async function send(name){"
          "const s=document.getElementById('stat');"
          "s.textContent='Sending '+name+'…';"
          "try{const r=await fetch('/api/cmd?name='+encodeURIComponent(name));"
              "const t=await r.text();"
              "s.textContent=t;"
          "}catch(e){s.textContent='Error: '+e}"
        "}"
      "</script>"
    "</div></body></html>"
  ));
}

// send one byte over I2C, with tiny retry
bool i2cSend(uint8_t b) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    Wire.beginTransmission(MEGA_ADDR);
    Wire.write(b);
    if (Wire.endTransmission() == 0) return true;
    delay(2);
  }
  return false;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleCmd() {
  String name = server.hasArg("name") ? server.arg("name") : "";
  bool ok = false;

  if      (name == "off")          ok = i2cSend(CMD_OFF);
  else if (name == "pattern")      ok = i2cSend(CMD_PATTERN_RAINBOW);
  else if (name == "3bhk")         ok = i2cSend(CMD_BHK3);
  else if (name == "4bhk")         ok = i2cSend(CMD_BHK4);
  else if (name == "all") {
    bool a = i2cSend(CMD_ALL_ON);
    digitalWrite(PIN_RELAY, HIGH); // trigger surround as part of ALL
    ok = a;
  }
  else if (name == "surround_on")  { digitalWrite(PIN_RELAY, HIGH); ok = true; }
  else if (name == "surround_off") { digitalWrite(PIN_RELAY, LOW);  ok = true; }

  if (ok) server.send(200, "text/plain", "OK: " + name);
  else    server.send(500, "text/plain", "I2C/Action failed for: " + name);
}

void setup() {
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP32-S3 Web + I2C Controller");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi…");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print('.'); }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);

  server.on("/", handleRoot);
  server.on("/api/cmd", handleCmd);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
