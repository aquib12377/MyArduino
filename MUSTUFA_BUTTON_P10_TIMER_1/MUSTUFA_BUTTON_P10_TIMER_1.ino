/*
  Ticker Arduino (NO INTERRUPTS):
  - 1st HIGH pulse on D2 -> START ticking (play TICK_T~1.WAV every 1s)
  - 2nd HIGH pulse on D2 -> STOP
  - Software debounce + rising-edge detection via digitalRead()

  WAV:
    - Put TICK_T~1.WAV in SD root (8.3 name). Mono, 8-bit unsigned PCM, ~16kHz.
*/

#include <SPI.h>
#include <SD.h>
#include <TMRpcm.h>

TMRpcm tmrpcm;

// ---------- Pins ----------
const uint8_t SIG_PIN    = 2;   // incoming control pulse (active HIGH)
const uint8_t SD_CS_PIN  = 4;   // SD card CS
const uint8_t SPK_PIN    = 9;   // TMRpcm audio out

// ---------- Files & audio ----------
const char*   TICK_FILE  = "TICK_T~1.WAV";  // change if your 8.3 name differs
const uint8_t VOLUME     = 5;               // 0..7
const bool    QUALITY_HQ = true;            // 1=better quality

// ---------- Debounce / timing ----------
const uint16_t DEBOUNCE_MS        = 30;     // input debounce
const uint16_t MIN_TOGGLE_GAP_MS  = 120;    // ignore edges too close together
const uint16_t TICK_INTERVAL_MS   = 1000;   // play tick every 1 second

// ---------- State ----------
bool playEnabled = false;
uint32_t lastToggleMs = 0;
uint32_t lastTickMs   = 0;

// Debounce state (for digitalRead only)
bool lastReading   = LOW;
bool stableState   = LOW;
uint32_t lastChangeMs = 0;

bool risingEdge() {
  bool reading = digitalRead(SIG_PIN);
  uint32_t now = millis();

  if (reading != lastReading) {
    lastReading = reading;
    lastChangeMs = now;           // potential change; start debounce timer
  }

  // if stable for long enough, accept the new state
  if ((now - lastChangeMs) > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == HIGH) {  // rising edge detected
        // Also enforce a small gap to avoid double toggles
        if (now - lastToggleMs >= MIN_TOGGLE_GAP_MS) {
          lastToggleMs = now;
          return true;
        }
      }
    }
  }
  return false;
}

void startTicking() {
  playEnabled = true;
  lastTickMs = millis();
  Serial.println(F("START -> ticking every 1s"));
}

void stopTicking() {
  playEnabled = false;
  tmrpcm.stopPlayback();
  if (tmrpcm.isPlaying()) tmrpcm.stopPlayback();
  Serial.println(F("STOP -> silence"));
}

void setup() {
  Serial.begin(9600);
  delay(150);

  pinMode(SIG_PIN, INPUT_PULLUP); // driven by other Arduino; share GND

  // Keep SPI in master mode (good practice on AVR)
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  // SD init
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("SD init FAILED"));
  } else {
    Serial.println(F("SD init OK"));
  }

  // Audio init
  tmrpcm.speakerPin = SPK_PIN;
  tmrpcm.setVolume(4);
  //tmrpcm.quality(QUALITY_HQ ? 1 : 0);

  // Initialize debounce states to current level
  lastReading  = digitalRead(SIG_PIN);
  stableState  = lastReading;
  lastChangeMs = millis();

  Serial.println(F("Ready. 1st pulse -> START, 2nd -> STOP."));
}

void loop() {
  // Edge-detect the control signal (no interrupts)
  if (risingEdge()) {
    if (!playEnabled) startTicking();
    else              stopTicking();
  }

  // If enabled, play the tick WAV once per second
  if (playEnabled) {
    uint32_t now = millis();
    if (now - lastTickMs >= TICK_INTERVAL_MS) {
      lastTickMs = now;

      // retrigger for a crisp attack
      if (tmrpcm.isPlaying()) tmrpcm.stopPlayback();

      if (SD.exists(TICK_FILE)) {
        tmrpcm.play((char*)TICK_FILE);
        // Optional: adjust playback speed if needed (depends on TMRpcm version)
        // tmrpcm.setPlaybackSpeed(1.0);
        // Serial.println(F("tick"));
      } else {
        Serial.println(F("File not found: TICK_T~1.WAV"));
      }
    }
  }

  // (Non-blocking loop; TMRpcm runs in background.)
}
