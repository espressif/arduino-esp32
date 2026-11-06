/*
 * LE Audio -- Stereo unicast over two CISes (TMAP 1P_2CIS)
 *
 * The server publishes two sink ASEs located front left and front right. The
 * client configures one mono stream on each, so the left and right channels
 * travel on separate Connected Isochronous Streams of the same connection.
 * The same topology with two single-ASE earbuds (2P_2CIS) only needs
 * audioClient.addPeer() for each bud followed by audioClient.start().
 *
 * Flash this sketch on two LE-Audio-capable boards: one with ROLE_SERVER set
 * to 1 and the other with 0.
 *
 * Callback style: server uses lambdas, client uses named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#define ROLE_SERVER 1

static const char *SERVER_NAME = "Stereo Unicast Server";

BLEAudio audio;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

#if ROLE_SERVER

BLEAudioUnicastServer server;

void setupRole() {
  server = audio.createUnicastServer();
  server.setSinkStreams(2)
    .setSourceStreams(0)
    .setSinkContexts(BLEAudioContext::Media)
    .setSinkLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight);

  for (size_t i = 0; i < server.streamCount(); i++) {
    BLEAudioStream s = server.stream(i);
    s.onStarted([](BLEAudioStream &stream) {
      const bool left = static_cast<uint32_t>(stream.codecConfig().channelAllocation) & static_cast<uint32_t>(BLEAudioLocation::FrontLeft);
      Serial.printf("%s channel streaming\n", left ? "Left" : "Right");
    });
    s.onReceive([i](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *sdu, uint16_t len) {
      static uint32_t count[2] = {};
      if ((count[i]++ % 100) == 0) {
        Serial.printf("[stream %u] seq=%u len=%u first=0x%02X\n", (unsigned)i, info.seq, len, len ? sdu[0] : 0);
      }
    });
  }
}

void startRole() {
  BLEAdvertising adv = BLE.getAdvertising();
  adv.setName(SERVER_NAME);
  BTStatus st = adv.start();
  if (!st) {
    halt("advertising", st);
  }
  Serial.println("Waiting for the stereo client...");
}

void loopRole() {
  delay(1000);
}

#else

BLEAudioUnicastClient audioClient;
BTAddress serverAddress;
volatile bool doConnect = false;

void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() == SERVER_NAME) {
    serverAddress = device.getAddress();
    doConnect = true;
    BLE.getScan().stop();
  }
}

void onLinkReady(uint16_t connHandle) {
  audioClient.connect(connHandle);
}

void onStereoStarted() {
  Serial.printf("Streaming stereo on %u CIS\n", (unsigned)audioClient.streamCount());
}

void setupRole() {
  audio.onLinkReady(onLinkReady);
  audioClient = audio.createUnicastClient();
  audioClient.setPreset(BLEAudioCodecPreset::LC3_48_2_1).setSinkStreams(2).onStarted(onStereoStarted);
}

void startRole() {
  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", SERVER_NAME);
}

void loopRole() {
  if (doConnect) {
    doConnect = false;
    audio.connect(serverAddress);
  }
  if (!audioClient.isStreaming()) {
    delay(100);
    return;
  }
  // LC3_48_2_1: 100 octets per channel every 10 ms. Left SDUs carry 0x11, right 0x22.
  static uint8_t left[100], right[100];
  memset(left, 0x11, sizeof(left));
  memset(right, 0x22, sizeof(right));
  audioClient.stream(BLEAudioStream::Direction::Tx, 0).write(left, sizeof(left));
  audioClient.stream(BLEAudioStream::Direction::Tx, 1).write(right, sizeof(right));
  delay(10);
}

#endif

void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== LE Audio stereo unicast (%s) ===\n", ROLE_SERVER ? "server" : "client");

  BTStatus st = BLE.begin(ROLE_SERVER ? SERVER_NAME : "Stereo Unicast Client");
  if (!st) {
    halt("BLE.begin", st);
  }
  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }
  setupRole();
  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }
  startRole();
}

void loop() {
  loopRole();
}
