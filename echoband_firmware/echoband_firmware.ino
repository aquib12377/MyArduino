/*
 * ============================================================================
 *  ECHOBAND FIRMWARE v1.0
 *  ESP32 30-Pin (WROOM-32 DevKit V1)
 *  
 *  Distress detection wearable — hardware data capture via REST API
 *  
 *  Sensors: INMP441 (I2S mic), MAX30102 (heart rate), Neo-6M (GPS),
 *           MPU6050 (accel/gyro), DS3231 (RTC), SSD1306 (OLED)
 *  
 *  Triggers: Wake word (3x via STT feedback) + High HR,
 *            Button press, Excess movement + High HR
 *  
 *  Author: LEOBOTICS / LogicLoop Ventures
 *  Date:   2026-05-09
 * ============================================================================
 */

// ========================== CONFIGURATION ==================================

// --- WiFi & Network ---
#define WIFI_SSID         "AAA"
#define WIFI_PASSWORD     "12345678"

// Static IP Configuration (change to match your router's subnet)
#define STATIC_IP         192, 168, 1, 100
#define GATEWAY_IP        192, 168, 1, 1
#define SUBNET_MASK       255, 255, 255, 0
#define DNS_SERVER        8, 8, 8, 8

#define WIFI_TIMEOUT_MS       15000
#define WIFI_RECONNECT_MS     5000
#define HTTP_PORT             80

// --- Heart Rate Sensor Selection ---
// Uncomment ONE of these:
#define USE_MAX30102        // I2C heart rate sensor (recommended)
// #define USE_PULSE_ANALOG    // Analog pulse sensor on ADC pin

// --- Pin Definitions (ESP32 30-Pin DevKit V1) ---
// I2S - INMP441 Microphone
#define I2S_SCK_PIN       26
#define I2S_WS_PIN        25
#define I2S_SD_PIN        33

// GPS - Neo-6M (Hardware Serial2)
#define GPS_RX_PIN        16  // ESP32 RX ← GPS TX
#define GPS_TX_PIN        17  // ESP32 TX → GPS RX
#define GPS_BAUD          9600

// I2C Bus (shared: MAX30102, MPU6050, DS3231, SSD1306)
#define I2C_SDA_PIN       21
#define I2C_SCL_PIN       22

// Analog Pulse Sensor (only if USE_PULSE_ANALOG defined)
#define PULSE_ANALOG_PIN  35  // ADC1, input-only — OK

// Button & Indicators
#define BUTTON_PIN        4   // Active LOW, internal pullup
#define LED_PIN           2   // Built-in blue LED
#define BUZZER_PIN        13  // Optional buzzer (PWM)

// --- I2C Addresses ---
#define MPU6050_ADDR      0x69  // AD0 pin pulled HIGH (DS3231 uses 0x68)
#define MAX30102_ADDR     0x57
#define DS3231_ADDR       0x68
#define SSD1306_ADDR      0x3C

// --- Thresholds ---
#define HR_PANIC_BPM          120   // Heart rate panic threshold
#define HR_LOW_BPM            40    // Unrealistic low HR (sensor error)
#define HR_HIGH_BPM           220   // Unrealistic high HR (sensor error)
#define ACCEL_EXCESS_G        2.5f  // Acceleration magnitude threshold (g)
#define ACCEL_SUSTAIN_MS      2000  // How long excess accel must persist
#define WAKE_WORD_THRESHOLD   3     // "help" detections needed
#define WAKE_WORD_WINDOW_MS   60000 // Reset wake word count after this
#define BUTTON_DEBOUNCE_MS    300

// --- Audio Configuration ---
#define I2S_SAMPLE_RATE       16000
#define I2S_BITS              16
#define AUDIO_CHUNK_SECS      10
#define AUDIO_CHUNK_BYTES     (I2S_SAMPLE_RATE * (I2S_BITS / 8) * AUDIO_CHUNK_SECS)
// 16000 * 2 * 10 = 320,000 bytes per chunk — stored on SPIFFS
#define MAX_AUDIO_FILES       3     // Keep last N chunks on SPIFFS
#define AUDIO_DMA_BUF_COUNT   8
#define AUDIO_DMA_BUF_LEN     1024

// --- Sensor Read Intervals ---
#define HR_READ_INTERVAL_MS       100
#define GPS_READ_INTERVAL_MS      1000
#define MPU_READ_INTERVAL_MS      200
#define OLED_UPDATE_INTERVAL_MS   500
#define SENSOR_LOG_INTERVAL_MS    3000

// --- OLED ---
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// ========================== INCLUDES =======================================

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#ifdef USE_MAX30102
  #include <MAX30105.h>
  #include <heartRate.h>
#endif

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <RTClib.h>

// Brownout detector control (needed to survive WiFi power spike)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_wifi.h>

// ========================== GLOBALS ========================================

// --- Network ---
WebServer server(HTTP_PORT);
IPAddress staticIP(STATIC_IP);
IPAddress gatewayIP(GATEWAY_IP);
IPAddress subnetMask(SUBNET_MASK);
IPAddress dnsServer(DNS_SERVER);

// --- Sensors ---
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Serial2

#ifdef USE_MAX30102
  MAX30105 hrSensor;
#endif

Adafruit_MPU6050 mpu;
RTC_DS3231 rtc;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define AUDIO_STREAM_BUF  64000   // 2s @ 16kHz 16-bit mono — fits in heap
static uint8_t  audioBuf[AUDIO_STREAM_BUF];
static size_t   audioBufLen = 0;
static bool     audioReady  = false;
static int      audioChunkIdx = 0;
// --- Sensor State ---
struct SensorData {
  // Heart Rate
  float bpm;
  bool  hrValid;
  unsigned long hrLastBeat;
  
  // GPS
  double latitude;
  double longitude;
  float  altitude;
  int    satellites;
  bool   gpsValid;
  
  // MPU6050
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  float accelMagnitude;
  bool  mpuValid;
  
  // RTC
  char  timestamp[24];  // "YYYY-MM-DD HH:MM:SS"
  bool  rtcValid;
  
  // Audio
  int   audioChunkIndex;
  bool  audioRecording;
  bool  audioChunkReady;
  char  latestAudioFile[32];
};

SensorData sensorData = {};

// --- Heart Rate BPM averaging ---
// ── Replace your HR globals block with this ──────────────────────────────────
#define RATE_SIZE 4
byte    hrRates[RATE_SIZE];
byte    hrRateSpot = 0;
long    hrLastBeat = 0;
float   hrBeatsPerMinute = 0;
int     hrBeatAvg = 0;
long    hrLastIRValue = 0;
// ─────────────────────────────────────────────────────────────────────────────

// --- Device State Machine ---
enum DeviceState {
  STATE_NORMAL,
  STATE_ALERT,       // Partial trigger detected
  STATE_EMERGENCY,   // Full emergency active
  STATE_STANDBY      // Post-emergency, awaiting reset
};

DeviceState deviceState = STATE_NORMAL;
const char* stateNames[] = {"NORMAL", "ALERT", "EMERGENCY", "STANDBY"};

// --- Trigger Flags ---
struct TriggerFlags {
  bool   highHeartRate;
  bool   excessMovement;
  int    wakeWordCount;
  bool   buttonPressed;
  unsigned long wakeWordFirstTime;
  unsigned long excessMoveStart;
  unsigned long lastButtonPress;
};

TriggerFlags triggers = {};

// --- Hardware Status ---
struct HWStatus {
  bool hrOK;
  bool gpsOK;
  bool mpuOK;
  bool rtcOK;
  bool oledOK;
  bool i2sOK;
  bool spiffsOK;
  bool wifiOK;
};

HWStatus hwStatus = {};

// --- Timing ---
unsigned long lastHRRead = 0;
unsigned long lastGPSRead = 0;
unsigned long lastMPURead = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastSensorLog = 0;
unsigned long lastWiFiCheck = 0;
unsigned long bootTime = 0;
unsigned long emergencyStartTime = 0;

