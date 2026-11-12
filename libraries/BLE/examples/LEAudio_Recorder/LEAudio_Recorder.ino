/*
 * LE Audio -- Recorder (I2S microphones -> LC3 encode -> Broadcast Source)
 *
 * Starts an Auracast broadcast (BAP Broadcast Source) with two BISes, left and
 * right, and hands both streams to a BLEAudioRecorder. The recorder captures
 * stereo PCM from an I2S microphone pair, LC3-encodes each channel and sends
 * one SDU per SDU interval on each BIS while the broadcast runs. Listen with
 * the LEAudio_BroadcastSink example or any Auracast receiver.
 *
 * For a single microphone use setChannels(1) and recorder.attach(source.stream(0)).
 *
 * Requires the LC3 codec (managed component esp_audio_codec) in the core; on
 * builds without it the sketch reports that and idles.
 *
 * Wire two I2S MEMS mics (e.g. INMP441, L/R pin low on one and high on the
 * other) sharing BCLK, WS and SD to the pins below.
 *
 * Callback style: lambdas and named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_LC3_SUPPORTED

// ---- I2S microphone pinout (edit for your board) ----
// Named MIC_* because some board variants already define I2S_* pins.
#define MIC_BCLK 5  // bit clock   (BCLK / SCK)
#define MIC_WS   6  // word select (LRCLK / WS)
#define MIC_DIN  4  // data in     (SD on the mics)

static const BLEAudioCodecPreset PRESET = BLEAudioCodecPreset::LC3_16_2_1;

BLEAudio audio;
BLEAudioBroadcastSource source;
BLEAudioRecorder recorder;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// Named-function callback.
void onLeftStarted(BLEAudioStream &stream) {
  Serial.printf("[bis] streaming, one SDU every %lu us\n", (unsigned long)stream.qos().sduIntervalUs);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Recorder (I2S -> LC3 -> Auracast) ===");

  BTStatus st = BLE.begin("LE Audio Recorder");
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  source = audio.createBroadcastSource();
  source.setPreset(PRESET).setChannels(2).setName("ESP32 Mic");

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEAudioStream left = source.stream(0);
  BLEAudioStream right = source.stream(1);
  left.onStarted(onLeftStarted);
  // Lambda callback.
  left.onStopped([](BLEAudioStream &, uint8_t reason) {
    Serial.printf("[bis] stopped (reason 0x%02X)\n", reason);
  });

  // The mics are clocked at the preset's sample rate once the BISes stream.
  BLEAudioI2sConfig i2s;
  i2s.bclk = MIC_BCLK;
  i2s.ws = MIC_WS;
  i2s.din = MIC_DIN;
  st = recorder.begin(i2s);
  if (!st) {
    halt("recorder.begin", st);
  }
  st = recorder.attach(left, right);
  if (!st) {
    halt("recorder.attach", st);
  }
  st = recorder.start();
  if (!st) {
    halt("recorder.start", st);
  }

  st = source.start();
  if (!st) {
    halt("source.start", st);
  }
  Serial.println("Broadcasting \"ESP32 Mic\"...");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (recorder.sampleRate()) {
      Serial.printf(
        "[recorder] %lu Hz x %u ch  sent %lu  errors %lu\n", (unsigned long)recorder.sampleRate(), recorder.channels(), (unsigned long)recorder.sdusSent(),
        (unsigned long)recorder.sendErrors()
      );
    }
  }
  delay(100);
}

#else  // !BLE_AUDIO_LC3_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println("\nLEAudio_Recorder requires the LC3 codec (esp_audio_codec) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_LC3_SUPPORTED */
