/*
 * LE Audio -- BAP Broadcast Assistant
 *
 * Acts like the "remote control" of an Auracast receiver: connects to a
 * Broadcast Sink that exposes the Scan Delegator (BASS), scans for broadcast
 * sources on its behalf and tells it which one to receive. The sink's receive
 * state (PA / BIS sync, encryption) is printed as it changes; when the source
 * is encrypted, the Broadcast Code below is sent. If the sink asks for the
 * periodic-advertising SyncInfo and this board is synced to the source, the
 * sync is handed over with PAST automatically.
 *
 * Pair it with LEAudio_BroadcastSink (the sink) and LEAudio_BroadcastSource
 * (the broadcast), each on its own LE-Audio-capable board.
 *
 * Callback style: lambdas for discovery / results, named functions for
 * sources and receive states.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const char *SINK_NAME = "Arduino Broadcast Sink";
static const char *SOURCE_NAME = "";     // "" = first broadcast found
static const char *BROADCAST_CODE = "";  // for encrypted sources (up to 16 characters)

BLEAudio audio;
BLEAudioBroadcastAssistant assistant;

BTAddress sinkAddress;
volatile bool doConnect = false;
volatile bool connected = false;
BLEAudioBroadcastSourceInfo chosen;
volatile bool doAdd = false;
bool added = false;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void onDeviceFound(BLEAdvertisedDevice device) {
  // The remote scan reports every advertiser here too, the connected sink included.
  if (connected || doConnect || device.getName() != SINK_NAME) {
    return;
  }
  Serial.printf("Found sink at %s\n", device.getAddress().toString().c_str());
  sinkAddress = device.getAddress();
  doConnect = true;
}

void onLinkReady(uint16_t connHandle) {
  Serial.printf("Link %u ready, discovering BASS\n", connHandle);
  connected = true;
  BTStatus st = assistant.discover(connHandle);
  if (!st) {
    Serial.printf("discover failed: %s\n", st.toString());
  }
}

void onLinkLost(uint16_t connHandle) {
  Serial.printf("Link %u lost, scanning for the sink again\n", connHandle);
  added = false;
  connected = false;
  doConnect = false;
  BLE.getScan().setActiveScan(true);
  BLE.getScan().start(0);
}

void onSourceFound(const BLEAudioBroadcastSourceInfo &src) {
  Serial.printf("Source \"%s\" id 0x%06lX at %s (sid %u, %d dBm)\n", src.name.c_str(), (unsigned long)src.broadcastId,
                src.address.toString().c_str(), src.sid, src.rssi);
  if (added || doAdd || (SOURCE_NAME[0] != '\0' && src.name != SOURCE_NAME)) {
    return;
  }
  chosen = src;
  doAdd = true;
}

void onReceiveState(const BLEAudioReceiveState &st) {
  if (st.removed) {
    Serial.printf("Source %u removed\n", st.sourceId);
    return;
  }
  static const char *const pa[] = {"not synced", "SyncInfo requested", "synced", "failed", "no PAST"};
  static const char *const enc[] = {"none", "code required", "decrypting", "bad code"};
  Serial.printf("Source %u (id 0x%06lX): PA %s, encryption %s, BIS 0x%08lX\n", st.sourceId, (unsigned long)st.broadcastId,
                pa[(uint8_t)st.paState % 5], enc[(uint8_t)st.encryption % 4], (unsigned long)(st.numSubgroups ? st.bisSync[0] : 0));
  if (st.encryption == BLEAudioReceiveState::Encryption::CodeRequired && BROADCAST_CODE[0] != '\0') {
    assistant.setBroadcastCode(st.sourceId, BROADCAST_CODE);
  }
  if (st.paState == BLEAudioReceiveState::PaState::Synced) {
    assistant.stopRemoteScan();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio Broadcast Assistant ===");

  BTStatus st = BLE.begin("Arduino Broadcast Assistant");
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

  assistant = audio.createBroadcastAssistant();
  if (!assistant) {
    halt("createBroadcastAssistant", BTStatus::NotSupported);
  }
  assistant
    .onDiscovered([](BTStatus status, uint8_t receiveStates) {
      if (!status) {
        Serial.printf("No BASS on the sink: %s\n", status.toString());
        return;
      }
      Serial.printf("Sink has %u receive state(s), scanning for sources\n", receiveStates);
      BTStatus s = assistant.startRemoteScan();
      if (!s) {
        Serial.printf("startRemoteScan failed: %s\n", s.toString());
      }
    })
    .onSourceFound(onSourceFound)
    .onReceiveState(onReceiveState)
    .onResult([](BLEAudioBroadcastAssistant::Op op, BTStatus status) {
      if (!status) {
        Serial.printf("Operation %u failed: %s\n", (unsigned)op, status.toString());
      }
    });

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", SINK_NAME);
}

void loop() {
  if (doConnect) {
    doConnect = false;
    BLE.getScan().stop();
    BTStatus st = audio.connect(sinkAddress);
    if (!st) {
      Serial.printf("connect failed: %s\n", st.toString());
      BLE.getScan().start(0);
    }
  }
  if (doAdd) {
    doAdd = false;
    BTStatus st = assistant.addSource(chosen);
    Serial.printf("Adding \"%s\" to the sink: %s\n", chosen.name.c_str(), st.toString());
    added = (bool)st;
  }
  delay(50);
}
