// ================================================================
//  ESP32_CAM_SmartLock.ino
//  Face Recognition → Serial command to Arduino Nano
//
//  Board  : ESP32 Arduino 1.0.6  (AI Thinker ESP32-CAM)
//  Camera : OV2640
//  Sends  : "FACE_OK\n"   when enrolled face is recognised
//           "FACE_FAIL\n"  when an unknown face is detected
//
//  Wiring to Nano:
//    ESP32 GPIO14 (TX) ──────────────── Nano D6 (camSerial RX)
//    ESP32 GPIO15 (RX) ← volt-divider ← Nano A0 (camSerial TX)
//
//  Web UI : Connect to WiFi AP "SmartLock-CAM" / "12345678"
//           Open http://192.168.4.1 in browser
//           • Live stream with face-detection overlay
//           • Enroll / Delete / Toggle recognition
// ================================================================

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include <WiFi.h>

// ── Face detection / recognition (ESP32 board 1.0.6) ────────────
#include "fd_forward.h"
#include "fr_forward.h"
#include "fr_flash.h"

// =================================================================
//  AI-Thinker ESP32-CAM pin map
// =================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =================================================================
//  Serial to Arduino Nano
// =================================================================
#define NANO_TX_PIN  14          // ESP32 TX → Nano D6 (RX)
#define NANO_RX_PIN  15          // ESP32 RX ← Nano A0 (TX) via divider
#define NANO_BAUD    9600
HardwareSerial NanoSerial(2);    // UART2 remapped to GPIO14/15

// =================================================================
//  WiFi Access Point
// =================================================================
const char *AP_SSID = "SmartLock-CAM";
const char *AP_PASS = "12345678";      // min 8 chars for WPA2

// =================================================================
//  Face Recognition Config
// =================================================================
#define ENROLL_CONFIRM_TIMES  5   // samples needed per enrollment
#define FACE_ID_SAVE_NUMBER   7   // max stored faces

static face_id_name_list st_face_list;
static dl_matrix3du_t   *aligned_face = NULL;
static mtmn_config_t     mtmn_config  = {0};

// =================================================================
//  Application State
// =================================================================
typedef enum { STATE_DETECTING, STATE_RECOGNISING, STATE_ENROLLING, STATE_IDLE } app_state_t;
static volatile app_state_t  g_state          = STATE_RECOGNISING;
static volatile int          g_enroll_left    = 0;
static String                g_enroll_name    = "";
static String                g_last_result    = "Waiting...";
static unsigned long         g_last_send_ms   = 0;
static const unsigned long   SEND_COOLDOWN_MS = 8000;  // 8 s between serial msgs
static volatile bool         g_recognition_on = true;   // toggle from UI

// Flash LED (GPIO4 on AI-Thinker)
#define FLASH_LED_PIN  4

// =================================================================
//  HTTP server handle
// =================================================================
httpd_handle_t stream_httpd = NULL;
httpd_handle_t control_httpd = NULL;

// =================================================================
//  MTMN config initialiser (for board 1.0.6)
// =================================================================
static void init_mtmn_config() {
  mtmn_config.type              = FAST;
  mtmn_config.min_face          = 80;
  mtmn_config.pyramid           = 0.707;
  mtmn_config.pyramid_times     = 4;
  mtmn_config.p_threshold.score = 0.6;
  mtmn_config.p_threshold.nms   = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7;
  mtmn_config.r_threshold.nms   = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7;
  mtmn_config.o_threshold.nms   = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;
}

// =================================================================
//  Camera initialisation
// =================================================================
static bool init_camera() {
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;   // will switch per-frame for face det
  config.frame_size   = FRAMESIZE_QVGA;   // 320×240 — best for face recognition
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  // PSRAM detected → can use 2 framebuffers
  if (psramFound()) {
    config.fb_count = 2;
    config.jpeg_quality = 10;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed: 0x%x\n", err);
    return false;
  }

  // Adjust sensor for indoor / face work
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);
  s->set_brightness(s, 1);
  s->set_saturation(s, -1);
  s->set_gainceiling(s, (gainceiling_t)6);

  Serial.println("[CAM] Camera ready  (QVGA 320x240)");
  return true;
}