// --- Audio Task ---
TaskHandle_t audioTaskHandle = NULL;
volatile bool audioTaskRunning = false;
SemaphoreHandle_t audioMutex = NULL;
volatile int currentAudioChunk = 0;

// --- Speech-to-Text feedback from software ---
char lastSTTText[256] = "";

// ========================== LOGGING HELPER =================================

#define LOG_BOOT(msg)     Serial.printf("[BOOT]     %s\n", msg)
#define LOG_INIT(msg)     Serial.printf("[INIT]     %s\n", msg)
#define LOG_INFO(msg)     Serial.printf("[INFO]     %s\n", msg)
#define LOG_WARN(msg)     Serial.printf("[WARN]     %s\n", msg)
#define LOG_ERROR(msg)    Serial.printf("[ERROR]    %s\n", msg)
#define LOG_STATE(msg)    Serial.printf("[STATE]    %s\n", msg)
#define LOG_RECOVERY(msg) Serial.printf("[RECOVERY] %s\n", msg)

// ========================== FORWARD DECLARATIONS ===========================

void initWiFi();
void initSensors();
void initI2S();
void initSPIFFS();
void initWebServer();
void readHeartRate();
void readGPS();
void readMPU();
void readRTC();
void updateOLED();
void checkTriggers();
void transitionState(DeviceState newState);
void audioRecordTask(void* param);
void handleEmergency();
String buildSensorJSON();
void serveDashboard();
void cleanOldAudioFiles();

// ========================== SETUP ==========================================

void setup() {
  // *** BROWNOUT FIX: Disable detector during init to survive WiFi power spike ***
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  delay(500);
  
  bootTime = millis();
  
  // --- Boot Banner ---
  Serial.println();
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║       ECHOBAND FIRMWARE v1.1             ║");
  Serial.println("║       ESP32 30-Pin | LEOBOTICS           ║");
  Serial.println("╚══════════════════════════════════════════╝");
  
  // Log reset reason
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("[BOOT]     Reset reason: %d\n", (int)reason);
  if (reason == ESP_RST_BROWNOUT) {
    LOG_WARN("Previous reset was BROWNOUT — check power supply!");
    LOG_WARN("Brownout detector DISABLED for this boot cycle");
  }
  
  // --- Pin Setup ---
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // --- I2C ---
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);  // 400kHz Fast I2C
  LOG_INIT("I2C bus started (SDA=21, SCL=22, 400kHz)");
  
  // Scan I2C for debugging
  scanI2C();
  
  // --- Initialize with staggered delays to reduce peak current ---
  //initSPIFFS();
  delay(100);  // Let power stabilize
  
  initSensors();
  delay(200);  // Sensors settled — pause before I2S
  
  initI2S();
  delay(300);  // I2S settled — longer pause before WiFi (biggest power hog)
  
  initWiFi();
  
  // *** BROWNOUT FIX: Re-enable detector now that WiFi is stable ***
  delay(500);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);
  LOG_INIT("Brownout detector re-enabled");
  
  // Create audio mutex
  audioMutex = xSemaphoreCreateMutex();
  
  // --- Start Web Server ---
  initWebServer();
  
  // --- Summary ---
  Serial.println("[BOOT]     ── Hardware Status ──");
  Serial.printf("[BOOT]     Heart Rate : %s\n", hwStatus.hrOK    ? "OK" : "FAIL");
  Serial.printf("[BOOT]     GPS        : %s\n", hwStatus.gpsOK   ? "OK" : "FAIL (acquiring)");
  Serial.printf("[BOOT]     MPU6050    : %s\n", hwStatus.mpuOK   ? "OK" : "FAIL");
  Serial.printf("[BOOT]     RTC        : %s\n", hwStatus.rtcOK   ? "OK" : "FAIL");
  Serial.printf("[BOOT]     OLED       : %s\n", hwStatus.oledOK  ? "OK" : "FAIL");
  Serial.printf("[BOOT]     I2S Mic    : %s\n", hwStatus.i2sOK   ? "OK" : "FAIL");
  Serial.printf("[BOOT]     SPIFFS     : %s\n", hwStatus.spiffsOK? "OK" : "FAIL");
  Serial.printf("[BOOT]     WiFi       : %s\n", hwStatus.wifiOK  ? "OK" : "FAIL");
  
  if (hwStatus.wifiOK) {
    Serial.printf("[BOOT]     Dashboard  : http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.printf("[BOOT]     API Base   : http://%s/api/\n", WiFi.localIP().toString().c_str());
  }
  
  Serial.println("[BOOT]     ── EchoBand Ready ──");
  
  // Blink LED to signal boot complete
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW);  delay(100);
  }
}

// ========================== MAIN LOOP ======================================

void loop() {
  unsigned long now = millis();
  
  // --- Handle HTTP ---
  server.handleClient();
  
  // --- Read Sensors (non-blocking, staggered) ---
  if (now - lastHRRead >= HR_READ_INTERVAL_MS) {
    lastHRRead = now;
    readHeartRate();
  }
  
  if (now - lastGPSRead >= GPS_READ_INTERVAL_MS) {
    lastGPSRead = now;
    readGPS();
  }
  
  if (now - lastMPURead >= MPU_READ_INTERVAL_MS) {
    lastMPURead = now;
    readMPU();
  }
  
  // Read RTC with GPS interval (it's slow-changing)
  if (now - lastGPSRead < 50) {
    readRTC();
  }
  
  // --- Check Button ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (now - triggers.lastButtonPress > BUTTON_DEBOUNCE_MS) {
      triggers.lastButtonPress = now;
      triggers.buttonPressed = true;
      LOG_INFO("Button pressed!");
    }
  }
  
  // --- Evaluate Triggers ---
  checkTriggers();
  
  // --- Handle Emergency State ---
  if (deviceState == STATE_EMERGENCY) {
    handleEmergency();
  }
  
  // --- Update OLED ---
  if (hwStatus.oledOK && (now - lastOLEDUpdate >= OLED_UPDATE_INTERVAL_MS)) {
    lastOLEDUpdate = now;
    updateOLED();
  }
  
  // --- Periodic Sensor Log ---
  if (now - lastSensorLog >= SENSOR_LOG_INTERVAL_MS) {
    lastSensorLog = now;
    if (sensorData.hrValid)
      Serial.printf("[INFO]     HR: %.0f BPM | ", sensorData.bpm);
    else
      Serial.print("[INFO]     HR: -- | ");
    
    if (sensorData.gpsValid)
      Serial.printf("GPS: %.6f,%.6f | ", sensorData.latitude, sensorData.longitude);
    else
      Serial.printf("GPS: no fix (sat:%d) | ", sensorData.satellites);
    
    Serial.printf("Accel: %.2fg | State: %s\n", sensorData.accelMagnitude, stateNames[deviceState]);
  }
  
  // --- WiFi Reconnect ---
  if (now - lastWiFiCheck >= WIFI_RECONNECT_MS) {
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      hwStatus.wifiOK = false;
      LOG_WARN("WiFi disconnected — reconnecting...");
      WiFi.reconnect();
    } else if (!hwStatus.wifiOK) {
      hwStatus.wifiOK = true;
      LOG_RECOVERY("WiFi reconnected");
      Serial.printf("[RECOVERY] IP: %s\n", WiFi.localIP().toString().c_str());
    }
  }
  
  // --- LED Status Indicator ---
  updateStatusLED(now);
  
  yield();
}

// ========================== INITIALIZATION =================================

void scanI2C() {
  LOG_INIT("Scanning I2C bus...");
  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[INIT]     Found I2C device at 0x%02X", addr);
      if (addr == MAX30102_ADDR)  Serial.print(" (MAX30102)");
      if (addr == MPU6050_ADDR)   Serial.print(" (MPU6050 AD0=HIGH)");
      if (addr == 0x68)           Serial.print(" (DS3231 / MPU6050 default)");
      if (addr == SSD1306_ADDR)   Serial.print(" (SSD1306 OLED)");
      Serial.println();
      found++;
    }
  }
  Serial.printf("[INIT]     I2C scan complete: %d devices found\n", found);
}

