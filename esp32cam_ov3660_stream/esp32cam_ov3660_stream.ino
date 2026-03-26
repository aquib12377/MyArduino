/*
 * ============================================================
 *  Alpha Electronz — ESP32-CAM Streaming Server
 *  Camera Module : OV3660 (3 MP, connected via ribbon cable)
 *  Board         : AI-Thinker ESP32-CAM
 *
 *  Endpoints (all on port 80):
 *    /           → Live control + preview HTML page
 *    /stream     → MJPEG stream  (used by Python face-rec)
 *    /capture    → Single JPEG snapshot
 *    /status     → JSON status (resolution, fps, uptime)
 *    /control?var=<name>&val=<int>  → Tune camera settings
 *
 *  Python stream URL  →  http://<IP>/stream
 *
 *  Board setup in Arduino IDE:
 *    Tools → Board       : "AI Thinker ESP32-CAM"
 *    Tools → Partition   : "Huge APP (3MB No OTA)"
 *    Tools → PSRAM       : "Enabled"
 *    Tools → CPU Freq    : "240 MHz"
 *    Upload speed        : 115200
 *    (Hold IO0 LOW + press EN to enter flash mode)
 *
 *  Libraries required:
 *    ESP32 board package by Espressif (via Board Manager)
 *    → includes esp_camera, WiFi, WebServer, ESPmDNS
 *
 *  No extra libraries needed from Library Manager.
 * ============================================================
 */

#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <ESPmDNS.h>

// ─────────────────────────────────────────────────────────────
//  WiFi credentials  —  EDIT THESE
// ─────────────────────────────────────────────────────────────
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube@123";

// mDNS hostname — access via  http://esp32cam.local/
const char* MDNS_HOST = "esp32cam";

// ─────────────────────────────────────────────────────────────
//  AI-Thinker ESP32-CAM pin map
//  Same connector for OV2640 and OV3660 ribbon cables
// ─────────────────────────────────────────────────────────────
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// Onboard LED flash (GPIO 4) — leave LOW during streaming
// to avoid overexposing the OV3660
#define LED_GPIO_NUM     4

// ─────────────────────────────────────────────────────────────
//  Streaming defaults  —  tune as needed
//
//  For face recognition accuracy, SVGA (800×600) is the
//  sweet spot: enough detail for dlib's HOG detector while
//  keeping per-frame processing time under ~120 ms on a
//  modern laptop CPU.
//
//  Available sizes (uncomment one):
//    FRAMESIZE_QVGA   320×240   ~30 fps  (fastest, lowest quality)
//    FRAMESIZE_CIF    400×296   ~25 fps
//    FRAMESIZE_VGA    640×480   ~20 fps
//    FRAMESIZE_SVGA   800×600   ~15 fps  ← DEFAULT — best for face-rec
//    FRAMESIZE_XGA   1024×768   ~10 fps
//    FRAMESIZE_SXGA  1280×1024  ~ 7 fps
//    FRAMESIZE_UXGA  1600×1200  ~ 5 fps  (OV3660 native 3 MP)
// ─────────────────────────────────────────────────────────────
#define DEFAULT_FRAMESIZE  FRAMESIZE_SVGA
#define DEFAULT_QUALITY    12    // 0 (best)–63 (worst). 10–15 = high quality

// Frame buffer count — 2 gives smooth streaming without tearing
#define FRAME_BUFFER_COUNT 2

// XCLK frequency — 20 MHz is the sweet spot for OV3660
// Increase to 24 MHz for slightly higher FPS (may increase noise)
#define XCLK_FREQ_HZ  20000000

// MJPEG stream part boundary string
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lld\r\n\r\n";

// ─────────────────────────────────────────────────────────────
//  Runtime stats
// ─────────────────────────────────────────────────────────────
static uint32_t streamFrameCount = 0;
static uint32_t streamStartTime  = 0;
static float    currentFPS       = 0.0f;

