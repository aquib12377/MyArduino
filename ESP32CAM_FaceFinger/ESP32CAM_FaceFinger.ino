/*
 * ================================================================
 *  ESP32-CAM : Face Recognition + R307 Fingerprint Sensor
 * ================================================================
 *  Compatible with:  Arduino-ESP32 board package v2.0.x
 *                    (uses fd_forward.h / fr_forward.h API)
 *
 *  BOARD SETTINGS (Arduino IDE)
 *  ----------------------------
 *  Board           : AI Thinker ESP32-CAM
 *  Partition Scheme: Huge APP (3 MB No OTA / 1 MB SPIFFS)
 *  PSRAM           : Enabled
 *  Upload Speed    : 115200
 *
 *  *** IMPORTANT — "fr" PARTITION ***
 *  Face data is stored in a custom flash partition named "fr".
 *  The default partition table does NOT include it.
 *  If you see "fr_flash: Not found" in Serial, face enrollment
 *  will be stored in RAM only (lost on reboot).
 *  To add it: Tools → Partition Scheme → choose a custom CSV,
 *  or just use RAM-only mode (works fine, re-enroll after reboot).
 *
 *  HARDWARE CONNECTIONS
 *  --------------------
 *  ESP32-CAM  GPIO 12  -->  Nano D6   (unlock signal)
 *  ESP32-CAM  GND      -->  Nano GND  (common ground!)
 *
 *  R307 / AS608 Fingerprint Sensor
 *  --------------------------------
 *  Sensor TX  -->  ESP32-CAM GPIO 14  (RX2)
 *  Sensor RX  -->  ESP32-CAM GPIO 15  (TX2)
 *  Sensor VCC -->  3.3 V  (or 5 V depending on variant)
 *  Sensor GND -->  GND
 *
 *  NOTE: GPIO 14 & 15 conflict with SD card — do NOT use SD.
 * ================================================================
 */

// ===================== INCLUDES =====================
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_partition.h"
#include "img_converters.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Face detection & recognition (Arduino-ESP32 core 2.0.x)
#include "fd_forward.h"
#include "fr_forward.h"
#include "fr_flash.h"

// Fingerprint
#include <Adafruit_Fingerprint.h>

// WiFi
#include <WiFi.h>

// ===================== USER CONFIG =====================
const char* WIFI_SSID     = "AAA";       // <-- CHANGE THIS
const char* WIFI_PASSWORD = "Acube@123";    // <-- CHANGE THIS

// ===================== FACE CONFIG =====================
#define FACE_ID_SAVE_NUMBER   10   // max enrolled faces
#define ENROLL_CONFIRM_TIMES   3   // scans needed to enroll one face

// ===================== AI-THINKER CAMERA PINS =====================
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

// ===================== SIGNAL & SENSOR PINS =====================
#define SIGNAL_PIN        12   // GPIO 12 -> Nano D6
#define FP_RX_PIN         14   // Sensor TX -> ESP32 GPIO 14
#define FP_TX_PIN         15   // Sensor RX -> ESP32 GPIO 15
#define LED_PIN            4   // On-board flash LED

// ===================== GLOBALS =====================
HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);

// Face detection config (MTMN model)
static mtmn_config_t mtmn_config = {0};

// Face recognition list (stored in RAM; optionally backed by flash)
static face_id_name_list st_face_list = {0};

// Flags
static bool faceRecogReady  = false;   // true when face list is usable
static bool frFlashAvailable = false;  // true when "fr" partition exists

// Enrolment state
static bool enrollMode      = false;
static int  enrollRemaining = 0;

// Web server
httpd_handle_t camera_httpd = NULL;

// ===================== SIGNAL HELPERS =====================
void sendUnlockPulse() {
  Serial.println("[SIGNAL] Sending unlock pulse to Nano");
  digitalWrite(SIGNAL_PIN, HIGH);
  delay(300);
  digitalWrite(SIGNAL_PIN, LOW);
}

