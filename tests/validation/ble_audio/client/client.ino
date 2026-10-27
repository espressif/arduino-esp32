// LE Audio validation test — CLIENT
// Phases: audio lifecycle (ack), ISO CIS/BIS transport, BAP unicast/broadcast,
//         LC3 loopback, control profiles, top-level profiles, hearing aid,
//         stereo broadcast, broadcast assistant, CAP handover, memory
//         release. Split out of the combined `ble` suite (see the server
//         sketch header). Each phase self-contains its own BLE.end()/begin()
//         and self-skips when the relevant feature is not compiled in.

#include <Arduino.h>
#include <BLE.h>
#include "esp_heap_caps.h"

// Phase 8 set control: CSIP coordinator lock/release plus CAP commander volume.
#define SET_CONTROL_TEST (BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED && BLE_AUDIO_CAP_COMMANDER_SUPPORTED && BLE_AUDIO_VCP_CONTROLLER_SUPPORTED)

String targetName;
BTAddress targetAddr;
volatile int currentPhase = 0;

// ISO transport state — written from the ISO host task, read from the phase code.
volatile bool isoCisClientConnected = false;
volatile uint32_t isoCisClientRxCount = 0;  // SDUs on the Peripheral-to-Central direction
volatile bool isoBisClientSynced = false;
volatile uint32_t isoBisRxCount = 0;

// BAP unicast client state (phase 4). txStreaming latches when the source
// stream reaches Streaming (CIS up); the SDU send loop counts transmitted SDUs.
volatile bool bapTxStreaming = false;
volatile bool bapPresetPublished = false;  // server's sink PACS accepts the preset in use
volatile bool bapBaseSeen = false;         // Broadcast Sink parsed the source's BASE

// BAP broadcast sink RX counter (phase 5) — written from the audio host task.
volatile uint32_t bapBcastRxCount = 0;

// Connection handle reported by BLEAudio::onLinkReady (GATT discovery done).
// Every connected audio phase opens its link with audioConnect() so the link
// is secured and discovered by the engine on both NimBLE and Bluedroid.
volatile uint16_t audioLinkConn = 0xFFFF;

// ========================= Phase coordination ================================