// ═════════════════════════════════════════════════════════════
//  Camera initialisation — OV3660 tuned settings
// ═════════════════════════════════════════════════════════════
bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = XCLK_FREQ_HZ;
  config.pixel_format = PIXFORMAT_JPEG;   // JPEG output directly from sensor
  config.frame_size   = DEFAULT_FRAMESIZE;
  config.jpeg_quality = DEFAULT_QUALITY;
  config.fb_count     = FRAME_BUFFER_COUNT;
  config.grab_mode    = CAMERA_GRAB_LATEST;   // Always serve newest frame

  // If PSRAM is available, use it for larger frame buffers
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = DEFAULT_QUALITY;
    config.fb_count = 2;
    Serial.println("[CAM] PSRAM found — using for frame buffers");
  } else {
    // No PSRAM: drop to VGA to fit in SRAM
    config.fb_location  = CAMERA_FB_IN_DRAM;
    config.frame_size   = FRAMESIZE_VGA;
    config.fb_count     = 1;
    Serial.println("[CAM] No PSRAM — falling back to VGA, 1 buffer");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed: 0x%x\n", err);
    return false;
  }

  // ── OV3660 specific sensor tuning ──────────────────────────
  sensor_t* s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("[CAM] Could not get sensor handle");
    return false;
  }

  // OV3660 outputs image upside-down by default on AI-Thinker board
  s->set_vflip(s, 1);        // flip vertically — correct orientation
  s->set_hmirror(s, 0);      // no horizontal mirror (set 1 if mirrored)

  // Colour / exposure / gain auto controls — all on for best quality
  s->set_whitebal(s, 1);     // Auto white balance
  s->set_awb_gain(s, 1);     // AWB gain
  s->set_wb_mode(s, 0);      // AWB mode: 0=auto, 1=sunny, 2=cloudy, 3=office, 4=home
  s->set_exposure_ctrl(s, 1);// AEC (auto exposure)
  s->set_aec2(s, 1);         // AEC DSP — improved night performance
  s->set_ae_level(s, 0);     // AEC level offset  (-2 to +2)
  s->set_aec_value(s, 300);  // Manual AEC (only when set_exposure_ctrl = 0)
  s->set_gain_ctrl(s, 1);    // AGC (auto gain)
  s->set_agc_gain(s, 0);     // Manual gain (only when AGC off)
  s->set_gainceiling(s, (gainceiling_t)2);  // Max gain ceiling (0-6)

  // Image quality / correction
  s->set_bpc(s, 1);          // Black pixel correction
  s->set_wpc(s, 1);          // White pixel correction
  s->set_raw_gma(s, 1);      // Raw gamma — better tonal response
  s->set_lenc(s, 1);         // Lens shading correction — evens out vignetting
  s->set_dcw(s, 1);          // Downsize EN (improves quality at smaller frames)

  // Image adjustments (all neutral — tweak to taste)
  s->set_brightness(s, 0);   // -2 to +2
  s->set_contrast(s, 0);     // -2 to +2
  s->set_saturation(s, 0);   // -2 to +2
  s->set_sharpness(s, 0);    // -2 to +2 (OV3660 only — ignored on OV2640)
  s->set_denoise(s, 1);      // Denoise (OV3660 only) — reduces grain in low light
  s->set_special_effect(s, 0);  // 0=normal, 1=neg, 2=bw, 3=red, 4=green, 5=blue, 6=sepia
  s->set_colorbar(s, 0);     // Test colour bar off

  Serial.println("[CAM] OV3660 tuned and ready");
  return true;
}

// ═════════════════════════════════════════════════════════════
//  HTTP handler — MJPEG stream  (/stream)
// ═════════════════════════════════════════════════════════════
static esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t*  fb   = NULL;
  esp_err_t     res  = ESP_OK;
  char          part_buf[128];

  // Set CORS headers so any origin (Python/browser) can connect
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate",                "15");
  httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);

  // Reset FPS counter for this client
  streamFrameCount = 0;
  streamStartTime  = millis();

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[STREAM] Frame capture failed");
      res = ESP_FAIL;
      break;
    }

    // Write MIME boundary
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY,
                                  strlen(_STREAM_BOUNDARY));
    }

    // Write part header with Content-Length
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART,
                             fb->len, esp_timer_get_time());
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }

    // Write JPEG payload
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) break;   // client disconnected

    // Update running FPS
    streamFrameCount++;
    uint32_t elapsed = millis() - streamStartTime;
    if (elapsed > 0) currentFPS = (float)streamFrameCount * 1000.0f / elapsed;

    // Yield so WiFi stack can breathe — keeps connection stable
    taskYIELD();
  }

  return res;
}

