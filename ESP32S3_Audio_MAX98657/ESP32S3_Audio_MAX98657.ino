#include <Arduino.h>
#include <ESP_I2S.h>
#include "audio.h"

// Arduino IDE: install a current 3.x "esp32 by Espressif Systems" core.
// Assumes a MAX98357A I2S amplifier (not a left-justified MAX98357B).
constexpr int PIN_BCLK = 4;
constexpr int PIN_LRC  = 5;
constexpr int PIN_DIN  = 6;  // ESP32 output -> amplifier DIN
constexpr int VOLUME_PERCENT = 60;  // 0 to 100
constexpr size_t BLOCK_SAMPLES = 256;

static_assert(VOLUME_PERCENT >= 0 && VOLUME_PERCENT <= 100,
              "Volume must be between 0 and 100");
static_assert(sizeof(audioData) > 0 && sizeof(audioData) % 2 == 0,
              "audio.h must contain signed 16-bit PCM samples");

I2SClass i2s;
int16_t stereoBuffer[BLOCK_SAMPLES * 2];
int16_t silenceBuffer[BLOCK_SAMPLES * 2] = {};

void haltPlayback() {
  i2s.end();
  Serial.println("I2S stopped. Check the wiring/settings, then reset.");
  while (true) delay(1000);
}

void writeAudioBytes(uint8_t *data, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    size_t written = i2s.write(data + sent, length - sent);
    if (written == 0) {
      Serial.println("I2S write failed.");
      haltPlayback();
    }
    sent += written;
  }
}

void playAudio() {
  Serial.println("Playing: Hello my name fayyaz takkar");

  for (size_t offset = 0; offset < audioDataSize; ) {
    size_t samples = (audioDataSize - offset) / 2;
    if (samples > BLOCK_SAMPLES) samples = BLOCK_SAMPLES;

    for (size_t n = 0; n < samples; ++n) {
      // Each little-endian pair of bytes is one signed 16-bit sample.
      uint16_t raw = uint16_t(audioData[offset]) |
                     (uint16_t(audioData[offset + 1]) << 8);
      int32_t sample = (raw & 0x8000) ? int32_t(raw) - 65536 : raw;
      int16_t scaled = int16_t(sample * VOLUME_PERCENT / 100);
      offset += 2;

      // Put the same sound in both slots for any amplifier channel choice.
      stereoBuffer[n * 2]     = scaled;
      stereoBuffer[n * 2 + 1] = scaled;
    }

    writeAudioBytes(reinterpret_cast<uint8_t *>(stereoBuffer),
                    samples * 2 * sizeof(int16_t));
  }

  // Queue 0.5 seconds of silence to let the speech drain from I2S DMA.
  for (size_t n = 0; n < AUDIO_SAMPLE_RATE / 2; n += BLOCK_SAMPLES) {
    writeAudioBytes(reinterpret_cast<uint8_t *>(silenceBuffer),
                    sizeof(silenceBuffer));
  }
  Serial.println("Finished. Send P in Serial Monitor to play again.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  i2s.setPins(PIN_BCLK, PIN_LRC, PIN_DIN);
  if (!i2s.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S initialization failed.");
    haltPlayback();
  }

  playAudio();  // Play once after power-on/reset.
}

void loop() {
  if (Serial.available()) {
    char command = Serial.read();
    if (command == 'p' || command == 'P') playAudio();
  }

  // Keep sending silence while idle; no old audio remains in DMA buffers.
  writeAudioBytes(reinterpret_cast<uint8_t *>(silenceBuffer),
                  sizeof(silenceBuffer));
  delay(1);
}