void initSPIFFS() {
  if (SPIFFS.begin(true)) {
    hwStatus.spiffsOK = true;
    size_t total = SPIFFS.totalBytes();
    size_t used  = SPIFFS.usedBytes();
    Serial.printf("[INIT]     SPIFFS OK — %u/%u bytes used\n", used, total);
  } else {
    hwStatus.spiffsOK = false;
    LOG_ERROR("SPIFFS mount FAILED — audio recording disabled");
  }
}

void initSensors() {
  // --- MAX30102 Heart Rate ---
  // ── Replace initSensors() MAX30102 block ─────────────────────────────────────
#ifdef USE_MAX30102
  // No address arg — let library use default 0x57 scan (matches SparkFun example)
  if (hrSensor.begin(Wire, I2C_SPEED_FAST)) {
    hrSensor.setup();
    hrSensor.setPulseAmplitudeRed(0x0A);
    hrSensor.setPulseAmplitudeGreen(0);
    hwStatus.hrOK = true;
    LOG_INIT("MAX30102 heart rate sensor OK");
  } else {
    hwStatus.hrOK = false;
    LOG_ERROR("MAX30102 not found — HR disabled");
  }
#endif
// ─────────────────────────────────────────────────────────────────────────────
  
  #ifdef USE_PULSE_ANALOG
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    hwStatus.hrOK = true;
    LOG_INIT("Analog pulse sensor on GPIO35 — HR enabled");
  #endif
  
  delay(50);  // Stabilize before next I2C device
  
  // --- MPU6050 ---
  // Try preferred address 0x69 (AD0=HIGH) first, then fallback to 0x68 (AD0=LOW)
  if (mpu.begin(MPU6050_ADDR)) {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    hwStatus.mpuOK = true;
    LOG_INIT("MPU6050 OK (addr 0x69, AD0=HIGH)");
  } else if (mpu.begin(0x68)) {
    // AD0 is LOW or floating — MPU6050 is at default 0x68
    // This works ONLY if DS3231 RTC is NOT connected (address conflict!)
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    hwStatus.mpuOK = true;
    LOG_INIT("MPU6050 OK (addr 0x68, AD0=LOW)");
    LOG_WARN("MPU6050 at 0x68 conflicts with DS3231 RTC!");
    LOG_WARN("Wire AD0 to 3V3 for address 0x69 to avoid conflict");
  } else {
    hwStatus.mpuOK = false;
    LOG_ERROR("MPU6050 not found at 0x69 or 0x68 — accelerometer disabled");
    LOG_WARN("Check: 1) MPU6050 VCC→3V3, GND→GND, SDA→21, SCL→22");
    LOG_WARN("Check: 2) AD0 pin wired to 3V3 for address 0x69");
  }
  
  delay(50);  // Stabilize before next I2C device
  
  // --- DS3231 RTC ---
  if (rtc.begin()) {
    if (rtc.lostPower()) {
      LOG_WARN("RTC lost power — setting to compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    DateTime now = rtc.now();
    Serial.printf("[INIT]     RTC OK — %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());
    hwStatus.rtcOK = true;
  } else {
    hwStatus.rtcOK = false;
    LOG_ERROR("DS3231 RTC not found — using millis() fallback");
  }
  
  // --- SSD1306 OLED ---
  if (display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDR)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("EchoBand v1.0");
    display.println("Booting...");
    display.display();
    hwStatus.oledOK = true;
    LOG_INIT("SSD1306 OLED OK (128x64)");
  } else {
    hwStatus.oledOK = false;
    LOG_ERROR("SSD1306 OLED not found — display disabled");
  }
  
  // --- GPS ---
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  hwStatus.gpsOK = true;  // GPS is always "OK" — it just may not have a fix yet
  LOG_INIT("GPS Serial2 started (RX=16, TX=17, 9600 baud)");
}

void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = AUDIO_DMA_BUF_COUNT,
    .dma_buf_len = AUDIO_DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_SCK_PIN,
    .ws_io_num    = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_SD_PIN
  };
  
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    hwStatus.i2sOK = false;
    Serial.printf("[ERROR]    I2S driver install failed: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    hwStatus.i2sOK = false;
    Serial.printf("[ERROR]    I2S pin config failed: %d\n", err);
    return;
  }
  
  i2s_zero_dma_buffer(I2S_NUM_0);
  hwStatus.i2sOK = true;
  Serial.printf("[INIT]     I2S OK — INMP441 (SCK=%d, WS=%d, SD=%d, %dHz)\n",
                I2S_SCK_PIN, I2S_WS_PIN, I2S_SD_PIN, I2S_SAMPLE_RATE);
}

void initWiFi() {
  LOG_INIT("Connecting to WiFi...");
  Serial.printf("[INIT]     SSID: %s\n", WIFI_SSID);
  Serial.printf("[INIT]     Static IP: %s\n", staticIP.toString().c_str());

  // AP+STA: ESP32 creates its own AP (direct PC access) AND connects to hotspot
  WiFi.mode(WIFI_AP_STA);

  // --- Soft AP (always reachable at 192.168.4.1 regardless of hotspot isolation) ---
  WiFi.softAP("EchoBand", "echoband123");
  Serial.printf("[INIT]     SoftAP started — SSID: EchoBand | IP: %s\n",
                WiFi.softAPIP().toString().c_str());

  // *** BROWNOUT FIX: Reduce TX power ***
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  LOG_INIT("WiFi TX power set to 8.5dBm (brownout safe)");

  // --- STA (connect to hotspot for internet, optional) ---
  WiFi.setAutoReconnect(true);

  if (!WiFi.config(staticIP, gatewayIP, subnetMask, dnsServer)) {
    LOG_WARN("Static IP config failed — will use DHCP");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    hwStatus.wifiOK = true;
    Serial.printf("[INIT]     STA connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[INIT]     RSSI: %d dBm\n", WiFi.RSSI());

    if (WiFi.localIP() != staticIP) {
      LOG_WARN("Static IP not applied — got DHCP address instead");
      Serial.printf("[WARN]     Expected: %s, Got: %s\n",
                    staticIP.toString().c_str(), WiFi.localIP().toString().c_str());
    }
  } else {
    // STA failed — AP is still up, so mark WiFi OK anyway
    hwStatus.wifiOK = true;
    LOG_WARN("STA (hotspot) connection FAILED — running AP-only mode");
    LOG_WARN("Device will keep trying to reconnect in background");
  }

  // AP is always the reliable access point — print it last so it's obvious
  Serial.printf("[INIT]     >>> Connect PC to WiFi 'EchoBand' (pass: echoband123)\n");
  Serial.printf("[INIT]     >>> Dashboard: http://%s/\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[INIT]     >>> API Base:  http://%s/api/\n", WiFi.softAPIP().toString().c_str());
}

// ========================== SENSOR READING =================================

// ── Replace readHeartRate() entirely ─────────────────────────────────────────
void readHeartRate() {
  if (!hwStatus.hrOK) return;

#ifdef USE_MAX30102
  long irValue = hrSensor.getIR();
  hrLastIRValue = irValue;

  if (checkForBeat(irValue)) {
    long delta  = millis() - hrLastBeat;
    hrLastBeat  = millis();
    hrBeatsPerMinute = 60.0 / (delta / 1000.0);

    if (hrBeatsPerMinute < 255 && hrBeatsPerMinute > 20) {
      hrRates[hrRateSpot++] = (byte)hrBeatsPerMinute;
      hrRateSpot %= RATE_SIZE;

      hrBeatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        hrBeatAvg += hrRates[x];
      hrBeatAvg /= RATE_SIZE;
    }
  }

  if (irValue < 50000) {
    sensorData.hrValid = false;
    sensorData.bpm     = 0;
  } else {
    sensorData.hrValid = (hrBeatAvg > 0);
    sensorData.bpm     = hrBeatAvg;
  }

  triggers.highHeartRate = (sensorData.hrValid && sensorData.bpm >= HR_PANIC_BPM);
#endif
}
// ─────────────────────────────────────────────────────────────────────────────