// ===================== FACE DETECTION CONFIG =====================
void initFaceDetection() {
  mtmn_config.type               = FAST;
  mtmn_config.min_face           = 80;
  mtmn_config.pyramid            = 0.707;
  mtmn_config.pyramid_times      = 4;

  mtmn_config.p_threshold.score            = 0.6;
  mtmn_config.p_threshold.nms              = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;

  mtmn_config.r_threshold.score            = 0.7;
  mtmn_config.r_threshold.nms              = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;

  mtmn_config.o_threshold.score            = 0.7;
  mtmn_config.o_threshold.nms              = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;

  Serial.println("[FACE] MTMN face detection configured");
}

void initFaceRecognition() {
  // Check if "fr" partition exists BEFORE calling flash functions
  const esp_partition_t *pt = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "fr"
  );

  if (pt) {
    frFlashAvailable = true;
    Serial.printf("[FACE] Found 'fr' partition (%d KB)\n", pt->size / 1024);
  } else {
    frFlashAvailable = false;
    Serial.println("[FACE] WARNING: No 'fr' partition found");
    Serial.println("[FACE] Enrolled faces will be stored in RAM only (lost on reboot)");
  }

  // Initialise the in-memory face list (always safe)
  face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);

  // Only try to load from flash if partition exists
  if (frFlashAvailable) {
    int loaded = read_face_id_from_flash_with_name(&st_face_list);
    if (loaded > 0) {
      Serial.printf("[FACE] Loaded %d enrolled face(s) from flash\n", loaded);
    } else {
      Serial.println("[FACE] No faces in flash yet (enroll via web UI)");
    }
  }

  faceRecogReady = true;
  Serial.println("[FACE] Face recognition ready");
}

// ===================== CAMERA INIT =====================
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA;   // 320x240

  // *** USE ONLY 1 FRAME BUFFER ***
  // Face detection needs ~230 KB for RGB888 conversion + MTMN internals.
  // Two frame buffers eat too much PSRAM and cause allocation failures.
  config.fb_count     = 1;
  config.jpeg_quality = 12;

  if (psramFound()) {
    Serial.println("[CAM] PSRAM found — using 1 frame buffer (saving RAM for face detect)");
  } else {
    Serial.println("[CAM] WARNING: No PSRAM! Face detection may fail.");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);      // flip if needed
  // s->set_hmirror(s, 1);  // mirror if needed

  Serial.println("[CAM] Camera initialised");
  return true;
}

