// LE Audio validation test — SERVER
// Phases: audio lifecycle, ISO CIS/BIS transport, BAP unicast/broadcast,
//         LC3 loopback, control profiles, top-level profiles, hearing aid,
//         stereo broadcast, broadcast assistant, CAP handover, memory
//         release. Split out of the combined `ble` suite so that test
//         stays focused on core GATT/GAP while this one owns the GAF stack.
//
// Every phase self-contains its own BLE.end()/begin() so the audio stack is
// brought up fresh per phase, and self-skips when the relevant feature is not
// compiled in — so the suite runs unchanged on non-audio silicon and only
// truly exercises the data plane on a dual-audio esp32s31 <-> esp32s31 bench.

#include <Arduino.h>
#include <BLE.h>
#include "esp_heap_caps.h"

static const BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID RW_CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Phase 1 (audio_lifecycle): a plain custom GATT service committed alongside
// the LE Audio profiles through the unified BLEGattDatabase coordinator, to
// prove classic-service + audio coexistence in one attribute table.
static const BLEUUID AUDIO_COEX_SVC_UUID("a10c0001-0000-1000-8000-00805f9b34fb");
static const BLEUUID AUDIO_COEX_CHAR_UUID("a10c0002-0000-1000-8000-00805f9b34fb");

// Phase 10 (stereo): the stereo broadcast is encrypted; the client uses the same code.
static const char STEREO_BROADCAST_CODE[] = "ArduinoStereo";

String serverName;
volatile int currentPhase = 0;

// Phase completion flags (each phase fires once).
bool phaseAudioDone = false;           // audio_lifecycle (phase 1)
bool phaseIsoCisDone = false;          // iso_cis (phase 2, CIS peripheral)
bool phaseIsoBisDone = false;          // iso_bis (phase 3, BIS broadcaster)
bool phaseBapUnicastDone = false;      // bap_unicast (phase 4, BAP Unicast Server)
bool phaseBapBroadcastDone = false;    // bap_broadcast (phase 5, BAP Broadcast Source)
bool phaseLc3LoopbackDone = false;     // lc3_loopback (phase 6, LC3 decode via BLEAudioPlayer)
bool phaseControlProfilesDone = false; // control_profiles (phase 7, VCP/MICP/MCP/CCP server)
bool phaseTopProfilesDone = false;     // top_profiles (phase 8, TMAP/GMAP identity server)
bool phaseHearingAidDone = false;      // hearing_aid (phase 9, HAS presets server)
bool phaseStereoDone = false;          // stereo (phase 10, 2-BIS Broadcast Source)
bool phaseAssistantDone = false;       // broadcast_assistant (phase 11, Broadcast Sink + Scan Delegator)
bool phaseHandoverDone = false;        // cap_handover (phase 12, CAP acceptor: unicast server + delegator sink)
bool phaseMemoryDone = false;          // memory_release (phase 13, tail of suite)

// Control-profile server-side observation counters (written from role callbacks).
volatile uint32_t ctrlVolWrites = 0;  // VCP renderer state changes seen
volatile uint32_t ctrlMicMutes = 0;   // MICP device mute changes seen
volatile uint32_t ctrlCallReqs = 0;   // CCP originate requests seen
volatile uint32_t haSelects = 0;      // HAS preset select requests seen
volatile uint32_t topSetLocks = 0;    // phase 8: CSIS lock changes made by the client's set coordinator
volatile uint8_t topVolume = 0;       // phase 8: last volume written (by the client's CAP commander)

// ISO transport counters — written from the ISO host task, read from loop().
volatile bool isoCisConnected = false;
volatile uint32_t isoCisRxCount = 0;
volatile bool isoBisConnected = false;

// BAP unicast server sink RX counter — written from the audio host task.
volatile uint32_t bapSrvRxCount = 0;

// LC3 loopback (phase 6) decode metrics — written from the pipeline PCM sink.
// samples counts decoded 16-bit PCM samples; energy accumulates |amplitude| so
// the test can assert the received audio is a real tone, not silence.
volatile uint32_t lc3RxSamples = 0;
volatile uint64_t lc3RxEnergy = 0;

// CAP handover (phase 12) SDU counters — written from the audio host task.
// The acceptor receives on its unicast sink stream first and on its broadcast
// sink stream after the handover.
volatile uint32_t hoUnicastRx = 0;
volatile uint32_t hoBroadcastRx = 0;
volatile bool hoBigJoined = false;

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
          Serial.printf("[SERVER] Phase %d started\n", phase);
        }
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

