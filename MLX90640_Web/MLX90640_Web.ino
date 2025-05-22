/* ──────────────────────────────────────────────────────────────────
 *  ESP32 + MLX90640   •   Simple Thermal‑Image WebServer
 *  ---------------------------------------------------------------
 *  ▸ Connects to an EXISTING Wi‑Fi hotspot (STA mode)
 *  ▸ /           → HTML canvas viewer (auto refresh)
 *  ▸ /frame      → 768‑byte binary (mapped 0..255)
 *
 *  Libraries  (install via Library‑Manager):
 *    ▸ WiFi       (comes with ESP32 core)
 *    ▸ WebServer  (comes with ESP32 core)
 *    ▸ Wire       (comes with ESP32 core)
 *    ▸ "MLX90640_API"  &  "MLX90640_I2C_Driver"  by Melexis
 *  Hardware:
 *    ▸ MLX90640 on GPIO 21 (SDA) / 22 (SCL)  – 3.3 V !!
 *  ---------------------------------------------------------------- */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

/* -------------------- user settings -------------------- */
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";
const byte  MLX_ADDRESS = 0x33;        // factory default
static constexpr float REFRESH_HZ = 8; // sensor frame‑rate (8 Hz)

/* -------------------- globals -------------------------- */
WebServer server(80);
paramsMLX90640 mlxParams;
/* ---- buffers ---- */
uint16_t frameRaw[834];     // 834 words → what the API wants
float    tempC[768];        // °C for every pixel (32 × 24)
uint8_t  mapped[768];       // 0–255 for the web page
                  // mapped 0‑255 for web
unsigned long framePeriod = 1000 / REFRESH_HZ;

/* -------------------- helper: create grayscale LUT ----- */
uint32_t lut[256];
void buildLUT()
{
  for (int i = 0; i < 256; ++i) {
    uint8_t g = i;         // pure grayscale (0‑255)
    lut[i] = 0xFF000000 | (g << 16) | (g << 8) | g; // ARGB8888
  }
}

/* -------------------- map temp to 0‑255 ---------------- */
void captureAndMap()
{
  /* 1. grab a full frame (two sub‑pages) */
  for (int sub = 0; sub < 2; ++sub)
      MLX90640_GetFrameData(MLX_ADDRESS, frameRaw);

  /* 2. convert to temperatures */
  float Ta = MLX90640_GetTa(frameRaw, &mlxParams);
  MLX90640_CalculateTo(frameRaw, &mlxParams, 0.95, Ta, tempC);

  /* 3. auto‑scale and map 0‑255 */
  float tMin = tempC[0], tMax = tempC[0];
  for (int i = 1; i < 768; ++i) {
    if (tempC[i] < tMin) tMin = tempC[i];
    if (tempC[i] > tMax) tMax = tempC[i];
  }
  if (tMax == tMin) tMax = tMin + 1;

  for (int i = 0; i < 768; ++i)
    mapped[i] = (uint8_t) constrain(255.0f * (tempC[i] - tMin) /
                                    (tMax - tMin), 0, 255);
}


/* -------------------- HTTP handlers ------------------- */
const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html>
<head><meta charset=utf-8><title>MLX90640 Thermal Cam</title>
<style>
  body{margin:0;background:#111;color:#eee;font-family:sans-serif;text-align:center}
  h1{font-size:1.4rem;margin:1rem 0}canvas{image-rendering:pixelated;border:4px solid #444}
</style></head><body>
<h1>ESP32 + MLX90640 Thermal Camera</h1>
<canvas id=cv width=32 height=24></canvas>
<script>
const cv=document.getElementById('cv'),ctx=cv.getContext('2d');
const imgData=ctx.createImageData(32,24);
async function getFrame(){
  const r=await fetch('frame');       // binary 768B
  if(!r.ok)return setTimeout(getFrame,500);
  const buf=new Uint8Array(await r.arrayBuffer());
  for(let i=0;i<768;i++){
    const v=buf[i];
    const idx=i*4;
    imgData.data[idx]=v;      // R
    imgData.data[idx+1]=v;    // G
    imgData.data[idx+2]=v;    // B
    imgData.data[idx+3]=255;  // A
  }
  ctx.putImageData(imgData,0,0);
  requestAnimationFrame(getFrame);
}
getFrame();
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleFrame()
{
  captureAndMap();                          // fill 'mapped'
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Length", "768");
  server.send_P(200, "application/octet-stream", (const char*)mapped, 768);
}

void setupWebServer()
{
  server.on("/", handleRoot);
  server.on("/frame", HTTP_GET, handleFrame);
  server.begin();
}

/* -------------------- setup --------------------------- */
void setup()
{
  Serial.begin(115200);
  delay(1000);

  /* ---- Wi‑Fi STA ---- */
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wi‑Fi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  Serial.println("\nConnected – IP: " + WiFi.localIP().toString());

  /* ---- I²C + MLX90640 ---- */
  Wire.begin();                  // SDA=21, SCL=22
  Wire.setClock(400000);
  Wire.begin(0x33);
  if (MLX90640_SetRefreshRate(MLX_ADDRESS, 0x03) != 0) // 8 Hz
    Serial.println("Failed to set refresh rate");

  MLX90640_SetChessMode(MLX_ADDRESS);
  int status = MLX90640_DumpEE(MLX_ADDRESS, (uint16_t*)frameRaw);
  if (status != 0) Serial.println("EE dump failed");

  status = MLX90640_ExtractParameters((uint16_t*)frameRaw, &mlxParams);
  if (status != 0) Serial.println("Parameter extraction failed");

  buildLUT();
  setupWebServer();

  Serial.println("HTTP server ready – open http://" + WiFi.localIP().toString());
}

/* -------------------- main loop ---------------------- */
void loop() { server.handleClient(); }