// ===================== FACE DETECT + RECOGNISE =====================
// Returns:  1 = recognised face
//           0 = face detected but unknown (or enrolling)
//          -1 = no face / error / not ready
int detectAndRecogniseFace() {
  if (!faceRecogReady) return -1;

  // Skip recognition if no faces enrolled (detect-only wastes CPU)
  bool canRecognise = (st_face_list.count > 0);

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[FACE] Failed to capture frame");
    return -1;
  }

  // Allocate RGB888 image matrix (320x240x3 = 230,400 bytes)
  dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  if (!image_matrix) {
    esp_camera_fb_return(fb);
    Serial.println("[FACE] ERR: image_matrix alloc failed (out of PSRAM?)");
    return -1;
  }

  // Convert JPEG to RGB888
  bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);
  esp_camera_fb_return(fb);   // release frame buffer ASAP
  fb = NULL;

  if (!converted) {
    dl_matrix3du_free(image_matrix);
    Serial.println("[FACE] ERR: JPEG to RGB888 conversion failed");
    return -1;
  }

  // Run MTMN face detection
  box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);

  int result = -1;  // default: no face

  if (net_boxes && net_boxes->len > 0) {
    result = 0;  // face found, unknown by default
    Serial.printf("[FACE] Detected %d face(s)\n", net_boxes->len);

    // Only proceed with alignment/recognition if we have faces enrolled
    // OR if we're in enrolment mode
    if (canRecognise || enrollMode) {

      // Allocate aligned-face buffer (56x56x3 = ~9 KB)
      dl_matrix3du_t *aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
      if (!aligned_face) {
        Serial.println("[FACE] ERR: aligned_face alloc failed");
        // still free net_boxes below
      } else {

        // ---- ENROLMENT MODE ----
        if (enrollMode && enrollRemaining > 0) {
          if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK) {
            dl_matrix3d_t *face_id = get_face_id(aligned_face);
            if (face_id) {
              int eid = enroll_face_id_to_flash_with_name(
                &st_face_list, face_id, "user"
              );
              if (eid >= 0) {
                enrollRemaining--;
                Serial.printf("[FACE] Enroll scan OK — %d remaining\n", enrollRemaining);
                if (enrollRemaining == 0) {
                  enrollMode = false;
                  Serial.printf("[FACE] Enrollment complete! Total: %d\n",
                                st_face_list.count);
                }
              } else {
                Serial.println("[FACE] Enroll scan failed, try again");
              }
              dl_matrix3d_free(face_id);
            } else {
              Serial.println("[FACE] ERR: get_face_id returned NULL");
            }
          }
          result = 0;  // don't unlock during enrolment
        }
        // ---- RECOGNITION MODE ----
        else if (canRecognise) {
          if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK) {
            dl_matrix3d_t *face_id = get_face_id(aligned_face);
            if (face_id) {
              face_id_node *matched = recognize_face_with_name(&st_face_list, face_id);
              if (matched) {
                Serial.printf("[FACE] Recognised: %s (ID %s)\n",
                              matched->id_name, matched->id_name);
                result = 1;  // AUTHORISED
              } else {
                Serial.println("[FACE] Unknown face");
              }
              dl_matrix3d_free(face_id);
            } else {
              Serial.println("[FACE] ERR: get_face_id returned NULL");
            }
          }
        }

        dl_matrix3du_free(aligned_face);
      }
    }

    // Free detection results
    if (net_boxes->score)    dl_lib_free(net_boxes->score);
    if (net_boxes->box)      dl_lib_free(net_boxes->box);
    if (net_boxes->landmark) dl_lib_free(net_boxes->landmark);
    dl_lib_free(net_boxes);
  }

  dl_matrix3du_free(image_matrix);
  return result;
}