void checkSerial() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      buf.trim();
      if (buf.startsWith("START_PHASE_")) {
        int phase = buf.substring(12).toInt();
        if (phase > currentPhase) {
          currentPhase = phase;
          Serial.printf("[CLIENT] Phase %d started\n", phase);
        }
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

#if BLE_ISO_SUPPORTED
// Same ordering guarantee as server: apply START_PHASE_* before BLE callback side effects.
// Only the ISO phases poll the host while they wait.
static void syncPhaseFromHost() {
  checkSerial();
}
#endif

void waitForPhase(int n) {
  while (currentPhase < n) {
    checkSerial();
    delay(10);
  }
}

// Active-scan for the server by name for up to 15 s.
bool findTarget(BTAddress &addr) {
  BLEScan scn = BLE.getScan();
  scn.resetCallbacks();
  scn.clearResults();
  scn.setActiveScan(true);
  unsigned long sdl = millis() + 15000;
  while (millis() < sdl) {
    BLEScan::Results res = scn.startBlocking(3000);
    for (const auto &dev : res) {
      BLEAdvertisedDevice d = dev;
      if (d.getName() == targetName) {
        addr = d.getAddress();
        scn.clearResults();
        return true;
      }
    }
    scn.clearResults();
  }
  return false;
}

#if BLE_AUDIO_SUPPORTED
// Connect through the audio controller and wait until the peer's GATT database
// is known. Needs audio.onLinkReady() to store audioLinkConn.
uint16_t audioConnect(BLEAudio &audio, const BTAddress &addr) {
  audioLinkConn = 0xFFFF;
  if (!audio.connect(addr)) {
    return 0xFFFF;
  }
  for (int i = 0; i < 200 && audioLinkConn == 0xFFFF; i++) {
    delay(50);
  }
  return audioLinkConn;
}
#endif

void readName() {
  Serial.println("[CLIENT] Device ready for name");
  Serial.println("[CLIENT] Send name:");
  while (targetName.length() == 0) {
    if (Serial.available()) {
      targetName = Serial.readStringUntil('\n');
      targetName.trim();
    }
    delay(100);
  }
  Serial.printf("[CLIENT] Target: %s\n", targetName.c_str());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();

  // Baseline stack so the first phase's BLE.end()/begin() cycle has a live stack
  // to tear down; also keeps `status` in scope for the memory_release phase.
  BTStatus status = BLE.begin("BLE_CLT");

  // ===== Phase 1: LE Audio lifecycle (server-driven; client just acks) =====
  // audio_lifecycle is a single-device (server) engine test: the client only
  // acknowledges the phase so the two-DUT handshake stays in lockstep.
  waitForPhase(1);
#if BLE_AUDIO_SUPPORTED
  Serial.println("[CLIENT] Audio lifecycle OK");
#else
  Serial.println("[CLIENT] Audio not supported, skipping");
#endif
  Serial.println("[CLIENT] Phase1 audio done");

  // ===== Phase 2: ISO CIS transport integrity (client = CIS Central) =====
  // Scans for the server, opens the ACL, brings up the BLEIso transport,
  // creates the CIG + bidirectional CIS on that link, streams transparent SDUs
  // the server counts and counts the SDUs the server streams back. Self-skips when host ISO is not compiled in, so it only truly runs
  // on a dual-ISO (esp32s31<->esp32s31) bench. Restores a normal stack after.
  waitForPhase(2);
#if BLE_ISO_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    isoCisClientConnected = false;
    isoCisClientRxCount = 0;
    BTStatus st = BLE.begin("BLE_CLT_ISO");
    if (st) {
      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);
      BTAddress addr;
      bool found = false;
      unsigned long sdl = millis() + 15000;
      while (!found && millis() < sdl) {
        BLEScan::Results res = scn.startBlocking(3000);
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            addr = d.getAddress();
            found = true;
            break;
          }
        }
        scn.clearResults();
      }
      uint32_t sent = 0;
      if (found) {
        BLEClient client = BLE.createClient();
        BTStatus cs = client.connect(addr);
        if (cs) {
          BTStatus ib = BLEIso::begin();
          Serial.printf("[CLIENT] IsoCis transport: %s\n", ib.toString());
          BLEIso::CisParams params;
          params.returnSduSize = 120;  // bidirectional: must match the server
          BLEIso::Channel *cis = BLEIso::connectCis(client.getHandle(), params);
          if (cis) {
            cis->onConnected([](BLEIso::Channel &) {
                 syncPhaseFromHost();
                 isoCisClientConnected = true;
               })
              .onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
                if (info.valid) {
                  isoCisClientRxCount = isoCisClientRxCount + 1;
                }
              });
            unsigned long cdl = millis() + 15000;
            while (!isoCisClientConnected && millis() < cdl) {
              delay(50);
            }
            // The channel numbers the SDUs itself (send without a sequence number).
            uint8_t sdu[120];
            for (int i = 0; i < 200 && isoCisClientConnected; i++) {
              memset(sdu, (uint8_t)i, sizeof(sdu));
              if (cis->send(sdu, sizeof(sdu))) {
                sent++;
              }
              delay(10);
            }
            delay(1000);
            // The host refuses to deinit ISO with a live CIS: take it down first.
            if (cis->disconnect()) {
              unsigned long ddl = millis() + 3000;
              while (cis->isConnected() && millis() < ddl) {
                delay(50);
              }
            }
          } else {
            Serial.println("[CLIENT] IsoCis connect FAILED");
          }
          BLEIso::end();
          client.disconnect();
        } else {
          Serial.println("[CLIENT] IsoCis ACL connect FAILED");
        }
      } else {
        Serial.println("[CLIENT] IsoCis target not found");
      }
      Serial.printf(
        "[CLIENT] IsoCis result connected=%d sent=%lu received=%lu\n", (int)isoCisClientConnected, (unsigned long)sent, (unsigned long)isoCisClientRxCount
      );
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] IsoCis not supported");
#endif
  Serial.println("[CLIENT] Phase2 iso_cis done");

  // ===== Phase 3: ISO BIS transport integrity (client = BIS Receiver) =====
  // Arms a BIG sync, then syncs to the server's periodic train. The scan handler
  // forwards the BIGInfo report into the ISO engine, which issues the BIG sync;
  // received SDUs are counted. Needs host ISO + BLE5; self-skips otherwise.
  // Restores a normal stack afterward.
  waitForPhase(3);
#if BLE_ISO_SUPPORTED && BLE5_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    isoBisClientSynced = false;
    isoBisRxCount = 0;
    BTStatus st = BLE.begin("BLE_CLT_ISO2");
    if (st) {
      BTStatus ib = BLEIso::begin();
      Serial.printf("[CLIENT] IsoBis transport: %s\n", ib.toString());
      BLEIso::BigParams params;
      BLEIso::Channel *bis = BLEIso::syncBig(1, params);
      if (bis) {
        bis->onConnected([](BLEIso::Channel &) {
          syncPhaseFromHost();
          isoBisClientSynced = true;
        });
        bis->onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
          if (info.valid) {
            isoBisRxCount = isoBisRxCount + 1;
          }
        });
        bool found = false;
        BLEScan scan = BLE.getScan();
        scan.onResult([&](BLEAdvertisedDevice dev) {
          syncPhaseFromHost();
          if (!found && dev.getName() == targetName) {
            found = true;
            targetAddr = dev.getAddress();
            scan.createPeriodicSync(targetAddr, 1);
          }
        });
        scan.startExtended(30000);
        unsigned long deadline = millis() + 28000;
        while (millis() < deadline) {
          delay(100);
        }
        scan.stop();
      } else {
        Serial.println("[CLIENT] IsoBis sync arm FAILED");
      }
      BLEIso::end();
      Serial.printf("[CLIENT] IsoBis result synced=%d received=%lu\n", (int)isoBisClientSynced, (unsigned long)isoBisRxCount);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] IsoBis not supported");
