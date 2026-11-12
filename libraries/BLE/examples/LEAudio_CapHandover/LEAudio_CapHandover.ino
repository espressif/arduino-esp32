/*
 * LE Audio -- CAP Handover (unicast <-> broadcast)
 *
 * Streams unicast to a CAP acceptor, then every 20 s hands the running
 * session over to a broadcast and back. During the handover the initiator
 * tells the acceptor (through its Broadcast Audio Scan Service) which
 * broadcast to receive, so the audio moves from the CIS to a BIS without
 * the sketch restarting anything: the same sink stream handle keeps
 * carrying the SDUs. While on broadcast, other receivers can join too.
 *
 * The acceptor must also be a Broadcast Sink with a Scan Delegator (BASS),
 * besides its unicast server: run LEAudio_CapAcceptor on a second
 * LE-Audio-capable board. When it asks for the periodic advertising sync
 * info, the initiator sends it over the connection (PAST) if the host
 * supports it.
 *
 * This is a raw-SDU demo (40 octets every 10 ms, the LC3_16_2_1 frame size).
 *
 * Callback style: a named function for the handover result, lambdas for the
 * rest.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_SUPPORTED

static const char *TARGET_NAME = "LE Audio Acceptor";
static const uint32_t SWITCH_MS = 20000;

BLEAudio audio;
BLEAudioCapInitiator initiator;

BTAddress acceptorAddress;
volatile bool doConnect = false;
volatile bool acceptorReady = false;
volatile bool live = false;       // Streaming, on unicast or broadcast.
volatile bool onBroadcast = false;
volatile bool busy = false;       // A start or a handover is in flight.
volatile uint32_t since = 0;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

void onHandoverDone(BTStatus status, bool toBroadcast) {
  busy = false;
  since = millis();
  if (!status) {
    Serial.printf("Handover to %s failed: %s\n", toBroadcast ? "broadcast" : "unicast", status.toString());
    live = initiator.isUnicastStreaming() || initiator.isBroadcasting();
    return;
  }
  onBroadcast = toBroadcast;
  Serial.printf("Now on %s\n", toBroadcast ? "broadcast" : "unicast");
  if (toBroadcast) {
    Serial.printf("Other receivers can join Broadcast ID 0x%06lX\n", (unsigned long)initiator.getBroadcastId());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio CAP Handover ===");

  BTStatus st = BLE.begin("CAP Handover");
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
    .setBroadcastName("CAP Handover Demo")
    .onDiscovered([](BTStatus status, const BLEAudioCapPeerInfo &peer) {
      Serial.printf("CAP discovery of link %u: %s (%u sink ASEs)\n", peer.connHandle, status.toString(), peer.sinkEndpoints);
      acceptorReady = status && peer.sinkEndpoints > 0;
    })
    .onUnicastStarted([](BTStatus status) {
      busy = false;
      live = (bool)status;
      onBroadcast = false;
      since = millis();
      Serial.printf("Unicast start: %s\n", status.toString());
    })
    .onUnicastStopped([] {
      live = initiator.isBroadcasting();
    })
    .onBroadcastStopped([](uint8_t reason) {
      Serial.printf("Broadcast stopped (reason 0x%02X)\n", reason);
      live = false;
    })
    .onHandover(onHandoverDone);

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.onResult([](BLEAdvertisedDevice device) {
    if (device.getName() == TARGET_NAME) {
      acceptorAddress = device.getAddress();
      doConnect = true;
      BLE.getScan().stop();
    }
  });
  scan.start(0);
  Serial.printf("Scanning for \"%s\"...\n", TARGET_NAME);
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

  // Start and hand over from loop(), never from the host-task callbacks.
  if (!busy && !live && acceptorReady && millis() - since > 2000) {
    busy = (bool)initiator.startUnicast();
    since = millis();
  } else if (!busy && live && millis() - since > SWITCH_MS) {
    BTStatus st = onBroadcast ? initiator.handoverToUnicast() : initiator.handoverToBroadcast();
    Serial.printf("Handover to %s: %s\n", onBroadcast ? "unicast" : "broadcast", st.toString());
    busy = (bool)st;
    since = millis();
  }

  BLEAudioStream tx = initiator.stream(BLEAudioStream::Direction::Tx);
  if (tx.isStreaming()) {
    static uint8_t sdu[40];
    static uint8_t n = 0;
    memset(sdu, n++, sizeof(sdu));
    tx.write(sdu, sizeof(sdu));
    delay(10);
  } else {
    delay(100);
  }
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_CapHandover requires the LE Audio engine (BLE_AUDIO_SUPPORTED) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