// ===================== WEB UI =====================
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Door Lock</title>
<style>
  body{font-family:Arial,sans-serif;text-align:center;background:#111;color:#eee;margin:0;padding:20px}
  h1{color:#0f0;margin-bottom:4px}
  p.sub{color:#888;margin-top:0}
  img{border:2px solid #333;border-radius:8px;margin:10px auto;max-width:320px;display:block}
  .btn-row{margin:10px 0}
  button{padding:12px 24px;margin:6px;font-size:15px;border:none;border-radius:6px;cursor:pointer;font-weight:bold}
  .enroll{background:#ff0;color:#000}
  .delete{background:#f44;color:#fff}
  .status{background:#0af;color:#fff}
  #msg{margin:12px;padding:12px;font-size:16px;border-radius:6px;min-height:20px}
  .ok{background:#1a3a1a;color:#4f4}
  .err{background:#3a1a1a;color:#f66}
  .info{background:#1a2a3a;color:#6bf}
</style></head><body>
<h1>ESP32-CAM Door Lock</h1>
<p class="sub">Face recognition + fingerprint</p>
<img id="stream" src="/stream">
<div class="btn-row">
  <button class="enroll" onclick="enrollFace()">Enroll face</button>
  <button class="delete" onclick="deleteFaces()">Delete all faces</button>
  <button class="status" onclick="getStatus()">Status</button>
</div>
<div id="msg"></div>

<script>
function showMsg(text, cls) {
  var m = document.getElementById('msg');
  m.textContent = text;
  m.className = cls || 'info';
}
function enrollFace() {
  showMsg('Look at the camera... enrolling 3 scans', 'info');
  fetch('/enroll').then(r=>r.text()).then(t => {
    showMsg(t, t.includes('started') ? 'ok' : 'err');
  }).catch(e => showMsg('Error: ' + e, 'err'));
}
function deleteFaces() {
  if (!confirm('Delete ALL enrolled faces?')) return;
  fetch('/delete_all').then(r=>r.text()).then(t => {
    showMsg(t, 'ok');
  }).catch(e => showMsg('Error: ' + e, 'err'));
}
function getStatus() {
  fetch('/status').then(r=>r.text()).then(t => {
    showMsg(t, 'info');
  }).catch(e => showMsg('Error: ' + e, 'err'));
}
</script>
</body></html>
)rawliteral";

// ---------- Stream handler (MJPEG) ----------
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res    = ESP_OK;
  char part_buf[64];

  static const char* STREAM_CT    = "multipart/x-mixed-replace;boundary=frame";
  static const char* STREAM_BOUND = "\r\n--frame\r\n";
  static const char* STREAM_PART  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  httpd_resp_set_type(req, STREAM_CT);

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    size_t hlen = snprintf(part_buf, 64, STREAM_PART, fb->len);
    res  = httpd_resp_send_chunk(req, STREAM_BOUND, strlen(STREAM_BOUND));
    res |= httpd_resp_send_chunk(req, part_buf, hlen);
    res |= httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (res != ESP_OK) break;
    delay(30);
  }
  return res;
}

// ---------- Index page ----------
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
}

// ---------- Enroll handler ----------
static esp_err_t enroll_handler(httpd_req_t *req) {
  if (st_face_list.count >= FACE_ID_SAVE_NUMBER) {
    return httpd_resp_sendstr(req,
      "Face database full! Delete some faces first.");
  }
  enrollMode      = true;
  enrollRemaining = ENROLL_CONFIRM_TIMES;
  Serial.printf("[WEB] Enrolment started (%d scans needed)\n",
                ENROLL_CONFIRM_TIMES);

  char msg[128];
  snprintf(msg, sizeof(msg),
    "Enrolment started — look at camera for %d scans. Hold still! %s",
    ENROLL_CONFIRM_TIMES,
    frFlashAvailable ? "(saves to flash)" : "(RAM only — lost on reboot)"
  );
  return httpd_resp_sendstr(req, msg);
}

// ---------- Delete all faces ----------
static esp_err_t delete_all_handler(httpd_req_t *req) {
  if (frFlashAvailable) {
    delete_face_all_in_flash_with_name(&st_face_list);
  } else {
    // RAM-only: re-init the list to clear it
    face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  }
  Serial.println("[WEB] All enrolled faces deleted");
  return httpd_resp_sendstr(req, "All faces deleted.");
}

// ---------- Status handler ----------
static esp_err_t status_handler(httpd_req_t *req) {
  char buf[300];
  snprintf(buf, sizeof(buf),
    "Enrolled faces: %d / %d\n"
    "Flash partition: %s\n"
    "Fingerprint sensor: %s\n"
    "Enrolled prints: %d\n"
    "Enroll mode: %s\n"
    "Free PSRAM: %d bytes",
    st_face_list.count, FACE_ID_SAVE_NUMBER,
    frFlashAvailable ? "available" : "NOT found (RAM-only mode)",
    finger.verifyPassword() ? "Connected" : "NOT found",
    finger.templateCount,
    enrollMode ? "ACTIVE" : "off",
    ESP.getFreePsram()
  );
  return httpd_resp_sendstr(req, buf);
}

// ---------- Start server ----------
void startWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 8;

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_uri_t uri_index  = { "/",           HTTP_GET, index_handler,      NULL };
    httpd_uri_t uri_stream = { "/stream",     HTTP_GET, stream_handler,     NULL };
    httpd_uri_t uri_enroll = { "/enroll",     HTTP_GET, enroll_handler,     NULL };
    httpd_uri_t uri_delete = { "/delete_all", HTTP_GET, delete_all_handler, NULL };
    httpd_uri_t uri_status = { "/status",     HTTP_GET, status_handler,     NULL };

    httpd_register_uri_handler(camera_httpd, &uri_index);
    httpd_register_uri_handler(camera_httpd, &uri_stream);
    httpd_register_uri_handler(camera_httpd, &uri_enroll);
    httpd_register_uri_handler(camera_httpd, &uri_delete);
    httpd_register_uri_handler(camera_httpd, &uri_status);

    Serial.println("[WEB] Server started");
  }
}

// ===================== FINGERPRINT =====================
bool initFingerprint() {
  fpSerial.begin(57600, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("[FP] Fingerprint sensor found");
    finger.getParameters();
    Serial.printf("[FP] Capacity: %d  |  Enrolled: %d\n",
                  finger.capacity, finger.templateCount);
    return true;
  } else {
    Serial.println("[FP] Fingerprint sensor NOT found — check wiring");
    return false;
  }
}

// Returns matched fingerprint ID, or -1 if no match / no finger
int checkFingerprint() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;   // no finger on sensor

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.printf("[FP] Match! ID=%d  confidence=%d\n",
                  finger.fingerID, finger.confidence);
    return finger.fingerID;
  }

  if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("[FP] No match");
  }
  return -1;
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-CAM Face + Fingerprint ===");

  // Disable brownout detector (common ESP32-CAM fix)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Signal pin to Nano
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);

  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ---------- Camera ----------
  if (!initCamera()) {
    Serial.println("[FATAL] Camera init failed — halting");
    while (true) delay(1000);
  }

  // Print free PSRAM after camera init
  Serial.printf("[MEM] Free PSRAM after camera: %d bytes\n", ESP.getFreePsram());

  // ---------- Face detection & recognition ----------
  initFaceDetection();
  initFaceRecognition();

  Serial.printf("[MEM] Free PSRAM after face init: %d bytes\n", ESP.getFreePsram());

  // ---------- Fingerprint sensor ----------
  initFingerprint();

  // ---------- WiFi ----------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected!  IP: %s\n",
                  WiFi.localIP().toString().c_str());
    startWebServer();
  } else {
    Serial.println("\n[WIFI] Connection failed — web UI unavailable");
    Serial.println("[WIFI] Face + fingerprint detection still active");
  }

  Serial.println("\n[READY] System running");
  Serial.printf("[READY] Faces enrolled: %d   |   Flash: %s\n",
                st_face_list.count,
                frFlashAvailable ? "yes" : "RAM-only");
  Serial.println("[READY] Checking face & fingerprint continuously...");
}

