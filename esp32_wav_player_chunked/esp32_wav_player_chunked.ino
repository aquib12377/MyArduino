// ============================================================================
//  I2S WAV Player — ESP32-S3 4MB + MAX98357A (Single Array)
//  Plays 6-second embedded audio from single array
//  Arduino-ESP32 core v3.x required (uses ESP_I2S library)
// ============================================================================

#include <Arduino.h>
#include <ESP_I2S.h>

// ---- Pin assignments ----
constexpr int8_t SPK_BCLK = 15;   // MAX98357A BCLK
constexpr int8_t SPK_LRC  = 7;    // MAX98357A LRC
constexpr int8_t SPK_DIN  = 16;   // MAX98357A DIN

// ---- Audio configuration ----
constexpr uint32_t SAMPLE_RATE       = 48000;   // Matches WAV file
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
//  Embedded Audio Data
// ============================================================================
#include "audio_bytes_6sec.h"
// Provides: AUDIO_BYTE_LENGTH, audioBytes[AUDIO_BYTE_LENGTH]

// ============================================================================
//  Helpers
// ============================================================================

/**
 * Fill TX buffer from embedded audio data.
 * Returns true if more audio remains, false if finished.
 */
static bool fillAudioBuffer() {
  size_t remaining = AUDIO_BYTE_LENGTH - audioIndex;
  
  if (remaining == 0) {
    memset(txBuffer, 0, sizeof(txBuffer));
    return false;
  }
  
  // Copy up to FRAMES_PER_WRITE samples (2 bytes each)
  size_t bytes_to_copy = (remaining < (FRAMES_PER_WRITE * 2)) ? remaining : (FRAMES_PER_WRITE * 2);
  
  for (size_t i = 0; i < bytes_to_copy; i += 2) {
    uint8_t lo = pgm_read_byte(&audioBytes[audioIndex + i]);
    uint8_t hi = pgm_read_byte(&audioBytes[audioIndex + i + 1]);
    int16_t sample = (int16_t)((hi << 8) | lo);  // Little-endian
    txBuffer[i / 2] = sample;
  }
  
  // Pad with silence if EOF
  size_t samples_filled = bytes_to_copy / 2;
  if (samples_filled < FRAMES_PER_WRITE) {
    memset(&txBuffer[samples_filled], 0, (FRAMES_PER_WRITE - samples_filled) * sizeof(int16_t));
  }
  
  audioIndex += bytes_to_copy;
  return (audioIndex < AUDIO_BYTE_LENGTH);
}

static void startPlayback() {
  audioIndex = 0;
  playState = STATE_PLAYING;
  Serial.println(F("[STATE] Playing..."));
}

static void stopPlayback() {
  audioIndex = 0;
  playState = STATE_IDLE;
  Serial.println(F("[STATE] Stopped"));
}

// ============================================================================
//  Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("[BOOT] I2S WAV Player — ESP32-S3 4MB"));
  Serial.printf("[BOOT] Audio: %u bytes, 48 kHz, 6.11 sec\n", AUDIO_BYTE_LENGTH);
  Serial.println(F("[INFO] Commands: p=play, s=stop, r=restart"));

  i2s.setPins(SPK_BCLK, SPK_LRC, SPK_DIN);

  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println(F("[ERROR] I2S init failed"));
    while (true) delay(1000);
  }
  Serial.println(F("[INIT] Ready. Send 'p' to play."));
  
  startPlayback();
}

// ============================================================================
//  Loop
// ============================================================================
void loop() {
 
}