// =================================================================
//  Draw rectangle + landmarks on RGB888 image
// =================================================================
static void draw_face_boxes(dl_matrix3du_t *image, box_array_t *boxes, bool recognised) {
  int w = image->w;
  uint8_t *img = image->item;

  uint8_t r = recognised ? 0 : 255;
  uint8_t g = recognised ? 255 : 0;
  uint8_t b = 0;

  for (int i = 0; i < boxes->len; i++) {
    int x = (int)boxes->box[i].box_p[0];
    int y = (int)boxes->box[i].box_p[1];
    int bw = (int)(boxes->box[i].box_p[2] - x);
    int bh = (int)(boxes->box[i].box_p[3] - y);

    // Clamp
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + bw > w) bw = w - x;
    if (y + bh > image->h) bh = image->h - y;

    // Draw top & bottom horizontal lines
    for (int dx = 0; dx < bw; dx++) {
      int idx_top = ((y) * w + (x + dx)) * 3;
      int idx_bot = ((y + bh - 1) * w + (x + dx)) * 3;
      img[idx_top] = r; img[idx_top + 1] = g; img[idx_top + 2] = b;
      img[idx_bot] = r; img[idx_bot + 1] = g; img[idx_bot + 2] = b;
    }
    // Draw left & right vertical lines
    for (int dy = 0; dy < bh; dy++) {
      int idx_lft = ((y + dy) * w + x) * 3;
      int idx_rgt = ((y + dy) * w + (x + bw - 1)) * 3;
      img[idx_lft] = r; img[idx_lft + 1] = g; img[idx_lft + 2] = b;
      img[idx_rgt] = r; img[idx_rgt + 1] = g; img[idx_rgt + 2] = b;
    }
  }
}

// =================================================================
//  Send command to Nano (with cooldown)
// =================================================================
static void sendToNano(const char *cmd) {
  unsigned long now = millis();
  if (now - g_last_send_ms < SEND_COOLDOWN_MS) return;
  g_last_send_ms = now;
  NanoSerial.println(cmd);
  Serial.printf("[NANO] Sent: %s\n", cmd);
}

