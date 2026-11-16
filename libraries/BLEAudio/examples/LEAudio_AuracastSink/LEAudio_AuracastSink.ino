/*
 * LE Audio -- Auracast sink (receiver)
 *
 * A portable Auracast speaker: it scans for a broadcast, syncs to its
 * periodic advertising and BIG, and plays the LC3 audio on an I2S DAC.
 *
 *   - BAP Broadcast Sink with two BISes (front left + front right)
 *   - BASS Scan Delegator, so a phone app or a BASS client can pick a source
 *     remotely
 *   - LC3 player decoding both BISes to the DAC
 *
 * By default it syncs to the broadcast named TARGET_NAME; leave it empty to
 * join the first source found. For an encrypted broadcast set BROADCAST_CODE
 * to the same code the source uses.
 *
 * Requires an LE Audio-capable target (ESP32-S31) and the LC3 codec
 * (esp_audio_codec). On the ESP32-S31 Function CoreBoard 1 the onboard ES8311
 * DAC feeds the NS4150B speaker amplifier; the board variant supplies the I2S
 * and PA pins.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "board_audio.h"

// ESP32-S31 Function CoreBoard 1: ES8311 codec -> NS4150B speaker amplifier.
// The pin macros come from the board variant.

static const char *DEVICE_NAME = "LE Audio Auracast Sink";
static const char *TARGET_NAME = "ESP32 Auracast";  // empty = first source found
static const char *BROADCAST_CODE = "";             // up to 16 characters; empty = unencrypted
static const uint8_t STREAMS = 2;                   // 1 = mono, 2 = stereo

BLEAudio audio;
BLEAudioBroadcastSink sink;
BLEAudioPlayer player;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Auracast Sink ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  // No explicit init: the engine initializes lazily on the first create*()
  // below. Create every role first, then call audio.begin() once to commit.

  sink = audio.createBroadcastSink();
  if (!sink) {
    halt("createBroadcastSink", BTStatus::NotSupported);
  }
  sink.configureAuracast(TARGET_NAME, STREAMS);
  if (BROADCAST_CODE[0] != '\0') {
    sink.setBroadcastCode(BROADCAST_CODE);
  }
  sink.onSourceFound([](const BLEAudioBroadcastSourceInfo &source) {
    Serial.printf("[scan] found \"%s\" id 0x%06lX (sid %u, %d dBm)\n", source.name.c_str(), (unsigned long)source.broadcastId,
                  source.sid, source.rssi);
  });
  sink.onBaseReceived([](const BLEAudioBroadcastBaseInfo &base) {
    Serial.printf("[BASE] %u subgroup(s), %lu us, %lu Hz, %u octets x %u ch\n", base.subgroups,
                  (unsigned long)base.presentationDelayUs, (unsigned long)base.codec.samplingRateHz, base.codec.octetsPerFrame,
                  base.codec.channels());
  });
  sink.onSyncFailed([](BTStatus status) {
    Serial.printf("[sync] failed: %s\n", status.toString());
  });
  sink.onStarted([]() {
    Serial.println("[BIG] joined, audio is streaming");
  });
  sink.onStopped([](uint8_t reason) {
    Serial.printf("[BIG] left (reason 0x%02X)\n", reason);
  });

  // audio.begin() commits the roles and publishes PACS/BASS. I2S and the
  // LC3 task are started after it; the broadcast is joined later by sink.start().
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // Configure the onboard ES8311 (I2C), then enable the NS4150B amplifier.
  // The codec follows the I2S clocks, so it works at every LC3 sample rate.
  if (!board_audio::es8311Begin()) {
    Serial.println("ES8311 init failed; check the codec I2C bus");
  }
  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);
  st = player.begin(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
  if (!st) {
    halt("player.begin", st);
  }
  // One argument handles a mono or one stereo stream; two arguments are for
  // two separate mono streams (left, right).
  st = STREAMS == 2 ? player.attach(sink.stream(0), sink.stream(1)) : player.attach(sink.stream(0));
  if (!st) {
    halt("player.attach", st);
  }
  st = player.startPlayback();
  if (!st) {
    halt("player.startPlayback", st);
  }

  // Advertise connectably so a phone app can reach BASS and change the source.
  BLEAdvertising adv = BLE.getAdvertising();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName(DEVICE_NAME);
  adv.setAppearance(BLE_APPEARANCE_AUDIO_SINK_STANDALONE_SPEAKER);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  st = sink.start();
  if (!st) {
    halt("sink.start", st);
  }

  Serial.printf("Scanning for \"%s\" (%s).\n", TARGET_NAME[0] ? TARGET_NAME : "any broadcast",
                BROADCAST_CODE[0] ? "encrypted" : "open");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    if (player.sampleRate()) {
      Serial.printf("[player] %lu Hz x %u ch, %lu SDUs, %lu lost, %lu PLC, %lu underruns\n", (unsigned long)player.sampleRate(),
                    player.channels(), (unsigned long)player.sdusReceived(), (unsigned long)player.sdusLost(),
                    (unsigned long)player.plcFrames(), (unsigned long)player.underruns());
    }
  }
  delay(100);
}