// BLE callbacks run on the stack's context and may preempt loop() before it calls
// checkSerial(). Drain UART first so START_PHASE_* is applied and [SERVER] Phase N started
// always precedes phase-dependent GATT logs (host and pexpect stay in order).
// Only the ISO and audio phases wait in loops that poll it.
#if BLE_ISO_SUPPORTED || BLE_AUDIO_SUPPORTED
static void syncPhaseFromHost() {
  checkSerial();
}
#endif

void readName() {
  Serial.println("[SERVER] Device ready for name");
  Serial.println("[SERVER] Send name:");
  while (serverName.length() == 0) {
    if (Serial.available()) {
      serverName = Serial.readStringUntil('\n');
      serverName.trim();
    }
    delay(100);
  }
  Serial.printf("[SERVER] Name: %s\n", serverName.c_str());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();

  // Bring up a baseline stack so the first phase's BLE.end()/begin() cycle has a
  // live stack to tear down (each audio phase re-begins its own stack anyway).
  (void)BLE.begin(serverName);
}

void loop() {
  checkSerial();

  // Phase 1: LE Audio lifecycle + classic-service coexistence. Tears down and
  // re-begins BLE for a clean stack, brings up the LE Audio engine
  // (common_init), stages a plain custom GATT service, then performs the single
  // coordinated commit (common_start via audio.start()) so the classic service
  // and the audio profiles live in one attribute table. Self-skips when the LE
  // Audio engine is not compiled in. Restores a normal stack afterward so the
  // memory_release phase still measures a meaningful free.
  if (currentPhase >= 1 && !phaseAudioDone) {
    phaseAudioDone = true;
#if BLE_AUDIO_SUPPORTED
    BLE.end(false);
    delay(500);
    BTStatus s = BLE.begin(serverName);
    if (!s) {
      Serial.printf("[SERVER] Audio init FAILED: %s\n", s.toString());
    } else {
      BLEAudio audio = BLE.getAudioController();
      if (!audio) {
        Serial.println("[SERVER] Audio controller unavailable");
      } else {
        audio.setPresentationDelay(40000);
        BTStatus ab = audio.begin();
        Serial.printf("[SERVER] Audio begin: %s\n", ab.toString());

        // Coexistence: a plain custom service staged alongside the audio
        // profiles; both are committed by the single audio.start() below.
        BLEServer srv = BLE.createServer();
        BLEService coexSvc = srv.createService(AUDIO_COEX_SVC_UUID);
        BLECharacteristic coexChr =
          coexSvc.createCharacteristic(AUDIO_COEX_CHAR_UUID, BLEProperty::Read | BLEProperty::Notify, BLEPermission::ReadOpen);
        coexChr.setValue("coexist");
        srv.start();  // stages the custom service (add_svcs) in audio mode

        BTStatus astart = audio.start();  // single coordinated ble_gatts_start()
        Serial.printf("[SERVER] Audio start: %s\n", astart.toString());
        Serial.printf("[SERVER] Audio active: %d\n", audio.isActive() ? 1 : 0);
        Serial.printf("[SERVER] Audio preset delay: %lu\n", (unsigned long)audio.getPresentationDelay());

        // Codec preset value-type sanity (also forces the value-type API to compile).
        BLEAudioCodecConfig cfg = BLEAudioCodecConfig::fromPreset(BLEAudioCodecPreset::LC3_16_2_1);
        Serial.printf("[SERVER] Audio preset LC3_16_2_1 sr=%lu octets=%u\n", (unsigned long)cfg.samplingRateHz, (unsigned)cfg.octetsPerFrame);

        BTStatus aend = audio.end();
        if (ab && astart && aend) {
          Serial.println("[SERVER] Audio lifecycle OK");
        } else {
          Serial.printf("[SERVER] Audio lifecycle FAILED begin=%d start=%d end=%d\n", ab ? 1 : 0, astart ? 1 : 0, aend ? 1 : 0);
        }

        // Re-initialization in the same boot: end() released the engine
        // (common_deinit), so begin() + start() must work again with a
        // different role set (a broadcast source, which adds no GATT service).
        BTStatus rb = audio.begin();
        BTStatus rs = BTStatus::Fail;
        if (rb) {
          BLEAudioBroadcastSource reSrc = audio.createBroadcastSource();
          reSrc.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
          rs = audio.start();
          audio.end();
        }
        Serial.printf("[SERVER] Audio reinit begin=%d start=%d\n", rb ? 1 : 0, rs ? 1 : 0);
      }
    }
    // Restore a normal connectable stack so memory_release frees real state.
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] Audio not supported, skipping");
#endif
    Serial.println("[SERVER] Phase1 audio done");
  }

  // Phase 2: ISO CIS transport integrity — the server is the CIS Peripheral.
  // Stands up a minimal connectable server so the client (Central) can create
  // the ACL, brings up the BLEIso transport, registers an ISO server that
  // accepts one inbound bidirectional CIS, counts the transparent SDUs streamed
  // by the client and streams SDUs back on the Peripheral-to-Central direction. Self-skips when host ISO is not compiled in (non-audio targets), so
  // this only truly runs on a dual-ISO (esp32s31<->esp32s31) bench. Restores a
  // normal stack afterward so memory_release still measures a meaningful free.
  if (currentPhase >= 2 && !phaseIsoCisDone) {
    phaseIsoCisDone = true;
#if BLE_ISO_SUPPORTED
    BLE.end(false);
    delay(500);
    isoCisConnected = false;
    isoCisRxCount = 0;
    BTStatus s = BLE.begin(serverName);
    if (!s) {
      Serial.printf("[SERVER] IsoCis init FAILED: %s\n", s.toString());
    } else {
      BLEServer srv = BLE.createServer();
      srv.advertiseOnDisconnect(true);
      BLEService svc = srv.createService(SERVICE_UUID);
      auto rc = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
      rc.setValue("iso_cis");
      srv.start();

      BTStatus ib = BLEIso::begin();
      Serial.printf("[SERVER] IsoCis transport: %s\n", ib.toString());

      BLEIso::CisParams params;  // defaults: 120 B SDU, 10 ms, 2M PHY
      params.returnSduSize = 120;  // bidirectional: the Peripheral streams back
      BLEIso::Channel *cis = BLEIso::listenCis(params);
      uint32_t returned = 0;
      if (cis) {
        cis->onConnected([](BLEIso::Channel &) {
          syncPhaseFromHost();
          isoCisConnected = true;
        });
        cis->onReceive([](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
          if (info.valid) {
            isoCisRxCount = isoCisRxCount + 1;
          }
        });
        cis->onDisconnected([](BLEIso::Channel &, uint8_t) {
          syncPhaseFromHost();
          isoCisConnected = false;
        });

        BLEAdvertising adv = BLE.getAdvertising();
        adv.reset();
        adv.setType(BLEAdvType::ConnectableScannable);
        adv.setName(serverName);
        adv.addServiceUUID(SERVICE_UUID);
        adv.start();
        Serial.println("[SERVER] IsoCis ready");

        // Let the client connect, establish the CIS, and stream SDUs; stream
        // back on the Peripheral-to-Central direction while the CIS is up.
        uint8_t sdu[120];
        unsigned long deadline = millis() + 25000;
        while (millis() < deadline) {
          syncPhaseFromHost();
          if (isoCisConnected && cis->canSend()) {
            memset(sdu, (uint8_t)returned, sizeof(sdu));
            if (cis->send(sdu, sizeof(sdu))) {
              returned++;
            }
          }
          delay(10);
        }
      } else {
        Serial.println("[SERVER] IsoCis listen FAILED");
      }
      BLEIso::end();
      Serial.printf(
        "[SERVER] IsoCis result connected=%d received=%lu returned=%lu\n", (int)isoCisConnected, (unsigned long)isoCisRxCount, (unsigned long)returned
      );
    }
    // Restore a normal connectable stack for the next phase.
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] IsoCis not supported");
#endif
    Serial.println("[SERVER] Phase2 iso_cis done");
  }

  // Phase 3: ISO BIS transport integrity — the server is the BIS Broadcaster.
  // Brings up extended + periodic advertising (the BIG carrier), creates a BIG
  // on that instance, and streams transparent SDUs for a fixed window. Needs
  // both host ISO and BLE5; self-skips otherwise. Restores a normal stack after.
  if (currentPhase >= 3 && !phaseIsoBisDone) {
    phaseIsoBisDone = true;
#if BLE_ISO_SUPPORTED && BLE5_SUPPORTED
    BLE.end(false);
    delay(500);
    isoBisConnected = false;
    BTStatus s = BLE.begin(serverName);
    if (!s) {
      Serial.printf("[SERVER] IsoBis init FAILED: %s\n", s.toString());
    } else {
      BLEAdvertising adv = BLE.getAdvertising();
      adv.setExtType(0, BLEAdvType::NonConnectable);
      adv.setExtPhy(0, BLEPhy::PHY_1M, BLEPhy::PHY_2M);
      adv.setExtSID(0, 1);
      BLEAdvertisementData extData;
      extData.setName(serverName);
      (void)adv.setExtAdvertisementData(0, extData);
      adv.setPeriodicAdvInterval(0, 0x20, 0x40);
      BLEAdvertisementData perData;
      perData.setName("BIGBroadcast");
      adv.setPeriodicAdvData(0, perData);
      (void)adv.startExtended(0, 0, 0);
      adv.startPeriodicAdv(0);

      BTStatus ib = BLEIso::begin();
      Serial.printf("[SERVER] IsoBis transport: %s\n", ib.toString());

      BLEIso::BigParams params;  // defaults: 120 B SDU, 10 ms, 2M PHY
      BLEIso::Channel *bis = BLEIso::createBig(0, params);
      uint32_t sent = 0;
      if (bis) {
        bis->onConnected([](BLEIso::Channel &) {
          syncPhaseFromHost();
          isoBisConnected = true;
        });
        Serial.println("[SERVER] IsoBis ready");

        // The channel numbers the SDUs itself (send without a sequence number).
        uint8_t sdu[120];
        unsigned long deadline = millis() + 25000;
        while (millis() < deadline) {
          if (isoBisConnected) {
            memset(sdu, (uint8_t)sent, sizeof(sdu));
            if (bis->send(sdu, sizeof(sdu))) {
              sent++;
            }
          }
          delay(10);
        }
        bis->disconnect();  // terminate the BIG before the transport goes down
        delay(200);
      } else {
        Serial.println("[SERVER] IsoBis create FAILED");
      }
      BLEIso::end();
      adv.stopPeriodicAdv(0);
      adv.stopExtended(0);
      Serial.printf("[SERVER] IsoBis result connected=%d sent=%lu\n", (int)isoBisConnected, (unsigned long)sent);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] IsoBis not supported");
#endif
    Serial.println("[SERVER] Phase3 iso_bis done");
  }

  // Phase 4: BAP Unicast transport — the server is the BAP Unicast Server
  // (acceptor). It runs a classic BLEServer *and* the audio Unicast Server so
  // both commit into one GATT table (coexistence), advertises connectably, and
  // counts the transparent SDUs the client streams to its sink ASE over a CIS.
  // Needs the LE Audio engine; self-skips otherwise (so only a dual-audio bench
  // truly runs it). Restores a normal stack afterward.
  if (currentPhase >= 4 && !phaseBapUnicastDone) {
    phaseBapUnicastDone = true;
#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED
    BLE.end(false);
    delay(500);
    bapSrvRxCount = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        // Classic server contributes a marker service to the same GATT table.
        BLEServer srv = BLE.createServer();
        srv.advertiseOnDisconnect(true);
        BLEService svc = srv.createService(SERVICE_UUID);
        auto ch = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
        ch.setValue("bap");
        srv.start();  // stages in audio mode

        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1).setSourceStreams(1).setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
        for (size_t i = 0; i < us.streamCount(); i++) {
          us.stream(i).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
            if (info.status == BLEAudioSduInfo::Status::Valid) {
              bapSrvRxCount = bapSrvRxCount + 1;
            }
          });
        }

        if (audio.start()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.addServiceUUID(SERVICE_UUID);
          adv.start();
          Serial.println("[SERVER] BapUnicast ready");
          // Covers the client's worst case: 15 s scan + link + 15 s start + 4 s streaming.
          unsigned long deadline = millis() + 60000;
          while (millis() < deadline) {
            syncPhaseFromHost();
            delay(100);
          }
        }
        audio.end();
      }
      Serial.printf("[SERVER] BapUnicast result received=%lu\n", (unsigned long)bapSrvRxCount);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] BapUnicast not supported");
