/*
  ESP32-S3 Dual-Mic WAV Recorder + S3 Uploader — v1.3 (new i2s_std driver)
  Board: ESP32-S3 N16R8, Arduino-ESP32 core 3.x (IDF 5.x)
  Mics: 2x INMP441 (shared BCLK/WS/DIN; one L/R->GND = LEFT, one L/R->3V3 = RIGHT)
  Storage: SD card via SPI
  Status: onboard WS2812 RGB LED
  Control: 2 buttons + full serial command set

  WHY THIS VERSION EXISTS
  v1.2 used the legacy <driver/i2s.h> RX API. On core 3.3.10 it returned a
  clean ESP_OK from i2s_read() with fully populated bytesRead — but every
  32-bit word in the DMA buffer was 0x00000000 for the entire recording.
  That is not a wiring, gain, or bit-alignment problem: it means the legacy
  RX path never actually captured data from the pins on this core generation.
  This is a known incompatibility — Arduino core 3.x sits on ESP-IDF 5.x,
  where the legacy i2s.h RX driver is unreliable, and the documented fix is
  migrating to the new channel-based driver/i2s_std.h API. This file does
  that migration; every serial command from v1.2 (shift, fmt, ch, rate, raw,
  upload, save, defaults) still works the same way.

  Serial commands (newline-terminated, case-insensitive):
    help                 - list commands
    show                 - print current config + state
    start / stop         - control recording (same as buttons)
    shift <11..16>       - bit shift for 32->16 conversion (16 = clean, lower = +gain)
    fmt <i2s|msb>        - slot standard: Philips I2S vs MSB-justified
    ch <stereo|left|right> - channel mode (use 'left' for single-mic isolation test)
    rate <8000|16000|22050|32000|44100> - sample rate
    raw <on|off>         - periodic raw 32-bit sample hex dump while recording
    upload <on|off>      - enable/disable WiFi upload after save
    save                 - persist current config to NVS
    defaults             - restore defaults (not persisted until 'save')

  Diagnosis workflow now that the driver is fixed:
    1) 'raw on', 'start', speak, watch hex. Aligned INMP441 words look like
       FFF3A200 / 000C5E00 (low byte 00, values move smoothly, NOT all zero).
    2) If raw is finally non-zero but audio still sounds noisy/garbled, THEN
       try 'fmt msb' and 'ch left' (single mic) as before — that points to
       wiring/format rather than driver.
    3) If raw is still all-zero after this migration, suspect wiring: verify
       INMP441 VDD is 3.3V (not 5V), SD pin actually reaches GPIO6, and L/R
       pin is a solid GND/3V3 connection, not floating.
*/

#include <Arduino.h>
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <vector>

// ---------------- Fixed config ----------------
static const char* WIFI_SSID        = "Redmi";
static const char* WIFI_PASSWORD    = "@0987654321";
static const char* PRESIGN_ENDPOINT = "https://your-backend.example.com/get-upload-url";

//mic
constexpr gpio_num_t PIN_I2S_WS  = GPIO_NUM_7;   // WS / LRCLK
constexpr gpio_num_t PIN_I2S_SCK = GPIO_NUM_4;   // BCLK
constexpr gpio_num_t PIN_I2S_SD  = GPIO_NUM_15;  // DIN

//sd card
constexpr int PIN_SD_CS   = 10;
constexpr int PIN_SD_MOSI = 11;
constexpr int PIN_SD_SCK  = 12;
constexpr int PIN_SD_MISO = 13;

//buttons
constexpr int PIN_BTN_START = 8;
constexpr int PIN_BTN_STOP  = 14;
constexpr int PIN_BATT_ADC  = 1;

//dont change
constexpr uint32_t I2S_DMA_DESC_NUM  = 6;     // number of DMA buffers
constexpr uint32_t I2S_DMA_FRAME_NUM = 512;   // frames (samples per active slot) per DMA buffer
constexpr float    LOW_BATT_VOLTAGE  = 3.3f;

constexpr unsigned long DEBOUNCE_MS               = 250;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS   = 10000;
constexpr unsigned long SAVING_BLINK_INTERVAL_MS  = 400;
constexpr unsigned long SAVING_DURATION_MS        = 5000;
constexpr uint32_t      RAW_DEBUG_EVERY_N_CHUNKS  = 50;   // ~1.6 s at 16 kHz stereo

