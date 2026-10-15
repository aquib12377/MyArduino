/*
  ESP32-S3 web-controlled audio player, ultrasonic sensor and waving servo.
  Audio is embedded directly in audio1.h ... audio5.h as MP3 byte arrays.
  There is no audio filesystem and no separate filesystem upload.

  Install in Arduino IDE:
    - A current 3.x "esp32 by Espressif Systems" board package
    - "ESP8266Audio" by Earle F. Philhower, III
    - "ESP32Servo"
  Keep this sketch and all five audio headers in the same sketch folder.

  MAX98357A: BCLK=GPIO5, LRC=GPIO6, DIN=GPIO7, VIN=5V, common GND.
  SD/EN=3.3V; speaker only across SPK+ and SPK- (neither to GND).
  HC-SR04: TRIG=GPIO4, ECHO=GPIO3 through a 5V-to-3.3V divider.
  Servo: signal=GPIO2, adequate 5V supply, common GND.

  The existing web page, WiFi settings and sensor/servo behavior are retained.
  Audio is serviced cooperatively in loop(). The original pulseIn() can wait
  up to 20 ms; I2S has extra buffering for this bounded sensor wait. Decoder
  start/stop and draining the final DMA samples may also take a short time.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "audio1.h"
#include "audio2.h"
#include "audio3.h"
#include "audio4.h"
#include "audio5.h"
#include <ESP32Servo.h>

// ---------------- WiFi ----------------
const char* ssid     = "AAA";
const char* password = "Acube123";

WebServer server(80);

// ---------------- I2S Audio ----------------
#define I2S_BCLK  5
#define I2S_LRC   6
#define I2S_DOUT  7

AudioFileSourcePROGMEM audioSource;
AudioGeneratorMP3 mp3;
AudioOutputI2S audioOutput;
bool decoderBound = false;

// These names are C++ arrays from the included headers, not filenames.
const unsigned char* const audioData[] = {
  audio1_mp3, audio2_mp3, audio3_mp3, audio4_mp3, audio5_mp3
};
const size_t audioSizes[] = {
  sizeof(audio1_mp3), sizeof(audio2_mp3), sizeof(audio3_mp3),
  sizeof(audio4_mp3), sizeof(audio5_mp3)
};
const int NUM_AUDIO_FILES = sizeof(audioData) / sizeof(audioData[0]);
constexpr float AUDIO_GAIN = 0.60f;  // 0.0 = mute; 1.0 = full digital level
static_assert(sizeof(audio1_mp3) == audio1_mp3_len, "audio1 length mismatch");
static_assert(sizeof(audio2_mp3) == audio2_mp3_len, "audio2 length mismatch");
static_assert(sizeof(audio3_mp3) == audio3_mp3_len, "audio3 length mismatch");
static_assert(sizeof(audio4_mp3) == audio4_mp3_len, "audio4 length mismatch");
static_assert(sizeof(audio5_mp3) == audio5_mp3_len, "audio5 length mismatch");

// Skip a leading ID3v2 metadata tag before decoding. This only changes the
// read position; the supplied arrays and their metadata remain unchanged.
bool findMp3Start(const unsigned char* data, size_t size, size_t &offset) {
  offset = 0;
  if (size < 3 || data[0] != 'I' || data[1] != 'D' || data[2] != '3')
    return size > 0;
  if (size < 10 || data[3] < 2 || data[3] > 4) return false;
  uint32_t tagSize = 0;
  for (int i = 6; i < 10; ++i) {
    if (data[i] & 0x80) return false;  // ID3 size is synchsafe, 7 bits/byte
    tagSize = (tagSize << 7) | data[i];
  }
  offset = 10u + tagSize;
  if (data[3] == 4 && (data[5] & 0x10)) offset += 10;  // optional v2.4 footer
  return offset < size;
}

void stopAudio() {
  // stop() requires valid source/output pointers, set by begin().
  if (decoderBound) {
    mp3.stop();
    decoderBound = false;
  }
  audioSource.close();
}

bool startAudio(int id) {
  if (id < 0 || id >= NUM_AUDIO_FILES) return false;
  size_t offset = 0;
  if (!findMp3Start(audioData[id], audioSizes[id], offset)) return false;

  // A new button press interrupts the previous clip.
  stopAudio();
  if (!audioSource.open(audioData[id] + offset, audioSizes[id] - offset))
    return false;

  decoderBound = true;
  if (!mp3.begin(&audioSource, &audioOutput)) {
    stopAudio();
    Serial.println("MP3 initialization failed.");
    return false;
  }
  Serial.printf("Playing audio%d.h: %u bytes, metadata skip %u bytes\n",
                id + 1, (unsigned)audioSizes[id], (unsigned)offset);
  return true;
}

void serviceAudio() {
  if (!decoderBound) return;
  if (!mp3.isRunning() || !mp3.loop()) {
    // Let the final sound reach the speaker before shutting down I2S.
    audioOutput.flush();
    stopAudio();
    Serial.println("Playback ended.");
  }
}

// ---------------- HC-SR04 Ultrasonic Sensor ----------------
#define TRIG_PIN 4
#define ECHO_PIN 3                      // through voltage divider!M
const float DETECT_DISTANCE_CM = 30.0;  // trigger when something is closer than this
const unsigned long SENSOR_INTERVAL_MS = 200; // how often to ping
unsigned long lastSensorCheck = 0;

// ---------------- Servo ----------------
#define SERVO_PIN 2
Servo waveServo;

bool waving = false;
unsigned long waveStartTime = 0;
const unsigned long WAVE_DURATION_MS = 3000;
unsigned long lastServoStepTime = 0;
const unsigned long SERVO_STEP_INTERVAL_MS = 150; // wave speed
int servoAngle = 90;
int servoDirection = 1;
const int SERVO_MIN_ANGLE = 60;
const int SERVO_MAX_ANGLE = 120;

// =====================================================================
// Web page (served directly from flash)
// =====================================================================
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-S3 Audio Player</title>
  <style>
    body { font-family: sans-serif; text-align:center; background:#111; color:#eee; padding-top:40px; }
    button {
      display:block; width:80%; max-width:320px; margin:12px auto; padding:16px;
      font-size:18px; border:none; border-radius:10px; background:#2d8cf0; color:white;
    }
    button:active { background:#1c6dc7; }
    #status { margin-top:20px; color:#8f8; }
  </style>
</head>
<body>
  <h2>Play Audio</h2>
  %BUTTONS%
  <div id="status"></div>
<script>
function playAudio(id){
  fetch('/play?id=' + id)
    .then(r => r.text())
    .then(t => { document.getElementById('status').innerText = t; });
}
</script>
</body>
</html>
)rawliteral";

// =====================================================================
// Web handlers
// =====================================================================
void handleRoot() {
  String page = htmlPage;
  String buttons = "";
  for (int i = 0; i < NUM_AUDIO_FILES; i++) {
    buttons += "<button onclick=\"playAudio(" + String(i) + ")\">Play Audio " + String(i + 1) + "</button>\n";
  }
  page.replace("%BUTTONS%", buttons);
  server.send(200, "text/html", page);
}

void handlePlay() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Missing id");
    return;
  }
  // The page sends IDs 0..4. Reject empty/non-numeric IDs explicitly.
  String value = server.arg("id");
  if (value.length() != 1 || value[0] < '0' ||
      value[0] >= '0' + NUM_AUDIO_FILES) {
    server.send(400, "text/plain", "Invalid id");
    return;
  }
  int id = value[0] - '0';
  bool ok = startAudio(id);
  if (ok) {
    server.send(200, "text/plain", "Playing Audio " + String(id + 1));
  } else {
    server.send(500, "text/plain", "Could not start Audio " + String(id + 1));
  }
}

// =====================================================================
// Ultrasonic distance measurement
// =====================================================================
float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 20000); // 20ms timeout (~3.4m range)
  if (duration == 0) return -1; // no echo / out of range
  return duration * 0.0343 / 2.0; // convert to cm
}

// =====================================================================
// Servo wave state machine (non-blocking)
// =====================================================================
void startWave() {
  waving = true;
  waveStartTime = millis();
  lastServoStepTime = millis();
  servoDirection = 1;
}

void updateWave() {
  if (!waving) return;

  unsigned long now = millis();

  if (now - waveStartTime >= WAVE_DURATION_MS) {
    waving = false;
    waveServo.write(90); // return to center
    return;
  }

  if (now - lastServoStepTime >= SERVO_STEP_INTERVAL_MS) {
    lastServoStepTime = now;
    servoAngle += servoDirection * 15;
    if (servoAngle >= SERVO_MAX_ANGLE) { servoAngle = SERVO_MAX_ANGLE; servoDirection = -1; }
    if (servoAngle <= SERVO_MIN_ANGLE) { servoAngle = SERVO_MIN_ANGLE; servoDirection = 1; }
    waveServo.write(servoAngle);
  }
}

// =====================================================================
// Setup
// =====================================================================
void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Audio (I2S)
  audioOutput.SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioOutput.SetGain(AUDIO_GAIN);
  audioOutput.SetBuffers(8, 1024);  // About 46 ms at 44.1 kHz stereo/16-bit

  // Ultrasonic sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Servo
  waveServo.setPeriodHertz(50);
  waveServo.attach(SERVO_PIN, 500, 2400);
  waveServo.write(90);

  // Web server routes
  server.on("/", handleRoot);
  server.on("/play", handlePlay);
  server.begin();
  Serial.println("HTTP server started");
}

// =====================================================================
// Loop
// =====================================================================
void loop() {
  server.handleClient();
  serviceAudio();           // decode the selected header array into I2S
  updateWave();              // advances the servo wave animation if active

  // Original distance measurement: bounded wait of up to 20 ms
  unsigned long now = millis();
  if (now - lastSensorCheck >= SENSOR_INTERVAL_MS) {
    lastSensorCheck = now;
    float dist = readDistanceCM();
    if (dist > 0 && dist < DETECT_DISTANCE_CM && !waving) {
      Serial.printf("Person detected at %.1f cm - waving!\n", dist);
      startWave();
    }
  }
}