#endif
  Serial.println("[CLIENT] Phase3 iso_bis done");

  // ===== Phase 4: BAP unicast transport (client = BAP Unicast Client) =====
  // Establishes the ACL to the unicast server, runs the full BAP setup
  // (discover -> config -> QoS -> enable -> CIS connect -> start) via the audio
  // Unicast Client, then streams transparent SDUs to the server's sink ASE and
  // reports the count sent. Needs the Unicast Client role; self-skips otherwise.
  // Restores a normal stack afterward.
  waitForPhase(4);
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    bapTxStreaming = false;
    bapPresetPublished = false;
    bool everStreamed = false;  // bapTxStreaming is cleared again by stop()
    uint32_t sent = 0;
    BTStatus st = BLE.begin("BLE_CLT_BAP");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BLEAudioUnicastClient uc = audio.createUnicastClient();
        uc.setPreset(BLEAudioCodecPreset::LC3_16_2_1).setSinkStreams(1);
        // PACS discovery: the server's sink PAC must cover the preset we configure.
        uc.onDiscovered([](const BLEAudioUnicastPeerInfo &peer) {
          bapPresetPublished = (peer.sinkPresets & BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1)) != 0;
        });
        uc.onStarted([]() {
          bapTxStreaming = true;
        });
        uc.onStopped([]() {
          bapTxStreaming = false;
        });
        if (audio.start()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              BTStatus bs = uc.connect(h);
              Serial.printf("[CLIENT] BapUnicast setup: %s\n", bs.toString());
              unsigned long cdl = millis() + 15000;
              while (!bapTxStreaming && millis() < cdl) {
                delay(50);
              }
              everStreamed = bapTxStreaming;
              if (bapTxStreaming) {
                BLEAudioStream tx = uc.stream(BLEAudioStream::Direction::Tx);
                uint8_t sdu[40];
                for (int i = 0; i < 300 && bapTxStreaming; i++) {
                  memset(sdu, (uint8_t)i, sizeof(sdu));
                  if (tx.write(sdu, sizeof(sdu))) {
                    sent++;
                  }
                  delay(10);
                }
                delay(1000);
              } else {
                Serial.println("[CLIENT] BapUnicast stream did not start");
              }
              uc.stop();
              for (int i = 0; i < 60 && uc.isStreaming(); i++) {
                delay(50);
              }
            } else {
              Serial.println("[CLIENT] BapUnicast ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] BapUnicast target not found");
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] BapUnicast result streaming=%d sent=%lu presets=%d\n", (int)everStreamed, (unsigned long)sent, (int)bapPresetPublished);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] BapUnicast not supported");
#endif
  Serial.println("[CLIENT] Phase4 bap_unicast done");

  // ===== Phase 5: BAP broadcast transport (client = BAP Broadcast Sink) =====
  // Scans for the server's Auracast broadcast, PA-syncs, syncs the BIG, and
  // counts the transparent SDUs streamed Source->Sink over the BIS. Needs the
  // Broadcast Sink role; self-skips otherwise. Restores a normal stack afterward.
  waitForPhase(5);
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    bapBcastRxCount = 0;
    bapBaseSeen = false;
    BTStatus st = BLE.begin("BLE_CLT_BCAST");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setTargetName(targetName);
        // The BASE on the periodic train must describe at least one BIS with a codec.
        sink.onBaseReceived([](const BLEAudioBroadcastBaseInfo &base) {
          bapBaseSeen = base.bisMask != 0 && base.codec.samplingRateHz != 0;
        });
        sink.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
          if (info.status == BLEAudioSduInfo::Status::Valid) {
            bapBcastRxCount = bapBcastRxCount + 1;
          }
        });
        if (audio.start() && sink.start()) {
          Serial.println("[CLIENT] BapBroadcast scanning");
          unsigned long deadline = millis() + 45000;
          while (millis() < deadline) {
            delay(100);
          }
          sink.stop();
        }
        audio.end();
      }
      Serial.printf("[CLIENT] BapBroadcast result received=%lu base=%d\n", (unsigned long)bapBcastRxCount, (int)bapBaseSeen);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] BapBroadcast not supported");
#endif
  Serial.println("[CLIENT] Phase5 bap_broadcast done");

  // ===== Phase 6: LC3 loopback (client = Unicast Client source + Recorder) =====
  // Connects to the Unicast Server, sets up one stream to its sink ASE and
  // hands that stream to a BLEAudioRecorder fed by a synthetic 1 kHz tone (PCM
  // source instead of I2S pins); the recorder LC3-encodes the PCM at the SDU
  // interval and streams the frames over the CIS. The server decodes them and
  // asserts the audio arrived (see the server's samples/meanamp result). Needs
  // the LC3 codec (BLE_AUDIO_LC3_SUPPORTED); self-skips otherwise.
  waitForPhase(6);
