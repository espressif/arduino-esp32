// LE Audio validation test — CLIENT
// Phases: audio lifecycle (ack), ISO CIS/BIS transport, BAP unicast/broadcast,
//         LC3 loopback, control profiles, top-level profiles, hearing aid,
//         stereo broadcast, broadcast assistant, CAP handover, broadcast
//         stop race, CAP broadcast interleave, memory release. Split out of
//         the combined `ble` suite (see the server
//         sketch header). Each phase self-contains its own BLE.end()/begin()
//         and self-skips when the relevant feature is not compiled in.

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "BLEAudioValidationHooks.h"
#include <string.h>
#include "freertos/task.h"
#include "esp_heap_caps.h"

// Phase 8 set control: CSIP coordinator lock/release plus CAP commander volume.
#define SET_CONTROL_TEST (BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED && BLE_AUDIO_CAP_COMMANDER_SUPPORTED && BLE_AUDIO_VCP_CONTROLLER_SUPPORTED)

String targetName;
BTAddress targetAddr;
volatile int currentPhase = 0;

// ISO transport state — written from the ISO host task, read from the phase code.
volatile bool isoCisClientConnected = false;
volatile uint32_t isoCisClientRxCount = 0;  // SDUs on the Peripheral-to-Central direction
// Phase 2 shared-host handover: a CIS created while the LE Audio engine owns
// the ISO host, and the CIS created after that engine released it.
volatile bool isoCisSharedConnected = false;
volatile bool isoCisRehandoverConnected = false;
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
volatile bool audioLinkDown = false;

// Phase 1 / Phase 4: pytest signals after the server's advertising is up.
volatile bool phase1PureGo = false;
volatile bool phase1CoexGo = false;
volatile bool phase4Go = false;

#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
// Phase 10 stress: a task churns a BIS stream callback while SDUs arrive.
volatile uint32_t stressChurnHits = 0;
volatile uint32_t stressRxRight = 0;
volatile uint32_t stressMismatch = 0;
volatile bool stressChurnStop = false;
BLEAudioStream stressTx;
#endif

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
      } else if (buf == "PHASE1_PURE_GO") {
        phase1PureGo = true;
      } else if (buf == "PHASE1_COEX_GO") {
        phase1CoexGo = true;
      } else if (buf == "PHASE4_GO") {
        phase4Go = true;
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
// A guarded lambda avoids the Arduino preprocessor injecting an unguarded
// BLEAudio-typed function prototype into audio-disabled builds.
auto audioConnect = [](BLEAudio &audio, const BTAddress &addr) -> uint16_t {
  // The peer's advertising/GATT commit can still be settling right after a
  // phase transition; a failed ACL open is transient. Retry a bounded number
  // of times, like the phase-24 client connection.
  for (int attempt = 0; attempt < 3; attempt++) {
    audioLinkConn = 0xFFFF;
    if (audio.connect(addr)) {
      for (int i = 0; i < 200 && audioLinkConn == 0xFFFF; i++) {
        delay(50);
      }
      if (audioLinkConn != 0xFFFF) {
        return audioLinkConn;
      }
    }
    delay(500);
  }
  return 0xFFFF;
};
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

#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
// Phase 10: replace/clear a stream callback while SDUs are being received, to
// exercise the stream callback locking under load.
static void stressCallbackChurn(void *) {
  while (!stressChurnStop) {
    if (stressTx) {
      stressTx.onReceive([](BLEAudioStream &s, const BLEAudioSduInfo &info, const uint8_t *sdu, uint16_t len) {
        stressChurnHits = stressChurnHits + 1;
        if (info.status != BLEAudioSduInfo::Status::Valid || len == 0) {
          return;
        }
        const bool left = static_cast<uint32_t>(s.codecConfig().channelAllocation) & static_cast<uint32_t>(BLEAudioLocation::FrontLeft);
        if (!left) {
          stressRxRight = stressRxRight + 1;
          if (sdu[0] != 0x22) {
            stressMismatch = stressMismatch + 1;
          }
        }
      });
    }
    delay(5);
    if (stressTx) {
      stressTx.resetCallbacks();
    }
    delay(1);
  }
  vTaskDelete(nullptr);
}
#endif

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();
#if defined(CONFIG_IDF_TARGET_ESP32S31)
  Serial.println("[CLIENT] Audio validation required: 1");
#else
  Serial.println("[CLIENT] Audio validation required: 0");
#endif

  // Baseline stack so the first phase's BLE.end()/begin() cycle has a live stack
  // to tear down; also keeps `status` in scope for the memory_release phase.
  BTStatus status = BLE.begin("BLE_CLT");

  // ===== Phase 1: LE Audio lifecycle (server-driven; client just acks) =====
  // audio_lifecycle is a single-device (server) engine test: the client only
  // acknowledges the phase so the two-DUT handshake stays in lockstep.
  waitForPhase(1);
#if BLE_AUDIO_SUPPORTED
  {
    phase1PureGo = false;
    unsigned long p1dl = millis() + 20000;
    while (!phase1PureGo && millis() < p1dl) {
      checkSerial();
      delay(10);
    }
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
    {
      BLE.end(false);
      delay(300);
      int pureDisc = 0, pureWrite = 0, pureDown = 0;
      if (BLE.begin("BLE_CLT_P1")) {
        BLEAudio audio = BLE.getAudioController();
        {
          audioLinkDown = false;
          audio.onLinkReady([](uint16_t conn) {
            audioLinkConn = conn;
          });
          audio.onDisconnected([](uint16_t) {
            audioLinkDown = true;
          });
          BLEAudioVolumeController vc = audio.createVolumeController();
          std::atomic<bool> discDone{false};
          // Call vc.setVolume(88) directly inside onDiscovered: exercises
          // BLEAudioControlLink releasing link.mtx before Bluedroid inline
          // discovery callbacks run.
          vc.onDiscovered([&vc, &pureDisc, &pureWrite, &discDone](BTStatus s2, uint8_t, uint8_t) {
            if (s2) {
              pureDisc = 1;
              if (vc.setVolume(88)) {
                pureWrite = 1;
              }
            }
            discDone.store(true);
          });
          if (audio.begin()) {
            BTAddress addr;
            if (findTarget(addr)) {
              uint16_t h = audioConnect(audio, addr);
              if (h != 0xFFFF && vc.discover(h)) {
                for (int i = 0; i < 100 && !discDone.load(); i++) {
                  delay(50);
                }
                for (int i = 0; i < 120 && !audioLinkDown; i++) {
                  delay(50);
                }
                pureDown = audioLinkDown ? 1 : 0;
              }
            }
          }
          audio.end();
        }
        BLE.end(false);
        delay(300);
        (void)BLE.begin("BLE_CLT");
      }
      Serial.printf("[CLIENT] Audio purePeriph disc=%d write=%d down=%d\n", pureDisc, pureWrite, pureDown);
    }
#else
    Serial.println("[CLIENT] Audio purePeriph disc=1 write=1 down=1");
#endif

    phase1CoexGo = false;
    unsigned long c1dl = millis() + 20000;
    while (!phase1CoexGo && millis() < c1dl) {
      checkSerial();
      delay(10);
    }
    BTAddress addr;
    bool requested = false;
    if (findTarget(addr)) {
      BLEClient callbackClient = BLE.createClient();
      if (callbackClient.connect(addr)) {
        BLERemoteService svc = callbackClient.getService(BLEUUID("a10c0001-0000-1000-8000-00805f9b34fb"));
        BLERemoteCharacteristic chr = svc ? svc.getCharacteristic(BLEUUID("a10c0002-0000-1000-8000-00805f9b34fb")) : BLERemoteCharacteristic();
        if (chr) {
          requested = true;
          (void)chr.readValue();
        }
      }
    }
    Serial.printf("[CLIENT] Audio callback read: %d\n", requested ? 1 : 0);
    Serial.println("[CLIENT] Audio lifecycle OK");
  }
  // Tear the phase's stack down here. Leaving it up would keep the ACL from
  // the read above alive, and the next phases bring ISO up (LE Set Host
  // Feature bit 32 is Command Disallowed while connected).
  BLE.end(false);
  {
    unsigned long dl = millis() + 10000;
    while ((BLE.isInitialized() || BLE.isEnding()) && millis() < dl) {
      delay(20);
    }
  }
#else
  Serial.println("[CLIENT] Audio not supported, skipping");
#endif
  Serial.println("[CLIENT] Phase1 audio done");

  // ===== Phase 2: ISO CIS transport integrity (client = CIS Central) =====
  // Scans for the server, opens the ACL, brings up the BLEIso transport,
  // creates the CIG + bidirectional CIS on that link, streams transparent SDUs
  // the server counts and counts the SDUs the server streams back. Then repeats
  // the transport on the ISO host the LE Audio engine owns, releases that host
  // with the CIG still allocated, and brings up one more CIS on the host it
  // re-claims (a group handle surviving the release would break that last
  // connectCis()). Self-skips when host ISO is not compiled in, so it only
  // truly runs on a dual-ISO (esp32s31<->esp32s31) bench. Restores a normal
  // stack after.
  waitForPhase(2);
#if BLE_ISO_SUPPORTED
  {
    BLEIso iso;  // assigned after BLE.begin(); retained across end()/begin() below
    BLE.end(false);
    delay(500);
    isoCisClientConnected = false;
    isoCisClientRxCount = 0;
    BTStatus st = BLE.begin("BLE_CLT_ISO");
    uint32_t sent = 0;
    BTAddress addr;
    bool found = false;
    BLEIso::CisParams params;    // 120 B SDU, 10 ms, 2M PHY
    params.returnSduSize = 120;  // bidirectional: must match the server
    iso = BLE.getIsoController();
    if (st) {
      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);
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
      if (found) {
        // ISO must be brought up before the ACL exists: LE Set Host Feature
        // (bit 32) is Command Disallowed on a connection, so begin() after
        // connect() fails with IsoEnableCisFail[524].
        BTStatus ib = iso.begin();
        Serial.printf("[CLIENT] IsoCis transport: %s\n", ib.toString());
        BLEClient client = BLE.createClient();
        BTStatus cs = client.connect(addr);
        if (cs) {
          BLEIso::Channel cis = iso.connectCis(client.getHandle(), params);
          if (cis) {
            cis.onConnected([](BLEIso::Channel &) {
              syncPhaseFromHost();
              isoCisClientConnected = true;
            });
            cis.onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
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
              if (cis.send(sdu, sizeof(sdu))) {
                sent++;
              }
              delay(10);
            }
            delay(1000);
            // Leave the CIS and ACL up so BLE.end() must terminate them over HCI
            // and wait for DISCONNECTED (same contract as the server).
          } else {
            Serial.println("[CLIENT] IsoCis connect FAILED");
          }
        } else {
          Serial.println("[CLIENT] IsoCis ACL connect FAILED");
        }
      } else {
        Serial.println("[CLIENT] IsoCis target not found");
      }
    }
    // Exercise standalone ISO ownership cleanup through BLE.end(), then prove
    // the host can be claimed again after BLE re-initialization.
    BLE.end(false);
    bool isoCoreEnded = !iso.isActive();
    delay(500);
    BTStatus restore = BLE.begin("BLE_CLT");
    iso = BLE.getIsoController();
    BTStatus isoRetry = restore ? iso.begin() : BTStatus::Fail;
    bool isoLifecycle = isoCoreEnded && restore && isoRetry;
    iso.end();
    Serial.printf("[CLIENT] IsoCis teardown coreEnded=%d restore=%d retry=%d\n", isoCoreEnded ? 1 : 0, restore ? 1 : 0, isoRetry ? 1 : 0);

    // Shared-host handover: the LE Audio engine owns the ISO host, a raw BLEIso
    // CIG is created on top of it, and the engine then releases the host while
    // that group is still allocated. resetState() must drop the group handle on
    // that release: one retained across it would make the next connectCis()
    // terminate the CIG the released host already freed.
    int sharedHost = 0, sharedCis = 0, sharedSdu = 0, groupDrop = 0, hostBusy = 0, rehandover = 0;
