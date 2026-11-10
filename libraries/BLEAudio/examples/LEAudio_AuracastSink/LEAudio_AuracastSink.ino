/*
 * LE Audio -- Auracast sink (receiver)
 *
 * A portable Auracast receiver / speaker: it scans for a broadcast, syncs to
 * its periodic advertising and BIG, and plays the LC3 audio on an I2S DAC.
 *
 *   - BAP Broadcast Sink with two BISes (front left + front right)
 *   - BASS Scan Delegator, so a phone or LEAudio_BroadcastAssistant can pick
 *     a source remotely
 *   - LC3 player decoding both BISes to the DAC
 *
 * By default it syncs to the broadcast named TARGET_NAME; leave it empty to
 * join the first source found. For an encrypted broadcast set BROADCAST_CODE
 * to the same code the source uses.
 *
 * Wire an I2S DAC (e.g. PCM5102 or MAX98357A) to the pins below. Pair it with
 * LEAudio_AuracastSource or any Auracast transmitter.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#if BLE_AUDIO_SUPPORTED

// ---- I2S DAC pinout (edit for your board) ----
#define DAC_BCLK 5  // bit clock   (BCLK / SCK)
#define DAC_WS   6  // word select (LRCLK / WS)
#define DAC_DOUT 7  // data out    (DIN on the DAC)

static const char *DEVICE_NAME = "LE Audio Auracast Sink";
static const char *TARGET_NAME = "ESP32 Auracast";  // empty = first source found
static const char *BROADCAST_CODE = "";             // up to 16 characters; empty = unencrypted
static const uint8_t STREAMS = 2;                   // 1 = mono, 2 = stereo

BLEAudio audio;
BLEAudioBroadcastSink sink;

#if BLE_AUDIO_LC3_SUPPORTED
BLEAudioPlayer player;
#endif

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
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

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

#if BLE_AUDIO_LC3_SUPPORTED
  BLEAudioI2sConfig dac;
  dac.bclk = DAC_BCLK;
  dac.ws = DAC_WS;
  dac.dout = DAC_DOUT;
  st = player.begin(dac);
  if (!st) {
    halt("player.begin", st);
  }
  BLEAudioStream left = sink.stream(0);
  BLEAudioStream right = STREAMS == 2 ? sink.stream(1) : BLEAudioStream();
  st = player.attach(left, right);
  if (!st) {
    halt("player.attach", st);
  }
  st = player.start();
  if (!st) {
    halt("player.start", st);
  }
#else
  // Without LC3, count valid SDUs so the link can still be observed.
  for (size_t i = 0; i < sink.streamCount(); i++) {
    sink.stream(i).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
      if (info.status == BLEAudioSduInfo::Status::Valid) {
        Serial.println("[BIS] SDU received (LC3 codec not compiled in)");
      }
    });
  }
#endif

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  // Advertise connectably so a phone or Broadcast Assistant can reach BASS.
  BLEAdvertising adv = BLE.getAdvertising();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName(DEVICE_NAME);
  adv.setAppearance(0x0840);  // Generic Audio Sink
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
#if BLE_AUDIO_LC3_SUPPORTED
    if (player.sampleRate()) {
      Serial.printf("[player] %lu Hz x %u ch, %lu SDUs, %lu lost, %lu PLC, %lu underruns\n", (unsigned long)player.sampleRate(),
                    player.channels(), (unsigned long)player.sdusReceived(), (unsigned long)player.sdusLost(),
                    (unsigned long)player.plcFrames(), (unsigned long)player.underruns());
    }
#endif
  }
  delay(100);
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_AuracastSink requires the LE Audio library (<BLEAudio.h>) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