// ---------------- Tunable audio config ----------------
enum ChMode : uint8_t { CH_STEREO = 0, CH_LEFT = 1, CH_RIGHT = 2 };
enum FmtMode : uint8_t { FMT_I2S = 0, FMT_MSB = 1 };   // Philips I2S vs MSB-justified slot standard

struct AudioConfig {
  uint32_t sampleRate = 16000;
  uint8_t  shiftBits  = 16;        // 32->16 conversion shift (16 = unity, 14 = +12 dB)
  uint8_t  fmt        = FMT_I2S;
  uint8_t  chMode     = CH_STEREO;
  bool     rawDebug   = false;
  bool     uploadEnabled = true;
};
AudioConfig cfg;

// ---------------- State ----------------
enum class State { IDLE, RECORDING, SAVING, UPLOAD_CHECK };
volatile State appState = State::IDLE;

volatile bool startFlag = false;
volatile bool stopFlag  = false;
volatile unsigned long lastStartIsrMs = 0;
volatile unsigned long lastStopIsrMs  = 0;

File     wavFile;
String   currentFilename;
uint32_t dataBytesWritten = 0;
uint8_t  recChannels = 2;         // channels locked in at record start
uint32_t recSampleRate = 16000;   // rate locked in at record start

unsigned long savingStateEnteredMs = 0;
unsigned long lastBlinkToggleMs = 0;
bool blinkOn = false;

i2s_chan_handle_t rxHandle = NULL;
bool i2sInstalled = false;
uint32_t chunkCounter = 0;

Preferences prefs;
Adafruit_NeoPixel rgb(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

// ---------------- Logging ----------------
void logMsg(const char* tag, const String& msg) {
  Serial.printf("[%s] %s\n", tag, msg.c_str());
}

// ---------------- Button ISRs ----------------
void IRAM_ATTR onStartPress() {
  unsigned long now = millis();
  if (now - lastStartIsrMs > DEBOUNCE_MS) { startFlag = true; lastStartIsrMs = now; }
}
void IRAM_ATTR onStopPress() {
  unsigned long now = millis();
  if (now - lastStopIsrMs > DEBOUNCE_MS) { stopFlag = true; lastStopIsrMs = now; }
}

// ---------------- LED (show only on change) ----------------
uint32_t lastLedColor = 0xFFFFFFFF;
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = rgb.Color(r, g, b);
  if (c == lastLedColor) return;
  lastLedColor = c;
  rgb.setPixelColor(0, c);
  rgb.show();
}
void ledOff()  { setLed(0, 0, 0); }
void ledRed()  { setLed(255, 0, 0); }
void ledBlue() { setLed(0, 0, 255); }

void updateLed() {
  switch (appState) {
    case State::RECORDING:
      ledRed();
      break;
    case State::SAVING: {
      unsigned long now = millis();
      if (now - lastBlinkToggleMs >= SAVING_BLINK_INTERVAL_MS) {
        blinkOn = !blinkOn;
        lastBlinkToggleMs = now;
      }
      if (blinkOn) ledRed(); else ledOff();
      break;
    }
    case State::IDLE:
    case State::UPLOAD_CHECK:
    default:
      if (WiFi.status() == WL_CONNECTED) ledBlue(); else ledOff();
      break;
  }
}

// ---------------- Config helpers ----------------
const char* fmtName(uint8_t f)  { return f == FMT_MSB ? "msb" : "i2s"; }
const char* chName(uint8_t c)   { return c == CH_LEFT ? "left" : (c == CH_RIGHT ? "right" : "stereo"); }
uint8_t channelCount(uint8_t c) { return c == CH_STEREO ? 2 : 1; }

void printConfig() {
  logMsg("CONFIG", "rate=" + String(cfg.sampleRate) + "Hz  shift=" + String(cfg.shiftBits) +
                   "  fmt=" + fmtName(cfg.fmt) + "  ch=" + chName(cfg.chMode) +
                   "  raw=" + (cfg.rawDebug ? "on" : "off") +
                   "  upload=" + (cfg.uploadEnabled ? "on" : "off"));
  logMsg("CONFIG", String("state=") +
         (appState == State::IDLE ? "IDLE" :
          appState == State::RECORDING ? "RECORDING" :
          appState == State::SAVING ? "SAVING" : "UPLOAD_CHECK"));
}

void loadConfig() {
  prefs.begin("recorder", false);
  cfg.sampleRate    = prefs.getUInt("rate", 16000);
  cfg.shiftBits     = prefs.getUChar("shift", 16);
  cfg.fmt           = prefs.getUChar("fmt", FMT_I2S);
  cfg.chMode        = prefs.getUChar("ch", CH_STEREO);
  cfg.rawDebug      = prefs.getBool("raw", false);
  cfg.uploadEnabled = prefs.getBool("upl", true);
  if (cfg.shiftBits < 11 || cfg.shiftBits > 16) cfg.shiftBits = 16;
}