// ═════════════════════════════════════════════════════════════
//  HTTP handler — Single JPEG snapshot  (/capture)
// ═════════════════════════════════════════════════════════════
static esp_err_t capture_handler(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ═════════════════════════════════════════════════════════════
//  HTTP handler — Status JSON  (/status)
// ═════════════════════════════════════════════════════════════
static esp_err_t status_handler(httpd_req_t* req) {
  sensor_t* s = esp_camera_sensor_get();
  static char json[512];

  snprintf(json, sizeof(json),
    "{"
    "\"board\":\"AI-Thinker ESP32-CAM\","
    "\"sensor\":\"OV3660\","
    "\"uptime_s\":%lu,"
    "\"fps\":%.1f,"
    "\"frames\":%u,"
    "\"framesize\":%d,"
    "\"quality\":%d,"
    "\"brightness\":%d,"
    "\"contrast\":%d,"
    "\"saturation\":%d,"
    "\"sharpness\":%d,"
    "\"vflip\":%d,"
    "\"hmirror\":%d,"
    "\"awb\":%d,"
    "\"aec\":%d,"
    "\"agc\":%d,"
    "\"psram\":%d,"
    "\"free_heap\":%u"
    "}",
    millis() / 1000,
    currentFPS,
    streamFrameCount,
    s->status.framesize,
    s->status.quality,
    s->status.brightness,
    s->status.contrast,
    s->status.saturation,
    s->status.sharpness,
    s->status.vflip,
    s->status.hmirror,
    s->status.awb,
    s->status.aec,
    s->status.agc,
    psramFound() ? 1 : 0,
    (uint32_t)esp_get_free_heap_size()
  );

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_sendstr(req, json);
}

// ═════════════════════════════════════════════════════════════
//  HTTP handler — Runtime control  (/control?var=X&val=Y)
//
//  Examples:
//    /control?var=framesize&val=8      (8=SVGA, 5=VGA, 4=CIF)
//    /control?var=quality&val=10
//    /control?var=brightness&val=1
//    /control?var=vflip&val=1
//    /control?var=wb_mode&val=2        (2=cloudy)
// ═════════════════════════════════════════════════════════════
static esp_err_t control_handler(httpd_req_t* req) {
  char buf[128];
  char variable[32] = {0};
  char value[8]     = {0};

  size_t buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > sizeof(buf)) buf_len = sizeof(buf);

  if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_query_key_value(buf, "var", variable, sizeof(variable));
  httpd_query_key_value(buf, "val", value,    sizeof(value));

  int val = atoi(value);
  sensor_t* s = esp_camera_sensor_get();
  int res = -1;

  // Map variable name to sensor setter
  if (!strcmp(variable, "framesize")) {
    res = s->set_framesize(s, (framesize_t)val);
  } else if (!strcmp(variable, "quality")) {
    res = s->set_quality(s, val);
  } else if (!strcmp(variable, "brightness")) {
    res = s->set_brightness(s, val);
  } else if (!strcmp(variable, "contrast")) {
    res = s->set_contrast(s, val);
  } else if (!strcmp(variable, "saturation")) {
    res = s->set_saturation(s, val);
  } else if (!strcmp(variable, "sharpness")) {
    res = s->set_sharpness(s, val);
  } else if (!strcmp(variable, "denoise")) {
    res = s->set_denoise(s, val);
  } else if (!strcmp(variable, "special_effect")) {
    res = s->set_special_effect(s, val);
  } else if (!strcmp(variable, "wb_mode")) {
    res = s->set_wb_mode(s, val);
  } else if (!strcmp(variable, "awb")) {
    res = s->set_whitebal(s, val);
  } else if (!strcmp(variable, "awb_gain")) {
    res = s->set_awb_gain(s, val);
  } else if (!strcmp(variable, "aec")) {
    res = s->set_exposure_ctrl(s, val);
  } else if (!strcmp(variable, "aec2")) {
    res = s->set_aec2(s, val);
  } else if (!strcmp(variable, "ae_level")) {
    res = s->set_ae_level(s, val);
  } else if (!strcmp(variable, "agc")) {
    res = s->set_gain_ctrl(s, val);
  } else if (!strcmp(variable, "agc_gain")) {
    res = s->set_agc_gain(s, val);
  } else if (!strcmp(variable, "gainceiling")) {
    res = s->set_gainceiling(s, (gainceiling_t)val);
  } else if (!strcmp(variable, "bpc")) {
    res = s->set_bpc(s, val);
  } else if (!strcmp(variable, "wpc")) {
    res = s->set_wpc(s, val);
  } else if (!strcmp(variable, "raw_gma")) {
    res = s->set_raw_gma(s, val);
  } else if (!strcmp(variable, "lenc")) {
    res = s->set_lenc(s, val);
  } else if (!strcmp(variable, "hmirror")) {
    res = s->set_hmirror(s, val);
  } else if (!strcmp(variable, "vflip")) {
    res = s->set_vflip(s, val);
  } else if (!strcmp(variable, "dcw")) {
    res = s->set_dcw(s, val);
  } else if (!strcmp(variable, "colorbar")) {
    res = s->set_colorbar(s, val);
  } else if (!strcmp(variable, "flash")) {
    // Toggle LED flash — careful: gets HOT on AI-Thinker boards
    digitalWrite(LED_GPIO_NUM, val ? HIGH : LOW);
    res = 0;
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  if (res != 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_sendstr(req, "OK");
}

// ═════════════════════════════════════════════════════════════
//  HTTP handler — Control page  (/)
//  A minimal HTML page with live stream + sliders
// ═════════════════════════════════════════════════════════════
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Alpha Electronz — ESP32-CAM</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#111;color:#eee;font-family:system-ui,sans-serif;padding:12px}
  h1{font-size:1rem;letter-spacing:.05em;margin-bottom:12px;color:#aaa}
  #stream-wrap{position:relative;display:inline-block;max-width:100%}
  #stream{width:100%;max-width:820px;display:block;border-radius:6px;background:#000}
  .badge{position:absolute;top:6px;right:6px;background:rgba(0,0,0,.55);
         color:#4f4;font-size:.7rem;padding:2px 7px;border-radius:4px;
         font-variant-numeric:tabular-nums}
  .controls{margin-top:14px;display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:10px}
  .ctrl{background:#1e1e1e;border-radius:6px;padding:10px}
  .ctrl label{display:flex;justify-content:space-between;font-size:.8rem;color:#aaa;margin-bottom:6px}
  .ctrl input[type=range]{width:100%;accent-color:#4f8}
  .ctrl select{width:100%;background:#2a2a2a;color:#eee;border:1px solid #444;
               border-radius:4px;padding:4px;font-size:.85rem}
  .btn{margin-top:12px;padding:7px 16px;background:#2a5;color:#fff;border:none;
       border-radius:4px;cursor:pointer;font-size:.85rem}
  .btn:hover{background:#3b6}
  #stats{font-size:.75rem;color:#666;margin-top:8px}
</style>
</head>
<body>
<h1>&#9679; Alpha Electronz &mdash; OV3660 Live Stream</h1>
<div id="stream-wrap">
  <img id="stream" src="/stream" alt="stream"/>
  <span class="badge" id="fps-badge">-- fps</span>
</div>
<div id="stats">Fetching status...</div>

<div class="controls">
  <div class="ctrl">
    <label>Resolution <span id="fs-val"></span></label>
    <select id="framesize" onchange="ctrl('framesize',this.value)">
      <option value="4">CIF 400&times;296</option>
      <option value="5">VGA 640&times;480</option>
      <option value="8" selected>SVGA 800&times;600</option>
      <option value="9">XGA 1024&times;768</option>
      <option value="10">SXGA 1280&times;1024</option>
    </select>
  </div>
  <div class="ctrl">
    <label>JPEG Quality <span id="q-val">12</span></label>
    <input type="range" min="4" max="40" value="12" id="quality"
           oninput="document.getElementById('q-val').textContent=this.value"
           onchange="ctrl('quality',this.value)">
  </div>
  <div class="ctrl">
    <label>Brightness <span id="br-val">0</span></label>
    <input type="range" min="-2" max="2" value="0" id="brightness"
           oninput="document.getElementById('br-val').textContent=this.value"
           onchange="ctrl('brightness',this.value)">
  </div>
  <div class="ctrl">
    <label>Contrast <span id="ct-val">0</span></label>
    <input type="range" min="-2" max="2" value="0" id="contrast"
           oninput="document.getElementById('ct-val').textContent=this.value"
           onchange="ctrl('contrast',this.value)">
  </div>
  <div class="ctrl">
    <label>Saturation <span id="sat-val">0</span></label>
    <input type="range" min="-2" max="2" value="0" id="saturation"
           oninput="document.getElementById('sat-val').textContent=this.value"
           onchange="ctrl('saturation',this.value)">
  </div>
  <div class="ctrl">
    <label>Sharpness <span id="sh-val">0</span></label>
    <input type="range" min="-2" max="2" value="0" id="sharpness"
           oninput="document.getElementById('sh-val').textContent=this.value"
           onchange="ctrl('sharpness',this.value)">
  </div>
  <div class="ctrl">
    <label>WB Mode <span></span></label>
    <select id="wb_mode" onchange="ctrl('wb_mode',this.value)">
      <option value="0" selected>Auto</option>
      <option value="1">Sunny</option>
      <option value="2">Cloudy</option>
      <option value="3">Office</option>
      <option value="4">Home</option>
    </select>
  </div>
  <div class="ctrl">
    <label>AE Level <span id="ae-val">0</span></label>
    <input type="range" min="-2" max="2" value="0" id="ae_level"
           oninput="document.getElementById('ae-val').textContent=this.value"
           onchange="ctrl('ae_level',this.value)">
  </div>
  <div class="ctrl">
    <label>Gain Ceiling <span id="gc-val">2</span></label>
    <input type="range" min="0" max="6" value="2" id="gainceiling"
           oninput="document.getElementById('gc-val').textContent=this.value"
           onchange="ctrl('gainceiling',this.value)">
  </div>
  <div class="ctrl">
    <label>V-Flip (OV3660 default on) <span></span></label>
    <select id="vflip" onchange="ctrl('vflip',this.value)">
      <option value="1" selected>On</option>
      <option value="0">Off</option>
    </select>
  </div>
  <div class="ctrl">
    <label>H-Mirror <span></span></label>
    <select id="hmirror" onchange="ctrl('hmirror',this.value)">
      <option value="0" selected>Off</option>
      <option value="1">On</option>
    </select>
  </div>
  <div class="ctrl">
    <label>Flash LED <span></span></label>
    <select id="flash" onchange="ctrl('flash',this.value)">
      <option value="0" selected>Off</option>
      <option value="1">On</option>
    </select>
  </div>
</div>

<a class="btn" href="/capture" target="_blank">&#128247; Snapshot</a>
<button class="btn" onclick="loadStatus()">&#8635; Refresh Status</button>

<script>
function ctrl(variable, value) {
  fetch('/control?var=' + variable + '&val=' + value)
    .catch(e => console.warn('ctrl error', e));
}

function loadStatus() {
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      document.getElementById('stats').textContent =
        'FPS: ' + d.fps.toFixed(1) +
        '  |  Free heap: ' + (d.free_heap/1024).toFixed(0) + ' KB' +
        '  |  PSRAM: ' + (d.psram ? 'yes' : 'no') +
        '  |  Uptime: ' + d.uptime_s + ' s';
      document.getElementById('fps-badge').textContent = d.fps.toFixed(1) + ' fps';
    })
    .catch(() => {});
}

setInterval(loadStatus, 3000);
loadStatus();
</script>
</body>
</html>
)rawhtml";

static esp_err_t index_handler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

// ═════════════════════════════════════════════════════════════
//  HTTP server startup
// ═════════════════════════════════════════════════════════════
httpd_handle_t streamHttpd = NULL;

void startWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 8;
  config.stack_size       = 8192;
  config.server_port      = 80;

  // Increase send/receive timeouts for stable streaming
  config.recv_wait_timeout = 10;
  config.send_wait_timeout = 10;

  if (httpd_start(&streamHttpd, &config) != ESP_OK) {
    Serial.println("[HTTP] Failed to start server");
    return;
  }

  httpd_uri_t index_uri  = { "/",        HTTP_GET, index_handler,   NULL };
  httpd_uri_t stream_uri = { "/stream",  HTTP_GET, stream_handler,  NULL };
  httpd_uri_t capture_uri= { "/capture", HTTP_GET, capture_handler, NULL };
  httpd_uri_t status_uri = { "/status",  HTTP_GET, status_handler,  NULL };
  httpd_uri_t ctrl_uri   = { "/control", HTTP_GET, control_handler, NULL };

  httpd_register_uri_handler(streamHttpd, &index_uri);
  httpd_register_uri_handler(streamHttpd, &stream_uri);
  httpd_register_uri_handler(streamHttpd, &capture_uri);
  httpd_register_uri_handler(streamHttpd, &status_uri);
  httpd_register_uri_handler(streamHttpd, &ctrl_uri);

  Serial.println("[HTTP] Server started on port 80");
}

// ═════════════════════════════════════════════════════════════
//  WiFi connection with retry
// ═════════════════════════════════════════════════════════════
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to '%s' ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);   // disable WiFi modem sleep for lower latency

  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] FAILED — restarting in 5 s");
    delay(5000);
    ESP.restart();
  }

  Serial.print("[WiFi] Connected! IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
}