#endif
    Serial.println("[SERVER] Phase4 bap_unicast done");
  }

  // Phase 5: BAP Broadcast transport — the server is the BAP Broadcast Source.
  // It announces an Auracast broadcast (extended + periodic adv carrying the
  // BASE) and streams transparent SDUs over a BIG. No connection: the client
  // (Broadcast Sink) scans, PA-syncs, syncs the BIG, and counts SDUs. Needs the
  // LE Audio engine; self-skips otherwise. Restores a normal stack afterward.
  if (currentPhase >= 5 && !phaseBapBroadcastDone) {
    phaseBapBroadcastDone = true;
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
    BLE.end(false);
    delay(500);
    bool bcStreaming = false;
    uint32_t sent = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioBroadcastSource source = audio.createBroadcastSource();
        source.setPreset(BLEAudioCodecPreset::LC3_16_2_1).setBroadcastId(0x123456).setName(serverName);
        source.onStarted([&bcStreaming]() {
          bcStreaming = true;
        });
        source.onStopped([&bcStreaming](uint8_t) {
          bcStreaming = false;
        });
        if (audio.start() && source.start()) {
          Serial.println("[SERVER] BapBroadcast ready");
          unsigned long deadline = millis() + 40000;
          uint8_t sdu[40];
          uint8_t n = 0;
          BLEAudioStream src = source.stream(0);
          while (millis() < deadline) {
            if (bcStreaming) {
              memset(sdu, n++, sizeof(sdu));
              if (src.write(sdu, sizeof(sdu))) {
                sent++;
              }
              delay(10);
            } else {
              delay(50);
            }
            syncPhaseFromHost();
          }
          source.stop();
        }
        audio.end();
      }
      Serial.printf("[SERVER] BapBroadcast result sent=%lu\n", (unsigned long)sent);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] BapBroadcast not supported");