// =================================================================
//  MJPEG Stream handler  (with face detection / recognition)
// =================================================================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t    res = ESP_OK;
  size_t       _jpg_buf_len = 0;
  uint8_t     *_jpg_buf     = NULL;
  char         part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[CAM] Capture failed");
      res = ESP_FAIL;
      break;
    }

    // ── Face detection branch ──────────────────────────
    if (g_recognition_on || g_state == STATE_ENROLLING) {
      // Decode JPEG → RGB888
      dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
      if (image_matrix) {
        bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);
        esp_camera_fb_return(fb);
        fb = NULL;

        if (converted) {
          box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);

          if (net_boxes) {
            bool recognised = false;

            // Align face for recognition / enrollment
            aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
            if (aligned_face && align_face(net_boxes, image_matrix, aligned_face) == ESP_OK) {

              // Convert uint8 aligned image → float face-ID embedding
              // (1.0.6 enroll/recognize expect dl_matrix3d_t*, not dl_matrix3du_t*)
              dl_matrix3d_t *face_id = get_face_id(aligned_face);

              if (face_id) {
                // ── ENROLL MODE ───────────────────────────
                if (g_state == STATE_ENROLLING && g_enroll_left > 0) {
                  int left = enroll_face_with_name(&st_face_list, face_id, (char *)g_enroll_name.c_str());
                  g_enroll_left = left;
                  Serial.printf("[ENROLL] Samples left: %d\n", left);
                  if (left == 0) {
                    g_state = STATE_RECOGNISING;
                    g_last_result = "Enrolled: " + g_enroll_name;
                    Serial.printf("[ENROLL] Done — %s\n", g_enroll_name.c_str());
                  } else {
                    g_last_result = "Enrolling... " + String(ENROLL_CONFIRM_TIMES - left) + "/" + String(ENROLL_CONFIRM_TIMES);
                  }
                }
                // ── RECOGNISE MODE ────────────────────────
                else if (g_recognition_on) {
                  face_id_node *f = recognize_face_with_name(&st_face_list, face_id);
                  if (f) {
                    recognised = true;
                    g_last_result = "✓ " + String(f->id_name);
                    sendToNano("FACE_OK");
                  } else {
                    g_last_result = "✗ Unknown face";
                    sendToNano("FACE_FAIL");
                  }
                }
                dl_matrix3d_free(face_id);
              }
            }
            if (aligned_face) { dl_matrix3du_free(aligned_face); aligned_face = NULL; }

            // Draw bounding boxes (green = recognised, red = unknown)
            draw_face_boxes(image_matrix, net_boxes, recognised);

            dl_lib_free(net_boxes->score);
            dl_lib_free(net_boxes->box);
            dl_lib_free(net_boxes->landmark);
            dl_lib_free(net_boxes);
          }
        }

        // Encode back to JPEG for streaming
        bool jpeg_ok = fmt2jpg(image_matrix->item, image_matrix->w * image_matrix->h * 3,
                               image_matrix->w, image_matrix->h, PIXFORMAT_RGB888, 80,
                               &_jpg_buf, &_jpg_buf_len);
        dl_matrix3du_free(image_matrix);
        if (!jpeg_ok) {
          Serial.println("[CAM] JPEG encode failed");
          res = ESP_FAIL;
          break;
        }
      } else {
        // alloc failed — just send raw JPEG without detection
        _jpg_buf_len = fb->len;
        _jpg_buf = (uint8_t *)malloc(fb->len);
        if (_jpg_buf) memcpy(_jpg_buf, fb->buf, fb->len);
        esp_camera_fb_return(fb);
        fb = NULL;
      }
    }
    // ── No detection — just forward JPEG ───────────────
    else {
      _jpg_buf_len = fb->len;
      _jpg_buf = (uint8_t *)malloc(fb->len);
      if (_jpg_buf) memcpy(_jpg_buf, fb->buf, fb->len);
      esp_camera_fb_return(fb);
      fb = NULL;
    }

    if (!_jpg_buf) { res = ESP_FAIL; break; }

    // Send MJPEG boundary + frame
    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }

    free(_jpg_buf);
    _jpg_buf = NULL;

    if (res != ESP_OK) break;
  }
  return res;
}

// =================================================================
//  API: /enroll?name=xxx
// =================================================================
static esp_err_t enroll_handler(httpd_req_t *req) {
  char buf[64] = {0};
  int  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1 && buf_len <= (int)sizeof(buf)) {
    httpd_req_get_url_query_str(req, buf, buf_len);
    char name[32] = {0};
    if (httpd_query_key_value(buf, "name", name, sizeof(name)) == ESP_OK && strlen(name) > 0) {
      g_enroll_name = String(name);
    } else {
      g_enroll_name = "user_" + String(st_face_list.count);
    }
  } else {
    g_enroll_name = "user_" + String(st_face_list.count);
  }

  g_enroll_left = ENROLL_CONFIRM_TIMES;
  g_state       = STATE_ENROLLING;
  g_last_result = "Enrolling: " + g_enroll_name + " — look at camera";
  Serial.printf("[ENROLL] Started for '%s'\n", g_enroll_name.c_str());

  char resp[128];
  snprintf(resp, sizeof(resp), "{\"ok\":true,\"name\":\"%s\",\"samples\":%d}", g_enroll_name.c_str(), ENROLL_CONFIRM_TIMES);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, strlen(resp));
}