void readGPS() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
  }
  
  if (gps.location.isUpdated() && gps.location.isValid()) {
    sensorData.latitude  = gps.location.lat();
    sensorData.longitude = gps.location.lng();
    sensorData.altitude  = gps.altitude.meters();
    sensorData.satellites = gps.satellites.value();
    sensorData.gpsValid  = true;
  } else {
    sensorData.satellites = gps.satellites.value();
    // Keep last known location if we had a fix before
    if (!sensorData.gpsValid) {
      sensorData.latitude  = 0;
      sensorData.longitude = 0;
    }
  }
}

void readMPU() {
  if (!hwStatus.mpuOK) return;
  
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  sensorData.accelX = a.acceleration.x;
  sensorData.accelY = a.acceleration.y;
  sensorData.accelZ = a.acceleration.z;
  sensorData.gyroX  = g.gyro.x;
  sensorData.gyroY  = g.gyro.y;
  sensorData.gyroZ  = g.gyro.z;
  
  // Magnitude in g (divide by 9.81)
  float mag = sqrt(sensorData.accelX * sensorData.accelX +
                   sensorData.accelY * sensorData.accelY +
                   sensorData.accelZ * sensorData.accelZ) / 9.81;
  sensorData.accelMagnitude = mag;
  sensorData.mpuValid = true;
}