#endif
    Serial.println("[SERVER] Phase5 bap_broadcast done");
  }

  // Phase 6: LC3 loopback — the server is the Unicast Server sink and hands its
  // sink stream to a BLEAudioPlayer, which LC3-decodes the frames the client
  // encodes from a tone and streams over the CIS. The player runs without I2S:
  // its PCM sink accumulates the decoded sample count + signal energy so the
  // host can assert real audio flowed end-to-end through the codec. The player
  // follows the codec the client configures on the ASE. Needs the LC3 codec
  // compiled in (BLE_AUDIO_LC3_SUPPORTED); self-skips otherwise. Restores a
  // normal stack afterward.
  if (currentPhase >= 6 && !phaseLc3LoopbackDone) {
    phaseLc3LoopbackDone = true;
#if BLE_AUDIO_LC3_SUPPORTED && BLE_AUDIO_UNICAST_SERVER_SUPPORTED
    BLE.end(false);
    delay(500);
    lc3RxSamples = 0;
    lc3RxEnergy = 0;
    uint32_t plcFrames = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1).setSourceStreams(0).setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);

        // PCM sink instead of I2S pins: decoded frames land here on the codec task.
        BLEAudioPlayer player;
        player.setPcmSink([](const int16_t *pcm, size_t n) {
          lc3RxSamples = lc3RxSamples + (uint32_t)n;
          uint64_t e = 0;
          for (size_t i = 0; i < n; i++) {
            int v = pcm[i];
            e += (v < 0) ? (uint64_t)(-v) : (uint64_t)v;
          }
          lc3RxEnergy = lc3RxEnergy + e;
        });

        if (audio.start()) {
          // The stream's direction is only known once the client configures
          // the ASE; the player accepts it before that and waits for Streaming.
          BTStatus pb = player.begin();
          BTStatus pa = pb ? player.attach(us.stream(0)) : pb;
          BTStatus ps = pa ? player.start() : pa;
          Serial.printf("[SERVER] Lc3Loopback player: %s\n", ps.toString());
          if (ps) {
            BLEAdvertising adv = BLE.getAdvertising();
            adv.reset();
            adv.setType(BLEAdvType::ConnectableScannable);
            adv.setName(serverName);
            adv.start();
            Serial.println("[SERVER] Lc3Loopback ready");
            unsigned long deadline = millis() + 60000;  // client worst case, as in phase 4
            while (millis() < deadline) {
              syncPhaseFromHost();
              delay(100);
            }
          }
          plcFrames = player.plcFrames();
          player.end();
        }
        audio.end();
      }
      unsigned long long avg = lc3RxSamples ? (unsigned long long)(lc3RxEnergy / lc3RxSamples) : 0ull;
      Serial.printf("[SERVER] Lc3Loopback result samples=%lu meanamp=%llu plc=%lu\n", (unsigned long)lc3RxSamples, avg, (unsigned long)plcFrames);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] Lc3Loopback not supported");