#if BLE_AUDIO_LC3_SUPPORTED && BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    bapTxStreaming = false;
    bool everStreamed = false;
    uint32_t encSent = 0, encErrors = 0;
    BTStatus st = BLE.begin("BLE_CLT_LC3");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BLEAudioUnicastClient uc = audio.createUnicastClient();
        uc.setPreset(BLEAudioCodecPreset::LC3_16_2_1).setSinkStreams(1);
        uc.onStarted([]() {
          bapTxStreaming = true;
        });
        uc.onStopped([]() {
          bapTxStreaming = false;
        });

        // A deterministic 1 kHz tone (16 samples/cycle at 16 kHz) so the server
        // sees a non-silent, well-defined signal after decode.
        BLEAudioRecorder recorder;
        recorder.setPcmSource([](int16_t *pcm, size_t maxSamples) -> size_t {
          static uint32_t ph = 0;
          for (size_t i = 0; i < maxSamples; i++) {
            float s = sinf((float)ph * 2.0f * 3.14159265f / 16.0f);
            pcm[i] = (int16_t)(s * 8000.0f);
            ph++;
          }
          return maxSamples;
        });

        if (audio.start() && recorder.begin()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              BTStatus bs = uc.connect(h);
              Serial.printf("[CLIENT] Lc3Loopback setup: %s\n", bs.toString());
              unsigned long cdl = millis() + 15000;
              while (!bapTxStreaming && millis() < cdl) {
                delay(50);
              }
              everStreamed = bapTxStreaming;
              if (bapTxStreaming) {
                // The client's streams exist once the setup started; the
                // recorder task then encodes + sends autonomously.
                BTStatus ra = recorder.attach(uc.stream(BLEAudioStream::Direction::Tx));
                BTStatus rs = ra ? recorder.start() : ra;
                Serial.printf("[CLIENT] Lc3Loopback recorder: %s\n", rs.toString());
                delay(4000);
                encSent = recorder.sdusSent();
                encErrors = recorder.sendErrors();
              } else {
                Serial.println("[CLIENT] Lc3Loopback stream did not start");
              }
              recorder.end();
              uc.stop();
              for (int i = 0; i < 60 && uc.isStreaming(); i++) {
                delay(50);
              }
            } else {
              Serial.println("[CLIENT] Lc3Loopback ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] Lc3Loopback target not found");
          }
        }
        recorder.end();
        audio.end();
      }
      Serial.printf("[CLIENT] Lc3Loopback result streaming=%d sent=%lu errors=%lu\n", (int)everStreamed, (unsigned long)encSent, (unsigned long)encErrors);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] Lc3Loopback not supported");