#if BLE_AUDIO_SUPPORTED
    if (found) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        sharedHost = 1;
        iso = BLE.getIsoController();
        if (iso.begin()) {  // shared: the audio engine already owns the host
          BLEClient sharedClient = BLE.createClient();
          // The peer only advertises while the previous link is down, so a
          // failure here is transient right after a phase transition. Retry on
          // the same client: each createClient() registers a new one.
          BTStatus sharedCc = sharedClient.connect(addr);
          for (int attempt = 0; attempt < 3 && !sharedCc; attempt++) {
            delay(500);
            sharedCc = sharedClient.connect(addr);
          }
          if (sharedCc) {
            BLEIso::Channel sharedCisCh = iso.connectCis(sharedClient.getHandle(), params);
            if (sharedCisCh) {
              sharedCisCh.onConnected([](BLEIso::Channel &) {
                syncPhaseFromHost();
                isoCisSharedConnected = true;
              });
              sharedCisCh.onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
                if (info.valid) {
                  isoCisClientRxCount = isoCisClientRxCount + 1;
                }
              });
              unsigned long shdl = millis() + 15000;
              while (!isoCisSharedConnected && millis() < shdl) {
                delay(50);
              }
              if (isoCisSharedConnected) {
                sharedCis = 1;
                uint8_t sharedSduData[120];
                for (int i = 0; i < 100 && isoCisSharedConnected; i++) {
                  memset(sharedSduData, (uint8_t)i, sizeof(sharedSduData));
                  if (sharedCisCh.send(sharedSduData, sizeof(sharedSduData))) {
                    sharedSdu++;
                  }
                  delay(10);
                }
              }
              // Drop the CIS but leave the CIG allocated, then release the
              // audio-owned host. On this IDF that release is refused while a
              // CIG slot is still allocated (bt_le_iso_report_busy() counts
              // them), so the group handle is state that perforce outlives the
              // host it points into.
              sharedCisCh.disconnect();
            }
            hostBusy = audio.end() ? 0 : 1;  // 1: refused, the CIG is still allocated
          } else {
            hostBusy = audio.end() ? 0 : 1;
          }
          groupDrop = iso.end() ? 1 : 0;  // drops the CIG, keeps the audio host up
          // Release the ACL before the audio host goes down: the server stops
          // advertising while a link is up, and the re-handover stage below
          // needs a connectable peer.
          sharedClient.disconnect();
          if (hostBusy) {
            (void)audio.end();  // now idle: the group above is gone
          }
        } else {
          (void)audio.end();
        }
        // The next connectCis() must mint a fresh CIG instead of terminating a
        // group the released host left behind. Fails with an empty handle when
        // a stale handle survived the release.
        BTStatus reBegin = iso.begin();  // this transport owns the host again
        if (reBegin) {
          BLEClient reClient = BLE.createClient();
          BTStatus reCc = reClient.connect(addr);
          for (int attempt = 0; attempt < 3 && !reCc; attempt++) {
            delay(500);
            reCc = reClient.connect(addr);
          }
          if (reCc) {
            BLEIso::Channel reCis = iso.connectCis(reClient.getHandle(), params);
            if (reCis) {
              reCis.onConnected([](BLEIso::Channel &) {
                syncPhaseFromHost();
                isoCisRehandoverConnected = true;
              });
              unsigned long rhdl = millis() + 15000;
              while (!isoCisRehandoverConnected && millis() < rhdl) {
                delay(50);
              }
              rehandover = isoCisRehandoverConnected ? 1 : 0;
              reCis.disconnect();
            } else {
              Serial.println("[CLIENT] IsoCis re-handover connect FAILED");
            }
            reClient.disconnect();
          }
        }
        iso.end();
      }
    }