#endif
    Serial.println("[SERVER] Phase6 lc3_loopback done");
  }

  // Phase 7: Control profiles — the server exposes the full LE Audio control
  // surface over a single ACL (a stereo-headset-like acceptor): CAP acceptor
  // (CAS+CSIS), VCP renderer (VCS), MICP device (MICS), MCP media player (MCS),
  // and CCP call server (GTBS). It advertises connectably and answers whatever
  // the client (the "phone") drives, tallying the writes it observes so the
  // host can assert an end-to-end control path. Needs the LE Audio engine;
  // self-skips otherwise. Restores a normal stack afterward.
  if (currentPhase >= 7 && !phaseControlProfilesDone) {
    phaseControlProfilesDone = true;
#if BLE_AUDIO_VCP_RENDERER_SUPPORTED && BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
    BLE.end(false);
    delay(500);
    ctrlVolWrites = 0;
    ctrlMicMutes = 0;
    ctrlCallReqs = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioCapAcceptor cap = audio.createCapAcceptor();
        cap.setSetSize(2).setRank(1);

        BLEAudioVolumeRenderer vol = audio.createVolumeRenderer();
        vol.setInitialVolume(100);
        vol.onStateChanged([](uint8_t volume, bool muted) {
          (void)volume;
          (void)muted;
          ctrlVolWrites = ctrlVolWrites + 1;
        });

        BLEAudioMicDevice mic = audio.createMicDevice();
        mic.onMuteChanged([](bool muted) {
          (void)muted;
          ctrlMicMutes = ctrlMicMutes + 1;
        });

        BLEAudioMediaPlayer media = audio.createMediaPlayer();
        (void)media;

        BLEAudioCallServer call = audio.createCallServer();
        call.setProviderName("ESP Phone");
        call.onOriginate([](uint8_t callIndex, const String &uri) -> bool {
          (void)callIndex;
          (void)uri;
          ctrlCallReqs = ctrlCallReqs + 1;
          return true;
        });

        if (audio.start()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          Serial.println("[SERVER] ControlProfiles ready");
          unsigned long deadline = millis() + 60000;  // client worst case: scan + link + 5 discoveries
          while (millis() < deadline) {
            syncPhaseFromHost();
            delay(100);
          }
        }
        audio.end();
      }
      Serial.printf("[SERVER] ControlProfiles result volwrites=%lu micmutes=%lu calls=%lu\n", (unsigned long)ctrlVolWrites, (unsigned long)ctrlMicMutes, (unsigned long)ctrlCallReqs);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] ControlProfiles not supported");