#endif
  Serial.println("[CLIENT] Phase6 lc3_loopback done");

  // ===== Phase 7: Control profiles (client = the "phone" controller) =====
  // Connects once to the server acceptor, then drives every control profile
  // over that single ACL: CAP initiator (CAS discovery), VCP controller
  // (discover + setVolume), MICP controller (discover + mute), MCP controller
  // (discover + play), and CCP controller (discover + originate). Each sub-step
  // is independently timed and its outcome recorded as a bit. Needs the VCP
  // controller and CAP initiator; self-skips otherwise. Restores a normal stack
  // afterward.
  waitForPhase(7);
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED && BLE_AUDIO_CAP_INITIATOR_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    int capDisc = 0, vcpOk = 0, micOk = 0, mediaOk = 0, callOk = 0;
    BTStatus st = BLE.begin("BLE_CLT_CTRL");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioCapInitiator capInit = audio.createCapInitiator();
        volatile bool capDone = false;
        capInit.onDiscovered([&capDone, &capDisc](BTStatus s2, const BLEAudioCapPeerInfo &) {
          capDisc = s2 ? 1 : 0;
          capDone = true;
        });

        BLEAudioVolumeController vc = audio.createVolumeController();
        volatile bool vcDisc = false;
        vc.onDiscovered([&vcDisc](BTStatus s2, uint8_t, uint8_t) {
          vcDisc = (bool)s2;
        });

        BLEAudioMicController mc = audio.createMicController();
        volatile bool micDisc = false;
        mc.onDiscovered([&micDisc](BTStatus s2, uint8_t) {
          micDisc = (bool)s2;
        });

        BLEAudioMediaController mediaCtl = audio.createMediaController();
        volatile bool mediaDisc = false;
        mediaCtl.onDiscovered([&mediaDisc](BTStatus s2) {
          mediaDisc = (bool)s2;
        });

        BLEAudioCallController callCtl = audio.createCallController();
        volatile bool callDisc = false;
        callCtl.onDiscovered([&callDisc](BTStatus s2, bool gtbs) {
          (void)gtbs;
          callDisc = (bool)s2;
        });

        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });

        if (audio.start()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              // CAP: confirm the peer is a Common Audio Profile acceptor (CAS, then its ASEs).
              capInit.discover(h);
              for (int i = 0; i < 100 && !capDone; i++) {
                delay(50);
              }

              // VCP: discover then set an absolute volume the server observes.
              vc.discover(h);
              for (int i = 0; i < 60 && !vcDisc; i++) {
                delay(50);
              }
              if (vcDisc && vc.setVolume(200)) {
                vcpOk = 1;
              }
              delay(300);

              // MICP: discover then mute the remote microphone.
              mc.discover(h);
              for (int i = 0; i < 60 && !micDisc; i++) {
                delay(50);
              }
              if (micDisc && mc.mute()) {
                micOk = 1;
              }
              delay(300);

              // MCP: discover then send Play to the reference media player.
              mediaCtl.discover(h);
              for (int i = 0; i < 60 && !mediaDisc; i++) {
                delay(50);
              }
              if (mediaDisc && mediaCtl.play()) {
                mediaOk = 1;
              }
              delay(300);

              // CCP: discover then originate a call on the peer's GTBS.
              callCtl.discover(h);
              for (int i = 0; i < 60 && !callDisc; i++) {
                delay(50);
              }
              if (callDisc && callCtl.originate("tel:123456")) {
                callOk = 1;
              }
              delay(500);
            } else {
              Serial.println("[CLIENT] ControlProfiles ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] ControlProfiles target not found");
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] ControlProfiles result cap=%d vcp=%d mic=%d media=%d call=%d\n", capDisc, vcpOk, micOk, mediaOk, callOk);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] ControlProfiles not supported");
#endif
  Serial.println("[CLIENT] Phase7 control_profiles done");

  // ===== Phase 8: Top-level profiles (client = TMAP / GMAP discoverer) =====
  // Connects to the server and discovers TMAS (roles must be CT|UMR) and GMAS
  // (roles must include UGT, with the UGT Sink feature). On the same link it
  // then exercises set control, when compiled in: the CSIP coordinator reads
  // the server's set (size 1), locks and releases it, and the CAP commander
  // discovers the acceptor (CAS + VCP) and sets its volume to 77.
  // Needs TMAP/GMAP compiled in; self-skips otherwise. Restores a normal stack after.
  waitForPhase(8);
#if BLE_AUDIO_TMAP_SUPPORTED && BLE_AUDIO_GMAP_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    int tmapRoles = -1, gmapRoles = -1, gmapUgt = -1;
#if SET_CONTROL_TEST
    int csipSize = -1, csipLocked = 0, cmdVolume = 0;
#endif
    BTStatus st = BLE.begin("BLE_CLT_TOP");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioTmap tmap = audio.createTmap();  // no local roles: client only
        volatile bool tmapDone = false;
        tmap.onDiscovered([&tmapDone, &tmapRoles](uint16_t, BTStatus s2, BLEAudioTmapRole roles) {
          tmapRoles = s2 ? (int)(uint16_t)roles : -1;
          tmapDone = true;
        });
        BLEAudioGmap gmap = audio.createGmap();  // no local roles: client only
        volatile bool gmapDone = false;
        gmap.onDiscovered([&gmapDone, &gmapRoles, &gmapUgt](uint16_t, BTStatus s2, BLEAudioGmapRole roles, const BLEAudioGmapFeatures &f) {
          gmapRoles = s2 ? (int)(uint8_t)roles : -1;
          gmapUgt = s2 ? f.unicastTerminal : -1;
          gmapDone = true;
        });
#if SET_CONTROL_TEST
        BLEAudioCoordinatedSetCoordinator csip = audio.createCoordinatedSetCoordinator();
        volatile bool csipDone = false, lockDone = false;
        volatile bool lockOk = false;
        csip.onDiscovered([&csipDone, &csipSize](BTStatus s2, const BLEAudioCoordinatedSetInfo &info) {
          csipSize = s2 ? info.setSize : -1;
          csipDone = true;
        });
        csip.onLock([&lockDone, &lockOk](BTStatus s2, bool locked) {
          (void)locked;
          lockOk = (bool)s2;
          lockDone = true;
        });
        BLEAudioCapCommander commander = audio.createCapCommander();
        volatile bool cmdDisc = false, cmdDone = false;
        commander.onDiscovered([&cmdDisc](BTStatus s2, uint16_t, bool, bool) {
          cmdDisc = (bool)s2;
        });
        commander.onResult([&cmdDone, &cmdVolume](BLEAudioCapCommander::Operation op, BTStatus s2) {
          if (op == BLEAudioCapCommander::Operation::Volume) {
            cmdVolume = s2 ? 1 : 0;
            cmdDone = true;
          }
        });