void saveConfig() {
  prefs.putUInt("rate", cfg.sampleRate);
  prefs.putUChar("shift", cfg.shiftBits);
  prefs.putUChar("fmt", cfg.fmt);
  prefs.putUChar("ch", cfg.chMode);
  prefs.putBool("raw", cfg.rawDebug);
  prefs.putBool("upl", cfg.uploadEnabled);
  logMsg("CONFIG", "Saved to NVS");
}

// ---------------- I2S (new i2s_std channel driver) ----------------
bool initI2S() {
  if (i2sInstalled && rxHandle) {
    i2s_channel_disable(rxHandle);
    i2s_del_channel(rxHandle);
    rxHandle = NULL;
    i2sInstalled = false;
  }

  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num  = I2S_DMA_DESC_NUM;
  chanCfg.dma_frame_num = I2S_DMA_FRAME_NUM;
  chanCfg.auto_clear    = false;

  esp_err_t err = i2s_new_channel(&chanCfg, NULL, &rxHandle);
  if (err != ESP_OK) {
    logMsg("ERROR", "i2s_new_channel failed: " + String((int)err));
    rxHandle = NULL;
    return false;
  }

  i2s_slot_mode_t slotMode = (cfg.chMode == CH_STEREO) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;

 i2s_std_slot_config_t slotCfg;
  if (cfg.fmt == FMT_MSB) {
    slotCfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, slotMode);
  } else {
    slotCfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, slotMode);
  }
  if (cfg.chMode == CH_RIGHT) slotCfg.slot_mask = I2S_STD_SLOT_RIGHT;
  else if (cfg.chMode == CH_LEFT) slotCfg.slot_mask = I2S_STD_SLOT_LEFT;

  i2s_std_config_t stdCfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(cfg.sampleRate),
    .slot_cfg = slotCfg,
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = PIN_I2S_SCK,
      .ws   = PIN_I2S_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = PIN_I2S_SD,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };

  err = i2s_channel_init_std_mode(rxHandle, &stdCfg);
  if (err != ESP_OK) {
    logMsg("ERROR", "i2s_channel_init_std_mode failed: " + String((int)err));
    i2s_del_channel(rxHandle);
    rxHandle = NULL;
    return false;
  }

  err = i2s_channel_enable(rxHandle);
  if (err != ESP_OK) {
    logMsg("ERROR", "i2s_channel_enable failed: " + String((int)err));
    i2s_del_channel(rxHandle);
    rxHandle = NULL;
    return false;
  }

  i2sInstalled = true;
  logMsg("INIT", "I2S ready (i2s_std driver): " + String(cfg.sampleRate) + "Hz " +
                 chName(cfg.chMode) + " fmt=" + fmtName(cfg.fmt));
  return true;
}

bool applyAudioSettings(const char* what) {
  if (appState != State::IDLE) {
    logMsg("WARN", String(what) + " change requires IDLE state — stop recording first");
    return false;
  }
  if (!initI2S()) {
    logMsg("ERROR", "I2S reinit failed after settings change");
    return false;
  }
  return true;
}

// ---------------- WAV header ----------------
void writeWavHeaderPlaceholder(File &f, uint8_t channels, uint32_t sampleRate) {
  uint8_t header[44] = {0};
  memcpy(header, "RIFF", 4);
  memcpy(header + 8, "WAVE", 4);
  memcpy(header + 12, "fmt ", 4);
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = channels;
  uint16_t bitsPerSample = 16;
  uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
  uint16_t blockAlign = numChannels * (bitsPerSample / 8);

  memcpy(header + 16, &subchunk1Size, 4);
  memcpy(header + 20, &audioFormat, 2);
  memcpy(header + 22, &numChannels, 2);
  memcpy(header + 24, &sampleRate, 4);
  memcpy(header + 28, &byteRate, 4);
  memcpy(header + 32, &blockAlign, 2);
  memcpy(header + 34, &bitsPerSample, 2);
  memcpy(header + 36, "data", 4);
  f.write(header, 44);
}

void finalizeWavHeader(File &f, uint32_t dataSize) {
  uint32_t riffSize = 36 + dataSize;
  f.seek(4);
  f.write((uint8_t*)&riffSize, 4);
  f.seek(40);
  f.write((uint8_t*)&dataSize, 4);
  f.flush();
}

