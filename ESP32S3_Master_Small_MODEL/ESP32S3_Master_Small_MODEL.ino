/*********************************************************************
 *  ESP32-S3  |  Building-Lights Web Front-End  |  I²C master
 *  Debug build with Serial prints
 *********************************************************************/
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

constexpr char WIFI_SSID[] = "Tanya";
constexpr char WIFI_PASS[] = "00000000";

/* ---- I²C ---- */
constexpr uint8_t I2C_ADDR  = 0x08;   // slave (Mega)
constexpr uint8_t SDA_PIN   = 11;     // pick any 3.3 V GPIOs
constexpr uint8_t SCL_PIN   = 12;
constexpr uint32_t I2C_FREQ = 400000; // fast-mode

WebServer server(80);

/* -------------------- HTML page -------------------- */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Building Lights</title>
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
<style>
body{background:#0d1117;color:#f8f9fa;font-family:system-ui,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
     display:flex;min-height:100vh;align-items:center;justify-content:center}
.btn-lg{width:260px;height:110px;font-size:1.4rem;font-weight:600;border-radius:1rem}
</style></head><body>
<div class="text-center">
  <h2 class="mb-4">Building-Lights Control</h2>
  <div class="d-grid gap-3">
    <button class="btn btn-success btn-lg" onclick="go('A')">Available Rooms</button>
    <button class="btn btn-info    btn-lg" onclick="go('P')">Patterns</button>
    <button class="btn btn-warning btn-lg" onclick="go('W')">All Warm White</button>
  </div>
  <p class="mt-4" id="msg"></p>
</div>
<script>
async function go(c){
  let r = await fetch('/cmd?op='+c);
  document.getElementById('msg').textContent = r.ok ? 'Command sent ✓':'Error';
}
</script></body></html>)rawliteral";

/* ------------------ handlers ------------------ */
void handleRoot() {
  Serial.printf("[HTTP] GET /\n");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleCmd() {
  String clientIP = server.client().remoteIP().toString();
  Serial.printf("[HTTP] GET /cmd from %s\n", clientIP.c_str());

  if (!server.hasArg("op") || server.arg("op").length() != 1) {
    Serial.println("  → Missing or invalid 'op' arg");
    server.send(400, "text/plain", "Bad op");
    return;
  }

  char c = server.arg("op")[0];  // 'A','P','W'
  Serial.printf("  → Requested op: '%c'\n", c);

  if (c!='A' && c!='P' && c!='W') {
    Serial.println("  → Invalid command character");
    server.send(400, "text/plain", "Bad cmd");
    return;
  }

  Wire.beginTransmission(I2C_ADDR);
  Wire.write(c);
  int ret = Wire.endTransmission();  // returns 0 on success
  Serial.printf("  → I2C write '%c' returned %d\n", c, ret);

  if (ret == 0) {
    server.send(200, "text/plain", "OK");
    Serial.println("  → Command sent to Mega successfully");
  } else {
    server.send(500, "text/plain", "I2C Error");
    Serial.println("  → ERROR sending I2C");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}  // wait for Serial
  Serial.println("\n=== ESP32-S3 Building-Lights Web (DEBUG) ===");

  // I2C init
  Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000UL);  // 400kHz

  Serial.printf("I2C initialized on SDA=%d, SCL=%d @%uHz\n", SDA_PIN, SCL_PIN, I2C_FREQ);

  // Wi-Fi connect
  Serial.printf("Connecting to WiFi '%s'…\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

  // HTTP handlers
  server.on("/",    handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  server.handleClient();
}