// ===================== LOOP =====================
unsigned long lastFaceCheck  = 0;
const unsigned long FACE_INTERVAL_MS = 500;   // check face every 500 ms

unsigned long unlockCooldown = 0;
const unsigned long COOLDOWN_MS = 8000;       // 8 s cooldown after unlock

void loop() {
  unsigned long now = millis();

  // Don't re-trigger while Nano is in the middle of unlocking
  if (now < unlockCooldown) {
    delay(100);
    return;
  }

  // ---------- FINGERPRINT CHECK (fast, runs every loop) ----------
  int fpID = checkFingerprint();
  if (fpID >= 0) {
    Serial.printf("[AUTH] Fingerprint ID %d -> UNLOCK\n", fpID);
    digitalWrite(LED_PIN, HIGH);
    sendUnlockPulse();
    digitalWrite(LED_PIN, LOW);
    unlockCooldown = millis() + COOLDOWN_MS;
    return;
  }

  // ---------- FACE CHECK (throttled) ----------
  if (now - lastFaceCheck >= FACE_INTERVAL_MS) {
    lastFaceCheck = now;

    int faceResult = detectAndRecogniseFace();

    if (faceResult == 1) {
      Serial.println("[AUTH] Face recognised -> UNLOCK");
      digitalWrite(LED_PIN, HIGH);
      sendUnlockPulse();
      digitalWrite(LED_PIN, LOW);
      unlockCooldown = millis() + COOLDOWN_MS;
    }
    // faceResult == 0  -> unknown face or enrolment in progress
    // faceResult == -1 -> no face detected / error
  }

  delay(50);  // small yield
}
