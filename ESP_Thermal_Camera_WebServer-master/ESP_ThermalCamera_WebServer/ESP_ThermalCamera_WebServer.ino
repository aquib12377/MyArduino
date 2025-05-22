/****************************************************************************************
 *  ESP32 + MLX90614 – simple Wi‑Fi thermometer with relay control
 *  --------------------------------------------------------------
 *  • Reads object temperature from an MLX90614 IR sensor (I²C).
 *  • Shows the live temperature on a tiny web page (auto‑refresh every second).
 *  • Energises a relay on GPIO 4 whenever the temperature rises above a set threshold.
 *
 *  Pin‑out (default ESP32 DevKit v1):
 *    ┌─────────┬──────────────┐
 *    │ MLX90614│ ESP32        │
 *    ├─────────┼──────────────┤
 *    │   VIN   │ 3V3          │
 *    │   GND   │ GND          │
 *    │   SDA   │ GPIO 21 (SDA)│
 *    │   SCL   │ GPIO 22 (SCL)│
 *    └─────────┴──────────────┘
 *    Relay module VCC ↔ 5 V, GND ↔ GND, IN ↔ GPIO 4
 *
 *  Tested with:
 *    • Arduino‑ESP32 core 3.1.x
 *    • Adafruit_MLX90614 library 2.2.0
 ****************************************************************************************/

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MLX90614.h>

// ──────────────────────────────────────────────────────────────────────────────
// USER CONFIG
// ──────────────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "MyProject";
const char* WIFI_PASSWORD = "12345678";

#define RELAY_PIN       4          // GPIO that drives the relay
#define RELAY_ACTIVE    LOW        // change to HIGH if your module is active‑high
#define TEMP_THRESHOLD  50.0       // °C – relay turns ON above this temperature

// ──────────────────────────────────────────────────────────────────────────────
// GLOBALS
// ──────────────────────────────────────────────────────────────────────────────
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
WebServer server(80);

float currentTemp = 0.0;           // last object temperature read

// ──────────────────────────────────────────────────────────────────────────────
// SIMPLE HTML PAGE (in PROGMEM)
// ──────────────────────────────────────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"html(
<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 MLX90614 Thermometer</title>
<style>
body{font-family:Arial;text-align:center;margin-top:2rem;color:#333}
span{font-size:3rem}
sup{font-size:1.5rem;margin-left:0.2rem}
</style>
</head><body>
<h2>ESP32&nbsp;Thermal&nbsp;Monitor</h2>
<p>Object temperature</p>
<span id="t">--</span><sup>&deg;C</sup>
<p>(Relay switches at %TH%&nbsp;°C)</p>
<script>
function upd(){fetch("/temp").then(r=>r.text()).then(v=>{document.getElementById("t").textContent=v})}
setInterval(upd,1000);upd();
</script>
</body></html>
)html";

// ──────────────────────────────────────────────────────────────────────────────
// HELPER: send root page with threshold substituted
// ──────────────────────────────────────────────────────────────────────────────
void handleRoot()
{
  String page = FPSTR(INDEX_HTML);
  page.replace("%TH%", String(TEMP_THRESHOLD, 1));
  server.send(200, "text/html", page);
}

void handleTemp() { server.send(200, "text/plain", String(currentTemp, 2)); }

// ──────────────────────────────────────────────────────────────────────────────
// SETUP
// ──────────────────────────────────────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);

  // relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, !RELAY_ACTIVE);   // make sure it starts OFF

  // I²C + sensor
  Wire.begin();               // default SDA=21, SCL=22 on ESP32
  if (!mlx.begin()) {
    Serial.println("MLX90614 not found – check wiring");
    //while (true) delay(1000);
  }

  // Wi‑Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi‑Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print('.');
  }
  Serial.println(); Serial.print("Connected, IP: "); Serial.println(WiFi.localIP());

  // web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/temp", HTTP_GET, handleTemp);
  server.begin();
  Serial.println("Web server started");
}

// ──────────────────────────────────────────────────────────────────────────────
// LOOP
// ──────────────────────────────────────────────────────────────────────────────
void loop()
{
  // 1. read sensor
  currentTemp = mlx.readObjectTempC();

  // 2. relay logic
  if (currentTemp >= TEMP_THRESHOLD)
    digitalWrite(RELAY_PIN, RELAY_ACTIVE);
  else
    digitalWrite(RELAY_PIN, !RELAY_ACTIVE);

  // 3. service clients
  server.handleClient();

  delay(200);   // ~5 Hz update is plenty for IR thermometer
}
