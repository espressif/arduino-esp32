/*
 * LE Audio -- BAP Unicast Server (acceptor / peripheral)
 *
 * Publishes PACS + ASCS, advertises connectably and logs the SDUs a Unicast
 * Client streams to it over a Connected Isochronous Stream (CIS). Each ASE the
 * client configures lands on one of the server's streams; its direction (Rx
 * for a sink ASE, Tx for a source ASE) is known once onConfigured() fires.
 *
 * This is a raw-SDU demo; see LEAudio_Player for LC3 decode + I2S output.
 * Pair it with LEAudio_UnicastClient on a second LE-Audio-capable board.
 *
 * Callback style: lambdas.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const char *DEVICE_NAME = "BAP Unicast Server";

BLEAudio audio;
BLEAudioUnicastServer unicastServer;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast Server ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }

  // One sink ASE (receive) and one source ASE (send), stereo locations so a
  // client may put one channel on each of two CISes.
  unicastServer = audio.createUnicastServer();
  unicastServer.setSinkStreams(1)
    .setSourceStreams(1)
    .setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational)
    .setSourceContexts(BLEAudioContext::Conversational)
    .setSinkLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight)
    .setSourceLocation(BLEAudioLocation::FrontLeft);

  for (size_t i = 0; i < unicastServer.streamCount(); i++) {
    BLEAudioStream s = unicastServer.stream(i);
    s.onConfigured([](BLEAudioStream &stream) {
      BLEAudioCodecConfig c = stream.codecConfig();
      Serial.printf("[%s] configured: %lu Hz, %u us, %u octets x %u ch\n", stream.direction() == BLEAudioStream::Direction::Rx ? "rx" : "tx",
                    (unsigned long)c.samplingRateHz, c.frameDurationUs, c.octetsPerFrame, c.channels());
    });
    s.onStarted([](BLEAudioStream &stream) {
      Serial.printf("[%s] streaming on conn %u\n", stream.direction() == BLEAudioStream::Direction::Rx ? "rx" : "tx", stream.connHandle());
    });
    s.onStopped([](BLEAudioStream &, uint8_t reason) {
      Serial.printf("stream stopped (reason 0x%02X)\n", reason);
    });
    s.onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t len) {
      static uint32_t count = 0;
      if ((count++ % 100) == 0) {
        Serial.printf("[rx] SDU #%lu seq=%u len=%u %s\n", (unsigned long)count, info.seq, len, info.status == BLEAudioSduInfo::Status::Valid ? "ok" : "bad");
      }
    });
  }

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setName(DEVICE_NAME);
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }

  Serial.println("Ready. Waiting for a Unicast Client...");
}

void loop() {
  // Echo a counter pattern on the source stream while a client listens.
  BLEAudioStream tx = unicastServer.stream(BLEAudioStream::Direction::Tx);
  if (tx.isStreaming()) {
    static uint8_t sdu[40];
    static uint8_t n = 0;
    memset(sdu, n++, sizeof(sdu));
    tx.write(sdu, tx.codecConfig().sduOctets() < sizeof(sdu) ? tx.codecConfig().sduOctets() : sizeof(sdu));
    delay(tx.qos().sduIntervalUs / 1000);
  } else {
    delay(100);
  }
}
