/*
 * LE Audio -- BAP Unicast Client (initiator / central)
 *
 * Scans for a BAP Unicast Server, connects through the audio controller and,
 * once the server's services are known, runs the BAP setup (discover ->
 * config -> QoS -> enable -> CIS -> start). It then streams SDUs to the
 * server's sink ASE and logs what arrives from its source ASE.
 *
 * This is a raw-SDU demo; see LEAudio_Recorder for microphone + LC3 encode.
 * Pair it with LEAudio_UnicastServer on a second LE-Audio-capable board.
 *
 * Callback style: named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const char *TARGET_NAME = "BAP Unicast Server";

BLEAudio audio;
BLEAudioUnicastClient audioClient;

BTAddress serverAddress;
volatile bool doConnect = false;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found %s at %s\n", TARGET_NAME, device.getAddress().toString().c_str());
  serverAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

// The server's GATT database is known: hand the link to the unicast client.
void onLinkReady(uint16_t connHandle) {
  Serial.printf("Link %u ready, starting BAP setup\n", connHandle);
  BTStatus st = audioClient.connect(connHandle);
  if (!st) {
    Serial.printf("BAP setup failed: %s\n", st.toString());
  }
}

void onLinkLost(uint16_t connHandle) {
  Serial.printf("Link %u lost, scanning again\n", connHandle);
  BLE.getScan().start(0);
}

void onPeerDiscovered(const BLEAudioUnicastPeerInfo &peer) {
  Serial.printf("Server has %u sink / %u source ASEs\n", peer.sinkEndpoints, peer.sourceEndpoints);
}

void onRx(BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t len) {
  static uint32_t count = 0;
  if ((count++ % 100) == 0) {
    Serial.printf("[rx] SDU #%lu seq=%u len=%u\n", (unsigned long)count, info.seq, len);
  }
}

void onStreamsStarted() {
  Serial.printf("%u stream(s) streaming\n", (unsigned)audioClient.streamCount());
  BLEAudioStream rx = audioClient.stream(BLEAudioStream::Direction::Rx);
  if (rx) {
    rx.onReceive(onRx);
  }
}

void onStreamsStopped() {
  Serial.println("Streams released");
}

void onSetupError(BLEAudioUnicastClient::Step step, uint8_t responseCode, uint8_t reason) {
  Serial.printf("ASCS step %u failed: rsp 0x%02X reason 0x%02X\n", (unsigned)step, responseCode, reason);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Unicast Client ===");

  BTStatus st = BLE.begin("BAP Unicast Client");
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }
  audio.onLinkReady(onLinkReady);
  audio.onDisconnected(onLinkLost);

  audioClient = audio.createUnicastClient();
  audioClient.setPreset(BLEAudioCodecPreset::LC3_16_2_1)
    .setContext(BLEAudioContext::Conversational)
    .setSinkStreams(1)
    .setSourceStreams(1)
    .onDiscovered(onPeerDiscovered)
    .onStarted(onStreamsStarted)
    .onStopped(onStreamsStopped)
    .onError(onSetupError);

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
}

void loop() {
  if (doConnect) {
    doConnect = false;
    BTStatus st = audio.connect(serverAddress);
    if (!st) {
      Serial.printf("connect failed: %s\n", st.toString());
      BLE.getScan().start(0);
    }
  }

  BLEAudioStream tx = audioClient.stream(BLEAudioStream::Direction::Tx);
  if (tx.isStreaming()) {
    static uint8_t sdu[40];  // LC3_16_2_1: 40 octets every 10 ms
    static uint8_t n = 0;
    memset(sdu, n++, sizeof(sdu));
    tx.write(sdu, sizeof(sdu));
    delay(10);
  } else {
    delay(100);
  }
}
