// ============================================================================
//  I2S WAV Player — ESP32-S3 N16R8 + MAX98357A
//  Plays embedded 16-bit mono audio (trimmed & resampled to 16 kHz)
//  Arduino-ESP32 core v3.x required (uses ESP_I2S library)
// ============================================================================

#include <Arduino.h>
#include <ESP_I2S.h>

// ---- Pin assignments (ESP32-S3 N16R8 safe pins) ----
//      Avoiding strapping (0,3,45,46), USB (19,20), octal-PSRAM (33-37)
constexpr int8_t SPK_BCLK = 15;   // MAX98357A BCLK
constexpr int8_t SPK_LRC  = 7;    // MAX98357A LRC
constexpr int8_t SPK_DIN  = 16;   // MAX98357A DIN

// ---- Audio configuration ----
constexpr uint32_t SAMPLE_RATE       = 48000;   // Matches resampled audio
constexpr size_t   FRAMES_PER_WRITE  = 256;     // DMA chunk size

// ---- Audio playback state ----
enum PlayState {
  STATE_IDLE,
  STATE_PLAYING,
  STATE_FINISHED
};

PlayState playState = STATE_IDLE;
size_t audioIndex = 0;

// ---- Globals ----
I2SClass i2s;
int16_t  txBuffer[FRAMES_PER_WRITE];

// ============================================================================
//  Embedded Audio Data (from audio_data.h)
// ============================================================================
#include "audio_data.h"
// Provides: AUDIO_SAMPLES, audioData[AUDIO_SAMPLES]

// ============================================================================
//  Helpers
// ============================================================================

/**
 * Fill TX buffer from embedded audio data.
 * Returns true if more audio remains, false if finished.
 */
static bool fillAudioBuffer() {
  size_t remaining = AUDIO_SAMPLES - audioIndex;
  
  if (remaining == 0) {
    // End of audio — fill with silence
    memset(txBuffer, 0, sizeof(txBuffer));
    return false;
  }
  
  // Copy up to FRAMES_PER_WRITE samples
  size_t to_copy = (remaining < FRAMES_PER_WRITE) ? remaining : FRAMES_PER_WRITE;
  
  for (size_t i = 0; i < to_copy; ++i) {
    txBuffer[i] = pgm_read_word(&audioData[audioIndex + i]);
  }
  
  // Pad remainder with silence if we've hit EOF
  if (to_copy < FRAMES_PER_WRITE) {
    memset(&txBuffer[to_copy], 0, (FRAMES_PER_WRITE - to_copy) * sizeof(int16_t));
  }
  
  audioIndex += to_copy;
  
  return (audioIndex < AUDIO_SAMPLES);
}

/**
 * Start audio playback from the beginning.
 */
static void startPlayback() {
  audioIndex = 0;
  playState = STATE_PLAYING;
  Serial.println(F("[STATE] START → playing audio"));
}

/**
 * Stop audio playback and return to idle.
 */
static void stopPlayback() {
  audioIndex = 0;
  playState = STATE_IDLE;
  Serial.println(F("[STATE] STOP → idle"));
}

// ============================================================================
//  Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("[BOOT] I2S WAV Player — ESP32-S3 N16R8 + MAX98357A"));
  Serial.printf("[BOOT] Sample Rate: %u Hz, Embedded Audio: %u samples\n",
                SAMPLE_RATE, AUDIO_SAMPLES);
  Serial.printf("[BOOT] Duration: %.2f seconds\n", (float)AUDIO_SAMPLES / SAMPLE_RATE);
  Serial.printf("[BOOT] SPK pins: BCLK=%d, LRC=%d, DIN=%d\n",
                SPK_BCLK, SPK_LRC, SPK_DIN);
  Serial.println(F("[INFO] Serial commands: 'p' = play, 's' = stop, 'r' = restart"));

  // Configure I2S TX pins (MAX98357A)
  i2s.setPins(SPK_BCLK, SPK_LRC, SPK_DIN);

  if (!i2s.begin(I2S_MODE_STD,
                 SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO)) {
    Serial.println(F("[ERROR] I2S TX init failed — halting."));
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("[INIT] I2S TX ready"));
  Serial.println(F("[INIT] Ready. Send 'p' to play."));
  Serial.println();
}

// ============================================================================
//  Loop
// ============================================================================
void loop() {
  // Handle serial commands
  if (Serial.available()) {
    int cmd = Serial.read();
    if (cmd == 'p' || cmd == 'P') {
      startPlayback();
    } else if (cmd == 's' || cmd == 'S') {
      stopPlayback();
    } else if (cmd == 'r' || cmd == 'R') {
      stopPlayback();
      delay(100);
      startPlayback();
    }
  }

  // Playback state machine
  if (playState == STATE_PLAYING) {
    bool has_more = fillAudioBuffer();
    
    const size_t want    = sizeof(txBuffer);
    const size_t written = i2s.write((uint8_t *)txBuffer, want);
    
    if (written != want) {
      static uint32_t lastWarn = 0;
      if (millis() - lastWarn > 1000) {
        Serial.printf("[WARN] Short I2S write: %u / %u bytes\n",
                      (unsigned)written, (unsigned)want);
        lastWarn = millis();
      }
    }
    
    if (!has_more) {
      playState = STATE_FINISHED;
      Serial.printf("[INFO] Playback finished (%u samples played)\n", AUDIO_SAMPLES);
      Serial.println(F("[INFO] Send 'p' to play again."));
    }
  } 
  else if (playState == STATE_FINISHED) {
    // Wait for user command before restarting
    delay(10);
  }
  else {
    // STATE_IDLE — just wait
    delay(10);
  }
}