#endif
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });

        if (audio.start()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              tmap.discover(h);
              for (int i = 0; i < 60 && !tmapDone; i++) {
                delay(50);
              }
              gmap.discover(h);
              for (int i = 0; i < 60 && !gmapDone; i++) {
                delay(50);
              }
#if SET_CONTROL_TEST
              // CSIP: read the set, then lock and release it (the server counts both changes).
              csip.discover(h);
              for (int i = 0; i < 100 && !csipDone; i++) {
                delay(50);
              }
              if (csipSize > 0 && csip.lock()) {
                for (int i = 0; i < 60 && !lockDone; i++) {
                  delay(50);
                }
                csipLocked = lockOk ? 1 : 0;
                lockDone = false;
                if (csip.unlock()) {
                  for (int i = 0; i < 60 && !lockDone; i++) {
                    delay(50);
                  }
                }
              }
              // CAP commander: CAS + VCP discovery, then an absolute volume.
              commander.discover(h);
              for (int i = 0; i < 100 && !cmdDisc; i++) {
                delay(50);
              }
              if (cmdDisc && commander.setVolume(77)) {
                for (int i = 0; i < 60 && !cmdDone; i++) {
                  delay(50);
                }
              }
#endif
            } else {
              Serial.println("[CLIENT] TopProfiles ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] TopProfiles target not found");
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] TopProfiles result tmap=%d gmap=%d ugt=%d\n", tmapRoles, gmapRoles, gmapUgt);
#if SET_CONTROL_TEST
      Serial.printf("[CLIENT] SetControl result csip=%d locked=%d commander=%d\n", csipSize, csipLocked, cmdVolume);
#endif
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] TopProfiles not supported");
#endif
  Serial.println("[CLIENT] Phase8 top_profiles done");

  // ===== Phase 9: Hearing Access Service (client = controller) =====
  // Connects to the hearing-aid server, discovers its HAS, reads the preset
  // list, and switches the active preset to index 2. Records how many preset
  // records it read and whether the switch was accepted. Needs the HAS client
  // (BLE_AUDIO_HAS_CLIENT_SUPPORTED); self-skips otherwise. Restores a normal
  // stack afterward.
  waitForPhase(9);
#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    int readCount = 0, switched = 0, discOk = 0;
    BTStatus st = BLE.begin("BLE_CLT_HA");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioHearingAidController hac = audio.createHearingAidController();
        volatile bool discDone = false;
        hac.onDiscovered([&discDone, &discOk](BTStatus s2, BLEHearingAidType type) {
          (void)type;
          discOk = s2 ? 1 : 0;
          discDone = true;
        });
        hac.onPreset([&readCount](uint8_t index, bool available, const String &name, bool isLast) {
          (void)index;
          (void)available;
          (void)name;
          (void)isLast;
          readCount = readCount + 1;
        });
        volatile bool switchDone = false;
        hac.onPresetSwitch([&switchDone, &switched](BTStatus s2, uint8_t index) {
          (void)index;
          switched = s2 ? 1 : 0;
          switchDone = true;
        });
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });

        if (audio.start()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              hac.discover(h);
              for (int i = 0; i < 60 && !discDone; i++) {
                delay(50);
              }
              if (discOk) {
                hac.readPresets(1, 255);
                delay(1500);  // let all preset records stream in
                if (hac.setActivePreset(2)) {
                  for (int i = 0; i < 60 && !switchDone; i++) {
                    delay(50);
                  }
                }
              }
              delay(300);
            } else {
              Serial.println("[CLIENT] HearingAid ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] HearingAid target not found");
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] HearingAid result disc=%d read=%d switched=%d\n", discOk, readCount, switched);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] HearingAid not supported");
#endif
  Serial.println("[CLIENT] Phase9 hearing_aid done");

  // ===== Phase 10: Stereo broadcast (client = 2-BIS Broadcast Sink) =====
  // Syncs both BISes of the server's encrypted stereo broadcast (local
  // Broadcast Code) and counts the SDUs on each, checking the per-channel
  // pattern (0x11 left, 0x22 right).
  waitForPhase(10);
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    static volatile uint32_t rx[2];
    static volatile uint32_t mismatched;
    rx[0] = 0;
    rx[1] = 0;
    mismatched = 0;
    BTStatus st = BLE.begin("BLE_CLT_STEREO");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setStreams(2)
          .setLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight)
          .setTargetName(targetName)
          .setBroadcastCode("ArduinoStereo");  // must match the server's STEREO_BROADCAST_CODE
        for (size_t i = 0; i < sink.streamCount() && i < 2; i++) {
          sink.stream(i).onReceive([](BLEAudioStream &s, const BLEAudioSduInfo &info, const uint8_t *sdu, uint16_t len) {
            if (info.status != BLEAudioSduInfo::Status::Valid || len == 0) {
              return;
            }
            const bool left = static_cast<uint32_t>(s.codecConfig().channelAllocation) & static_cast<uint32_t>(BLEAudioLocation::FrontLeft);
            rx[left ? 0 : 1] = rx[left ? 0 : 1] + 1;
            if (sdu[0] != (left ? 0x11 : 0x22)) {
              mismatched = mismatched + 1;
            }
          });
        }
        if (audio.start() && sink.start()) {
          unsigned long deadline = millis() + 45000;
          while (millis() < deadline) {
            delay(100);
          }
          sink.stop();
        }
        audio.end();
      }
      Serial.printf("[CLIENT] Stereo result left=%lu right=%lu mismatched=%lu\n", (unsigned long)rx[0], (unsigned long)rx[1], (unsigned long)mismatched);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] Stereo not supported");