// ═════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════
void setup() {
  // Disable brownout detector — prevents resets during WiFi transmit spikes
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\n=== Alpha Electronz ESP32-CAM OV3660 ===");

  // LED pin
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);   // flash OFF during boot

  // Initialise camera
  if (!initCamera()) {
    Serial.println("[BOOT] Camera init failed — restarting in 5 s");
    delay(5000);
    ESP.restart();
  }

  // Connect to WiFi
  connectWiFi();

  // mDNS — accessible via  http://esp32cam.local/
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] http://%s.local/\n", MDNS_HOST);
  } else {
    Serial.println("[mDNS] Failed to start");
  }

  // Start HTTP server
  startWebServer();

  // Print access URLs
  Serial.println("\n--- Access URLs ---");
  Serial.printf("  Preview page : http://%s/\n",        WiFi.localIP().toString().c_str());
  Serial.printf("  MJPEG stream : http://%s/stream\n",  WiFi.localIP().toString().c_str());
  Serial.printf("  Snapshot     : http://%s/capture\n", WiFi.localIP().toString().c_str());
  Serial.printf("  Status JSON  : http://%s/status\n",  WiFi.localIP().toString().c_str());
  Serial.printf("  Control      : http://%s/control?var=quality&val=10\n",
                WiFi.localIP().toString().c_str());
  Serial.println("-------------------");
  Serial.println("Ready.\n");
}

// ═════════════════════════════════════════════════════════════
//  LOOP — watchdog and WiFi reconnect
// ═════════════════════════════════════════════════════════════
void loop() {
  // Reconnect if WiFi drops
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost connection — reconnecting...");
    WiFi.reconnect();
    uint8_t t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 20) {
      delay(500);
      t++;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Reconnect failed — restarting");
      ESP.restart();
    }
    Serial.printf("[WiFi] Reconnected. IP: %s\n", WiFi.localIP().toString().c_str());
  }

  // Print a heartbeat every 30 s
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.printf("[HEARTBEAT] Uptime: %lu s | FPS: %.1f | RSSI: %d dBm | Heap: %u B\n",
                  millis() / 1000,
                  currentFPS,
                  WiFi.RSSI(),
                  esp_get_free_heap_size());
  }

  delay(100);
}
