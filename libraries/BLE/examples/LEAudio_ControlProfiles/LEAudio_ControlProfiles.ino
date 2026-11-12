/*
 * LE Audio -- Control Profiles (central / "phone")
 *
 * One generic sketch that drives the whole LE Audio control surface over a
 * single ACL connection, the way a phone controls a headset:
 *
 *   - CAP  (Common Audio Profile) : confirm the peer is a CAP acceptor (CAS)
 *   - VCP  (Volume Control)       : set the peer's absolute volume
 *   - MICP (Microphone Control)   : mute the peer's microphone
 *   - MCP  (Media Control)        : send Play to the peer's media player
 *   - CCP  (Call Control)         : originate a call on the peer's bearer
 *
 * It scans for an acceptor by name, connects through the audio controller,
 * waits until the engine has secured and discovered the link
 * (BLEAudio::onLinkReady), then discovers and exercises each profile as its
 * discovery completes. Pair it with LEAudio_CapAcceptor on a second
 * LE-Audio-capable board, or any CAP acceptor exposing these services under
 * the name below.
 *
 * Requires the LE Audio engine (BLE_AUDIO_SUPPORTED); on builds without it the
 * sketch self-reports and idles.
 *
 * Callback style: named functions for the discovery flow, lambdas for the
 * media command results and call states.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_SUPPORTED

static const char *TARGET_NAME = "LE Audio Acceptor";

BLEAudio audio;
BLEAudioCapInitiator capInitiator;
BLEAudioVolumeController volumeController;
BLEAudioMicController micController;
BLEAudioMediaController mediaController;
BLEAudioCallController callController;

BTAddress targetAddress;
volatile bool doConnect = false;
volatile bool doDiscover = false;
volatile uint16_t connHandle = 0;

// ---- Profile callbacks: each acts as soon as its discovery completes --------

void onCapDiscovered(BTStatus status, const BLEAudioCapPeerInfo &peer) {
  Serial.printf("[CAP ] discovery %s (coordinated set: %s, %u sink ASEs)\n", status.toString(), peer.coordinatedSet ? "yes" : "no",
                peer.sinkEndpoints);
}

void onVolumeDiscovered(BTStatus status, uint8_t vocsCount, uint8_t aicsCount) {
  Serial.printf("[VCP ] discovery %s (VOCS=%u AICS=%u)\n", status.toString(), vocsCount, aicsCount);
  if (status) {
    Serial.println("[VCP ] setting volume to 180/255");
    volumeController.setVolume(180);
  }
}

void onVolumeState(uint8_t volume, bool muted) {
  Serial.printf("[VCP ] peer volume=%u muted=%d\n", volume, muted);
}

void onMicDiscovered(BTStatus status, uint8_t aicsCount) {
  Serial.printf("[MICP] discovery %s (AICS=%u)\n", status.toString(), aicsCount);
  if (status) {
    Serial.println("[MICP] muting peer microphone");
    micController.mute();
  }
}

void onMicMute(bool muted) {
  Serial.printf("[MICP] peer mic muted=%d\n", muted);
}

void onMediaDiscovered(BTStatus status) {
  Serial.printf("[MCP ] discovery %s\n", status.toString());
  if (status) {
    Serial.println("[MCP ] sending Play");
    mediaController.play();
  }
}

void onMediaState(BLEAudioMediaState state) {
  Serial.printf("[MCP ] peer media state=%u\n", (unsigned)state);
}

void onCallDiscovered(BTStatus status, bool gtbsFound) {
  Serial.printf("[CCP ] discovery %s (GTBS: %s)\n", status.toString(), gtbsFound ? "yes" : "no");
  if (status) {
    Serial.println("[CCP ] originating call to tel:+15551234567");
    callController.originate("tel:+15551234567");
  }
}

void onCallResult(BLEAudioCallOperation op, BTStatus status, uint8_t callIndex) {
  Serial.printf("[CCP ] op=%u %s (callIndex=%u)\n", (unsigned)op, status.toString(), callIndex);
}

// ---- Scan / connect ---------------------------------------------------------

void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found \"%s\" at %s\n", TARGET_NAME, device.getAddress().toString().c_str());
  targetAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

// Host task: the link is encrypted and its GATT database is known.
void onLinkReady(uint16_t handle) {
  Serial.printf("Link %u ready\n", handle);
  connHandle = handle;
  doDiscover = true;
}

void onLinkLost(uint16_t handle) {
  Serial.printf("Link %u lost, scanning again\n", handle);
  doDiscover = false;
  BLE.getScan().start(0);
}

void connect() {
  BTStatus st = audio.connect(targetAddress);
  if (!st) {
    Serial.printf("connect failed: %s\n", st.toString());
    BLE.getScan().start(0);
  }
}

// Runs from loop(), never from the host-task callbacks.
void discoverProfiles() {
  // Discover each profile in turn. The per-profile onDiscovered callbacks fire
  // when each completes and perform a representative action. Spacing the
  // discoveries out keeps their GATT procedures from overlapping.
  Serial.println("Discovering control profiles...");
  capInitiator.discover(connHandle);
  delay(800);
  volumeController.discover(connHandle);
  delay(800);
  micController.discover(connHandle);
  delay(800);
  mediaController.discover(connHandle);
  delay(800);
  callController.discover(connHandle);
}

void haltWith(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== LE Audio Control Profiles (central) ===");

  BTStatus st = BLE.begin("LE Audio Controller");
  if (!st) {
    haltWith("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    haltWith("audio.begin", st);
  }
  audio.onLinkReady(onLinkReady);
  audio.onDisconnected(onLinkLost);

  // Create every controller role; they all share the one ACL connection.
  capInitiator = audio.createCapInitiator();
  capInitiator.onDiscovered(onCapDiscovered);

  volumeController = audio.createVolumeController();
  volumeController.onDiscovered(onVolumeDiscovered).onStateChanged(onVolumeState);

  micController = audio.createMicController();
  micController.onDiscovered(onMicDiscovered).onMuteChanged(onMicMute);

  mediaController = audio.createMediaController();
  mediaController.onDiscovered(onMediaDiscovered).onStateChanged(onMediaState).onCommandResult([](BLEAudioMediaCommand cmd, BTStatus result) {
    Serial.printf("[MCP ] command 0x%02x %s\n", (unsigned)cmd, result.toString());
  });

  callController = audio.createCallController();
  callController.onDiscovered(onCallDiscovered).onResult(onCallResult).onCallState([](uint8_t callIndex, BLEAudioCallState state) {
    Serial.printf("[CCP ] call %u state=%u\n", callIndex, (unsigned)state);
  });

  st = audio.start();
  if (!st) {
    haltWith("audio.start", st);
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
    connect();
  }
  if (doDiscover) {
    doDiscover = false;
    discoverProfiles();
  }
  delay(100);
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_ControlProfiles requires the LE Audio engine (BLE_AUDIO_SUPPORTED) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