// ---------------- Recording ----------------
bool startRecording() {
  recChannels   = channelCount(cfg.chMode);
  recSampleRate = cfg.sampleRate;
  currentFilename = "/rec_" + String(millis()) + ".wav";
  wavFile = SD.open(currentFilename, FILE_WRITE);
  if (!wavFile) {
    logMsg("ERROR", "Failed to open file for recording: " + currentFilename);
    return false;
  }
  writeWavHeaderPlaceholder(wavFile, recChannels, recSampleRate);
  dataBytesWritten = 0;
  chunkCounter = 0;
  logMsg("STATE", "Recording -> " + currentFilename + " (" + String(recSampleRate) + "Hz, " +
                  String(recChannels) + "ch, shift=" + String(cfg.shiftBits) + ")");
  return true;
}

void recordChunk() {
  static int32_t i2sBuffer[I2S_DMA_FRAME_NUM * 2];
  static int16_t pcmBuffer[I2S_DMA_FRAME_NUM * 2];

  size_t bytesRead = 0;
  esp_err_t res = i2s_channel_read(rxHandle, i2sBuffer, sizeof(i2sBuffer), &bytesRead, portMAX_DELAY);
  if (res != ESP_OK || bytesRead == 0) {
    logMsg("WARN", "i2s_channel_read returned no data (err=" + String((int)res) + ")");
    return;
  }

  int samplesRead = bytesRead / sizeof(int32_t);
  chunkCounter++;

  if (cfg.rawDebug && (chunkCounter % RAW_DEBUG_EVERY_N_CHUNKS == 1)) {
    Serial.printf("[DEBUG] raw: %08X %08X %08X %08X %08X %08X\n",
                  (unsigned)i2sBuffer[0], (unsigned)i2sBuffer[1], (unsigned)i2sBuffer[2],
                  (unsigned)i2sBuffer[3], (unsigned)i2sBuffer[4], (unsigned)i2sBuffer[5]);
  }

  const uint8_t shift = cfg.shiftBits;
  for (int i = 0; i < samplesRead; i++) {
    int32_t v = i2sBuffer[i] >> shift;
    if (v > 32767) v = 32767;          // saturate, never wrap
    else if (v < -32768) v = -32768;
    pcmBuffer[i] = (int16_t)v;
  }

  size_t bytesToWrite = samplesRead * sizeof(int16_t);
  size_t written = wavFile.write((uint8_t*)pcmBuffer, bytesToWrite);
  if (written != bytesToWrite) {
    logMsg("ERROR", "SD write incomplete — possible full card or write failure");
  }
  dataBytesWritten += written;
}

void enterSavingState() {
  finalizeWavHeader(wavFile, dataBytesWritten);
  wavFile.close();
  uint32_t bytesPerSec = recSampleRate * recChannels * 2;
  float seconds = bytesPerSec ? (float)dataBytesWritten / bytesPerSec : 0.0f;
  logMsg("STATE", "Stopped. " + currentFilename + " (" + String(dataBytesWritten) +
                  " bytes = " + String(seconds, 1) + "s)");
  appState = State::SAVING;
  savingStateEnteredMs = millis();
  lastBlinkToggleMs = millis();
  blinkOn = true;
}

// ---------------- Pending queue ----------------
void addToPendingQueue(const String& filename) {
  File f = SD.open("/pending.txt", FILE_APPEND);
  if (f) {
    f.println(filename);
    f.close();
    logMsg("INFO", "Queued for later upload: " + filename);
  } else {
    logMsg("ERROR", "Could not open pending.txt to queue file");
  }
}

// ---------------- WiFi / upload ----------------
bool connectWiFiIfAvailable() {
  if (WiFi.status() == WL_CONNECTED) return true;
  logMsg("INFO", "Attempting WiFi connection...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    logMsg("RECOVERY", "WiFi connected: " + WiFi.localIP().toString());
    return true;
  }
  logMsg("WARN", "WiFi not available — will retry later");
  return false;
}

bool requestPresignedUrl(const String& filename, String &outUrl) {
  HTTPClient http;
  String endpoint = String(PRESIGN_ENDPOINT) + "?filename=" + filename;
  http.begin(endpoint);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    int idx = payload.indexOf("\"url\":\"");
    if (idx >= 0) {
      int start = idx + 7;
      int end = payload.indexOf("\"", start);
      outUrl = payload.substring(start, end);
      http.end();
      return true;
    }
  }
  logMsg("ERROR", "Failed to get pre-signed URL, HTTP code: " + String(code));
  http.end();
  return false;
}