#endif
    Serial.println("[SERVER] Phase7 control_profiles done");
  }

  // Phase 8: Top-level profiles — the server publishes the TMAP (TMAS) and
  // GMAP (GMAS) identity services; the client discovers both. Backed by the
  // CAP acceptor + unicast server + VCP renderer, which the client also uses
  // for set control: its CSIP coordinator locks and releases the (one-member)
  // coordinated set and its CAP commander sets the volume. The server counts
  // lock changes and records the last volume. Needs TMAP/GMAP compiled in
  // (and CONFIG_BT_GMAP_UGT_SUPPORTED); self-skips otherwise.
  // Restores a normal stack afterward.
  if (currentPhase >= 8 && !phaseTopProfilesDone) {
    phaseTopProfilesDone = true;
#if BLE_AUDIO_TMAP_SUPPORTED && BLE_AUDIO_GMAP_SUPPORTED
    BLE.end(false);
    delay(500);
    topSetLocks = 0;
    topVolume = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        // Underlying roles that back the advertised TMAP/GMAP identities.
        BLEAudioCapAcceptor cap = audio.createCapAcceptor();
        cap.setSetSize(1).setRank(1);
        cap.coordinatedSet().onLockChanged([](bool locked, uint16_t connHandle) {
          (void)locked;
          (void)connHandle;
          topSetLocks = topSetLocks + 1;
        });
        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1).setSourceStreams(1);
        BLEAudioVolumeRenderer vol = audio.createVolumeRenderer();
        vol.setInitialVolume(100);
        vol.onStateChanged([](uint8_t volume, bool muted) {
          (void)muted;
          topVolume = volume;
        });

        // TMAP Call Terminal + Unicast Media Receiver; GMAP Unicast Game Terminal.
        BLEAudioTmap tmap = audio.createTmap();
        tmap.setRoles(BLEAudioTmapRole::CallTerminal | BLEAudioTmapRole::UnicastMediaReceiver);
        BLEAudioGmap gmap = audio.createGmap();
        BLEAudioGmapFeatures gf;
        gf.unicastTerminal = BLEAudioGmapFeatures::UgtSink;
        gmap.setRoles(BLEAudioGmapRole::UnicastGameTerminal).setFeatures(gf);

        bool started = (bool)audio.start();
        if (started) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
        }
        Serial.printf("[SERVER] TopProfiles ready registered=%d\n", (int)started);
        unsigned long deadline = millis() + 60000;  // client worst case: scan + link + 4 discoveries + lock/unlock
        while (millis() < deadline) {
          syncPhaseFromHost();
          delay(100);
        }
        audio.end();
      }
      Serial.printf("[SERVER] TopProfiles result done locks=%lu volume=%u\n", (unsigned long)topSetLocks, (unsigned)topVolume);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] TopProfiles not supported");
