/*
 * LE Audio -- BAP Broadcast Sink (Auracast receiver)
 *
 * Registers PACS and the Scan Delegator (BASS), scans for Broadcast Sources,
 * syncs to the one named TARGET_NAME (periodic advertising -> BASE -> BIG) and
 * logs the SDUs it receives. It also advertises connectably, so a phone or
 * LEAudio_BroadcastAssistant can connect and pick the source through BASS.
 *
 * Set STREAMS to 2 to receive both BISes of a stereo broadcast.
 *
 * This is a raw-SDU demo; see LEAudio_Player for LC3 decode + I2S output.
 * Pair it with LEAudio_BroadcastSource on a second LE-Audio-capable board.
 *
 * Callback style: lambdas.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const char *TARGET_NAME = "Arduino Auracast";
static const uint8_t STREAMS = 1;

BLEAudio audio;
BLEAudioBroadcastSink sink;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Broadcast Sink ===");

  BTStatus st = BLE.begin("Arduino Broadcast Sink");
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  sink = audio.createBroadcastSink();
  sink.setStreams(STREAMS)
    .setLocation(STREAMS == 2 ? BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight : BLEAudioLocation::FrontLeft)
    .setTargetName(TARGET_NAME)
    .onSourceFound([](const BLEAudioBroadcastSourceInfo &src) {
      Serial.printf("Source \"%s\" id 0x%06lX rssi %d\n", src.name.c_str(), (unsigned long)src.broadcastId, src.rssi);
    })
    .onSynced([]() {
      Serial.println("Synced to periodic advertising");
    })
    .onSyncLost([](uint8_t reason) {
      Serial.printf("Sync lost (reason 0x%02X)\n", reason);
    })
    .onSyncFailed([](BTStatus status) {
      Serial.printf("BIG sync failed: %s\n", status.toString());
    })
    .onStarted([]() {
      Serial.printf("Receiving %u BIS\n", (unsigned)sink.streamCount());
    })
    .onStopped([](uint8_t reason) {
      Serial.printf("Reception stopped (reason 0x%02X)\n", reason);
    });

  for (size_t i = 0; i < sink.streamCount(); i++) {
    sink.stream(i).onReceive([i](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t len) {
      static uint32_t count[2] = {};
      if ((count[i]++ % 100) == 0) {
        Serial.printf("[bis %u] SDU #%lu seq=%u len=%u %s\n", (unsigned)i, (unsigned long)count[i], info.seq, len,
                      info.status == BLEAudioSduInfo::Status::Valid ? "ok" : "lost");
      }
    });
  }

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  st = sink.start();
  if (!st) {
    halt("sink.start", st);
  }
  Serial.printf("Scanning for broadcast \"%s\"...\n", TARGET_NAME);

#if BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED
  // Connectable advertising so a Broadcast Assistant (phone or
  // LEAudio_BroadcastAssistant) can find this sink by name and reach BASS.
  BLE.createServer().advertiseOnDisconnect(true);
  BLEAdvertising adv = BLE.getAdvertising();
  adv.reset();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName("Arduino Broadcast Sink");
  adv.setAppearance(0x0840);                      // Generic Audio Sink
  adv.addServiceUUID(BLEUUID((uint16_t)0x184F));  // Broadcast Audio Scan Service
  st = adv.start();
  if (!st) {
    Serial.printf("advertising failed: %s (assistants cannot connect)\n", st.toString());
  }
#endif
}

void loop() {
  delay(1000);
}