#endif
  Serial.println("[CLIENT] Phase10 stereo done");

  // ===== Phase 11: Broadcast Assistant (client = BASS client) =====
  // Connects to the server's Broadcast Sink / Scan Delegator and drives the
  // whole BASS control point: discovery (receive-state count), Remote Scan
  // Started/Stopped, Add Source for a fixed source description, then Remove
  // Source. PA and BIS sync are not requested, so no third (broadcasting)
  // device is needed and the sink accepts the removal right away. Records each
  // operation result and the receive-state notifications that confirm them.
  // Needs the assistant role; self-skips otherwise. Restores a normal stack.
  waitForPhase(11);
#if BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    static const uint32_t kTestBroadcastId = 0xA11CE5;
    volatile int discOk = 0, states = 0, scanStart = 0, scanStop = 0, addOk = 0, removeOk = 0;
    volatile int addedId = -1;
    volatile bool discDone = false, removedSeen = false;
    volatile int pending = -1;  // Op awaited by the loop below, -1 when none.
    BTStatus st = BLE.begin("BLE_CLT_BA");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BLEAudioBroadcastAssistant ba = audio.createBroadcastAssistant();
        ba.onDiscovered([&](BTStatus s2, uint8_t receiveStates) {
          discOk = s2 ? 1 : 0;
          states = receiveStates;
          discDone = true;
        });
        ba.onResult([&](BLEAudioBroadcastAssistant::Op op, BTStatus s2) {
          int ok = s2 ? 1 : 0;
          switch (op) {
            case BLEAudioBroadcastAssistant::Op::ScanStart:    scanStart = ok; break;
            case BLEAudioBroadcastAssistant::Op::ScanStop:     scanStop = ok; break;
            case BLEAudioBroadcastAssistant::Op::AddSource:    addOk = ok; break;
            case BLEAudioBroadcastAssistant::Op::RemoveSource: removeOk = ok; break;
            default:                                           break;
          }
          if ((int)op == pending) {
            pending = -1;
          }
        });
        ba.onReceiveState([&](const BLEAudioReceiveState &rs) {
          if (rs.removed) {
            if ((int)rs.sourceId == addedId) {
              removedSeen = true;
            }
          } else if (rs.broadcastId == kTestBroadcastId) {
            addedId = rs.sourceId;
          }
        });

        // Helper: wait up to @p ms for the result of @p op (set before issuing it).
        auto waitOp = [&](int ms) {
          for (int i = 0; i < ms / 50 && pending != -1; i++) {
            delay(50);
          }
          pending = -1;
        };

        if (ba && audio.start()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              ba.discover(h);
              for (int i = 0; i < 100 && !discDone; i++) {
                delay(50);
              }
              if (discOk) {
                pending = (int)BLEAudioBroadcastAssistant::Op::ScanStart;
                if (ba.startRemoteScan()) {
                  waitOp(3000);
                }
                delay(500);
                pending = (int)BLEAudioBroadcastAssistant::Op::ScanStop;
                if (ba.stopRemoteScan()) {
                  waitOp(3000);
                }

                // A static random address and SID 1 for a source that need not exist.
                static const uint8_t kSrcAddr[6] = {0x01, 0x00, 0xDE, 0xC0, 0xDE, 0xC0};
                BLEAudioBroadcastSourceInfo src;
                src.address = BTAddress(kSrcAddr, BTAddress::Random);
                src.sid = 1;
                src.broadcastId = kTestBroadcastId;
                pending = (int)BLEAudioBroadcastAssistant::Op::AddSource;
                if (ba.addSource(src, false, 0)) {
                  waitOp(3000);
                  for (int i = 0; i < 60 && addedId < 0; i++) {
                    delay(50);
                  }
                }
                if (addedId >= 0) {
                  pending = (int)BLEAudioBroadcastAssistant::Op::RemoveSource;
                  if (ba.removeSource((uint8_t)addedId)) {
                    waitOp(3000);
                    for (int i = 0; i < 60 && !removedSeen; i++) {
                      delay(50);
                    }
                  }
                }
              }
            } else {
              Serial.println("[CLIENT] BroadcastAssistant ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] BroadcastAssistant target not found");
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] BroadcastAssistant result disc=%d states=%d scan=%d added=%d state=%d removed=%d gone=%d\n", discOk, states,
                    (scanStart && scanStop) ? 1 : 0, addOk, addedId >= 0 ? 1 : 0, removeOk, removedSeen ? 1 : 0);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] BroadcastAssistant not supported");