#endif
    Serial.println("[SERVER] Phase8 top_profiles done");
  }

  // Phase 9: Hearing Access Service (server = hearing aid). Publishes a HAS
  // instance with a small preset list, then waits for the controller to
  // discover, read the presets, and switch the active one. Counts select
  // requests it observes. Needs the HAS server (BLE_AUDIO_HAS_SUPPORTED);
  // self-skips otherwise. Restores a normal stack afterward.
  if (currentPhase >= 9 && !phaseHearingAidDone) {
    phaseHearingAidDone = true;
#if BLE_AUDIO_HAS_SUPPORTED
    BLE.end(false);
    delay(500);
    haSelects = 0;
    uint8_t haActive = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioHearingAidDevice ha = audio.createHearingAidDevice();
        ha.setType(BLEHearingAidType::Binaural)
          .setPresetSync(false)
          .addPreset(1, "Universal")
          .addPreset(2, "Outdoor")
          .addPreset(3, "Restaurant");
        ha.onPresetSelected([](uint8_t index, bool sync) {
          (void)index;
          (void)sync;
          haSelects = haSelects + 1;
        });

        if (audio.start()) {
          ha.setActivePreset(1);
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          Serial.println("[SERVER] HearingAid ready");
          unsigned long deadline = millis() + 35000;
          while (millis() < deadline) {
            syncPhaseFromHost();
            delay(100);
          }
        }
        haActive = ha.getActivePreset();
        audio.end();
      }
      // Printed on every path so a failed begin() reports instead of timing out.
      Serial.printf("[SERVER] HearingAid result presets=3 selects=%lu active=%u\n", (unsigned long)haSelects, (unsigned)haActive);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] HearingAid not supported");
#endif
    Serial.println("[SERVER] Phase9 hearing_aid done");
  }

  // Phase 10: Stereo broadcast — the server is an encrypted 2-BIS Broadcast
  // Source (front left + front right, Broadcast Code shared with the client)
  // and streams a distinct pattern on each BIS. Phase 5 covers the unencrypted
  // path. Restores a normal stack afterward.
  if (currentPhase >= 10 && !phaseStereoDone) {
    phaseStereoDone = true;
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
    BLE.end(false);
    delay(500);
    volatile bool bcStreaming = false;
    uint32_t sent = 0;
    uint8_t bis = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioBroadcastSource source = audio.createBroadcastSource();
        source.setPreset(BLEAudioCodecPreset::LC3_16_2_1).setChannels(2).setName(serverName).setBroadcastCode(STEREO_BROADCAST_CODE);
        source.onStarted([&bcStreaming]() {
          bcStreaming = true;
        });
        source.onStopped([&bcStreaming](uint8_t) {
          bcStreaming = false;
        });
        if (audio.start() && source.start()) {
          bis = (uint8_t)source.streamCount();
          Serial.printf("[SERVER] Stereo ready bis=%u\n", bis);
          uint8_t left[40], right[40];
          memset(left, 0x11, sizeof(left));
          memset(right, 0x22, sizeof(right));
          unsigned long deadline = millis() + 40000;
          while (millis() < deadline) {
            if (bcStreaming && bis == 2) {
              if (source.stream(0).write(left, sizeof(left)) && source.stream(1).write(right, sizeof(right))) {
                sent++;
              }
              delay(10);
            } else {
              delay(50);
            }
            syncPhaseFromHost();
          }
          source.stop();
        }
        audio.end();
      }
      Serial.printf("[SERVER] Stereo result bis=%u sent=%lu\n", bis, (unsigned long)sent);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] Stereo not supported");