bool uploadFile(const String& filename) {
  File f = SD.open(filename, FILE_READ);
  if (!f) {
    logMsg("ERROR", "Cannot open file for upload: " + filename);
    return false;
  }
  size_t fileSize = f.size();

  String presignedUrl;
  if (!requestPresignedUrl(filename, presignedUrl)) {
    f.close();
    return false;
  }

  HTTPClient http;
  http.begin(presignedUrl);
  http.addHeader("Content-Type", "audio/wav");
  int code = http.sendRequest("PUT", &f, fileSize);
  f.close();

  if (code == 200 || code == 201) {
    logMsg("INFO", "Upload successful: " + filename);
    http.end();
    return true;
  }
  logMsg("ERROR", "Upload failed, HTTP code: " + String(code));
  http.end();
  return false;
}

void processPendingQueue() {
  if (!SD.exists("/pending.txt")) return;
  File f = SD.open("/pending.txt", FILE_READ);
  if (!f) return;

  std::vector<String> remaining;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    if (!uploadFile(line)) {
      remaining.push_back(line);
    } else {
      SD.remove(line);
    }
  }
  f.close();
  SD.remove("/pending.txt");
  for (auto &l : remaining) addToPendingQueue(l);
}

// ---------------- Battery ----------------
float readBatteryVoltage() {
  int raw = analogRead(PIN_BATT_ADC);
  float vAdc = (raw / 4095.0f) * 3.3f;
  return vAdc * 2.0f;
}

void checkBatteryAndSleepIfLow() {
  // Intentionally disabled during bench testing (same as v1.1/v1.2).
  (void)readBatteryVoltage();
}

// ---------------- Serial command handling (non-blocking) ----------------
char   lineBuf[64];
size_t lineLen = 0;

void printHelp() {
  Serial.println(F("[HELP] Commands:"));
  Serial.println(F("  start | stop            record control"));
  Serial.println(F("  show                    print config + state"));
  Serial.println(F("  shift <11..16>          32->16 shift (16=clean, 14=+12dB)"));
  Serial.println(F("  fmt <i2s|msb>           slot standard: Philips I2S vs MSB-justified"));
  Serial.println(F("  ch <stereo|left|right>  channel mode"));
  Serial.println(F("  rate <hz>               8000/16000/22050/32000/44100"));
  Serial.println(F("  raw <on|off>            raw 32-bit sample hex dump"));
  Serial.println(F("  upload <on|off>         WiFi upload after save"));
  Serial.println(F("  save | defaults         persist / reset config"));
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  int sp = cmd.indexOf(' ');
  String verb = (sp < 0) ? cmd : cmd.substring(0, sp);
  String arg  = (sp < 0) ? ""  : cmd.substring(sp + 1);
  arg.trim();

  if (verb == "start")      { logMsg("DEBUG", "Serial: start"); startFlag = true; }
  else if (verb == "stop")  { logMsg("DEBUG", "Serial: stop");  stopFlag = true; }
  else if (verb == "help")  { printHelp(); }
  else if (verb == "show")  { printConfig(); }
  else if (verb == "save")  { saveConfig(); }
  else if (verb == "defaults") {
    AudioConfig d;
    bool needReinit = (d.sampleRate != cfg.sampleRate || d.fmt != cfg.fmt || d.chMode != cfg.chMode);
    cfg = d;
    if (needReinit) applyAudioSettings("defaults");
    logMsg("CONFIG", "Defaults restored (use 'save' to persist)");
    printConfig();
  }
  else if (verb == "shift") {
    int v = arg.toInt();
    if (v >= 11 && v <= 16) {
      cfg.shiftBits = (uint8_t)v;   // applies live, even mid-recording
      logMsg("CONFIG", "shift=" + String(v) + (v < 16 ? " (+" + String((16 - v) * 6) + "dB gain)" : " (unity)"));
    } else {
      logMsg("WARN", "shift must be 11..16");
    }
  }
  else if (verb == "fmt") {
    if (arg == "i2s" || arg == "msb") {
      uint8_t nf = (arg == "msb") ? FMT_MSB : FMT_I2S;
      if (nf != cfg.fmt) {
        uint8_t prev = cfg.fmt;
        cfg.fmt = nf;
        if (!applyAudioSettings("fmt")) cfg.fmt = prev;
      }
      logMsg("CONFIG", "fmt=" + String(fmtName(cfg.fmt)));
    } else logMsg("WARN", "fmt must be i2s or msb");
  }
  else if (verb == "ch") {
    uint8_t nc;
    if (arg == "stereo") nc = CH_STEREO;
    else if (arg == "left") nc = CH_LEFT;
    else if (arg == "right") nc = CH_RIGHT;
    else { logMsg("WARN", "ch must be stereo, left, or right"); return; }
    if (nc != cfg.chMode) {
      uint8_t prev = cfg.chMode;
      cfg.chMode = nc;
      if (!applyAudioSettings("ch")) cfg.chMode = prev;
    }
    logMsg("CONFIG", "ch=" + String(chName(cfg.chMode)));
  }
  else if (verb == "rate") {
    uint32_t r = (uint32_t)arg.toInt();
    if (r == 8000 || r == 16000 || r == 22050 || r == 32000 || r == 44100) {
      if (r != cfg.sampleRate) {
        uint32_t prev = cfg.sampleRate;
        cfg.sampleRate = r;
        if (!applyAudioSettings("rate")) cfg.sampleRate = prev;
      }
      logMsg("CONFIG", "rate=" + String(cfg.sampleRate) + "Hz");
    } else logMsg("WARN", "rate must be 8000/16000/22050/32000/44100");
  }
  else if (verb == "raw") {
    if (arg == "on")  { cfg.rawDebug = true;  logMsg("CONFIG", "raw dump ON"); }
    else if (arg == "off") { cfg.rawDebug = false; logMsg("CONFIG", "raw dump OFF"); }
    else logMsg("WARN", "raw must be on or off");
  }
  else if (verb == "upload") {
    if (arg == "on")  { cfg.uploadEnabled = true;  logMsg("CONFIG", "upload ON"); }
    else if (arg == "off") { cfg.uploadEnabled = false; logMsg("CONFIG", "upload OFF (files stay on SD, queued)"); }
    else logMsg("WARN", "upload must be on or off");
  }
  else {
    logMsg("WARN", "Unknown command: " + verb + " (type 'help')");
  }
}

void checkSerialCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleCommand(String(lineBuf));
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineLen = 0;   // overflow: discard line
      logMsg("WARN", "Serial line too long, discarded");
    }
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) delay(10);
  delay(200);
  logMsg("BOOT", "ESP32-S3 Dual-Mic Recorder v1.3 (i2s_std driver, core 3.x)");

  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_STOP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_START), onStartPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_STOP), onStopPress, FALLING);

  rgb.begin();
  rgb.setBrightness(60);
  ledOff();

  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS)) {
    logMsg("ERROR", "SD card mount failed — halting");
    while (true) delay(1000);
  }
  logMsg("INIT", "SD card mounted");

  loadConfig();
  printConfig();

  if (!initI2S()) {
    logMsg("ERROR", "I2S init failed — halting");
    while (true) delay(1000);
  }

  logMsg("BOOT", "Ready. 'help' for commands, 'start' to record. Try 'raw on' first.");
}

void loop() {
  checkSerialCommands();
  checkBatteryAndSleepIfLow();

  if (startFlag) {
    startFlag = false;
    if (appState == State::IDLE) {
      if (startRecording()) appState = State::RECORDING;
    } else {
      logMsg("WARN", "Start ignored — already recording or busy");
    }
  }

  if (stopFlag) {
    stopFlag = false;
    if (appState == State::RECORDING) {
      enterSavingState();
    } else {
      logMsg("WARN", "Stop ignored — not currently recording");
    }
  }

  switch (appState) {
    case State::RECORDING:
      recordChunk();
      break;

    case State::SAVING:
      if (millis() - savingStateEnteredMs >= SAVING_DURATION_MS) {
        appState = State::UPLOAD_CHECK;
        logMsg("STATE", "Saving complete, checking upload");
      }
      break;

    case State::UPLOAD_CHECK:
      if (!cfg.uploadEnabled) {
        addToPendingQueue(currentFilename);
        logMsg("INFO", "Upload disabled — file kept on SD");
      } else if (connectWiFiIfAvailable()) {
        if (!uploadFile(currentFilename)) {
          addToPendingQueue(currentFilename);
        } else {
          SD.remove(currentFilename);
        }
        processPendingQueue();
      } else {
        addToPendingQueue(currentFilename);
      }
      appState = State::IDLE;
      logMsg("STATE", "Back to idle.");
      break;

    case State::IDLE:
    default:
      break;
  }

  updateLed();
}
