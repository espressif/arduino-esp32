/*
 * LE Audio -- CAP Initiator (phone / PC)
 *
 * Connects to a CAP acceptor, discovers it through CAP (CAS, then its PACS
 * and ASCS) and streams to it. Every 30 s it switches between a unicast
 * session to the acceptor and a broadcast of the same audio, both driven by
 * the one BLEAudioCapInitiator.
 *
 * Every discovered acceptor joins the next unicast session, so the two
 * earbuds of a coordinated set are started together: connect to both and let
 * onLinkReady() discover each of them.
 *
 * This is a raw-SDU demo (40 octets every 10 ms, the LC3_16_2_1 frame size);
 * see LEAudio_Recorder for real LC3 audio. Pair it with LEAudio_HearingAid on
 * a second LE-Audio-capable board.
 *
 * Callback style: a named function for discovery, lambdas for the session
 * events.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_SUPPORTED

static const char *TARGET_NAME = "LE Audio Hearing Aid";
static const uint32_t SWITCH_MS = 30000;

BLEAudio audio;
BLEAudioCapInitiator initiator;

BTAddress acceptorAddress;
volatile bool doConnect = false;
volatile bool acceptorReady = false;
volatile bool sessionEnded = false;

enum class Mode { Idle, Unicast, ToBroadcast, Broadcast, ToUnicast };
Mode mode = Mode::Idle;
uint32_t modeSince = 0;

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
  acceptorAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

void onAcceptorDiscovered(BTStatus status, const BLEAudioCapPeerInfo &peer) {
  if (!status) {
    Serial.printf("CAP discovery of link %u failed: %s\n", peer.connHandle, status.toString());
    return;
  }
  Serial.printf("Acceptor on link %u: %u sink / %u source ASEs%s\n", peer.connHandle, peer.sinkEndpoints, peer.sourceEndpoints,
                peer.coordinatedSet ? ", member of a coordinated set" : "");
  acceptorReady = true;
}

void enter(Mode m) {
  mode = m;
  modeSince = millis();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio CAP Initiator ===");

  BTStatus st = BLE.begin("CAP Initiator");
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }
  audio.onLinkReady([](uint16_t connHandle) {
    initiator.discover(connHandle);
  });
  audio.onDisconnected([](uint16_t connHandle) {
    Serial.printf("Link %u lost, scanning again\n", connHandle);
    acceptorReady = false;
    BLE.getScan().start(0);
  });

  initiator = audio.createCapInitiator();
  if (!initiator) {
    halt("createCapInitiator", BTStatus::NotSupported);
  }
  initiator.setPreset(BLEAudioCodecPreset::LC3_16_2_1)
    .setContext(BLEAudioContext::Media)
    .setBroadcastName("CAP Initiator Demo")
    .onDiscovered(onAcceptorDiscovered)
    .onUnicastStarted([](BTStatus status) {
      Serial.printf("Unicast start: %s\n", status.toString());
    })
    .onUnicastStopped([] {
      Serial.println("Unicast stopped");
      sessionEnded = true;
    })
    .onBroadcastStarted([] {
      Serial.printf("Broadcasting, Broadcast ID 0x%06lX\n", (unsigned long)initiator.getBroadcastId());
    })
    .onBroadcastStopped([](uint8_t reason) {
      Serial.printf("Broadcast stopped (reason 0x%02X)\n", reason);
      sessionEnded = true;
    });

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

// Switch sessions from loop(): the CAP procedures must not be started from
// inside the callbacks, which run on the Bluetooth host task.
void runSessions() {
  const bool expired = millis() - modeSince > SWITCH_MS;
  switch (mode) {
    case Mode::Idle:
      if (acceptorReady && initiator.startUnicast()) {
        enter(Mode::Unicast);
      }
      break;
    case Mode::Unicast:
      if (!acceptorReady) {
        enter(Mode::Idle);
      } else if (expired) {
        sessionEnded = false;
        enter(initiator.stopUnicast() ? Mode::ToBroadcast : Mode::Idle);
      }
      break;
    case Mode::ToBroadcast:
      if (sessionEnded) {
        enter(initiator.startBroadcast(1) ? Mode::Broadcast : Mode::Idle);
      }
      break;
    case Mode::Broadcast:
      if (expired) {
        sessionEnded = false;
        enter(initiator.stopBroadcast() ? Mode::ToUnicast : Mode::Idle);
      }
      break;
    case Mode::ToUnicast:
      if (sessionEnded) {
        enter(Mode::Idle);
      }
      break;
  }
}

void loop() {
  if (doConnect) {
    doConnect = false;
    BTStatus st = audio.connect(acceptorAddress);
    if (!st) {
      Serial.printf("connect failed: %s\n", st.toString());
      BLE.getScan().start(0);
    }
  }
  runSessions();

  // One SDU per 10 ms on every sink stream of the current session.
  static uint8_t sdu[40];
  static uint8_t n = 0;
  bool sent = false;
  memset(sdu, n++, sizeof(sdu));
  for (size_t i = 0; i < initiator.streamCount(); i++) {
    BLEAudioStream s = initiator.stream(i);
    if (s.direction() == BLEAudioStream::Direction::Tx && s.isStreaming()) {
      s.write(sdu, sizeof(sdu));
      sent = true;
    }
  }
  delay(sent ? 10 : 100);
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_CapInitiator requires the LE Audio engine (BLE_AUDIO_SUPPORTED) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