#endif
    Serial.println("[SERVER] Phase10 stereo done");
  }

  // Phase 11: Broadcast Assistant — the server is a Broadcast Sink with the
  // Scan Delegator (BASS) and advertises connectably so the client (the
  // assistant) can connect and drive the BASS control point. Auto-sync is off
  // and the sink is not started: it only acts on assistant requests. All
  // assertions are on the client, which sees the operation results and the
  // receive-state notifications. Needs the scan delegator; self-skips
  // otherwise. Restores a normal stack afterward.
  if (currentPhase >= 11 && !phaseAssistantDone) {
    phaseAssistantDone = true;
#if BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED && BLE_AUDIO_BROADCAST_SINK_SUPPORTED
    BLE.end(false);
    delay(500);
    bool ready = false;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setScanDelegator(true).setAutoSync(false);
        if (sink && audio.start()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.addServiceUUID(BLEUUID((uint16_t)0x184F));  // Broadcast Audio Scan Service
          adv.start();
          ready = true;
          Serial.println("[SERVER] BroadcastAssistant ready");
          unsigned long deadline = millis() + 70000;  // client worst case is about 50 s
          while (millis() < deadline) {
            syncPhaseFromHost();
            delay(100);
          }
        }
        audio.end();
      }
      Serial.printf("[SERVER] BroadcastAssistant result delegator=%d\n", ready ? 1 : 0);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] BroadcastAssistant not supported");
#endif
    Serial.println("[SERVER] Phase11 broadcast_assistant done");
  }

  // Phase 12: CAP handover — the server is a CAP acceptor that can receive
  // both ways: a Unicast Server sink for the CIS and a Broadcast Sink with the
  // Scan Delegator for the BIS the initiator hands the session over to. The
  // sink does not scan on its own (auto-sync off, not started); it syncs when
  // the initiator adds the broadcast through BASS. SDUs are counted per
  // transport so the host can assert audio arrived on both sides of the
  // handover. Needs a CAP acceptor, the unicast server and the scan delegator;
  // self-skips otherwise. Restores a normal stack afterward.
  if (currentPhase >= 12 && !phaseHandoverDone) {
    phaseHandoverDone = true;
#if BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED && BLE_AUDIO_UNICAST_SERVER_SUPPORTED && BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED && BLE_AUDIO_BROADCAST_SINK_SUPPORTED
    BLE.end(false);
    delay(500);
    hoUnicastRx = 0;
    hoBroadcastRx = 0;
    hoBigJoined = false;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      if (audio.begin()) {
        BLEAudioCapAcceptor cap = audio.createCapAcceptor();
        cap.setSetSize(1).setRank(1);

        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1).setSourceStreams(0).setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
        us.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
          if (info.status == BLEAudioSduInfo::Status::Valid) {
            hoUnicastRx = hoUnicastRx + 1;
          }
        });

        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setScanDelegator(true).setAutoSync(false).onStarted([]() {
          hoBigJoined = true;
        });
        sink.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
          if (info.status == BLEAudioSduInfo::Status::Valid) {
            hoBroadcastRx = hoBroadcastRx + 1;
          }
        });

        if (cap && sink && audio.start()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          Serial.println("[SERVER] CapHandover ready");
          unsigned long deadline = millis() + 100000;  // client worst case is about 80 s
          while (millis() < deadline) {
            syncPhaseFromHost();
            delay(100);
          }
          sink.stop();
        }
        audio.end();
      }
      Serial.printf("[SERVER] CapHandover result unicast=%lu joined=%d broadcast=%lu\n", (unsigned long)hoUnicastRx, (int)hoBigJoined,
                    (unsigned long)hoBroadcastRx);
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] CapHandover not supported");
#endif
    Serial.println("[SERVER] Phase12 cap_handover done");
  }

  // Phase 13: Memory release (tail of the suite; runs after every audio phase
  // has restored a normal stack).
  if (currentPhase >= 13 && !phaseMemoryDone) {
    phaseMemoryDone = true;
    delay(500);

    size_t heapBeforeRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.printf("[SERVER] Heap before release: %u\n", (unsigned)heapBeforeRelease);

    BLE.end(true);

    delay(500);
    size_t heapAfterRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.printf("[SERVER] Heap after release: %u\n", (unsigned)heapAfterRelease);

    int32_t freed = (int32_t)heapAfterRelease - (int32_t)heapBeforeRelease;
    Serial.printf("[SERVER] Memory freed: %ld bytes\n", (long)freed);
    if (freed >= 10240) {
      Serial.println("[SERVER] Memory release OK");
    } else {
      Serial.println("[SERVER] Memory release INSUFFICIENT");
    }

    BTStatus s = BLE.begin("ShouldFail");
    if (!s) {
      Serial.println("[SERVER] Reinit blocked OK");
    } else {
      Serial.println("[SERVER] Reinit was NOT blocked");
    }

    Serial.println("[SERVER] All phases complete");
  }

  delay(50);
}
