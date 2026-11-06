/*
 * LE Audio -- CAP Acceptor (headset / speaker side)
 *
 * A Common Audio Profile acceptor that a phone, PC or another LE-Audio board
 * can drive as one device:
 *
 *   - CAP  acceptor (CAS + CSIS)            : one device of a set of one
 *   - BAP  unicast server, one sink ASE     : receives audio over a CIS
 *   - BAP  broadcast sink + Scan Delegator  : receives the broadcast an
 *                                             initiator/assistant selects
 *   - VCP  volume renderer, MICP mic device : remote volume / mic mute
 *   - MCP  media player, CCP call server    : media and call control targets
 *
 * The broadcast sink does not scan on its own (auto-sync off, not started):
 * it only syncs when a Broadcast Assistant or a CAP initiator adds a source
 * through BASS -- which is exactly what a CAP handover does when it moves a
 * unicast session to a broadcast.
 *
 * Pair it with LEAudio_CapHandover (unicast <-> broadcast handover) or
 * LEAudio_ControlProfiles (VCP/MICP/MCP/CCP) on a second LE-Audio-capable
 * board; both look for the name below. Received SDUs are counted per
 * transport and printed every 2 s. This is a raw-SDU demo; see LEAudio_Player
 * for LC3 decode + I2S output.
 *
 * Callback style: lambdas.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#if BLE_AUDIO_SUPPORTED

static const char *DEVICE_NAME = "LE Audio Acceptor";

BLEAudio audio;
BLEAudioCapAcceptor acceptor;
BLEAudioUnicastServer unicastServer;
BLEAudioBroadcastSink broadcastSink;
BLEAudioVolumeRenderer volume;
BLEAudioMicDevice mic;
BLEAudioMediaPlayer media;
BLEAudioCallServer call;

volatile uint32_t unicastSdus = 0;
volatile uint32_t broadcastSdus = 0;

void halt(const char *what, BTStatus st) {
  Serial.printf("%s failed: %s\n", what, st.toString());
  while (true) {
    delay(1000);
  }
}

// A role whose Kconfig option is off in this build comes back as an empty
// handle; the acceptor still runs with the others.
void report(const char *role, bool created) {
  if (!created) {
    Serial.printf("%s not available in this build, skipped\n", role);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LE Audio CAP Acceptor ===");

  BTStatus st = BLE.begin(DEVICE_NAME);
  if (!st) {
    halt("BLE.begin", st);
  }

  audio = BLE.getAudioController();
  st = audio.begin();
  if (!st) {
    halt("audio.begin", st);
  }
  audio.onLinkReady([](uint16_t connHandle) {
    Serial.printf("Link %u ready\n", connHandle);
  });
  audio.onDisconnected([](uint16_t connHandle) {
    // The server keeps advertising after a disconnect (advertiseOnDisconnect).
    Serial.printf("Link %u closed\n", connHandle);
  });

  acceptor = audio.createCapAcceptor();
  if (!acceptor) {
    halt("createCapAcceptor", BTStatus::NotSupported);
  }
  acceptor.setSetSize(1).setRank(1);

  // Unicast: one sink ASE, i.e. one stream the initiator sends to.
  unicastServer = audio.createUnicastServer();
  if (!unicastServer) {
    halt("createUnicastServer", BTStatus::NotSupported);
  }
  unicastServer.setSinkStreams(1).setSourceStreams(0).setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
  unicastServer.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
    if (info.status == BLEAudioSduInfo::Status::Valid) {
      unicastSdus = unicastSdus + 1;
    }
  });
  unicastServer.stream(0).onStarted([](BLEAudioStream &) {
    Serial.println("Unicast stream started");
  });
  unicastServer.stream(0).onStopped([](BLEAudioStream &, uint8_t reason) {
    Serial.printf("Unicast stream stopped (reason 0x%02X)\n", reason);
  });

  // Broadcast: only follows BASS requests (handover / assistant).
  broadcastSink = audio.createBroadcastSink();
  report("Broadcast sink", (bool)broadcastSink);
  if (broadcastSink) {
    broadcastSink.setScanDelegator(true)
      .setAutoSync(false)
      .onSynced([]() {
        Serial.println("Broadcast: PA synced (source selected over BASS)");
      })
      .onStarted([]() {
        Serial.println("Broadcast: BIG joined");
      })
      .onStopped([](uint8_t reason) {
        Serial.printf("Broadcast: BIG left (reason 0x%02X)\n", reason);
      })
      .onSyncFailed([](BTStatus status) {
        Serial.printf("Broadcast: BIG sync failed: %s\n", status.toString());
      });
    broadcastSink.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
      if (info.status == BLEAudioSduInfo::Status::Valid) {
        broadcastSdus = broadcastSdus + 1;
      }
    });
  }

  // Control profiles.
  volume = audio.createVolumeRenderer();
  report("Volume renderer", (bool)volume);
  if (volume) {
    volume.setInitialVolume(100);
    volume.onStateChanged([](uint8_t level, bool muted) {
      Serial.printf("Volume %u%s\n", level, muted ? " (muted)" : "");
    });
  }

  mic = audio.createMicDevice();
  report("Microphone device", (bool)mic);
  if (mic) {
    mic.onMuteChanged([](bool muted) {
      Serial.printf("Microphone %s\n", muted ? "muted" : "unmuted");
    });
  }

  media = audio.createMediaPlayer();
  report("Media player", (bool)media);

  call = audio.createCallServer();
  report("Call server", (bool)call);
  if (call) {
    call.setProviderName("ESP Acceptor");
    call.onOriginate([](uint8_t callIndex, const String &uri) -> bool {
      Serial.printf("Call %u requested to %s\n", callIndex, uri.c_str());
      return true;
    });
  }

  st = audio.start();
  if (!st) {
    halt("audio.start", st);
  }

  BLEServer server = BLE.createServer();
  server.advertiseOnDisconnect(true);

  BLEAdvertising adv = BLE.getAdvertising();
  adv.reset();
  adv.setType(BLEAdvType::ConnectableScannable);
  adv.setName(DEVICE_NAME);
  adv.setAppearance(0x0840);  // Generic Audio Sink
  st = adv.start();
  if (!st) {
    halt("advertising", st);
  }
  Serial.printf("Advertising as \"%s\"\n", DEVICE_NAME);
}

void loop() {
  static uint32_t lastUnicast = 0, lastBroadcast = 0;
  uint32_t u = unicastSdus, b = broadcastSdus;
  if (u != lastUnicast || b != lastBroadcast) {
    Serial.printf("SDUs received: unicast %lu, broadcast %lu\n", (unsigned long)u, (unsigned long)b);
    lastUnicast = u;
    lastBroadcast = b;
  }
  delay(2000);
}

#else  // !BLE_AUDIO_SUPPORTED

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("LEAudio_CapAcceptor requires the LE Audio engine (BLE_AUDIO_SUPPORTED) in this build.");
}

void loop() {
  delay(1000);
}

#endif /* BLE_AUDIO_SUPPORTED */
