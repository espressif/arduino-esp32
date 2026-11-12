/*
 * LE Audio -- BAP Broadcast Source (Auracast transmitter)
 *
 * Creates a Broadcast Source, announces it with extended advertising (Broadcast
 * Audio Announcement, Public Broadcast Announcement and Broadcast Name) plus
 * periodic advertising (BASE), and streams SDUs over a Broadcast Isochronous
 * Group (BIG). Any Broadcast Sink in range can sync; no connection is needed.
 *
 * Set CHANNELS to 2 for a stereo broadcast (one BIS per channel, front left +
 * front right).
 *
 * This is a raw-SDU demo; see LEAudio_Recorder for microphone + LC3 encode.
 * Pair it with LEAudio_BroadcastSink on a second LE-Audio-capable board.
 *
 * Callback style: named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const char *BROADCAST_NAME = "Arduino Auracast";
static const uint8_t CHANNELS = 1;

BLEAudio audio;
BLEAudioBroadcastSource source;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void onBroadcastStarted() {
  Serial.printf("Broadcasting \"%s\" (id 0x%06lX) on %u BIS\n", BROADCAST_NAME, (unsigned long)source.getBroadcastId(), (unsigned)source.streamCount());
}

void onBroadcastStopped(uint8_t reason) {
  Serial.printf("Broadcast stopped (reason 0x%02X)\n", reason);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Broadcast Source ===");

  BTStatus st = BLE.begin(BROADCAST_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  source = audio.createBroadcastSource();
  source.setPreset(BLEAudioCodecPreset::LC3_16_2_1)
    .setChannels(CHANNELS)
    .setName(BROADCAST_NAME)
    .setContext(BLEAudioContext::Media)
    .setPublicBroadcast(true)
    .onStarted(onBroadcastStarted)
    .onStopped(onBroadcastStopped);
  // Uncomment for an encrypted broadcast (sinks need the same code):
  // source.setBroadcastCode("0000");

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  st = source.start();
  if (!st) {
    halt("source.start", st);
  }
}

void loop() {
  if (!source.isStreaming()) {
    delay(100);
    return;
  }
  // One SDU per BIS every SDU interval (10 ms for LC3_16_2_1, 40 octets).
  static uint8_t sdu[40];
  static uint8_t n = 0;
  memset(sdu, n++, sizeof(sdu));
  for (size_t i = 0; i < source.streamCount(); i++) {
    source.stream(i).write(sdu, sizeof(sdu));
  }
  delay(10);
}
