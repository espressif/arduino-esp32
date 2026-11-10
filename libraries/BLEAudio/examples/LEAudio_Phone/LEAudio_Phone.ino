/*
 * LE Audio -- Phone (source and controller)
 *
 * The phone/PC side of a unicast headset or speaker:
 *
 *   - CAP initiator: discovers the acceptor and starts a unicast session
 *   - BAP unicast client: one sink stream to the peer + one source stream back
 *   - VCP / MICP / MCP / CCP controllers: volume, mic, media and call control
 *   - LC3 recorder: encodes PCM into the stream sent to the headset
 *
 * The PCM source is a synthetic 1 kHz tone so the example needs no microphone.
 * Replace it with your call/media decoder output in a real product. Without
 * the LC3 codec the sketch sends raw test SDUs instead, so the LE Audio link
 * still runs.
 *
 * Pair it with LEAudio_Headset or LEAudio_Speaker on a second LE-Audio-capable
 * board, or use it as a starting point for a phone/PC integration.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>

#if BLE_AUDIO_SUPPORTED

static const char *TARGET_NAME = "LE Audio Headset";

BLEAudio audio;
BLEAudioCapInitiator capInitiator;
BLEAudioVolumeController volumeController;
BLEAudioMicController micController;
BLEAudioMediaController mediaController;
BLEAudioCallController callController;

BTAddress peerAddress;
volatile bool doConnect = false;
volatile bool doDiscover = false;
volatile uint16_t connHandle = 0;
volatile bool capReady = false;

#if BLE_AUDIO_LC3_SUPPORTED
BLEAudioRecorder recorder;
bool recorderStarted = false;

// A deterministic 1 kHz tone (16 samples per cycle at 16 kHz).
size_t readTone(int16_t *pcm, size_t samples) {
  static uint32_t phase = 0;
  for (size_t i = 0; i < samples; i++) {
    float s = sinf((float)phase * 2.0f * 3.14159265f / 16.0f);
    pcm[i] = (int16_t)(s * 8000.0f);
    phase++;
  }
  return samples;
}
#endif

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// ---- CAP / BAP -------------------------------------------------------------

void onCapDiscovered(BTStatus status, const BLEAudioCapPeerInfo &peer) {
  Serial.printf("[CAP] discovery %s (%u sink / %u source ASEs, coordinated set: %s)\n", status.toString(), peer.sinkEndpoints,
                peer.sourceEndpoints, peer.coordinatedSet ? "yes" : "no");
  capReady = (bool)status;
}

void onUnicastStarted(BTStatus status) {
  Serial.printf("[CAP] unicast %s, %u stream(s)\n", status.toString(), (unsigned)capInitiator.streamCount());
  if (!status) {
    return;
  }
#if BLE_AUDIO_LC3_SUPPORTED
  BLEAudioStream tx = capInitiator.stream(BLEAudioStream::Direction::Tx);
  if (tx) {
    BTStatus st = recorder.attach(tx);
    if (st) {
      st = recorder.start();
    }
    recorderStarted = (bool)st;
    Serial.printf("[LC3] recorder %s\n", st.toString());
  }
#endif
}

void onUnicastStopped() {
  Serial.println("[CAP] unicast streams released");
#if BLE_AUDIO_LC3_SUPPORTED
  if (recorderStarted) {
    recorder.stop();
    recorderStarted = false;
  }
#endif
}

// ---- Control-profile discovery --------------------------------------------

void onVolumeDiscovered(BTStatus status, uint8_t offsetCount, uint8_t inputCount) {
  Serial.printf("[VCP] discovery %s (VOCS=%u AICS=%u)\n", status.toString(), offsetCount, inputCount);
  if (status) {
    volumeController.setVolume(180);
  }
}

void onMicDiscovered(BTStatus status, uint8_t inputCount) {
  Serial.printf("[MICP] discovery %s (AICS=%u)\n", status.toString(), inputCount);
  if (status) {
    micController.mute();
  }
}

void onMediaDiscovered(BTStatus status) {
  Serial.printf("[MCP] discovery %s\n", status.toString());
  if (status) {
    mediaController.play();
  }
}

void onCallDiscovered(BTStatus status, bool gtbsFound) {
  Serial.printf("[CCP] discovery %s (GTBS: %s)\n", status.toString(), gtbsFound ? "yes" : "no");
  if (status && gtbsFound) {
    callController.originate("tel:+15551234567");
  }
}

// ---- Scan / connect -------------------------------------------------------

void onDeviceFound(BLEAdvertisedDevice device) {
  if (device.getName() != TARGET_NAME) {
    return;
  }
  Serial.printf("Found \"%s\" at %s\n", TARGET_NAME, device.getAddress().toString().c_str());
  peerAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

void onLinkReady(uint16_t handle) {
  Serial.printf("Link %u ready, discovering profiles\n", handle);
  connHandle = handle;
  doDiscover = true;
}

void onLinkLost(uint16_t handle) {
  Serial.printf("Link %u lost, scanning again\n", handle);
  connHandle = 0;
  doDiscover = false;
  capReady = false;
  BLE.getScan().start(0);
}

// Discover each control profile in turn. Spacing them out keeps their GATT
// procedures from overlapping. Start the audio session only once they are
// done, so ASCS configuration does not race the profile discoveries.
void discoverProfiles(uint16_t handle) {
  capInitiator.discover(handle);
  delay(800);
  volumeController.discover(handle);
  delay(800);
  micController.discover(handle);
  delay(800);
  mediaController.discover(handle);
  delay(800);
  callController.discover(handle);
  delay(800);
  if (capReady) {
    BTStatus st = capInitiator.startUnicast();
    if (!st) {
      Serial.printf("[CAP] startUnicast failed: %s\n", st.toString());
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Phone ===");

  BTStatus st = BLE.begin("LE Audio Phone");
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

  // CAP initiator starts the unicast session on every discovered acceptor.
  capInitiator = audio.createCapInitiator();
  if (!capInitiator) {
    halt("createCapInitiator", BTStatus::NotSupported);
  }
  capInitiator.setPreset(BLEAudioCodecPreset::LC3_16_2_1)
    .setContext(BLEAudioContext::Conversational)
    .setSinkStreams(1)
    .setSourceStreams(1)
    .onDiscovered(onCapDiscovered)
    .onUnicastStarted(onUnicastStarted)
    .onUnicastStopped(onUnicastStopped);

  // Control clients share the same ACL connection as the audio session.
  volumeController = audio.createVolumeController();
  volumeController.onDiscovered(onVolumeDiscovered).onStateChanged([](uint8_t volume, bool muted) {
    Serial.printf("[VCP] peer volume=%u muted=%d\n", volume, muted);
  });

  micController = audio.createMicController();
  micController.onDiscovered(onMicDiscovered).onMuteChanged([](bool muted) {
    Serial.printf("[MICP] peer microphone muted=%d\n", muted);
  });

  mediaController = audio.createMediaController();
  mediaController.onDiscovered(onMediaDiscovered).onStateChanged([](BLEAudioMediaState state) {
    Serial.printf("[MCP] peer media state=%u\n", (unsigned)state);
  });

  callController = audio.createCallController();
  callController.onDiscovered(onCallDiscovered).onResult([](BLEAudioCallOperation op, BTStatus status, uint8_t callIndex) {
    Serial.printf("[CCP] op=%u %s (call %u)\n", (unsigned)op, status.toString(), callIndex);
  });

#if BLE_AUDIO_LC3_SUPPORTED
  recorder.setPcmSource(readTone);
  st = recorder.begin();
  if (!st) {
    halt("recorder.begin", st);
  }
#endif

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
    BTStatus st = audio.connect(peerAddress);
    if (!st) {
      Serial.printf("connect failed: %s\n", st.toString());
      BLE.getScan().start(0);
    }
  }

  if (doDiscover) {
    doDiscover = false;
    discoverProfiles(connHandle);
  }

#if !BLE_AUDIO_LC3_SUPPORTED
  // No LC3 codec in this build: send a raw pattern so the link still runs.
  BLEAudioStream tx = capInitiator.stream(BLEAudioStream::Direction::Tx);
  if (tx.isStreaming()) {
    uint8_t sdu[40] = {0};
    tx.write(sdu, sizeof(sdu));
    delay(10);
  } else {
    delay(100);
  }
#else
  delay(100);
#endif
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_Phone requires the LE Audio library (<BLEAudio.h>) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