void readRTC() {
  if (!hwStatus.rtcOK) {
    // Fallback: millis-based timestamp
    unsigned long secs = millis() / 1000;
    snprintf(sensorData.timestamp, sizeof(sensorData.timestamp),
             "UPTIME %lu s", secs);
    sensorData.rtcValid = false;
    return;
  }
  
  DateTime now = rtc.now();
  snprintf(sensorData.timestamp, sizeof(sensorData.timestamp),
           "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  sensorData.rtcValid = true;
}

// ========================== TRIGGER LOGIC ===================================

void checkTriggers() {
  unsigned long now = millis();
  
  // --- Heart Rate Check ---
  triggers.highHeartRate = (sensorData.hrValid && sensorData.bpm >= HR_PANIC_BPM);
  
  // --- Accelerometer Check ---
  if (hwStatus.mpuOK && sensorData.accelMagnitude > ACCEL_EXCESS_G) {
    if (triggers.excessMoveStart == 0) {
      triggers.excessMoveStart = now;
    } else if (now - triggers.excessMoveStart >= ACCEL_SUSTAIN_MS) {
      triggers.excessMovement = true;
    }
  } else {
    triggers.excessMoveStart = 0;
    triggers.excessMovement = false;
  }
  
  // --- Wake Word Timeout ---
  if (triggers.wakeWordCount > 0 && triggers.wakeWordFirstTime > 0) {
    if (now - triggers.wakeWordFirstTime > WAKE_WORD_WINDOW_MS) {
      triggers.wakeWordCount = 0;
      triggers.wakeWordFirstTime = 0;
    }
  }
  
  // --- State Transitions ---
  if (deviceState == STATE_NORMAL || deviceState == STATE_ALERT) {
    
    // EMERGENCY Triggers (any one of these combos):
    // 1. Wake word 3x + High HR
    // 2. Button + High HR
    // 3. Excess Movement + High HR
    // 4. Button alone (manual panic — always works)
    
    bool trigger1 = (triggers.wakeWordCount >= WAKE_WORD_THRESHOLD) && triggers.highHeartRate;
    bool trigger2 = triggers.buttonPressed && triggers.highHeartRate;
    bool trigger3 = triggers.excessMovement && triggers.highHeartRate;
    bool trigger4 = triggers.buttonPressed;  // Manual override — no HR needed
    
    if (trigger1 || trigger2 || trigger3) {
      char reason[64];
      if (trigger1) snprintf(reason, sizeof(reason), "Wake word (%d) + High HR (%.0f)", 
                              triggers.wakeWordCount, sensorData.bpm);
      else if (trigger2) snprintf(reason, sizeof(reason), "Button + High HR (%.0f)", sensorData.bpm);
      else snprintf(reason, sizeof(reason), "Excess movement + High HR (%.0f)", sensorData.bpm);
      
      Serial.printf("[STATE]    EMERGENCY triggered: %s\n", reason);
      transitionState(STATE_EMERGENCY);
    }
    else if (trigger4) {
      LOG_STATE("EMERGENCY triggered: Manual button press");
      transitionState(STATE_EMERGENCY);
    }
    else if (triggers.highHeartRate || triggers.excessMovement || triggers.wakeWordCount > 0) {
      if (deviceState != STATE_ALERT) {
        transitionState(STATE_ALERT);
      }
    }
    else {
      if (deviceState != STATE_NORMAL) {
        transitionState(STATE_NORMAL);
      }
    }
    
    // Reset button flag after processing
    triggers.buttonPressed = false;
  }
}

void transitionState(DeviceState newState) {
  if (newState == deviceState) return;
  
  Serial.printf("[STATE]    %s → %s\n", stateNames[deviceState], stateNames[newState]);
  deviceState = newState;
  
  switch (newState) {
    case STATE_EMERGENCY:
      emergencyStartTime = millis();
      startAudioRecording();
      // Buzzer alert
      tone(BUZZER_PIN, 2000, 500);
      break;
      
    case STATE_NORMAL:
      stopAudioRecording();
      noTone(BUZZER_PIN);
      triggers.wakeWordCount = 0;
      triggers.wakeWordFirstTime = 0;
      break;
      
    case STATE_STANDBY:
      stopAudioRecording();
      noTone(BUZZER_PIN);
      break;
      
    case STATE_ALERT:
      // Quick beep
      tone(BUZZER_PIN, 1000, 100);
      break;
  }
}

// ========================== AUDIO RECORDING ================================

void startAudioRecording() {
  if (!hwStatus.i2sOK || !hwStatus.spiffsOK) {
    LOG_WARN("Cannot record — I2S or SPIFFS not available");
    return;
  }
  
  if (audioTaskRunning) {
    LOG_WARN("Audio recording already active");
    return;
  }
  
  audioTaskRunning = true;
  sensorData.audioRecording = true;
  
  xTaskCreatePinnedToCore(
    audioRecordTask,
    "AudioRec",
    8192,          // Stack size
    NULL,
    2,             // Priority (above idle)
    &audioTaskHandle,
    1              // Run on core 1 (core 0 handles WiFi)
  );
  
  LOG_INFO("Audio recording started (FreeRTOS task on Core 1)");
}

void stopAudioRecording() {
  if (audioTaskRunning) {
    audioTaskRunning = false;
    // Wait for task to finish current chunk
    delay(100);
    sensorData.audioRecording = false;
    LOG_INFO("Audio recording stopped");
  }
}

void audioRecordTask(void* param) {
  int16_t dma[512];
  size_t  bytesRead;

  while (audioTaskRunning) {
    audioBufLen   = 0;
    audioReady    = false;
    audioChunkIdx++;

    // Fill buffer from I2S
    while (audioTaskRunning && audioBufLen < AUDIO_STREAM_BUF) {
      size_t space = AUDIO_STREAM_BUF - audioBufLen;
      size_t toRead = min(sizeof(dma), space);
      esp_err_t err = i2s_read(I2S_NUM_0, dma, toRead, &bytesRead, pdMS_TO_TICKS(100));
      if (err == ESP_OK && bytesRead > 0) {
        memcpy(audioBuf + audioBufLen, dma, bytesRead);
        audioBufLen += bytesRead;
      }
      taskYIELD();
    }

    // Mark ready for laptop to fetch
    if (xSemaphoreTake(audioMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      sensorData.audioChunkIndex = audioChunkIdx;
      sensorData.audioChunkReady = true;
      xSemaphoreGive(audioMutex);
    }

    Serial.printf("[AUDIO] Chunk %d ready — %u bytes in RAM\n", audioChunkIdx, audioBufLen);
  }

  audioTaskHandle = NULL;
  vTaskDelete(NULL);
}

// ========================== EMERGENCY HANDLING ==============================

void handleEmergency() {
  unsigned long elapsed = millis() - emergencyStartTime;
  
  // Pulse LED during emergency
  digitalWrite(LED_PIN, (millis() / 250) % 2);
  
  // Periodic buzzer beep
  if ((elapsed / 5000) % 2 == 0 && (millis() % 5000) < 200) {
    tone(BUZZER_PIN, 2000, 200);
  }
}

// ========================== OLED DISPLAY ====================================

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  // Line 1: State
  display.printf("State: %s", stateNames[deviceState]);
  if (deviceState == STATE_EMERGENCY) {
    // Blinking indicator
    if ((millis() / 500) % 2) display.print(" !");
  }
  
  // Line 2: Heart Rate
  display.setCursor(0, 12);
  if (sensorData.hrValid)
    display.printf("HR: %.0f BPM", sensorData.bpm);
  else
    display.print("HR: --");
  if (triggers.highHeartRate) display.print(" HIGH!");
  
  // Line 3: GPS
  display.setCursor(0, 24);
  if (sensorData.gpsValid)
    display.printf("GPS: %.4f,%.4f", sensorData.latitude, sensorData.longitude);
  else
    display.printf("GPS: No fix (%d sat)", sensorData.satellites);
  
  // Line 4: Accelerometer
  display.setCursor(0, 36);
  display.printf("Accel: %.1fg", sensorData.accelMagnitude);
  if (triggers.excessMovement) display.print(" MOVE!");
  
  // Line 5: Wake word count & Audio
  display.setCursor(0, 48);
  display.printf("Wake:%d/%d", triggers.wakeWordCount, WAKE_WORD_THRESHOLD);
  if (sensorData.audioRecording) display.print(" [REC]");
  
  // Line 6: WiFi & Time
  display.setCursor(0, 56);
  if (hwStatus.wifiOK)
    display.printf("WiFi OK  %s", sensorData.timestamp + 11);  // Just time portion
  else
    display.print("WiFi OFF");
  
  display.display();
}

// ========================== LED STATUS ======================================

void updateStatusLED(unsigned long now) {
  switch (deviceState) {
    case STATE_NORMAL:
      // Slow breathe: ON 100ms every 3 seconds
      digitalWrite(LED_PIN, (now % 3000) < 100);
      break;
    case STATE_ALERT:
      // Medium blink: 500ms on/off
      digitalWrite(LED_PIN, (now % 1000) < 500);
      break;
    case STATE_EMERGENCY:
      // Fast blink: 150ms on/off
      digitalWrite(LED_PIN, (now % 300) < 150);
      break;
    case STATE_STANDBY:
      // Double blink every 2 seconds
      {
        unsigned long phase = now % 2000;
        digitalWrite(LED_PIN, (phase < 100) || (phase > 200 && phase < 300));
      }
      break;
  }
}

// ========================== JSON BUILDERS ==================================

String buildSensorJSON() {
  JsonDocument doc;
  
  doc["device"]     = "EchoBand";
  doc["version"]    = "1.1";
  doc["state"]      = stateNames[deviceState];
  doc["uptime_ms"]  = millis() - bootTime;
  doc["timestamp"]  = sensorData.timestamp;
  
  // Heart Rate
  JsonObject hr = doc["heart_rate"].to<JsonObject>();
  hr["bpm"]       = sensorData.hrValid ? sensorData.bpm : 0;
  hr["valid"]     = sensorData.hrValid;
  hr["panic"]     = triggers.highHeartRate;
  #ifdef USE_MAX30102
    hr["ir_value"]  = hrLastIRValue;
    hr["sensor"]    = "MAX30102";
  #else
    hr["sensor"]    = "ANALOG_PULSE";
  #endif
  
  // GPS
  JsonObject gpsObj = doc["gps"].to<JsonObject>();
  gpsObj["latitude"]   = sensorData.gpsValid ? sensorData.latitude : 0;
  gpsObj["longitude"]  = sensorData.gpsValid ? sensorData.longitude : 0;
  gpsObj["altitude"]   = sensorData.altitude;
  gpsObj["satellites"] = sensorData.satellites;
  gpsObj["valid"]      = sensorData.gpsValid;
  
  // MPU6050
  JsonObject mpuObj = doc["accelerometer"].to<JsonObject>();
  mpuObj["accel_x"]    = sensorData.accelX;
  mpuObj["accel_y"]    = sensorData.accelY;
  mpuObj["accel_z"]    = sensorData.accelZ;
  mpuObj["gyro_x"]     = sensorData.gyroX;
  mpuObj["gyro_y"]     = sensorData.gyroY;
  mpuObj["gyro_z"]     = sensorData.gyroZ;
  mpuObj["magnitude_g"] = sensorData.accelMagnitude;
  mpuObj["excess_movement"] = triggers.excessMovement;
  mpuObj["valid"]      = sensorData.mpuValid;
  
  // Triggers
  JsonObject trig = doc["triggers"].to<JsonObject>();
  trig["high_hr"]          = triggers.highHeartRate;
  trig["excess_movement"]  = triggers.excessMovement;
  trig["wake_word_count"]  = triggers.wakeWordCount;
  trig["wake_word_needed"] = WAKE_WORD_THRESHOLD;
  trig["button"]           = false;  // Momentary
  
  // Audio
  JsonObject audio = doc["audio"].to<JsonObject>();
  audio["recording"]     = sensorData.audioRecording;
  audio["chunk_ready"]   = sensorData.audioChunkReady;
  audio["chunk_index"]   = sensorData.audioChunkIndex;
  audio["sample_rate"]   = I2S_SAMPLE_RATE;
  audio["bits"]          = I2S_BITS;
  audio["chunk_seconds"] = AUDIO_CHUNK_SECS;
  
  // Hardware Status
  JsonObject hw = doc["hardware"].to<JsonObject>();
  hw["heart_rate"] = hwStatus.hrOK;
  hw["gps"]        = hwStatus.gpsOK;
  hw["mpu6050"]    = hwStatus.mpuOK;
  hw["rtc"]        = hwStatus.rtcOK;
  hw["oled"]       = hwStatus.oledOK;
  hw["i2s_mic"]    = hwStatus.i2sOK;
  hw["spiffs"]     = hwStatus.spiffsOK;
  hw["wifi"]       = hwStatus.wifiOK;
  
  String output;
  serializeJsonPretty(doc, output);
  return output;
}

String buildHRJSON() {
  JsonDocument doc;
  doc["bpm"]     = sensorData.hrValid ? sensorData.bpm : 0;
  doc["valid"]   = sensorData.hrValid;
  doc["panic"]   = triggers.highHeartRate;
  doc["threshold_bpm"] = HR_PANIC_BPM;
  doc["timestamp"] = sensorData.timestamp;
  String output;
  serializeJson(doc, output);
  return output;
}

String buildGPSJSON() {
  JsonDocument doc;
  doc["latitude"]   = sensorData.gpsValid ? sensorData.latitude : 0;
  doc["longitude"]  = sensorData.gpsValid ? sensorData.longitude : 0;
  doc["altitude"]   = sensorData.altitude;
  doc["satellites"] = sensorData.satellites;
  doc["valid"]      = sensorData.gpsValid;
  doc["timestamp"]  = sensorData.timestamp;
  String output;
  serializeJson(doc, output);
  return output;
}

String buildMPUJSON() {
  JsonDocument doc;
  doc["accel_x"]    = sensorData.accelX;
  doc["accel_y"]    = sensorData.accelY;
  doc["accel_z"]    = sensorData.accelZ;
  doc["gyro_x"]     = sensorData.gyroX;
  doc["gyro_y"]     = sensorData.gyroY;
  doc["gyro_z"]     = sensorData.gyroZ;
  doc["magnitude_g"] = sensorData.accelMagnitude;
  doc["excess"]     = triggers.excessMovement;
  doc["threshold_g"] = ACCEL_EXCESS_G;
  doc["timestamp"]  = sensorData.timestamp;
  String output;
  serializeJson(doc, output);
  return output;
}

// ========================== HTTP API ROUTES =================================

void initWebServer() {
  // --- CORS headers for all responses ---
  server.enableCORS(true);
  
  // --- Dashboard (test page) ---
  server.on("/", HTTP_GET, serveDashboard);
  
  // --- REST API Endpoints ---
  
  // GET /api/all — Full sensor dump
  server.on("/api/all", HTTP_GET, []() {
    server.send(200, "application/json", buildSensorJSON());
  });
  
  // GET /api/heartrate
  server.on("/api/heartrate", HTTP_GET, []() {
    server.send(200, "application/json", buildHRJSON());
  });
  
  // GET /api/gps
  server.on("/api/gps", HTTP_GET, []() {
    server.send(200, "application/json", buildGPSJSON());
  });
  
  // GET /api/mpu
  server.on("/api/mpu", HTTP_GET, []() {
    server.send(200, "application/json", buildMPUJSON());
  });
  // GET /api/audio/stream — Record 3 s on demand, return raw PCM
// No SPIFFS needed — records straight to heap, streams back immediately
// GET /api/audio/stream — record N seconds, stream directly to client (no heap buffer)
server.on("/api/audio/stream", HTTP_GET, []() {
  if (!hwStatus.i2sOK) {
    server.send(503, "application/json", "{\"error\":\"I2S mic not available\"}");
    return;
  }

  const int    REC_SECS   = 2;
  const size_t TOTAL_BYTES = (size_t)I2S_SAMPLE_RATE * (I2S_BITS / 8) * REC_SECS; // 64 000

  // Send headers manually so we can stream the body in chunks
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/octet-stream");
  client.println("Content-Length: " + String(TOTAL_BYTES));
  client.println("X-Audio-SampleRate: " + String(I2S_SAMPLE_RATE));
  client.println("X-Audio-Bits: 16");
  client.println("X-Audio-Channels: 1");
  client.println("Content-Disposition: attachment; filename=\"test.raw\"");
  client.println("Access-Control-Allow-Origin: *");
  client.println();  // End of headers

  // DMA read buffer — only 1 KB on the stack, reused each chunk
  int16_t dmaBuf[512];   // 512 samples = 1024 bytes
  size_t  totalSent = 0;

  while (totalSent < TOTAL_BYTES) {
    size_t remaining = TOTAL_BYTES - totalSent;
    size_t toRead    = min(sizeof(dmaBuf), remaining);
    size_t bytesRead = 0;

    esp_err_t err = i2s_read(I2S_NUM_0, dmaBuf, toRead, &bytesRead, pdMS_TO_TICKS(200));
    if (err == ESP_OK && bytesRead > 0) {
      client.write((const uint8_t*)dmaBuf, bytesRead);
      totalSent += bytesRead;
    }
    yield();
  }

  Serial.printf("[AUDIO] Stream test: sent %u bytes\n", totalSent);
});
  // GET /api/status — Device state & triggers
  server.on("/api/status", HTTP_GET, []() {
    JsonDocument doc;
    doc["state"]       = stateNames[deviceState];
    doc["uptime_ms"]   = millis() - bootTime;
    doc["timestamp"]   = sensorData.timestamp;
    doc["wifi_rssi"]   = WiFi.RSSI();
    doc["free_heap"]   = ESP.getFreeHeap();
    
    JsonObject trig = doc["triggers"].to<JsonObject>();
    trig["high_hr"]         = triggers.highHeartRate;
    trig["excess_movement"] = triggers.excessMovement;
    trig["wake_word_count"] = triggers.wakeWordCount;
    trig["button"]          = false;
    
    JsonObject hw = doc["hardware"].to<JsonObject>();
    hw["hr"] = hwStatus.hrOK;  hw["gps"] = hwStatus.gpsOK;
    hw["mpu"] = hwStatus.mpuOK; hw["rtc"] = hwStatus.rtcOK;
    hw["oled"] = hwStatus.oledOK; hw["i2s"] = hwStatus.i2sOK;
    //hw["spiffs"] = hwStatus.spiffsOK;
    
    String output;
    serializeJsonPretty(doc, output);
    server.send(200, "application/json", output);
  });
  
  // GET /api/audio — Download latest audio chunk as raw bytes
  // GET /api/audio — serve audio chunk directly from RAM (no SPIFFS)
server.on("/api/audio", HTTP_GET, []() {
  if (!audioReady || audioBufLen == 0) {
    server.send(404, "application/json", "{\"error\":\"No audio ready\"}");
    return;
  }

  server.sendHeader("Content-Type",        "application/octet-stream");
  server.sendHeader("Content-Length",       String(audioBufLen));
  server.sendHeader("X-Audio-SampleRate",   String(I2S_SAMPLE_RATE));
  server.sendHeader("X-Audio-Bits",         "16");
  server.sendHeader("X-Audio-Channels",     "1");
  server.sendHeader("X-Audio-ChunkIndex",   String(audioChunkIdx));
  server.sendHeader("Content-Disposition",  "attachment; filename=\"chunk.raw\"");

  // Send directly from RAM buffer
  server.send_P(200, "application/octet-stream", (const char*)audioBuf, audioBufLen);

  // Mark consumed so laptop knows it got it
  audioReady = false;
  sensorData.audioChunkReady = false;
});
  
  // GET /api/audio/info — Audio metadata without downloading
  server.on("/api/audio/info", HTTP_GET, []() {
    JsonDocument doc;
    doc["recording"]     = sensorData.audioRecording;
    doc["chunk_ready"]   = sensorData.audioChunkReady;
    doc["chunk_index"]   = sensorData.audioChunkIndex;
    doc["sample_rate"]   = I2S_SAMPLE_RATE;
    doc["bits"]          = I2S_BITS;
    doc["channels"]      = 1;
    doc["chunk_seconds"] = AUDIO_CHUNK_SECS;
    doc["format"]        = "raw PCM signed 16-bit little-endian";
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
  });
  
  // POST /api/stt — Receive speech-to-text result from software
  // Body: {"text": "help me please help help"}
  server.on("/api/stt", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"No body\"}");
      return;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    const char* text = doc["text"] | "";
    strncpy(lastSTTText, text, sizeof(lastSTTText) - 1);
    
    // Count occurrences of "help" (case-insensitive)
    String lower = String(text);
    lower.toLowerCase();
    int helpCount = 0;
    int searchFrom = 0;
    while (true) {
      int idx = lower.indexOf("help", searchFrom);
      if (idx == -1) break;
      helpCount++;
      searchFrom = idx + 4;
    }
    
    if (helpCount > 0) {
      Serial.printf("[INFO]     STT received: \"%s\" — 'help' found %d times\n", text, helpCount);
      
      // Increment wake word counter
      if (triggers.wakeWordCount == 0) {
        triggers.wakeWordFirstTime = millis();
      }
      triggers.wakeWordCount += helpCount;
      
      Serial.printf("[INFO]     Wake word count: %d/%d\n", 
                    triggers.wakeWordCount, WAKE_WORD_THRESHOLD);
    }
    
    JsonDocument resp;
    resp["status"]         = "ok";
    resp["help_found"]     = helpCount;
    resp["wake_word_total"] = triggers.wakeWordCount;
    resp["threshold"]      = WAKE_WORD_THRESHOLD;
    resp["triggered"]      = (triggers.wakeWordCount >= WAKE_WORD_THRESHOLD);
    
    String output;
    serializeJson(resp, output);
    server.send(200, "application/json", output);
  });
  
  // POST /api/trigger — Manual trigger for testing (simulate button/wake/hr)
  // Body: {"type": "button"} or {"type": "wakeword"} or {"type": "heartrate", "bpm": 150}
  server.on("/api/trigger", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"No body\"}");
      return;
    }
    
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));
    const char* type = doc["type"] | "";
    
    if (strcmp(type, "button") == 0) {
      triggers.buttonPressed = true;
      server.send(200, "application/json", "{\"status\":\"Button trigger simulated\"}");
    }
    else if (strcmp(type, "wakeword") == 0) {
      if (triggers.wakeWordCount == 0) triggers.wakeWordFirstTime = millis();
      triggers.wakeWordCount++;
      String resp = "{\"status\":\"Wake word incremented\",\"count\":" + 
                    String(triggers.wakeWordCount) + "}";
      server.send(200, "application/json", resp);
    }
    else if (strcmp(type, "heartrate") == 0) {
      float fakeBPM = doc["bpm"] | 130.0f;
      sensorData.bpm = fakeBPM;
      sensorData.hrValid = true;
      String resp = "{\"status\":\"Heart rate simulated\",\"bpm\":" + String(fakeBPM) + "}";
      server.send(200, "application/json", resp);
    }
    else if (strcmp(type, "movement") == 0) {
      triggers.excessMovement = true;
      triggers.excessMoveStart = millis() - ACCEL_SUSTAIN_MS - 1;
      server.send(200, "application/json", "{\"status\":\"Excess movement simulated\"}");
    }
    else if (strcmp(type, "emergency") == 0) {
      // Force full emergency
      transitionState(STATE_EMERGENCY);
      server.send(200, "application/json", "{\"status\":\"Emergency forced\"}");
    }
    else {
      server.send(400, "application/json", 
        "{\"error\":\"Unknown type. Use: button, wakeword, heartrate, movement, emergency\"}");
    }
  });
  
  // POST /api/reset — Reset emergency, go back to NORMAL
  server.on("/api/reset", HTTP_POST, []() {
    transitionState(STATE_NORMAL);
    triggers = {};  // Clear all triggers
    server.send(200, "application/json", "{\"status\":\"Device reset to NORMAL\"}");
  });
  
  // GET /api/config — Current thresholds
  server.on("/api/config", HTTP_GET, []() {
    JsonDocument doc;
    doc["hr_panic_bpm"]        = HR_PANIC_BPM;
    doc["accel_excess_g"]      = ACCEL_EXCESS_G;
    doc["accel_sustain_ms"]    = ACCEL_SUSTAIN_MS;
    doc["wake_word_threshold"] = WAKE_WORD_THRESHOLD;
    doc["wake_word_window_ms"] = WAKE_WORD_WINDOW_MS;
    doc["audio_sample_rate"]   = I2S_SAMPLE_RATE;
    doc["audio_bits"]          = I2S_BITS;
    doc["audio_chunk_secs"]    = AUDIO_CHUNK_SECS;
    doc["static_ip"]           = WiFi.localIP().toString();
    
    String output;
    serializeJsonPretty(doc, output);
    server.send(200, "application/json", output);
  });
  
  // 404
  server.onNotFound([]() {
    String msg = "{\"error\":\"Not found\",\"endpoints\":["
      "\"/api/all\",\"/api/heartrate\",\"/api/gps\",\"/api/mpu\","
      "\"/api/status\",\"/api/audio\",\"/api/audio/info\","
      "\"/api/stt\",\"/api/trigger\",\"/api/reset\",\"/api/config\""
    "]}";
    server.send(404, "application/json", msg);
  });
  
  server.begin();
  Serial.printf("[INIT]     HTTP server started on port %d\n", HTTP_PORT);
}

