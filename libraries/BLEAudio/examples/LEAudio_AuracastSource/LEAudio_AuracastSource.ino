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
 * (esp_audio_codec). Wire an I2S microphone pair or an I2S ADC line input to
 * the pins below. Listen with LEAudio_AuracastSink or any Auracast receiver.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

// ---- I2S input pinout (edit for your board) ----
#define MIC_BCLK 5  // bit clock   (BCLK / SCK)
#define MIC_WS   6  // word select (LRCLK / WS)
#define MIC_DIN  4  // data in     (SD on the microphone/ADC)

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
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

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
  st = recorder.begin(MIC_BCLK, MIC_WS, MIC_DIN);
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