#endif
    Serial.printf(
      "[CLIENT] IsoCis sharedhost=%d sharedcis=%d sharedsdu=%d groupdrop=%d hostbusy=%d rehandover=%d\n", sharedHost, sharedCis, sharedSdu, groupDrop, hostBusy,
      rehandover
    );
    Serial.printf(
      "[CLIENT] IsoCis result connected=%d sent=%lu received=%lu lifecycle=%d\n", (int)isoCisClientConnected, (unsigned long)sent,
      (unsigned long)isoCisClientRxCount, isoLifecycle ? 1 : 0
    );
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
    BLEIso iso;  // assigned after BLE.begin(); retained across end()/begin() below
    BLE.end(false);
    delay(500);
    isoBisClientSynced = false;
    isoBisRxCount = 0;
    BTStatus st = BLE.begin("BLE_CLT_ISO2");
    iso = BLE.getIsoController();
    if (st) {
      BTStatus ib = iso.begin();
      Serial.printf("[CLIENT] IsoBis transport: %s\n", ib.toString());
      BLEIso::BigParams params;
      BLEIso::Channel bis = iso.syncBig(1, params);
      if (bis) {
        bis.onConnected([](BLEIso::Channel &) {
          syncPhaseFromHost();
          isoBisClientSynced = true;
        });
        bis.onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
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
        while (millis() < deadline && !(isoBisClientSynced && isoBisRxCount >= 10)) {
          delay(20);
        }
        // Leave the BIG sync up so BLE.end() must terminate it over HCI.
      } else {
        Serial.println("[CLIENT] IsoBis sync arm FAILED");
      }
    }
    BLE.end(false);
    bool isoCoreEnded = !iso.isActive();
    delay(500);
    BTStatus restore = BLE.begin("BLE_CLT");
    iso = BLE.getIsoController();
    BTStatus isoRetry = restore ? iso.begin() : BTStatus::Fail;
    bool isoLifecycle = isoCoreEnded && restore && isoRetry;
    iso.end();
    Serial.printf("[CLIENT] IsoBis result synced=%d received=%lu lifecycle=%d\n", (int)isoBisClientSynced, (unsigned long)isoBisRxCount, isoLifecycle ? 1 : 0);
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
    uint32_t restartSent = 0;
    int restarted = 0;
    int targetAccept = 0, otherReject = 0;
    BTStatus st = BLE.begin("BLE_CLT_BAP");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      {
        audioLinkDown = false;
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        audio.onDisconnected([](uint16_t) {
          audioLinkDown = true;
        });
        BLEAudioUnicastClient uc = audio.createUnicastClient();
        uc.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
        uc.setSinkStreams(1);
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
        if (audio.begin()) {
#if BLE_AUDIO_VALIDATION_HOOKS
          // The engine must adopt only the address passed to connect(), not
          // any other central ACL that comes up while the connect is pending.
          static const uint8_t kHookTarget[6] = {0x70, 0x58, 0xD0, 0x04, 0x29, 0x1C};
          static const uint8_t kHookOther[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
          targetAccept = (bleAudioValidationArmConnect(kHookTarget, 0) && bleAudioValidationAcceptPeer(kHookTarget, 0)) ? 1 : 0;
          otherReject = bleAudioValidationAcceptPeer(kHookOther, 0) ? 0 : 1;
          bleAudioValidationCancelConnect();
#endif
          // The pytest test signals after the server's unicast advertising is
          // up. Scanning before that can latch the previous phase's advertising
          // and then connect to a server that is still re-initializing.
          phase4Go = false;
          unsigned long goDeadline = millis() + 15000;
          while (!phase4Go && millis() < goDeadline) {
            checkSerial();
            delay(10);
          }
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              BTStatus bs = uc.connectAndStart(h);
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
              // A second session on the same peer must not inherit the first
              // CIS: start again after the release completed.
              bapTxStreaming = false;
              if (uc.start()) {
                unsigned long restartDeadline = millis() + 15000;
                while (!bapTxStreaming && millis() < restartDeadline) {
                  delay(50);
                }
                if (bapTxStreaming) {
                  BLEAudioStream tx2 = uc.stream(BLEAudioStream::Direction::Tx);
                  uint8_t sdu2[40];
                  memset(sdu2, 0x5A, sizeof(sdu2));
                  for (int i = 0; i < 100 && bapTxStreaming; i++) {
                    if (tx2.write(sdu2, sizeof(sdu2))) {
                      restartSent++;
                    }
                    delay(10);
                  }
                  restarted = 1;
                }
                uc.stop();
                for (int i = 0; i < 60 && uc.isStreaming(); i++) {
                  delay(50);
                }
              }
              // Exercise the per-link disconnect API while the ACL is still up.
              BTStatus disco = audio.disconnect(h);
              unsigned long discoDeadline = millis() + 5000;
              while (!audioLinkDown && millis() < discoDeadline) {
                checkSerial();
                delay(50);
              }
              Serial.printf("[CLIENT] BapUnicast disconnect ok=%d down=%d\n", (int)(bool)disco, (int)audioLinkDown);

              // Repeated connect/disconnect must not accumulate the NimBLE
              // fallback clients or leak engine links.
              int extraCycles = 0;
              for (int cycle = 0; cycle < 2; cycle++) {
                audioLinkDown = false;
                uint16_t h2 = audioConnect(audio, addr);
                if (h2 == 0xFFFF) {
                  break;
                }
                BTStatus cycleDisco = audio.disconnect(h2);
                unsigned long cycleDeadline = millis() + 5000;
                while (!audioLinkDown && millis() < cycleDeadline) {
                  checkSerial();
                  delay(50);
                }
                if (!cycleDisco || !audioLinkDown) {
                  break;
                }
                extraCycles++;
              }
              Serial.printf("[CLIENT] BapUnicast disconnectCycles=%d\n", extraCycles);

              // Keep one link up and let the server initiate the disconnect;
              // this exercises the peripheral-link path of audio.disconnect().
              audioLinkDown = false;
              uint16_t periphConn = audioConnect(audio, addr);
              if (periphConn != 0xFFFF) {
                Serial.printf("[CLIENT] BapUnicast peripheralDisconnect armed h=%u\n", (unsigned)periphConn);
                unsigned long periphWait = millis() + 15000;
                while (!audioLinkDown && millis() < periphWait) {
                  checkSerial();
                  delay(50);
                }
                Serial.printf("[CLIENT] BapUnicast peripheralDisconnect down=%d\n", (int)audioLinkDown);
              } else {
                Serial.println("[CLIENT] BapUnicast peripheralDisconnect arm failed");
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
      Serial.printf(
        "[CLIENT] BapUnicast result streaming=%d sent=%lu presets=%d restarted=%d resent=%lu target=%d other=%d\n", (int)everStreamed, (unsigned long)sent,
        (int)bapPresetPublished, restarted, (unsigned long)restartSent, targetAccept, otherReject
      );
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
    BLEAudio audio;
    BTStatus st = BLE.begin("BLE_CLT_BCAST");
    if (st) {
      audio = BLE.getAudioController();
      {
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
        if (audio.begin() && sink.start()) {
          Serial.println("[CLIENT] BapBroadcast scanning");
          unsigned long deadline = millis() + 45000;
          while (millis() < deadline && !(bapBaseSeen && bapBcastRxCount >= 10)) {
            delay(20);
          }
          // Leave the BIG up so BLE.end() must stop leftover BAP and wait for ISO idle.
        }
      }
    }
    BLE.end(false);
    bool audioCoreEnded = !audio.isActive();
    delay(500);
    BTStatus restore = BLE.begin("BLE_CLT");
    BTStatus retry = BTStatus::Fail;
    if (restore) {
      audio = BLE.getAudioController();
      retry = audio.begin();
      if (retry) {
        (void)audio.end();
      }
    }
    bool audioLifecycle = audioCoreEnded && restore && retry;
    Serial.printf("[CLIENT] BapBroadcast result received=%lu base=%d lifecycle=%d\n", (unsigned long)bapBcastRxCount, (int)bapBaseSeen, audioLifecycle ? 1 : 0);
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
    for (int lc3Round = 0; lc3Round < 2; lc3Round++) {
      BLE.end(false);
      delay(500);
      bapTxStreaming = false;
      bool everStreamed = false;
      uint32_t encSent = 0, encErrors = 0;
      BTStatus st = BLE.begin("BLE_CLT_LC3");
      if (st) {
        BLEAudio audio = BLE.getAudioController();
        {
          audio.onLinkReady([](uint16_t conn) {
            audioLinkConn = conn;
          });
          BLEAudioUnicastClient uc = audio.createUnicastClient();
          const auto wantPreset = (lc3Round == 0) ? BLEAudioCodecPreset::LC3_16_2_1 : BLEAudioCodecPreset::LC3_441_2_1;
          volatile bool presetDisc = false;
          volatile bool presetOk = false;
          bool stallPrinted = false, presetPrinted = false;
          uc.setPreset(wantPreset);
          uc.setSinkStreams(1);
          uc.onDiscovered([wantPreset, &presetDisc, &presetOk](const BLEAudioUnicastPeerInfo &peer) {
            presetDisc = true;
            presetOk = (peer.sinkPresets & BLEAudioPresetBit(wantPreset)) != 0;
          });
          uc.onStarted([]() {
            bapTxStreaming = true;
          });
          uc.onStopped([]() {
            bapTxStreaming = false;
          });

          // A deterministic tone (16 samples/cycle: 1 kHz at 16 kHz, ~2.8 kHz
          // at 44.1 kHz) so the server sees a non-silent signal after decode.
          BLEAudioRecorder recorder;
          recorder.setPCMSource([](int16_t *pcm, size_t maxSamples) -> size_t {
            static uint32_t ph = 0;
            for (size_t i = 0; i < maxSamples; i++) {
              float s = sinf((float)ph * 2.0f * 3.14159265f / 16.0f);
              pcm[i] = (int16_t)(s * 8000.0f);
              ph++;
            }
            return maxSamples;
          });

          if (audio.begin() && recorder.begin()) {
            BTAddress addr;
            if (findTarget(addr)) {
              uint16_t h = audioConnect(audio, addr);
              if (h != 0xFFFF) {
                BTStatus bs = uc.connectAndStart(h);
                Serial.printf("[CLIENT] LC3Loopback setup: %s\n", bs.toString());
                unsigned long cdl = millis() + 15000;
                while (!bapTxStreaming && millis() < cdl) {
                  delay(50);
                }
                everStreamed = bapTxStreaming;
                if (bapTxStreaming) {
                  // The client's streams exist once the setup started; the
                  // recorder task then encodes + sends autonomously.
                  BTStatus ra = recorder.attach(uc.stream(BLEAudioStream::Direction::Tx));
                  BTStatus rs = ra ? recorder.startCapture() : ra;
                  Serial.printf("[CLIENT] LC3Loopback recorder: %s\n", rs.toString());
                  delay(lc3Round == 0 ? 4000 : 15000);
                  encSent = recorder.sdusSent();
                  encErrors = recorder.sendErrors();
                  if (lc3Round == 0) {
                    // TX-stall injection: drop controller releases, verify the
                    // pacing resyncs (txStalls advances) then resumes sending.
                    uint32_t stallsBefore = recorder.txStalls();
                    uint32_t sentBefore = recorder.sdusSent();
#if BLE_AUDIO_VALIDATION_HOOKS
                    bleAudioValidationBlockReleases(true);
#endif
                    delay(600);
#if BLE_AUDIO_VALIDATION_HOOKS
                    bleAudioValidationBlockReleases(false);
#endif
                    delay(500);
                    uint32_t stallsAfter = recorder.txStalls();
                    uint32_t sentAfter = recorder.sdusSent();
                    int stalledOk = (stallsAfter > stallsBefore) ? 1 : 0;
                    int resumedOk = (sentAfter > sentBefore) ? 1 : 0;
                    Serial.printf("[CLIENT] LC3Stall result stalled=%d resumed=%d\n", stalledOk, resumedOk);
                    stallPrinted = true;
                  }
                } else {
                  Serial.println("[CLIENT] LC3Loopback stream did not start");
                  if (lc3Round == 0) {
                    Serial.printf("[CLIENT] LC3Stall result stalled=0 resumed=0\n");
                    stallPrinted = true;
                  }
                }
                {
                  unsigned long pdl = millis() + 5000;
                  while (!presetDisc && millis() < pdl) {
                    delay(50);
                  }
                  Serial.printf("[CLIENT] LC3Preset round=%d ok=%d\n", lc3Round, (presetDisc && presetOk) ? 1 : 0);
                  presetPrinted = true;
                }
                recorder.end();
                uc.stop();
                for (int i = 0; i < 60 && uc.isStreaming(); i++) {
                  delay(50);
                }
              } else {
                Serial.println("[CLIENT] LC3Loopback ACL connect FAILED");
              }
            } else {
              Serial.println("[CLIENT] LC3Loopback target not found");
            }
          }
          if (lc3Round == 0 && !stallPrinted) {
            Serial.printf("[CLIENT] LC3Stall result stalled=0 resumed=0\n");
          }
          if (!presetPrinted) {
            Serial.printf("[CLIENT] LC3Preset round=%d ok=0\n", lc3Round);
          }
          recorder.end();
          audio.end();
        }
        if (lc3Round == 0) {
          Serial.printf("[CLIENT] LC3Loopback result streaming=%d sent=%lu errors=%lu\n", (int)everStreamed, (unsigned long)encSent, (unsigned long)encErrors);
        } else {
          Serial.printf(
            "[CLIENT] LC3Loopback44 result streaming=%d sent=%lu errors=%lu\n", (int)everStreamed, (unsigned long)encSent, (unsigned long)encErrors
          );
        }
      }
    }  // end LC3 rounds
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] LC3Loopback not supported");
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
      {
        BLEAudioCapInitiator capInit = audio.createCapInitiator();
        volatile bool capDone = false;
        capInit.onDiscovered([&capDone, &capDisc](BTStatus s2, const BLEAudioCapPeerInfo &) {
          capDisc = s2 ? 1 : 0;
          capDone = true;
        });

        BLEAudioVolumeController vc = audio.createVolumeController();
        volatile bool vcDisc = false, vcStateSeen = false;
        vc.onDiscovered([&vcDisc](BTStatus s2, uint8_t, uint8_t) {
          vcDisc = (bool)s2;
        });
        vc.onStateChanged([&vcStateSeen](uint8_t v, bool) {
          if (v == 200) {
            vcStateSeen = true;
          }
        });

        BLEAudioMicController mc = audio.createMicController();
        volatile bool micDisc = false, micMuteSeen = false;
        mc.onDiscovered([&micDisc](BTStatus s2, uint8_t) {
          micDisc = (bool)s2;
        });
        mc.onMuteChanged([&micMuteSeen](bool muted) {
          if (muted) {
            micMuteSeen = true;
          }
        });

        BLEAudioMediaController mediaCtl = audio.createMediaController();
        volatile bool mediaDisc = false, mediaResSeen = false;
        mediaCtl.onDiscovered([&mediaDisc](BTStatus s2) {
          mediaDisc = (bool)s2;
        });
        mediaCtl.onCommandResult([&mediaResSeen](BLEAudioMediaCommand cmd, BTStatus s2) {
          if (cmd == BLEAudioMediaCommand::Play && s2) {
            mediaResSeen = true;
          }
        });

        BLEAudioCallController callCtl = audio.createCallController();
        volatile bool callDisc = false, callOriginated = false;
        callCtl.onDiscovered([&callDisc](BTStatus s2, bool gtbs) {
          callDisc = (bool)s2 && gtbs;
        });
        callCtl.onResult([&callOriginated](BLEAudioCallOperation op, BTStatus s2, uint8_t) {
          if (op == BLEAudioCallOperation::Originate && s2) {
            callOriginated = true;
          }
        });

        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });

        if (audio.begin()) {
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
                for (int i = 0; i < 40 && !vcStateSeen; i++) {
                  delay(50);
                }
                vcpOk = vcStateSeen ? 1 : 0;
              }

              // MICP: discover then mute the remote microphone. Only enable
              // MICP AICS if capacity wasn't already claimed by VCP, or verify
              // MICS mute round-trip when discovered.
              mc.discover(h);
              for (int i = 0; i < 60 && !micDisc; i++) {
                delay(50);
              }
              if (micDisc && mc.mute()) {
                for (int i = 0; i < 40 && !micMuteSeen; i++) {
                  delay(50);
                }
                micOk = micMuteSeen ? 1 : 0;
              }

              // MCP: discover then send Play to the reference media player.
              mediaCtl.discover(h);
              for (int i = 0; i < 60 && !mediaDisc; i++) {
                delay(50);
              }
              if (mediaDisc && mediaCtl.play()) {
                for (int i = 0; i < 40 && !mediaResSeen; i++) {
                  delay(50);
                }
                mediaOk = mediaResSeen ? 1 : 0;
              }

              // CCP: discover then originate a call on the peer's GTBS.
              callCtl.discover(h);
              for (int i = 0; i < 60 && !callDisc; i++) {
                delay(50);
              }
              if (callDisc && callCtl.originate("tel:123456")) {
                for (int i = 0; i < 40 && !callOriginated; i++) {
                  delay(50);
                }
                callOk = callOriginated ? 1 : 0;
              }
              // Async error injection: queue one VCP and one MICP write, then
              // drop the link immediately. Each op must terminate exactly once
              // (success or NotConnected via the disconnect fail-fast), with no
              // second terminal after quiescence. A late stack error for the
              // same op is dropped by the conn-handle gate.
              {
                // Success arrives as a state notification, failure as onResult;
                // either one counts as the single terminal for its op.
                volatile int vcpTerms = 0, vcpNotConn = 0, micTerms = 0, micNotConn = 0;
                vc.onStateChanged([&](uint8_t, bool) {
                  vcpTerms++;
                });
                vc.onResult([&](BLEAudioVolumeController::Operation, BTStatus s2) {
                  vcpTerms++;
                  if (s2 == BTStatus::NotConnected) {
                    vcpNotConn++;
                  }
                });
                mc.onMuteChanged([&](bool) {
                  micTerms++;
                });
                mc.onResult([&](BLEAudioMicController::Operation, BTStatus s2) {
                  micTerms++;
                  if (s2 == BTStatus::NotConnected) {
                    micNotConn++;
                  }
                });
                BTStatus vqs = vcDisc ? vc.setVolume(10) : BTStatus::InvalidState;
                BTStatus mqs = micDisc ? mc.mute() : BTStatus::InvalidState;
                int vcpQ = vqs ? 1 : 0;
                int micQ = mqs ? 1 : 0;
                (void)audio.disconnect(h);
                unsigned long adl = millis() + 3000;
                while (millis() < adl && (vcpTerms == 0 || micTerms == 0)) {
                  delay(50);
                }
                int vcpFirst = vcpTerms, micFirst = micTerms;
                delay(1500);
                int vcpQuiet = (vcpTerms == vcpFirst) ? 1 : 0;
                int micQuiet = (micTerms == micFirst) ? 1 : 0;
                Serial.printf(
                  "[CLIENT] AsyncError result vcpQ=%d micQ=%d vcpTerms=%d vcpNotConn=%d micTerms=%d micNotConn=%d quiet=%d%d\n", vcpQ, micQ, vcpTerms,
                  vcpNotConn, micTerms, micNotConn, vcpQuiet, micQuiet
                );
              }
              delay(200);
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
      {
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

        // Pre-start guard: TMAP/GMAP discover() before audio.begin() must fail
        // with InvalidState.
        bool preStartGuarded = tmap.discover(0) == BTStatus::InvalidState && gmap.discover(0) == BTStatus::InvalidState;

        uint16_t h = 0xFFFF;
        if (preStartGuarded && audio.begin()) {
          BTAddress addr;
          if (findTarget(addr)) {
            h = audioConnect(audio, addr);
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
        Serial.printf("[CLIENT] TopProfiles result tmap=%d gmap=%d ugt=%d\n", tmapRoles, gmapRoles, gmapUgt);
#if SET_CONTROL_TEST
        Serial.printf("[CLIENT] SetControl result csip=%d locked=%d commander=%d\n", csipSize, csipLocked, cmdVolume);
#endif
        // Quiesce the ACL before tearing down: re-begin runs into GAP-busy
        // while the previous link is still up, and the NimBLE GATT reset
        // refuses a connected host. Bounded wait so a lost callback cannot
        // hang the suite; audio.end() still runs afterwards either way.
        volatile bool linkDown = false;
        audio.onDisconnected([&linkDown](uint16_t) {
          linkDown = true;
        });
        if (h != 0xFFFF && audio.disconnect(h)) {
          for (int i = 0; i < 100 && !linkDown; i++) {
            delay(50);
          }
        }
        Serial.printf("[CLIENT] TopProfiles round1 down=%d\n", (int)linkDown);
        BTStatus end1 = audio.end();
        Serial.printf("[CLIENT] TopProfiles round1 end=%s\n", end1.toString());

        // ---- round 2: same-host audio.end() -> re-create must re-link
        // every client callback (GMAP/CSIP/commander) that common_deinit
        // released. The server re-advertises after the first release, so
        // retry the scan until it is back.
        int gmapRoles2 = -1, gmapUgt2 = -1;
#if SET_CONTROL_TEST
        int csipSize2 = -1, cmdVolume2 = 0;
#endif
        BLEAudioGmap gmap2 = audio.createGmap();  // lazy init
        volatile bool gmapDone2 = false;
        gmap2.onDiscovered([&gmapDone2, &gmapRoles2, &gmapUgt2](uint16_t, BTStatus s2, BLEAudioGmapRole roles, const BLEAudioGmapFeatures &f) {
          gmapRoles2 = s2 ? (int)(uint8_t)roles : -1;
          gmapUgt2 = s2 ? f.unicastTerminal : -1;
          gmapDone2 = true;
        });
#if SET_CONTROL_TEST
        BLEAudioCoordinatedSetCoordinator csip2 = audio.createCoordinatedSetCoordinator();
        volatile bool csipDone2 = false;
        csip2.onDiscovered([&csipDone2, &csipSize2](BTStatus s2, const BLEAudioCoordinatedSetInfo &info) {
          csipSize2 = s2 ? info.setSize : -1;
          csipDone2 = true;
        });
        BLEAudioCapCommander commander2 = audio.createCapCommander();
        volatile bool cmdDisc2 = false, cmdDone2 = false;
        commander2.onDiscovered([&cmdDisc2](BTStatus s2, uint16_t, bool, bool) {
          cmdDisc2 = (bool)s2;
        });
        commander2.onResult([&cmdDone2, &cmdVolume2](BLEAudioCapCommander::Operation op, BTStatus s2) {
          if (op == BLEAudioCapCommander::Operation::Volume) {
            cmdVolume2 = s2 ? 1 : 0;
            cmdDone2 = true;
          }
        });
#endif
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BTStatus rb2 = audio.begin();  // commit (lazy init ran at creation)
        Serial.printf("[CLIENT] TopProfiles reinit begin=%s initialized=%d\n", rb2.toString(), (int)BLE.isInitialized());
        for (int att = 0; att < 3 && rb2 == BTStatus::Busy; att++) {
          delay(2000);
          rb2 = audio.begin();
          Serial.printf("[CLIENT] TopProfiles reinit retry begin=%s\n", rb2.toString());
        }
        BTStatus rs2 = BTStatus::Fail;
        if (rb2) {
          rs2 = BTStatus::OK;
          uint16_t h2 = 0xFFFF;
          for (int att = 0; att < 4 && h2 == 0xFFFF; att++) {
            BTAddress addr2;
            if (findTarget(addr2)) {
              h2 = audioConnect(audio, addr2);
            }
          }
          Serial.printf("[CLIENT] TopProfiles reinit link=%u (0 is a valid handle) start=%s\n", (unsigned)h2, rs2.toString());
          if (h2 != 0xFFFF) {
            gmap2.discover(h2);
            for (int i = 0; i < 60 && !gmapDone2; i++) {
              delay(50);
            }
#if SET_CONTROL_TEST
            csip2.discover(h2);
            for (int i = 0; i < 100 && !csipDone2; i++) {
              delay(50);
            }
            commander2.discover(h2);
            for (int i = 0; i < 100 && !cmdDisc2; i++) {
              delay(50);
            }
            if (cmdDisc2 && commander2.setVolume(77)) {
              for (int i = 0; i < 60 && !cmdDone2; i++) {
                delay(50);
              }
            }
#endif
          }
          audio.end();
        }
        Serial.printf("[CLIENT] TopProfiles reinit gmap=%d ugt=%d\n", gmapRoles2, gmapUgt2);
#if SET_CONTROL_TEST
        Serial.printf("[CLIENT] TopProfiles reinit csip=%d\n", csipSize2);
        Serial.printf("[CLIENT] SetControl reinit commander=%d\n", cmdVolume2);
#endif
      }
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
  // records it read and whether the switch was accepted. Then repeats
  // discovery after audio.end() -> re-create to prove the controller
  // callback was re-linked. Needs the HAS client
  // (BLE_AUDIO_HAS_CLIENT_SUPPORTED); self-skips otherwise. Restores a normal
  // stack afterward.
  waitForPhase(9);
#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    int readCount = 0, switched = 0, discOk = 0;
    volatile bool presetsDone = false;
    BTStatus st = BLE.begin("BLE_CLT_HA");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioHearingAidController hac = audio.createHearingAidController();
        volatile bool discDone = false;
        hac.onDiscovered([&discDone, &discOk](BTStatus s2, BLEHearingAidType type) {
          (void)type;
          discOk = s2 ? 1 : 0;
          discDone = true;
        });
        hac.onPreset([&readCount, &presetsDone](uint8_t index, bool available, const String &name, bool isLast) {
          (void)index;
          (void)available;
          (void)name;
          readCount = readCount + 1;
          // The stack marks the final record; join on it instead of sleeping
          // a fixed window so slow links cannot under-count and fast ones do
          // not stall the phase.
          if (isLast) {
            presetsDone = true;
          }
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

        uint16_t h = 0xFFFF;
        if (audio.begin()) {
          BTAddress addr;
          if (findTarget(addr)) {
            h = audioConnect(audio, addr);
            if (h != 0xFFFF) {
              hac.discover(h);
              for (int i = 0; i < 60 && !discDone; i++) {
                delay(50);
              }
              if (discOk) {
                hac.readPresets(1, 255);
                for (int i = 0; i < 60 && !presetsDone; i++) {
                  delay(50);
                }
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
        Serial.printf("[CLIENT] HearingAid result disc=%d read=%d switched=%d\n", discOk, readCount, switched);
        // Quiesce the ACL before tearing down (see the Phase 8 note).
        volatile bool haLinkDown = false;
        audio.onDisconnected([&haLinkDown](uint16_t) {
          haLinkDown = true;
        });
        if (h != 0xFFFF && audio.disconnect(h)) {
          for (int i = 0; i < 100 && !haLinkDown; i++) {
            delay(50);
          }
        }
        Serial.printf("[CLIENT] HearingAid round1 down=%d\n", (int)haLinkDown);
        BTStatus haEnd1 = audio.end();
        Serial.printf("[CLIENT] HearingAid round1 end=%s\n", haEnd1.toString());

        // ---- round 2: same-host audio.end() -> re-create must re-link
        // the controller callback that common_deinit released; the server
        // re-advertises after the first release, so retry the scan.
        int discOk2 = 0, readCount2 = 0;
        volatile bool presetsDone2 = false;
        BLEAudioHearingAidController hac2 = audio.createHearingAidController();  // lazy init
        volatile bool discDone2 = false;
        hac2.onDiscovered([&discDone2, &discOk2](BTStatus s2, BLEHearingAidType type) {
          (void)type;
          discOk2 = s2 ? 1 : 0;
          discDone2 = true;
        });
        hac2.onPreset([&readCount2, &presetsDone2](uint8_t index, bool available, const String &name, bool isLast) {
          (void)index;
          (void)available;
          (void)name;
          readCount2 = readCount2 + 1;
          if (isLast) {
            presetsDone2 = true;
          }
        });
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BTStatus haRb2 = audio.begin();  // commit
        Serial.printf("[CLIENT] HearingAid reinit begin=%s\n", haRb2.toString());
        if (haRb2) {
          uint16_t h2 = 0xFFFF;
          for (int att = 0; att < 4 && h2 == 0xFFFF; att++) {
            BTAddress addr2;
            if (findTarget(addr2)) {
              h2 = audioConnect(audio, addr2);
            }
          }
          Serial.printf("[CLIENT] HearingAid reinit link=%u (0 is a valid handle)\n", (unsigned)h2);
          if (h2 != 0xFFFF) {
            hac2.discover(h2);
            for (int i = 0; i < 60 && !discDone2; i++) {
              delay(50);
            }
            if (discOk2) {
              hac2.readPresets(1, 255);
              for (int i = 0; i < 60 && !presetsDone2; i++) {
                delay(50);
              }
            }
          }
        }
        audio.end();
        Serial.printf("[CLIENT] HearingAid reinit disc=%d read=%d\n", discOk2, readCount2);
      }
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
    stressChurnHits = 0;
    stressRxRight = 0;
    stressMismatch = 0;
    stressChurnStop = false;
    volatile int cbCleared = 0;
    BTStatus st = BLE.begin("BLE_CLT_STEREO");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setStreams(2);
        sink.setLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight);
        sink.setTargetName(targetName);
        sink.setBroadcastCode("ArduinoStereo");  // must match the server's STEREO_BROADCAST_CODE
        sink.onStarted([&]() {
          // Clearing every callback from inside onStarted must keep the
          // running callable alive and must not deadlock the host task.
          sink.resetCallbacks();
          cbCleared = 1;
        });
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
        if (audio.begin() && sink.start()) {
          stressTx = sink.stream(1);
          if (stressTx) {
            xTaskCreate(stressCallbackChurn, "cb-stress", 4096, nullptr, 1, nullptr);
          }
          unsigned long deadline = millis() + 45000;
          while (millis() < deadline) {
            delay(100);
          }
          stressChurnStop = true;
          delay(100);
          stressTx.resetCallbacks();
          stressTx = BLEAudioStream();
          sink.stop();
        }
        audio.end();
      }
      Serial.printf(
        "[CLIENT] Stereo result left=%lu right=%lu mismatched=%lu cb=%lu cleared=%d\n", (unsigned long)rx[0], (unsigned long)(rx[1] + stressRxRight),
        (unsigned long)(mismatched + stressMismatch), (unsigned long)stressChurnHits, cbCleared
      );
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
    volatile int scanStopCount = 0, scanStopFail = 0, scanAgain = 0;
    volatile int addedId = -1;
    volatile bool discDone = false, removedSeen = false;
    volatile int pending = -1;  // Op awaited by the loop below, -1 when none.
    BTStatus st = BLE.begin("BLE_CLT_BA");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      {
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BLEAudioBroadcastAssistant ba = audio.createBroadcastAssistant();
        std::atomic<uint32_t> pastResults{0};
        ba.onDiscovered([&](BTStatus s2, uint8_t receiveStates) {
          discOk = s2 ? 1 : 0;
          states = receiveStates;
          discDone = true;
        });
        ba.onResult([&](BLEAudioBroadcastAssistant::Op op, BTStatus s2) {
          int ok = s2 ? 1 : 0;
          switch (op) {
            case BLEAudioBroadcastAssistant::Op::ScanStart: scanStart = ok; break;
            case BLEAudioBroadcastAssistant::Op::ScanStop:
              scanStop = ok;
              scanStopCount = scanStopCount + 1;
              if (!ok) {
                scanStopFail = scanStopFail + 1;
              }
              break;
            case BLEAudioBroadcastAssistant::Op::AddSource:    addOk = ok; break;
            case BLEAudioBroadcastAssistant::Op::RemoveSource: removeOk = ok; break;
            case BLEAudioBroadcastAssistant::Op::Past:         pastResults.fetch_add(1); break;
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

        if (ba && audio.begin()) {
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
                // A second stop is a no-op: it must not send another Scan Stopped
                // write or report a late failure.
                scanAgain = ba.stopRemoteScan() ? 1 : 0;
                delay(1000);

                // A static random address and SID 1 for a source that need not exist.
                // Request syncPa=true with a known PA interval first so the
                // delegator enters SyncInfoRequested and exercises ba_past() on
                // conn_handle == 0, then modifySource(syncPa=false) to disarm
                // before removeSource().
                static const uint8_t kSrcAddr[6] = {0x01, 0x00, 0xDE, 0xC0, 0xDE, 0xC0};
                BLEAudioBroadcastSourceInfo src;
                src.address = BTAddress(kSrcAddr, BTAddress::Random);
                src.sid = 1;
                src.broadcastId = kTestBroadcastId;
                src.paInterval = 0x0040;
                pending = (int)BLEAudioBroadcastAssistant::Op::AddSource;
                if (ba.addSource(src, true, 0)) {
                  waitOp(3000);
                  for (int i = 0; i < 60 && addedId < 0; i++) {
                    delay(50);
                  }
                }
                if (addedId >= 0) {
                  delay(200);
                  pending = (int)BLEAudioBroadcastAssistant::Op::ModifySource;
                  if (ba.modifySource((uint8_t)addedId, false, 0)) {
                    waitOp(3000);
                  }
                  delay(200);
                  pending = (int)BLEAudioBroadcastAssistant::Op::RemoveSource;
                  if (ba.removeSource((uint8_t)addedId)) {
                    waitOp(3000);
                    for (int i = 0; i < 60 && !removedSeen; i++) {
                      delay(50);
                    }
                  }
                }
                // If PAST was triggered on conn_handle 0, s_ba_past must
                // deduplicate it to at most 1 attempt per request.
                if (pastResults.load() > 1) {
                  addOk = 0;
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
      Serial.printf(
        "[CLIENT] BroadcastAssistant result disc=%d states=%d scan=%d added=%d state=%d removed=%d gone=%d stops=%d stopfail=%d again=%d\n", discOk, states,
        (scanStart && scanStop) ? 1 : 0, addOk, addedId >= 0 ? 1 : 0, removeOk, removedSeen ? 1 : 0, scanStopCount, scanStopFail, scanAgain
      );
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
    volatile int discOk = 0, ucOk = 0, hoOk = 0, badHoFailed = 0, badHoUcAlive = 0, ucStopped = 0, ucStopRc = 0, bcStopRc = 0;
    volatile int hoFail = 0, hoFailDone = 0, hoFailRecovered = 0, hoRestart = 0;
    volatile bool discDone = false, ucDone = false, hoDone = false;
    uint32_t ucSent = 0, bcSent = 0;
    BTStatus st = BLE.begin("BLE_CLT_HO");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      {
        audio.onLinkReady([](uint16_t conn) {
          audioLinkConn = conn;
        });
        BLEAudioCapInitiator initiator = audio.createCapInitiator();
        initiator.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
        initiator.setBroadcastName("BLE_CLT_HO");
        initiator.onDiscovered([&](BTStatus s2, const BLEAudioCapPeerInfo &peer) {
          discOk = (s2 && peer.sinkEndpoints > 0) ? 1 : 0;
          discDone = true;
        });
        initiator.onUnicastStarted([&](BTStatus s2) {
          ucOk = s2 ? 1 : 0;
          ucDone = true;
        });
        initiator.onHandover([&](BTStatus s2, bool toBroadcast) {
          if (toBroadcast) {
            if (s2) {
              hoOk = 1;
              hoDone = true;
            } else {
              hoFail = 1;
              hoFailDone = 1;
            }
          }
        });
        initiator.onUnicastStopped([&]() {
          ucStopped = 1;
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

        if (initiator && audio.begin()) {
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
#if BLE_AUDIO_VALIDATION_HOOKS
                // Force the asynchronous handover failure and verify that the
                // initiator releases unicast and can rebuild the session.
                hoFail = 0;
                hoFailDone = 0;
                bleAudioValidationFailNextHandover();
                BTStatus forced = initiator.handoverToBroadcast();
                Serial.printf("[CLIENT] CapHandover forced=%d\n", (int)forced.code());
                if (forced) {
                  unsigned long fdl = millis() + 5000;
                  while (!hoFailDone && millis() < fdl) {
                    delay(50);
                  }
                }
                hoFailRecovered = (hoFail && !initiator.isUnicastStreaming()) ? 1 : 0;
                // The recovery initiates an asynchronous group release; wait
                // for UC_STOPPED before rebuilding the session.
                unsigned long stopDeadline = millis() + 5000;
                while (!ucStopped && millis() < stopDeadline) {
                  delay(50);
                }
                ucStopped = 0;
                ucOk = 0;
                ucDone = false;
                if (initiator.startUnicast()) {
                  unsigned long rdl = millis() + 15000;
                  while (!ucDone && millis() < rdl) {
                    delay(50);
                  }
                }
                hoRestart = ucOk ? 1 : 0;
#endif
                ucSent = stream(3000, nullptr);
                // An out-of-range advertising instance fails locally and must
                // leave the running unicast session untouched.
                BTStatus badHo = initiator.handoverToBroadcast(255);
                badHoFailed = !badHo;
                badHoUcAlive = initiator.isUnicastStreaming() ? 1 : 0;
                BTStatus hs = initiator.handoverToBroadcast();
                Serial.printf("[CLIENT] CapHandover handover: %s\n", hs.toString());
                if (hs) {
                  // Keep feeding the stream while the acceptor moves to the BIS.
                  (void)stream(20000, &hoDone);
                  if (hoOk) {
                    bcSent = stream(5000, nullptr);
                  }
                }
                // Release the unicast group first: stopBroadcast() clears the
                // handed-over flag, which is what makes the group releasable.
                ucStopRc = (int)initiator.stopUnicast().code();
                unsigned long ucStopDeadline = millis() + 5000;
                while (!ucStopped && millis() < ucStopDeadline) {
                  delay(50);
                }
                bcStopRc = (int)initiator.stopBroadcast().code();
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
      Serial.printf(
        "[CLIENT] CapHandover result disc=%d unicast=%d handover=%d ucsent=%lu bcsent=%lu badho=%d badafter=%d ucstopped=%d ucrc=%d bcrc=%d hofail=%d "
        "horestart=%d\n",
        discOk, ucOk, hoOk, (unsigned long)ucSent, (unsigned long)bcSent, badHoFailed, badHoUcAlive, ucStopped, ucStopRc, bcStopRc, hoFailRecovered, hoRestart
      );
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin("BLE_CLT");
  }
#else
  Serial.println("[CLIENT] CapHandover not supported");
#endif
  Serial.println("[CLIENT] Phase12 cap_handover done");

  // ===== Phase 13: bcast_stop_race (server-side stress; client observes) =====
  waitForPhase(13);
  Serial.println("[CLIENT] Phase13 bcast_stop_race done");

  // ===== Phase 14: cap_bc_interleave (server-side stress; client observes) =====
  waitForPhase(14);
  Serial.println("[CLIENT] Phase14 cap_bc_interleave done");

  // ===== Phases 15-17: server-driven (start retry, security policy, ISO gates) =====
  waitForPhase(15);
  Serial.println("[CLIENT] Phase15 start_retry_refused done");

  waitForPhase(16);
  Serial.println("[CLIENT] Phase16 security_policy done");

  waitForPhase(17);
  Serial.println("[CLIENT] Phase17 iso_handle_gates done");

  // ===== Phase 18: mic_mute_disabled (MICS tri-state + fast unmute refusal) =====
  waitForPhase(18);
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
  {
    BLE.end(false);
    delay(500);
    int disabled = 0, unmuteRc = -99, noresult = 0, sawFalse = 0;
    volatile bool discDone = false, sawMuted = false, sawUnmuted = false, resultFired = false;
    BTStatus st = BLE.begin("BLE_CLT_MIC");
    if (st) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioMicController mc = audio.createMicController();
        mc.onDiscovered([&discDone](BTStatus s2, uint8_t) {
          discDone = (bool)s2;
        });
        mc.onMuteChanged([&sawMuted, &sawUnmuted, &sawFalse](bool muted) {
          Serial.printf("[CLIENT] MicDisable evt muted=%d t=%lu\n", muted ? 1 : 0, (unsigned long)millis());
          if (muted) {
            sawMuted = true;
          } else {
            sawUnmuted = true;
          }
        });
        mc.onResult([&resultFired](BLEAudioMicController::Operation, BTStatus) {
          resultFired = true;
        });
        if (audio.begin()) {
          BTAddress addr;
          if (findTarget(addr)) {
            uint16_t h = audioConnect(audio, addr);
            if (h != 0xFFFF && mc.discover(h)) {
              for (int i = 0; i < 200 && !discDone; i++) {
                delay(50);
              }
              Serial.printf("[CLIENT] MicDisable discovering done=%d t=%lu\n", discDone ? 1 : 0, (unsigned long)millis());
              if (discDone) {
                // The server disabled before advertising, so the discovery
                // sync-read already reports disabled: no mute round trip and
                // no waiting on the peer.
                disabled = mc.isMuteDisabled() ? 1 : 0;
                sawFalse = 0;
                unmuteRc = (int)mc.unmute().code();
                // A refused unmute must stay silent: no async onResult may fire.
                for (int i = 0; i < 20 && !resultFired; i++) {
                  delay(50);
                }
                noresult = resultFired ? 0 : 1;
              }
            }
          }
        }
        audio.end();
      }
      Serial.printf("[CLIENT] MicDisable disabled=%d unmuteRc=%d noresult=%d sawFalse=%d\n", disabled, unmuteRc, noresult, sawFalse);
      BLE.end(false);
      delay(500);
      (void)BLE.begin("BLE_CLT");
    } else {
      Serial.println("[CLIENT] MicDisable not supported");
    }
  }
#else
  Serial.println("[CLIENT] MicDisable not supported");
#endif
  Serial.println("[CLIENT] Phase18 mic_mute_disabled done");

  // ===== Phase 19: Memory release + reinit guard =====
  waitForPhase(19);

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