// ========================== DASHBOARD HTML ==================================

void serveDashboard() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>EchoBand Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', Arial, sans-serif; background: #0f1923; color: #e0e0e0; padding: 16px; }
  h1 { color: #00e5ff; text-align: center; margin-bottom: 4px; font-size: 24px; }
  .subtitle { text-align: center; color: #607d8b; margin-bottom: 16px; font-size: 12px; }
  .state-bar { text-align: center; padding: 10px; border-radius: 8px; margin-bottom: 16px; font-weight: bold; font-size: 18px; }
  .state-NORMAL    { background: #1b5e20; color: #a5d6a7; }
  .state-ALERT     { background: #e65100; color: #ffcc80; }
  .state-EMERGENCY { background: #b71c1c; color: #ef9a9a; animation: pulse 0.5s infinite; }
  .state-STANDBY   { background: #1a237e; color: #9fa8da; }
  @keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.6; } }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 12px; }
  .card { background: #1a2a3a; border: 1px solid #263a4a; border-radius: 10px; padding: 14px; }
  .card h3 { color: #00e5ff; margin-bottom: 8px; font-size: 14px; border-bottom: 1px solid #263a4a; padding-bottom: 6px; }
  .row { display: flex; justify-content: space-between; padding: 4px 0; font-size: 13px; }
  .label { color: #90a4ae; }
  .value { color: #fff; font-weight: 600; font-family: monospace; }
  .value.danger { color: #ef5350; }
  .value.ok { color: #66bb6a; }
  .value.warn { color: #ffa726; }
  .btn-group { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }
  .btn { padding: 8px 16px; border: none; border-radius: 6px; cursor: pointer; font-size: 13px; font-weight: 600; }
  .btn-red { background: #c62828; color: white; }
  .btn-orange { background: #e65100; color: white; }
  .btn-blue { background: #0277bd; color: white; }
  .btn-green { background: #2e7d32; color: white; }
  .btn-teal { background: #00695c; color: white; }
  .btn:hover { opacity: 0.85; }
  .hw-tag { display: inline-block; padding: 2px 8px; border-radius: 4px; font-size: 11px; margin: 2px; font-weight: 600; }
  .hw-ok   { background: #1b5e20; color: #a5d6a7; }
  .hw-fail { background: #b71c1c; color: #ef9a9a; }
  #log { background: #0a1520; border: 1px solid #263a4a; border-radius: 8px; padding: 10px; 
         font-family: monospace; font-size: 11px; max-height: 200px; overflow-y: auto; margin-top: 12px; }
  .log-entry { padding: 2px 0; border-bottom: 1px solid #162530; }
</style>
</head>
<body>
<h1>&#128737; EchoBand Dashboard</h1>
<p class="subtitle">Hardware Test Interface | Firmware v1.1</p>
<div id="stateBar" class="state-bar state-NORMAL">NORMAL</div>

<div class="grid">
  <!-- Heart Rate -->
  <div class="card">
    <h3>&#10084;&#65039; Heart Rate</h3>
    <div class="row"><span class="label">BPM</span><span id="hr_bpm" class="value">--</span></div>
    <div class="row"><span class="label">Valid</span><span id="hr_valid" class="value">--</span></div>
    <div class="row"><span class="label">Panic</span><span id="hr_panic" class="value">--</span></div>
    <div class="row"><span class="label">Sensor</span><span id="hr_sensor" class="value">--</span></div>
  </div>
  
  <!-- GPS -->
  <div class="card">
    <h3>&#128205; GPS Location</h3>
    <div class="row"><span class="label">Latitude</span><span id="gps_lat" class="value">--</span></div>
    <div class="row"><span class="label">Longitude</span><span id="gps_lng" class="value">--</span></div>
    <div class="row"><span class="label">Altitude</span><span id="gps_alt" class="value">--</span></div>
    <div class="row"><span class="label">Satellites</span><span id="gps_sat" class="value">--</span></div>
    <div class="row"><span class="label">Fix</span><span id="gps_valid" class="value">--</span></div>
  </div>
  
  <!-- MPU6050 -->
  <div class="card">
    <h3>&#128640; Accelerometer</h3>
    <div class="row"><span class="label">Accel X</span><span id="accel_x" class="value">--</span></div>
    <div class="row"><span class="label">Accel Y</span><span id="accel_y" class="value">--</span></div>
    <div class="row"><span class="label">Accel Z</span><span id="accel_z" class="value">--</span></div>
    <div class="row"><span class="label">Magnitude</span><span id="accel_mag" class="value">--</span></div>
    <div class="row"><span class="label">Excess</span><span id="accel_excess" class="value">--</span></div>
  </div>
  
  <!-- Triggers & Audio -->
  <div class="card">
    <h3>&#9888;&#65039; Triggers & Audio</h3>
    <div class="row"><span class="label">Wake Word</span><span id="wake_count" class="value">--</span></div>
    <div class="row"><span class="label">High HR</span><span id="trig_hr" class="value">--</span></div>
    <div class="row"><span class="label">Movement</span><span id="trig_move" class="value">--</span></div>
    <div class="row"><span class="label">Recording</span><span id="audio_rec" class="value">--</span></div>
    <div class="row"><span class="label">Chunk #</span><span id="audio_chunk" class="value">--</span></div>
  </div>
  
  <!-- Device Info -->
  <div class="card">
    <h3>&#128225; Device Info</h3>
    <div class="row"><span class="label">Uptime</span><span id="uptime" class="value">--</span></div>
    <div class="row"><span class="label">Timestamp</span><span id="timestamp" class="value">--</span></div>
    <div class="row"><span class="label">Free Heap</span><span id="heap" class="value">--</span></div>
    <div class="row"><span class="label">RSSI</span><span id="rssi" class="value">--</span></div>
    <div id="hw_tags" style="margin-top:8px;"></div>
  </div>
  
  <!-- Controls -->
  <div class="card">
    <h3>&#127918; Test Controls</h3>
    <p style="font-size:12px;color:#607d8b;margin-bottom:8px;">Simulate triggers without real sensor data</p>
    <div class="btn-group">
      <button class="btn btn-red" onclick="trigger('emergency')">&#128680; Force Emergency</button>
      <button class="btn btn-orange" onclick="trigger('button')">&#128992; Button Press</button>
      <button class="btn btn-blue" onclick="trigger('wakeword')">&#127908; +1 Wake Word</button>
      <button class="btn btn-teal" onclick="trigger('heartrate')">&#10084; Fake HR 150</button>
      <button class="btn btn-orange" onclick="trigger('movement')">&#128675; Fake Movement</button>
      <button class="btn btn-green" onclick="resetDevice()">&#9193; Reset</button>
      <button class="btn btn-blue" onclick="downloadAudio()">&#127925; Get Audio</button>
    </div>
  </div>
</div>

<div id="log"><div class="log-entry" style="color:#00e5ff;">Dashboard started — polling /api/all every 1s</div></div>

<script>
function addLog(msg) {
  const log = document.getElementById('log');
  const e = document.createElement('div');
  e.className = 'log-entry';
  e.textContent = new Date().toLocaleTimeString() + ' | ' + msg;
  log.prepend(e);
  if (log.children.length > 50) log.removeChild(log.lastChild);
}

function fmt(v, d) { return typeof v === 'number' ? v.toFixed(d || 2) : '--'; }

function fmtUptime(ms) {
  let s = Math.floor(ms/1000), m = Math.floor(s/60), h = Math.floor(m/60);
  return h + 'h ' + (m%60) + 'm ' + (s%60) + 's';
}

async function fetchData() {
  try {
    const r = await fetch('/api/all');
    const d = await r.json();
    
    // State
    const sb = document.getElementById('stateBar');
    sb.textContent = d.state;
    sb.className = 'state-bar state-' + d.state;
    
    // HR
    document.getElementById('hr_bpm').textContent = d.heart_rate.valid ? fmt(d.heart_rate.bpm, 0) + ' BPM' : 'No finger';
    document.getElementById('hr_bpm').className = 'value' + (d.heart_rate.panic ? ' danger' : d.heart_rate.valid ? ' ok' : '');
    document.getElementById('hr_valid').textContent = d.heart_rate.valid ? 'Yes' : 'No';
    document.getElementById('hr_panic').textContent = d.heart_rate.panic ? 'YES!' : 'No';
    document.getElementById('hr_panic').className = 'value' + (d.heart_rate.panic ? ' danger' : ' ok');
    document.getElementById('hr_sensor').textContent = d.heart_rate.sensor;
    
    // GPS
    document.getElementById('gps_lat').textContent = d.gps.valid ? fmt(d.gps.latitude, 6) : '--';
    document.getElementById('gps_lng').textContent = d.gps.valid ? fmt(d.gps.longitude, 6) : '--';
    document.getElementById('gps_alt').textContent = fmt(d.gps.altitude, 1) + ' m';
    document.getElementById('gps_sat').textContent = d.gps.satellites;
    document.getElementById('gps_valid').textContent = d.gps.valid ? 'Fix OK' : 'No Fix';
    document.getElementById('gps_valid').className = 'value' + (d.gps.valid ? ' ok' : ' warn');
    
    // MPU
    document.getElementById('accel_x').textContent = fmt(d.accelerometer.accel_x);
    document.getElementById('accel_y').textContent = fmt(d.accelerometer.accel_y);
    document.getElementById('accel_z').textContent = fmt(d.accelerometer.accel_z);
    document.getElementById('accel_mag').textContent = fmt(d.accelerometer.magnitude_g) + ' g';
    document.getElementById('accel_mag').className = 'value' + (d.accelerometer.excess_movement ? ' danger' : ' ok');
    document.getElementById('accel_excess').textContent = d.accelerometer.excess_movement ? 'YES!' : 'No';
    document.getElementById('accel_excess').className = 'value' + (d.accelerometer.excess_movement ? ' danger' : ' ok');
    
    // Triggers
    document.getElementById('wake_count').textContent = d.triggers.wake_word_count + '/' + d.triggers.wake_word_needed;
    document.getElementById('wake_count').className = 'value' + (d.triggers.wake_word_count >= d.triggers.wake_word_needed ? ' danger' : d.triggers.wake_word_count > 0 ? ' warn' : '');
    document.getElementById('trig_hr').textContent = d.triggers.high_hr ? 'YES!' : 'No';
    document.getElementById('trig_hr').className = 'value' + (d.triggers.high_hr ? ' danger' : ' ok');
    document.getElementById('trig_move').textContent = d.triggers.excess_movement ? 'YES!' : 'No';
    document.getElementById('trig_move').className = 'value' + (d.triggers.excess_movement ? ' danger' : ' ok');
    
    // Audio
    document.getElementById('audio_rec').textContent = d.audio.recording ? 'RECORDING' : 'Idle';
    document.getElementById('audio_rec').className = 'value' + (d.audio.recording ? ' danger' : '');
    document.getElementById('audio_chunk').textContent = d.audio.chunk_index || '--';
    
    // Device
    document.getElementById('uptime').textContent = fmtUptime(d.uptime_ms);
    document.getElementById('timestamp').textContent = d.timestamp;
    document.getElementById('heap').textContent = (d.hardware ? Math.round(ESP.getFreeHeap()/1024) : '--') + ' KB';
    
    // HW Tags
    if (d.hardware) {
      const hw = d.hardware;
      let tags = '';
      for (const [k,v] of Object.entries(hw)) {
        tags += '<span class="hw-tag ' + (v ? 'hw-ok' : 'hw-fail') + '">' + k + '</span>';
      }
      document.getElementById('hw_tags').innerHTML = tags;
    }
  } catch(e) {
    addLog('Fetch error: ' + e.message);
  }
}

async function trigger(type) {
  try {
    let body = {type: type};
    if (type === 'heartrate') body.bpm = 150;
    const r = await fetch('/api/trigger', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});
    const d = await r.json();
    addLog('Trigger [' + type + ']: ' + (d.status || JSON.stringify(d)));
  } catch(e) { addLog('Trigger error: ' + e.message); }
}

async function resetDevice() {
  try {
    const r = await fetch('/api/reset', {method:'POST'});
    const d = await r.json();
    addLog('Reset: ' + d.status);
  } catch(e) { addLog('Reset error: ' + e.message); }
}

function downloadAudio() {
  window.open('/api/audio', '_blank');
  addLog('Audio download triggered');
}

// Free heap display fix
document.getElementById('heap').textContent = '--';

setInterval(fetchData, 1000);
fetchData();
</script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}