// =================================================================
//  API: /delete — wipe all enrolled faces
// =================================================================
static esp_err_t delete_handler(httpd_req_t *req) {
  delete_face_all_with_name(&st_face_list);
  g_state       = STATE_RECOGNISING;
  g_last_result = "All faces deleted";
  Serial.println("[FACE] All enrolled faces deleted");

  const char *resp = "{\"ok\":true,\"msg\":\"All faces deleted\"}";
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, strlen(resp));
}

// =================================================================
//  API: /toggle — enable / disable recognition
// =================================================================
static esp_err_t toggle_handler(httpd_req_t *req) {
  g_recognition_on = !g_recognition_on;
  g_last_result = g_recognition_on ? "Recognition ON" : "Recognition OFF (stream only)";
  Serial.printf("[MODE] Recognition %s\n", g_recognition_on ? "ON" : "OFF");

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"ok\":true,\"recognition\":%s}", g_recognition_on ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, strlen(resp));
}

// =================================================================
//  API: /status — JSON status for UI polling
// =================================================================
static esp_err_t status_handler(httpd_req_t *req) {
  char resp[256];
  snprintf(resp, sizeof(resp),
    "{\"enrolled\":%d,\"max\":%d,\"recognition\":%s,\"state\":\"%s\",\"result\":\"%s\",\"enroll_left\":%d}",
    st_face_list.count, FACE_ID_SAVE_NUMBER,
    g_recognition_on ? "true" : "false",
    g_state == STATE_ENROLLING ? "enrolling" : "ready",
    g_last_result.c_str(),
    g_enroll_left
  );
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, resp, strlen(resp));
}