#endif
  Serial.println("[CLIENT] Phase11 broadcast_assistant done");

  // ===== Phase 12: CAP handover (client = CAP Initiator) =====
  // Starts a unicast stream to the server's CAP acceptor, streams raw SDUs,
  // then hands the session over to a broadcast: the initiator tells the
  // acceptor (through its BASS) to receive the broadcast, and the same sink
  // stream handle keeps carrying the SDUs as a BIS. The server counts SDUs on
  // its unicast and broadcast streams separately. Needs CAP handover
  // (CONFIG_BT_CAP_HANDOVER); self-skips otherwise. Restores a normal stack.
  waitForPhase(12);
#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    volatile int discOk = 0, ucOk = 0, hoOk = 0;
    volatile bool discDone = false, ucDone = false, hoDone = false;
    uint32_t ucSent = 0, bcSent = 0;
    BTStatus st = BLE.begin("BLE_CLT_HO");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BLEAudioCapInitiator initiator = audio.createCapInitiator();
        initiator.setPreset(BLEAudioCodecPreset::LC3_16_2_1).setBroadcastName("BLE_CLT_HO");
        initiator.onDiscovered([&](BTStatus s2, const BLEAudioCapPeerInfo &peer) {
          discOk = (s2 && peer.sinkEndpoints > 0) ? 1 : 0;
          discDone = true;
        });
        initiator.onUnicastStarted([&](BTStatus s2) {
          ucOk = s2 ? 1 : 0;
          ucDone = true;
        });
        initiator.onHandover([&](BTStatus s2, bool toBroadcast) {
          hoOk = (s2 && toBroadcast) ? 1 : 0;
          hoDone = true;
        });

        // Send one raw 40-octet SDU (the LC3_16_2_1 frame size) every 10 ms for
        // @p ms; returns how many the stack accepted.
        auto stream = [&](uint32_t ms, volatile bool *until) -> uint32_t {
          uint32_t n = 0;
          uint8_t sdu[40];
          unsigned long end = millis() + ms;
          while (millis() < end && !(until && *until)) {
            BLEAudioStream tx = initiator.stream(BLEAudioStream::Direction::Tx);
            memset(sdu, (uint8_t)n, sizeof(sdu));
            if (tx.isStreaming() && tx.write(sdu, sizeof(sdu))) {
              n++;
            }
            delay(10);
          }
          return n;
        };

        if (initiator && audio.start()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              initiator.discover(h);
              for (int i = 0; i < 200 && !discDone; i++) {
                delay(50);
              }
              if (discOk && initiator.startUnicast()) {
                for (int i = 0; i < 300 && !ucDone; i++) {
                  delay(50);
                }
              }
              if (ucOk) {
                ucSent = stream(3000, nullptr);
                BTStatus hs = initiator.handoverToBroadcast();
                Serial.printf("[CLIENT] CapHandover handover: %s\n", hs.toString());
                if (hs) {
                  // Keep feeding the stream while the acceptor moves to the BIS.
                  (void)stream(20000, &hoDone);
                  if (hoOk) {
                    bcSent = stream(5000, nullptr);
                  }
                }
                initiator.stopBroadcast();
                initiator.stopUnicast();
                delay(500);
              }
            } else {
              Serial.println("[CLIENT] CapHandover ACL connect FAILED");
            }
          } else {
            Serial.println("[CLIENT] CapHandover target not found");
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] CapHandover result disc=%d unicast=%d handover=%d ucsent=%lu bcsent=%lu\n", discOk, ucOk, hoOk, (unsigned long)ucSent,
                    (unsigned long)bcSent);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] CapHandover not supported");
#endif
  Serial.println("[CLIENT] Phase12 cap_handover done");

  // ===== Phase 13: Memory release + reinit guard =====
  waitForPhase(13);

  size_t heapBeforeRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap before release: %u\n", (unsigned)heapBeforeRelease);

  BLE.end(true);

  delay(500);
  size_t heapAfterRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap after release: %u\n", (unsigned)heapAfterRelease);

  int32_t freed = (int32_t)heapAfterRelease - (int32_t)heapBeforeRelease;
  Serial.printf("[CLIENT] Memory freed: %ld bytes\n", (long)freed);
  if (freed >= 10240) {
    Serial.println("[CLIENT] Memory release OK");
  } else {
    Serial.println("[CLIENT] Memory release INSUFFICIENT");
  }

  status = BLE.begin("ShouldFail");
  if (!status) {
    Serial.println("[CLIENT] Reinit blocked OK");
  } else {
    Serial.println("[CLIENT] Reinit was NOT blocked");
  }

  Serial.println("[CLIENT] All phases complete");
}

void loop() {
  delay(1000);
}
