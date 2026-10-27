/*
 * LE Audio -- Top-Level Profiles (TMAP + GMAP identity)
 *
 * A headset that publishes its top-level LE Audio profile identity:
 *
 *   - TMAP (Telephony & Media Audio Profile): Call Terminal + Unicast Media
 *     Receiver -- takes calls and receives media.
 *   - GMAP (Gaming Audio Profile): Unicast Game Terminal -- the headset end of
 *     a low-latency game-audio link, with sink (game audio) and source (voice).
 *
 * These profiles do NOT move audio themselves; they publish a small TMAS/GMAS
 * identity service so a central (phone/console) can tell which top-level
 * roles this device plays. The audio flows through the CAP acceptor, unicast
 * server and VCP renderer that also come up here and that back those roles.
 * When a central connects, the sketch also reads the central's TMAP/GMAP
 * roles.
 *
 * Callback style: named functions.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_TMAP_SUPPORTED && BLE_AUDIO_GMAP_SUPPORTED

static const char *DEVICE_NAME = "LE Audio Terminal";

BLEAudio audio;
BLEAudioCapAcceptor capAcceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioVolumeRenderer volumeRenderer;
BLEAudioTmap tmap;
BLEAudioGmap gmap;

void haltWith(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void onVolumeChanged(uint8_t volume, bool muted) {
  Serial.printf("[VCP] local volume=%u muted=%d\n", volume, muted);
}

void onTmapDiscovered(uint16_t connHandle, BTStatus status, BLEAudioTmapRole roles) {
  if (!status) {
    Serial.printf("[TMAP] conn %u: no TMAS (%s)\n", connHandle, status.toString());
    return;
  }
  Serial.printf("[TMAP] conn %u roles 0x%04X%s%s\n", connHandle, (unsigned)roles, (roles & BLEAudioTmapRole::CallGateway) ? " CG" : "",
                (roles & BLEAudioTmapRole::UnicastMediaSender) ? " UMS" : "");
}

void onGmapDiscovered(uint16_t connHandle, BTStatus status, BLEAudioGmapRole roles, const BLEAudioGmapFeatures &features) {
  if (!status) {
    Serial.printf("[GMAP] conn %u: no GMAS (%s)\n", connHandle, status.toString());
    return;
  }
  Serial.printf("[GMAP] conn %u roles 0x%02X, UGG features 0x%02X\n", connHandle, (unsigned)roles, features.unicastGateway);
}

// The central's GATT database is known: read its top-level roles.
void onLinkReady(uint16_t connHandle) {
  BTStatus st = tmap.discover(connHandle);
  if (!st) {
    Serial.printf("TMAP discover failed: %s\n", st.toString());
  }
  st = gmap.discover(connHandle);
  if (!st) {
    Serial.printf("GMAP discover failed: %s\n", st.toString());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== LE Audio Top-Level Profiles (TMAP + GMAP) ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    haltWith("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    haltWith("audio.begin", st);
  }
  audio.onLinkReady(onLinkReady);

  // Underlying roles that satisfy the TMAP/GMAP prerequisites: a CAP acceptor
  // (CAS), a unicast server (PACS/ASCS sink + source) and a VCP renderer (VCS).
  capAcceptor = audio.createCapAcceptor();
  capAcceptor.setSetSize(1).setRank(1);

  unicastServer = audio.createUnicastServer();
  unicastServer.setSinkStreams(1)
    .setSourceStreams(1)
    .setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational)
    .setSourceContexts(BLEAudioContext::Conversational);

  volumeRenderer = audio.createVolumeRenderer();
  volumeRenderer.setInitialVolume(128).onStateChanged(onVolumeChanged);

  tmap = audio.createTmap();
  tmap.setRoles(BLEAudioTmapRole::CallTerminal | BLEAudioTmapRole::UnicastMediaReceiver).onDiscovered(onTmapDiscovered);

  BLEAudioGmapFeatures features;
  features.unicastTerminal = BLEAudioGmapFeatures::UgtSink | BLEAudioGmapFeatures::UgtSource;
  gmap = audio.createGmap();
  gmap.setRoles(BLEAudioGmapRole::UnicastGameTerminal).setFeatures(features).onDiscovered(onGmapDiscovered);

  // Single commit publishes CAS + PACS/ASCS + VCS + TMAS + GMAS.
  st = audio.start();
  if (!st) {
    haltWith("audio.start", st);
  }

  BLEAdvertising adv = BLE.getAdvertising();
  adv.setName(DEVICE_NAME);
  st = adv.start();
  if (!st) {
    haltWith("advertising", st);
  }

  Serial.println("Ready. Publishing TMAP (CT+UMR) and GMAP (UGT) identity.");
}

void loop() {
  delay(1000);
}

#else  // !(BLE_AUDIO_TMAP_SUPPORTED && BLE_AUDIO_GMAP_SUPPORTED)

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_TopProfiles requires TMAP + GMAP (CONFIG_BT_TMAP=y, CONFIG_BT_GMAP=y) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_TMAP_SUPPORTED && BLE_AUDIO_GMAP_SUPPORTED */