// =================================================================
//  WEB UI — served from flash
// =================================================================
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartLock Face Control</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{
    font-family:'Segoe UI',system-ui,-apple-system,sans-serif;
    background:#0f0f1a;
    color:#e0e0e0;
    min-height:100vh;
    display:flex;
    flex-direction:column;
    align-items:center;
  }
  /* ── Header ─────────────────────────────────── */
  .header{
    width:100%;
    background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);
    padding:14px 20px;
    display:flex;
    align-items:center;
    justify-content:center;
    gap:10px;
    border-bottom:2px solid #0f3460;
    box-shadow:0 2px 20px rgba(0,0,0,0.4);
  }
  .header svg{width:26px;height:26px;fill:#00d4ff}
  .header h1{font-size:1.15rem;font-weight:600;color:#fff;letter-spacing:.5px}

  /* ── Main card ──────────────────────────────── */
  .card{
    background:#1a1a2e;
    border:1px solid #1e2a4a;
    border-radius:16px;
    margin:16px;
    max-width:420px;
    width:calc(100% - 32px);
    overflow:hidden;
    box-shadow:0 8px 32px rgba(0,0,0,0.5);
  }

  /* ── Stream viewer ──────────────────────────── */
  .stream-wrap{
    position:relative;
    background:#000;
    width:100%;
    aspect-ratio:4/3;
  }
  .stream-wrap img{
    width:100%;
    height:100%;
    object-fit:contain;
    display:block;
  }
  .stream-overlay{
    position:absolute;
    top:8px;right:8px;
    display:flex;gap:6px;
  }
  .badge{
    font-size:.65rem;
    padding:3px 8px;
    border-radius:20px;
    font-weight:600;
    text-transform:uppercase;
    letter-spacing:.5px;
  }
  .badge-live{background:#e53e3e;color:#fff}
  .badge-rec{background:#38a169;color:#fff}
  .badge-off{background:#718096;color:#fff}
  .badge-enroll{background:#d69e2e;color:#1a1a2e}

  /* ── Status bar ─────────────────────────────── */
  .status-bar{
    padding:10px 16px;
    background:#12121f;
    border-top:1px solid #1e2a4a;
    display:flex;
    align-items:center;
    gap:8px;
    min-height:42px;
  }
  .status-dot{
    width:10px;height:10px;
    border-radius:50%;
    flex-shrink:0;
    animation:pulse 2s infinite;
  }
  .dot-green{background:#38a169}
  .dot-red{background:#e53e3e}
  .dot-yellow{background:#d69e2e}
  .dot-blue{background:#4299e1}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
  .status-text{font-size:.82rem;color:#a0aec0;flex:1}

  /* ── Info row ────────────────────────────────── */
  .info-row{
    display:flex;
    justify-content:space-around;
    padding:12px 16px;
    border-top:1px solid #1e2a4a;
    background:#14142a;
  }
  .info-item{text-align:center}
  .info-val{font-size:1.3rem;font-weight:700;color:#00d4ff}
  .info-label{font-size:.65rem;color:#718096;text-transform:uppercase;margin-top:2px;letter-spacing:.5px}

  /* ── Controls ────────────────────────────────── */
  .controls{
    padding:14px 16px;
    display:flex;
    flex-direction:column;
    gap:10px;
    border-top:1px solid #1e2a4a;
  }
  .input-row{
    display:flex;gap:8px;
  }
  .input-row input{
    flex:1;
    padding:10px 12px;
    border-radius:10px;
    border:1px solid #2d3748;
    background:#0f0f1a;
    color:#fff;
    font-size:.85rem;
    outline:none;
    transition:border .2s;
  }
  .input-row input:focus{border-color:#4299e1}
  .input-row input::placeholder{color:#4a5568}

  .btn-row{display:flex;gap:8px}
  .btn{
    flex:1;
    padding:11px 0;
    border:none;
    border-radius:10px;
    font-size:.82rem;
    font-weight:600;
    cursor:pointer;
    transition:all .2s;
    letter-spacing:.3px;
    display:flex;
    align-items:center;
    justify-content:center;
    gap:6px;
  }
  .btn:active{transform:scale(.97)}
  .btn-enroll{background:linear-gradient(135deg,#2b6cb0,#4299e1);color:#fff}
  .btn-enroll:hover{background:linear-gradient(135deg,#2c5282,#3182ce)}
  .btn-toggle{background:linear-gradient(135deg,#276749,#38a169);color:#fff}
  .btn-toggle:hover{background:linear-gradient(135deg,#22543d,#2f855a)}
  .btn-toggle.off{background:linear-gradient(135deg,#744210,#d69e2e);color:#1a1a2e}
  .btn-delete{background:linear-gradient(135deg,#9b2c2c,#e53e3e);color:#fff}
  .btn-delete:hover{background:linear-gradient(135deg,#822727,#c53030)}
  .btn:disabled{opacity:.5;cursor:not-allowed;transform:none}
  .btn svg{width:15px;height:15px;fill:currentColor}

  /* ── Footer ─────────────────────────────────── */
  .footer{
    padding:10px;
    text-align:center;
    font-size:.65rem;
    color:#4a5568;
  }

  /* ── Toast notification ─────────────────────── */
  .toast{
    position:fixed;
    bottom:24px;
    left:50%;
    transform:translateX(-50%) translateY(80px);
    background:#2d3748;
    color:#fff;
    padding:10px 20px;
    border-radius:10px;
    font-size:.82rem;
    box-shadow:0 4px 20px rgba(0,0,0,0.5);
    transition:transform .3s ease;
    z-index:100;
    pointer-events:none;
  }
  .toast.show{transform:translateX(-50%) translateY(0)}
</style>
</head>
<body>

<div class="header">
  <svg viewBox="0 0 24 24"><path d="M12 1a5 5 0 00-5 5v2H6a2 2 0 00-2 2v10a2 2 0 002 2h12a2 2 0 002-2V10a2 2 0 00-2-2h-1V6a5 5 0 00-5-5zm-3 5a3 3 0 116 0v2H9V6zm3 7a2 2 0 110 4 2 2 0 010-4z"/></svg>
  <h1>SmartLock &middot; Face Control</h1>
</div>

<div class="card">
  <!-- Stream -->
  <div class="stream-wrap">
    <img id="stream" src="/stream" alt="Camera stream">
    <div class="stream-overlay">
      <span class="badge badge-live">LIVE</span>
      <span class="badge badge-rec" id="modeBadge">REC ON</span>
    </div>
  </div>

  <!-- Status -->
  <div class="status-bar">
    <div class="status-dot dot-blue" id="statusDot"></div>
    <div class="status-text" id="statusText">Connecting...</div>
  </div>

  <!-- Info -->
  <div class="info-row">
    <div class="info-item">
      <div class="info-val" id="enrolledCount">-</div>
      <div class="info-label">Enrolled</div>
    </div>
    <div class="info-item">
      <div class="info-val" id="maxSlots">-</div>
      <div class="info-label">Max Slots</div>
    </div>
    <div class="info-item">
      <div class="info-val" id="stateLabel">-</div>
      <div class="info-label">State</div>
    </div>
  </div>

  <!-- Controls -->
  <div class="controls">
    <div class="input-row">
      <input type="text" id="nameInput" placeholder="Face name (e.g. Aquib)" maxlength="20">
      <button class="btn btn-enroll" id="btnEnroll" onclick="doEnroll()">
        <svg viewBox="0 0 24 24"><path d="M15 12a4 4 0 10-8 0 4 4 0 008 0zm-4-6a6 6 0 110 12 6 6 0 010-12zm7 1h2v2h2v2h-2v2h-2v-2h-2V9h2V7z"/></svg>
        Enroll
      </button>
    </div>
    <div class="btn-row">
      <button class="btn btn-toggle" id="btnToggle" onclick="doToggle()">
        <svg viewBox="0 0 24 24"><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17a5 5 0 110-10 5 5 0 010 10zm0-8a3 3 0 100 6 3 3 0 000-6z"/></svg>
        <span id="toggleLabel">Rec ON</span>
      </button>
      <button class="btn btn-delete" id="btnDelete" onclick="doDelete()">
        <svg viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
        Delete All
      </button>
    </div>
  </div>
</div>

<div class="footer">ESP32-CAM &middot; Face Recognition &middot; Smart Lock System</div>
<div class="toast" id="toast"></div>

<script>
  const $ = id => document.getElementById(id);

  function toast(msg, ms=2500){
    const t=$('toast'); t.textContent=msg;
    t.classList.add('show');
    setTimeout(()=>t.classList.remove('show'), ms);
  }

  // Poll status every second
  setInterval(async ()=>{
    try{
      const r = await fetch('/status');
      const d = await r.json();
      $('enrolledCount').textContent = d.enrolled;
      $('maxSlots').textContent      = d.max;
      $('stateLabel').textContent     = d.state === 'enrolling' ? 'ENROLL' : 'READY';
      $('statusText').textContent     = d.result;

      // Badge & dot colour
      const badge = $('modeBadge');
      const dot   = $('statusDot');
      if(d.state === 'enrolling'){
        badge.className='badge badge-enroll'; badge.textContent='ENROLL';
        dot.className='status-dot dot-yellow';
      } else if(d.recognition){
        badge.className='badge badge-rec'; badge.textContent='REC ON';
        dot.className='status-dot dot-green';
      } else {
        badge.className='badge badge-off'; badge.textContent='REC OFF';
        dot.className='status-dot dot-blue';
      }

      // Colour status text based on result
      const st = $('statusText');
      if(d.result.startsWith('\u2713'))       st.style.color='#68d391';
      else if(d.result.startsWith('\u2717'))   st.style.color='#fc8181';
      else if(d.result.includes('Enrolling'))  st.style.color='#f6e05e';
      else                                     st.style.color='#a0aec0';

      // Toggle button label
      const tb=$('btnToggle'), tl=$('toggleLabel');
      if(d.recognition){tl.textContent='Rec ON';  tb.classList.remove('off');}
      else             {tl.textContent='Rec OFF'; tb.classList.add('off');}

      // Disable enroll if already enrolling
      $('btnEnroll').disabled = (d.state === 'enrolling');

    }catch(e){}
  }, 1000);

  async function doEnroll(){
    const name = $('nameInput').value.trim() || '';
    toast('Look at the camera — hold still');
    await fetch('/enroll?name='+encodeURIComponent(name));
    $('nameInput').value='';
  }

  async function doToggle(){
    const r = await fetch('/toggle');
    const d = await r.json();
    toast(d.recognition ? 'Recognition ON' : 'Recognition OFF');
  }

  async function doDelete(){
    if(!confirm('Delete ALL enrolled faces?')) return;
    await fetch('/delete');
    toast('All faces deleted');
  }
</script>
</body>
</html>
)rawhtml";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// =================================================================
//  Start HTTP servers (stream on :81, control on :80)
// =================================================================
static void start_servers() {
  // ── Control server on port 80 ──────────────────
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port    = 80;
  config.ctrl_port      = 32768;
  config.max_uri_handlers = 8;

  if (httpd_start(&control_httpd, &config) == ESP_OK) {
    httpd_uri_t uri_index  = { .uri = "/",       .method = HTTP_GET, .handler = index_handler,  .user_ctx = NULL };
    httpd_uri_t uri_enroll = { .uri = "/enroll",  .method = HTTP_GET, .handler = enroll_handler, .user_ctx = NULL };
    httpd_uri_t uri_delete = { .uri = "/delete",  .method = HTTP_GET, .handler = delete_handler, .user_ctx = NULL };
    httpd_uri_t uri_toggle = { .uri = "/toggle",  .method = HTTP_GET, .handler = toggle_handler, .user_ctx = NULL };
    httpd_uri_t uri_status = { .uri = "/status",  .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL };
    httpd_register_uri_handler(control_httpd, &uri_index);
    httpd_register_uri_handler(control_httpd, &uri_enroll);
    httpd_register_uri_handler(control_httpd, &uri_delete);
    httpd_register_uri_handler(control_httpd, &uri_toggle);
    httpd_register_uri_handler(control_httpd, &uri_status);
    Serial.println("[HTTP] Control server on port 80");
  }

  // ── Stream server on port 81 ───────────────────
  config.server_port = 81;
  config.ctrl_port   = 32769;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_uri_t uri_stream = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    httpd_register_uri_handler(stream_httpd, &uri_stream);
    Serial.println("[HTTP] Stream server on port 81");
  }
}

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n======================================");
  Serial.println(" ESP32-CAM  Smart Lock  Face Recognition");
  Serial.println(" Board pkg 1.0.6  |  AI-Thinker module");
  Serial.println("======================================\n");

  // Flash LED off
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Serial to Nano (UART2 on custom pins)
  NanoSerial.begin(NANO_BAUD, SERIAL_8N1, NANO_RX_PIN, NANO_TX_PIN);
  Serial.printf("[NANO] UART2  TX=GPIO%d  RX=GPIO%d  @ %d baud\n", NANO_TX_PIN, NANO_RX_PIN, NANO_BAUD);

  // Camera
  if (!init_camera()) {
    Serial.println("[FATAL] Camera init failed — halting");
    while (true) delay(1000);
  }

  // Face detection engine
  init_mtmn_config();

  // Face recognition — load saved faces from flash
  face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  read_face_id_from_flash_with_name(&st_face_list);
  Serial.printf("[FACE] Loaded %d enrolled face(s) from flash\n", st_face_list.count);

  // WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(300);
  Serial.printf("[WiFi] AP SSID : %s\n", AP_SSID);
  Serial.printf("[WiFi] AP Pass : %s\n", AP_PASS);
  Serial.printf("[WiFi] AP IP   : %s\n", WiFi.softAPIP().toString().c_str());

  // HTTP servers
  start_servers();

  Serial.println("\n[READY] Open http://192.168.4.1 in your browser");
  Serial.println("[READY] Stream at  http://192.168.4.1:81/stream\n");
}

// =================================================================
//  LOOP  (nothing heavy here — work happens in HTTP stream handler)
// =================================================================
void loop() {
  delay(10);
}
