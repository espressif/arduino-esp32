/*
 * LE Audio -- Auracast source (transmitter)
 *
 * A venue/TV transmitter: it announces an Auracast broadcast and streams live
 * audio from an I2S input to every receiver in range.
 *
 *   - BAP Broadcast Source with two BISes (front left + front right)
 *   - Public Broadcast Announcement and Broadcast Name in the advertising
 *   - LC3 recorder encoding the I2S input into both BISes
 *
 * The broadcast is unencrypted by default. Set BROADCAST_CODE to encrypt it;
 * receivers then need the same code (see LEAudio_AuracastSink). For a mono
 * broadcast use CHANNELS = 1 and attach only source.stream(0).
 *
 * Requires an LE Audio-capable target (ESP32-S31) and the LC3 codec
 * (esp_audio_codec). On the ESP32-S31 Function CoreBoard 1 the onboard
 * microphone feeds the ES8311 ADC; the board variant supplies the I2S pins.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "board_audio.h"

// ESP32-S31 Function CoreBoard 1: onboard microphone -> ES8311 ADC.
// The pin macros come from the board variant.

static const char *BROADCAST_NAME = "ESP32 Auracast";
static const char *BROADCAST_CODE = "";  // up to 16 characters; empty = unencrypted
static const uint8_t CHANNELS = 2;       // 1 = mono, 2 = stereo

BLEAudio audio;
BLEAudioBroadcastSource source;
BLEAudioRecorder recorder;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Auracast Source ===");

  BTStatus st = BLE.begin("LE Audio Auracast Source");
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  // No audio.begin(): the engine initializes lazily on the first create*()
  // or audio.start() below. Call audio.begin() explicitly instead for early,
  // checked initialization.

  source = audio.createBroadcastSource();
  if (!source) {
    halt("createBroadcastSource", BTStatus::NotSupported);
  }
  source.configureAuracast(BROADCAST_NAME, CHANNELS);
  if (BROADCAST_CODE[0] != '\0') {
    source.setBroadcastCode(BROADCAST_CODE);
  }
  source.onStarted([]() {
    Serial.printf("[BIG] streaming \"%s\" (id 0x%06lX)\n", BROADCAST_NAME, (unsigned long)source.getBroadcastId());
  });
  source.onStopped([](uint8_t reason) {
    Serial.printf("[BIG] stopped (reason 0x%02X)\n", reason);
  });

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  // The recorder opens the input at the preset's sample rate once the BISes
  // stream; attach both channels (or only stream 0 for mono).
  // Configure the onboard ES8311 ADC; it follows the I2S clocks, so it works
  // at every LC3 sample rate.
  if (!board_audio::es8311Begin()) {
    Serial.println("ES8311 init failed; check the codec I2C bus");
  }
  st = recorder.begin(I2S_SCLK, I2S_LRCK, I2S_DIN, I2S_MCLK);
  if (!st) {
    halt("recorder.begin", st);
  }
  // One argument handles a mono or one stereo stream; two arguments are for
  // two separate mono streams (left, right).
  st = CHANNELS == 2 ? recorder.attach(source.stream(0), source.stream(1)) : recorder.attach(source.stream(0));
  if (!st) {
    halt("recorder.attach", st);
  }
  st = recorder.startCapture();
  if (!st) {
    halt("recorder.startCapture", st);
  }

  st = source.start();
  if (!st) {
    halt("source.start", st);
  }

  Serial.printf("Broadcasting \"%s\" (%u channel(s), %s).\n", BROADCAST_NAME, CHANNELS, BROADCAST_CODE[0] ? "encrypted" : "open");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (recorder.sampleRate()) {
      Serial.printf("[recorder] %lu Hz x %u ch, %lu SDUs sent, %lu errors\n", (unsigned long)recorder.sampleRate(), recorder.channels(),
                    (unsigned long)recorder.sdusSent(), (unsigned long)recorder.sendErrors());
    }
  }
  delay(100);
}
