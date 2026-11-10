/*
 * LE Audio -- Auracast source (transmitter)
 *
 * A venue transmitter: it announces an Auracast broadcast and streams live
 * audio from an I2S microphone pair to every receiver in range.
 *
 *   - BAP Broadcast Source with two BISes (front left + front right)
 *   - Public Broadcast Announcement and Broadcast Name in the advertising
 *   - LC3 recorder encoding the I2S microphones into both BISes
 *
 * The broadcast is unencrypted by default. Set BROADCAST_CODE to encrypt it;
 * receivers then need the same code (see LEAudio_AuracastSink). For a mono
 * broadcast use channels = 1 and attach only source.stream(0).
 *
 * Wire two I2S MEMS microphones (L/R pin low on one and high on the other) to
 * the pins below. Listen with LEAudio_AuracastSink or any Auracast receiver.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#if BLE_AUDIO_SUPPORTED

// ---- I2S microphone pinout (edit for your board) ----
#define MIC_BCLK 5  // bit clock   (BCLK / SCK)
#define MIC_WS   6  // word select (LRCLK / WS)
#define MIC_DIN  4  // data in     (SD on the microphones)

static const char *BROADCAST_NAME = "ESP32 Auracast";
static const char *BROADCAST_CODE = "";  // up to 16 characters; empty = unencrypted
static const uint8_t CHANNELS = 2;       // 1 = mono, 2 = stereo

#if BLE_AUDIO_LC3_SUPPORTED

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

  // The recorder opens the microphones at the preset's sample rate once the
  // BISes stream; attach both channels (or only stream 0 for mono).
  BLEAudioI2sConfig mic;
  mic.bclk = MIC_BCLK;
  mic.ws = MIC_WS;
  mic.din = MIC_DIN;
  st = recorder.begin(mic);
  if (!st) {
    halt("recorder.begin", st);
  }
  st = recorder.attach(source.stream(0), CHANNELS == 2 ? source.stream(1) : BLEAudioStream());
  if (!st) {
    halt("recorder.attach", st);
  }
  st = recorder.start();
  if (!st) {
    halt("recorder.start", st);
  }

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
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

#else  // !BLE_AUDIO_LC3_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_AuracastSource requires the LC3 codec (esp_audio_codec) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_LC3_SUPPORTED */

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_AuracastSource requires the LE Audio library (<BLEAudio.h>) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
