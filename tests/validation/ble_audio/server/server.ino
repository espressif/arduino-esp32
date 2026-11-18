// LE Audio validation test — SERVER
// Phases: audio lifecycle, ISO CIS/BIS transport, BAP unicast/broadcast,
//         LC3 loopback, control profiles, top-level profiles, hearing aid,
//         stereo broadcast, broadcast assistant, CAP handover, broadcast
//         stop race, CAP broadcast interleave, memory release. Split out of
//         the combined `ble` suite so that test
//         stays focused on core GATT/GAP while this one owns the GAF stack.
//
// Every phase self-contains its own BLE.end()/begin() so the audio stack is
// brought up fresh per phase, and self-skips when the relevant feature is not
// compiled in — so the suite runs unchanged on non-audio silicon and only
// truly exercises the data plane on a dual-audio esp32s31 <-> esp32s31 bench.

#include <Arduino.h>
#include <BLE.h>
#include <BLEAudio.h>
#include "esp_heap_caps.h"

static const BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID RW_CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Phase 1 (audio_lifecycle): a plain custom GATT service committed alongside
// the LE Audio profiles through the unified BLEGattDatabase coordinator, to
// prove plain-GATT + audio coexistence in one attribute table.
static const BLEUUID AUDIO_COEX_SVC_UUID("a10c0001-0000-1000-8000-00805f9b34fb");
static const BLEUUID AUDIO_COEX_CHAR_UUID("a10c0002-0000-1000-8000-00805f9b34fb");
static const BLEUUID AUDIO_COEX_CHAR2_UUID("a10c0007-0000-1000-8000-00805f9b34fb");
static const BLEUUID AUDIO_REMOVE_SVC_UUID("a10c0003-0000-1000-8000-00805f9b34fb");
static const BLEUUID AUDIO_REMOVE_CHAR_UUID("a10c0004-0000-1000-8000-00805f9b34fb");

// Phase 10 (stereo): the stereo broadcast is encrypted; the client uses the same code.
static const char STEREO_BROADCAST_CODE[] = "ArduinoStereo";

String serverName;
volatile int currentPhase = 0;
volatile bool phaseRelease = false;  // pytest PHASE_RELEASE: client finished this phase

// Phase completion flags (each phase fires once).
bool phaseAudioDone = false;            // audio_lifecycle (phase 1)
bool phaseIsoCisDone = false;           // iso_cis (phase 2, CIS peripheral)
bool phaseIsoBisDone = false;           // iso_bis (phase 3, BIS broadcaster)
bool phaseBapUnicastDone = false;       // bap_unicast (phase 4, BAP Unicast Server)
bool phaseBapBroadcastDone = false;     // bap_broadcast (phase 5, BAP Broadcast Source)
bool phaseLC3LoopbackDone = false;      // lc3_loopback (phase 6, LC3 decode via BLEAudioPlayer)
bool phaseControlProfilesDone = false;  // control_profiles (phase 7, VCP/MICP/MCP/CCP server)
bool phaseTopProfilesDone = false;      // top_profiles (phase 8, TMAP/GMAP identity server)
bool phaseHearingAidDone = false;       // hearing_aid (phase 9, HAS presets server)
bool phaseStereoDone = false;           // stereo (phase 10, 2-BIS Broadcast Source)
bool phaseAssistantDone = false;        // broadcast_assistant (phase 11, Broadcast Sink + Scan Delegator)
bool phaseHandoverDone = false;         // cap_handover (phase 12, CAP acceptor: unicast server + delegator sink)
bool phaseStopRaceDone = false;         // bcast_stop_race (phase 13, BroadcastSource start/stop race)
bool phaseCapBcDone = false;            // cap_bc_interleave (phase 14, CAP startBroadcast vs streams() snapshot)
bool phaseStartRetryDone = false;       // start_retry_refused (phase 15, failed start() must not retry)
bool phaseSecPolicyDone = false;        // security_policy (phase 16, explicit audio security policy)
bool phaseIsoGatesDone = false;         // iso_handle_gates (phase 17, dead-handle connectCis refusal)
bool phaseMicDisableDone = false;       // mic_mute_disabled (phase 18, MICS tri-state)
bool phaseMemoryDone = false;           // memory_release (phase 19, tail of suite)

// Control-profile server-side observation counters (written from role callbacks).
volatile uint32_t ctrlVolWrites = 0;  // VCP renderer state changes seen
volatile uint32_t ctrlMicMutes = 0;   // MICP device mute changes seen
volatile uint32_t ctrlMediaCmds = 0;  // MCP media player state/command events seen
volatile uint32_t ctrlCallReqs = 0;   // CCP originate requests seen
volatile uint32_t haSelects = 0;      // HAS preset select requests seen
volatile uint32_t topSetLocks = 0;    // phase 8: CSIS lock changes made by the client's set coordinator
volatile uint8_t topVolume = 0;       // phase 8: last volume written (by the client's CAP commander)

// ISO transport counters — written from the ISO host task, read from loop().
volatile bool isoCisConnected = false;
volatile uint32_t isoCisRxCount = 0;
volatile uint32_t isoCisConnects = 0;  // CISes accepted in phase 2 (one per client handover stage)
volatile bool isoBisConnected = false;

// BAP unicast server sink RX counter — written from the audio host task.
volatile uint32_t bapSrvRxCount = 0;
volatile uint16_t bapSrvConn = 0xFFFF;  // audio onLinkReady handle
volatile bool bapSrvLinkDown = false;   // audio onDisconnected fired
volatile bool bapSrvDisconnectReq = false;
volatile bool bapSrvDisconnectDone = false;

// LC3 loopback (phase 6) decode metrics — written from the pipeline PCM sink.
// samples counts decoded 16-bit PCM samples; energy accumulates |amplitude| so
// the test can assert the received audio is a real tone, not silence.
volatile uint32_t lc3RxSamples = 0;
volatile uint64_t lc3RxEnergy = 0;
// Goertzel tone detector on the same PCM: the client sends a sine at exactly
// fs/16, so with a 160-sample block the tone always lands exactly on bin 10
// (no leakage) at either sample rate. tonePower/totalPower near 1 proves the
// decoded audio is the transmitted tone, not just loud noise.
constexpr float kGoertzelCoeff = 1.847759065f;  // 2*cos(2*pi*10/160)
constexpr uint32_t kGoertzelN = 160;
volatile uint32_t goertzelCount = 0;  // samples in the current block
volatile float goertzelS1 = 0.0f, goertzelS2 = 0.0f;
volatile double goertzelBlockEnergy = 0.0;
volatile double goertzelTone = 0.0;   // accumulated tone-bin power
volatile double goertzelTotal = 0.0;  // accumulated total power
volatile uint32_t goertzelBlocks = 0;

// CAP handover (phase 12) SDU counters — written from the audio host task.
// The acceptor receives on its unicast sink stream first and on its broadcast
// sink stream after the handover.
volatile uint32_t hoUnicastRx = 0;
volatile uint32_t hoBroadcastRx = 0;
volatile bool hoBigJoined = false;
volatile bool audioEndCallbackEntered = false;

// ========================= Phase coordination ================================

void checkSerial() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      buf.trim();
      if (buf == "SRV_DISC_GO") {
        bapSrvDisconnectReq = true;
      } else if (buf == "PHASE_RELEASE") {
        phaseRelease = true;
      } else if (buf.startsWith("START_PHASE_")) {
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

// Drain a leftover PHASE_RELEASE from the previous phase, then wait until
// pytest says the client is done (or the ceiling). Timeouts stay as ceilings
// so a stuck client still fails instead of hanging forever.
static void waitForRelease(unsigned long timeoutMs) {
  syncPhaseFromHost();
  phaseRelease = false;
  unsigned long deadline = millis() + timeoutMs;
  while (millis() < deadline && !phaseRelease) {
    syncPhaseFromHost();
    delay(20);
  }
}

static void beginPhaseWait() {
  syncPhaseFromHost();
  phaseRelease = false;
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
#if defined(CONFIG_IDF_TARGET_ESP32S31)
  Serial.println("[SERVER] Audio validation required: 1");
#else
  Serial.println("[SERVER] Audio validation required: 0");
#endif

  // Bring up a baseline stack so the first phase's BLE.end()/begin() cycle has a
  // live stack to tear down (each audio phase re-begins its own stack anyway).
  (void)BLE.begin(serverName);
}

// Phase 14 helper: concurrent streams() snapshot reader (see the phase 14
// block in loop()). xTaskCreate needs a plain function, so the shared state
// travels through this struct.
#if BLE_AUDIO_CAP_INITIATOR_SUPPORTED
struct CapBcReaderArg {
  BLEAudioCapInitiator *cap;
  std::atomic<bool> *stop;
  std::atomic<uint32_t> *reads;
};

static void capBcSnapshotReader(void *arg) {
  auto *p = static_cast<CapBcReaderArg *>(arg);
  while (!p->stop->load(std::memory_order_acquire)) {
    (void)p->cap->streamCount();
    (void)p->cap->stream(0);
    (void)p->cap->stream(1);
    (void)p->cap->stream(BLEAudioStream::Direction::Tx, 0);
    (void)p->cap->stream(BLEAudioStream::Direction::Tx, 1);
    p->reads->fetch_add(1, std::memory_order_relaxed);
    vTaskDelay(1);
  }
  vTaskDelete(nullptr);
}
#endif  // BLE_AUDIO_CAP_INITIATOR_SUPPORTED

void loop() {
  checkSerial();

  // Phase 1: LE Audio lifecycle + plain-GATT coexistence. Tears down and
  // re-begins BLE for a clean stack, brings up the LE Audio engine
  // (common_init), stages a plain custom GATT service, then performs the single
  // coordinated commit (common_start via audio.begin()) so the plain-GATT service
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
        // 1a) Active-GAP guard: on NimBLE, audio.begin() while advertising is
        // active must return Busy instead of panicking inside ble_svc_gap_init.
        BLEAdvertising preAdv = BLE.getAdvertising();
        preAdv.setType(BLEAdvType::ConnectableScannable);
        preAdv.setName(serverName);
        BTStatus preAdvSt = preAdv.start();
        BTStatus beginWhileAdv = audio.begin();
        (void)preAdv.stop();
        // Known backend divergence (see DESIGN.md "GATT coexistence"): NimBLE
        // must refuse the commit while GAP is active (its single GATT table
        // needs a reset, so Busy); Bluedroid registers separate GATTS apps
        // and commits fine, so end the session right away to leave the stack
        // clean for the pure-peripheral leg below.
        bool busyGuardOk;
#if BLE_NIMBLE
        busyGuardOk = (bool)preAdvSt && beginWhileAdv == BTStatus::Busy;
#else
        busyGuardOk = (bool)preAdvSt && (bool)beginWhileAdv;
        if (beginWhileAdv) {
          (void)audio.end();
        }
#endif

        // 1b) Pure-audio peripheral BEFORE BLE.createServer() has ever been
        // called this boot: proves NimBLE peripheral GAP/GATT/SMP forwarding
        // and Bluedroid peripheral audio.disconnect() work standalone.
        bool purePeriphOk = false;
#if BLE_AUDIO_VCP_RENDERER_SUPPORTED
        {
          std::atomic<uint16_t> pureConn{0xFFFF};
          std::atomic<bool> pureDown{false};
          std::atomic<uint32_t> pureVol{0};
          audio.onLinkReady([&pureConn, &pureDown](uint16_t c) {
            pureConn.store(c);
            pureDown.store(false);
          });
          audio.onDisconnected([&pureDown](uint16_t) {
            pureDown.store(true);
          });
          BLEAudioVolumeRenderer vrTmp = audio.createVolumeRenderer();  // lazy init
          BLEAudioVolumeRenderer vr(std::move(vrTmp));
          vr.setInitialVolume(42);
          vr.onStateChanged([&pureVol](uint8_t v, bool) {
            if (v == 88) {
              pureVol.fetch_add(1);
            }
          });
          BTStatus ps = vr ? audio.begin() : BTStatus::Fail;
          preAdv.reset();
          preAdv.setType(BLEAdvType::ConnectableScannable);
          preAdv.setName(serverName);
          BTStatus pa = ps ? preAdv.start() : BTStatus::Fail;
          Serial.printf("[SERVER] Audio purePeriph ready=%d\n", (int)(bool)pa);
          unsigned long pdl = millis() + 25000;
          while ((pureConn.load() == 0xFFFF || pureVol.load() < 1) && millis() < pdl) {
            checkSerial();
            delay(20);
          }
          BTStatus pdisc = pureConn.load() != 0xFFFF ? audio.disconnect(pureConn.load()) : BTStatus::Fail;
          unsigned long ddl = millis() + 5000;
          while (!pureDown.load() && millis() < ddl) {
            checkSerial();
            delay(20);
          }
          (void)preAdv.stop();
          audio.resetCallbacks();
          BTStatus pend = audio.end();
          purePeriphOk = pureConn.load() != 0xFFFF && pureVol.load() >= 1 && (bool)pdisc && pureDown.load() && (bool)pend && busyGuardOk;
          Serial.printf(
            "[SERVER] Audio purePeriph link=%d vol=%d disc=%d end=%d busyGuard=%d\n", pureConn.load() != 0xFFFF ? 1 : 0, pureVol.load() >= 1 ? 1 : 0,
            (pdisc && pureDown.load()) ? 1 : 0, (int)(bool)pend, (int)busyGuardOk
          );
        }
#else
        purePeriphOk = busyGuardOk;
        Serial.println("[SERVER] Audio purePeriph ready=1");
        Serial.printf("[SERVER] Audio purePeriph link=1 vol=1 disc=1 end=1 busyGuard=%d\n", (int)busyGuardOk);
#endif

        // 1c) Same-host audio.end() -> re-create -> audio.begin() without
        // BLE.end(), plus fast audio.end() (< 1 s, no 5 s stall) after a
        // locally rejected BroadcastSource::start(255).
        bool sameHostOk = false;
        {
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
          BLEAudioBroadcastSource failSrc = audio.createBroadcastSource();  // lazy init
#endif
          BTStatus sb = audio.isActive() ? BTStatus::OK : BTStatus::Fail;
          BTStatus ss = sb ? audio.begin() : BTStatus::Fail;
          bool badStartRejected = true;
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
          badStartRejected = ss && !failSrc.start(255);
#endif
          unsigned long t0 = millis();
          BTStatus se = audio.end();
          unsigned long endMs = millis() - t0;
          sameHostOk = (bool)sb && (bool)ss && badStartRejected && (bool)se && endMs < 1000;
          Serial.printf(
            "[SERVER] Audio sameHostReinit ok=%d badStart=%d fastEnd=%d ms=%lu\n", (int)(sb && ss && se), (int)badStartRejected, (int)(endMs < 1000), endMs
          );
        }

        // 1d) Third audio.begin() in the same BLE.begin() session, now with
        // plain-GATT BLEServer coexistence and callback-safe BLE.end().
        audio.setPresentationDelay(40000);
        // (no explicit begin: the role below triggers lazy init; the single
        // audio.begin() commits everything)
        // An audio role first: lazy engine init must precede plain-GATT staging
        // so the services stage into the coordinated (audio-owned) table
        // instead of publishing standalone.
        BLEAudioVolumeRenderer coexVol = audio.createVolumeRenderer();
        coexVol.setInitialVolume(100);

        // Coexistence: a plain custom service staged alongside the audio
        // profiles; both are committed by the single audio.begin() below.
        BLEServer srv = BLE.createServer();
        BLEService coexSvc = srv.createService(AUDIO_COEX_SVC_UUID);
        BLECharacteristic coexChr = coexSvc.createCharacteristic(AUDIO_COEX_CHAR_UUID, BLEProperty::Read | BLEProperty::Notify, BLEPermission::ReadOpen);
        coexChr.setValue("coexist");
        coexChr.onRead([](BLECharacteristic &, const BLEConnInfo &) {
          audioEndCallbackEntered = true;
          BLE.end(false);
        });
        BTStatus firstStage = srv.start();

        // Re-stage before the audio commit after extending both an existing
        // service and the service list. NimBLE must publish only the final
        // immutable definition batch (no duplicate or dangling svc_def).
        coexSvc.createCharacteristic(AUDIO_COEX_CHAR2_UUID, BLEProperty::Read, BLEPermission::ReadOpen).setValue("restaged");
        BLEService removeSvc = srv.createService(AUDIO_REMOVE_SVC_UUID);
        removeSvc.createCharacteristic(AUDIO_REMOVE_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen).setValue("remove");
        BTStatus secondStage = srv.start();
        Serial.printf("[SERVER] Audio GATT staging first=%d second=%d\n", firstStage ? 1 : 0, secondStage ? 1 : 0);

        BTStatus ab = audio.begin();  // single coordinated ble_gatts_start()
        Serial.printf("[SERVER] Audio begin: %s\n", ab.toString());
        Serial.printf("[SERVER] Audio active: %d\n", audio.isActive() ? 1 : 0);
        Serial.printf("[SERVER] Audio preset delay: %lu\n", (unsigned long)audio.getPresentationDelay());

        // Codec preset value-type + move-semantics sanity.
        BLEAudio movedAudio(std::move(audio));
        audio = std::move(movedAudio);
        BLEAudioCodecConfig cfg = BLEAudioCodecConfig::fromPreset(BLEAudioCodecPreset::LC3_16_2_1);
        Serial.printf("[SERVER] Audio preset LC3_16_2_1 sr=%lu octets=%u\n", (unsigned long)cfg.samplingRateHz, (unsigned)cfg.octetsPerFrame);

        // Once audio owns the committed table, service removal must hide only
        // that range and a late restage must be refused on both backends
        // (NimBLE's frozen table and Bluedroid's matched InvalidState).
        BTStatus removed = srv.removeService(removeSvc);
        BLEService lateSvc = srv.createService(BLEUUID("a10c0005-0000-1000-8000-00805f9b34fb"));
        lateSvc.createCharacteristic(BLEUUID("a10c0006-0000-1000-8000-00805f9b34fb"), BLEProperty::Read, BLEPermission::ReadOpen);
        BTStatus restaged = srv.start();
        // `removed` is true when the removal was accepted; the late restage
        // must be refused while audio owns the committed table.
        bool gattGuarded = static_cast<bool>(removed) && restaged == BTStatus::InvalidState;
        Serial.printf("[SERVER] Audio GATT remove=%d guarded=%d\n", removed ? 1 : 0, gattGuarded ? 1 : 0);

        // Read from the peer enters through the stack callback. BLE.end() in
        // that callback must defer, stop the still-active audio controller,
        // close retained-operation admission, and finish on its worker.
        BLEScan retainedScan = BLE.getScan();
        BLEAdvertising adv = BLE.getAdvertising();
        adv.reset();
        adv.setType(BLEAdvType::ConnectableScannable);
        adv.setName(serverName);
        adv.addServiceUUID(AUDIO_COEX_SVC_UUID);
        audioEndCallbackEntered = false;
        BTStatus advStart = adv.start();
        Serial.printf("[SERVER] Audio coex ready=%d\n", (int)(bool)advStart);
        unsigned long callbackDeadline = millis() + 20000;
        while (!audioEndCallbackEntered && millis() < callbackDeadline) {
          checkSerial();
          delay(20);
        }
        unsigned long endDeadline = millis() + 10000;
        // isInitialized() is cleared before the deferred worker finishes, so
        // wait out isEnding() too or the following BLE.begin() returns InvalidState.
        while ((BLE.isInitialized() || BLE.isEnding()) && millis() < endDeadline) {
          delay(20);
        }
        BTStatus pastAfterEnd = retainedScan.receivePeriodicSync(0xFFFF);
        BTStatus audioAfterEnd = audio.begin();
#if BLE_PAST_SUPPORTED
        bool admissionClosed = pastAfterEnd == BTStatus::InvalidState && audioAfterEnd == BTStatus::InvalidState;
#else
        bool admissionClosed = pastAfterEnd == BTStatus::NotSupported && audioAfterEnd == BTStatus::InvalidState;
#endif
        bool activeEnded = audioEndCallbackEntered && !BLE.isInitialized() && !BLE.isEnding() && !audio.isActive();
        Serial.printf(
          "[SERVER] Audio callback end=%d activeEnded=%d admission=%d\n", audioEndCallbackEntered ? 1 : 0, activeEnded ? 1 : 0, admissionClosed ? 1 : 0
        );

        // Re-initialize both BLE and audio after callback-safe terminal
        // teardown, using a different role set.
        BTStatus bleRebegin = BLE.begin(serverName);
        BTStatus rb = BTStatus::Fail;
        BTStatus rs = BTStatus::Fail;
        BTStatus aend = BTStatus::Fail;
        if (bleRebegin) {
          BLEAudioBroadcastSource reSrc = audio.createBroadcastSource();  // lazy init
          reSrc.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
          rb = (reSrc && audio.isActive()) ? BTStatus::OK : BTStatus::Fail;
          if (rb) {
            rs = audio.begin();  // commit
            aend = audio.end();
          }
        }
        Serial.printf("[SERVER] Audio reinit begin=%d start=%d\n", rb ? 1 : 0, rs ? 1 : 0);
        // GATT-first order: stage classic services BEFORE any audio role
        // exists, then commit once. The commit must adopt them into the same
        // frozen table (no separate publish survives it).
        bool gfirstOk = false;
        {
          BLEServer srvFirst = BLE.createServer();
          BLEService firstSvc = srvFirst.createService(BLEUUID("a10c0007-0000-1000-8000-00805f9b34fb"));
          auto firstChr = firstSvc.createCharacteristic(BLEUUID("a10c0008-0000-1000-8000-00805f9b34fb"), BLEProperty::Read, BLEPermission::ReadOpen);
          firstChr.setValue("first");
          bool firstPublished = (bool)srvFirst.start();                            // standalone (no audio roles yet)
          BTStatus firstCommit = firstPublished ? audio.begin() : BTStatus::Fail;  // adopt + single commit
          bool firstLive = firstCommit && firstSvc.getHandle() != 0;
          BLEService lateFirst = srvFirst.createService(BLEUUID("a10c0009-0000-1000-8000-00805f9b34fb"));
          lateFirst.createCharacteristic(BLEUUID("a10c000a-0000-1000-8000-00805f9b34fb"), BLEProperty::Read, BLEPermission::ReadOpen);
          bool firstGuarded = srvFirst.start() == BTStatus::InvalidState;
          bool firstRemoved = (bool)srvFirst.removeService(firstSvc);
          gfirstOk = (bool)firstCommit && firstLive && firstGuarded && (bool)firstRemoved;
          Serial.printf("[SERVER] Audio gattfirst live=%d guard=%d remove=%d\n", firstLive ? 1 : 0, firstGuarded ? 1 : 0, firstRemoved ? 1 : 0);
          audio.end();
        }
        if (purePeriphOk && sameHostOk && ab && firstStage && secondStage && gattGuarded && advStart && activeEnded && admissionClosed && rb && rs && aend
            && gfirstOk) {
          Serial.println("[SERVER] Audio lifecycle OK");
        } else {
          Serial.printf("[SERVER] Audio lifecycle FAILED begin=%d end=%d\n", ab ? 1 : 0, aend ? 1 : 0);
        }
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
  // accepts one inbound bidirectional CIS per client handover stage (standalone
  // transport, LE-Audio-owned host, re-claimed host), counts the transparent
  // SDUs streamed by the client and streams SDUs back on the Peripheral-to-
  // Central direction. Self-skips when host ISO is not compiled in (non-audio
  // targets), so this only truly runs on a dual-ISO (esp32s31<->esp32s31)
  // bench. Restores a normal stack afterward so memory_release still measures
  // a meaningful free.
  if (currentPhase >= 2 && !phaseIsoCisDone) {
    phaseIsoCisDone = true;
#if BLE_ISO_SUPPORTED
    BLEIso iso;  // assigned after BLE.begin(); retained across end()/begin() below
    BLE.end(false);
    delay(500);
    isoCisConnected = false;
    isoCisRxCount = 0;
    isoCisConnects = 0;
    BTStatus s = BLE.begin(serverName);
    iso = BLE.getIsoController();
    uint32_t returned = 0;
    std::atomic<bool> isoCbEndBusy{false};
    if (!s) {
      Serial.printf("[SERVER] IsoCis init FAILED: %s\n", s.toString());
    } else {
      BLEServer srv = BLE.createServer();
      srv.advertiseOnDisconnect(true);
      BLEService svc = srv.createService(SERVICE_UUID);
      auto rc = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
      rc.setValue("iso_cis");
      srv.start();

      BTStatus ib = iso.begin();
      Serial.printf("[SERVER] IsoCis transport: %s\n", ib.toString());

      BLEIso::CisParams params;    // defaults: 120 B SDU, 10 ms, 2M PHY
      params.returnSduSize = 120;  // bidirectional: the Peripheral streams back
      auto onIsoRx = [](BLEIso::Channel &, const BLEIso::SduInfo &info, const uint8_t *, uint16_t) {
        if (info.valid) {
          isoCisRxCount = isoCisRxCount + 1;
        }
      };
      // One listening channel per CIS the client brings up: the standalone
      // transport, the transport on the host the LE Audio engine owns, and the
      // transport after that engine released it. A dedicated slot per stage
      // keeps acceptance independent of how each stack retires a disconnected
      // Peripheral channel.
      BLEIso::Channel cisList[3];
      int cisN = 0;
      for (int i = 0; i < 3; i++) {
        cisList[i] = iso.listenCis(params);
        if (!cisList[i]) {
          continue;
        }
        cisN++;
        cisList[i].onConnected([&isoCbEndBusy, &iso](BLEIso::Channel &) {
          syncPhaseFromHost();
          isoCbEndBusy.store(iso.end() == BTStatus::Busy);
          isoCisConnects = isoCisConnects + 1;
          isoCisConnected = true;
        });
        cisList[i].onReceive(onIsoRx);
        cisList[i].onDisconnected([](BLEIso::Channel &, uint8_t) {
          syncPhaseFromHost();
          isoCisConnected = false;
        });
      }
      if (cisN > 0) {
        BLEAdvertising adv = BLE.getAdvertising();
        adv.reset();
        adv.setType(BLEAdvType::ConnectableScannable);
        adv.setName(serverName);
        adv.addServiceUUID(SERVICE_UUID);
        adv.start();
        Serial.println("[SERVER] IsoCis ready");

        // Let the client connect, establish the CIS, and stream SDUs; stream
        // back on the Peripheral-to-Central direction while re-binding the
        // receive callback to exercise BLEIso callback locking under load.
        // The window covers the client's three CIS stages, so it stays open
        // until pytest releases the phase (the deadline is only a backstop for
        // a client that never reports).
        uint8_t sdu[120];
        beginPhaseWait();
        unsigned long deadline = millis() + 180000;
        while (millis() < deadline && !phaseRelease) {
          syncPhaseFromHost();
          for (int i = 0; i < cisN; i++) {
            if (isoCisConnected && cisList[i].canSend()) {
              cisList[i].onReceive(onIsoRx);
              memset(sdu, (uint8_t)returned, sizeof(sdu));
              if (cisList[i].send(sdu, sizeof(sdu))) {
                returned++;
              }
            }
          }
          delay(10);
        }
      } else {
        Serial.println("[SERVER] IsoCis listen FAILED");
      }
    }
    // Leave the standalone ISO owner active and make BLE.end() release it.
    // A fresh iso.begin() after host re-init proves common_deinit did not
    // leave stale ownership or channel state behind.
    BLE.end(false);
    bool isoCoreEnded = !iso.isActive();
    delay(500);
    BTStatus restore = BLE.begin(serverName);
    iso = BLE.getIsoController();
    BTStatus isoRetry = restore ? iso.begin() : BTStatus::Fail;
    BTStatus isoCleanEnd = isoRetry ? iso.end() : BTStatus::Fail;
    bool isoLifecycle = isoCoreEnded && restore && isoRetry && isoCleanEnd && isoCbEndBusy.load();
    Serial.printf(
      "[SERVER] IsoCis result connected=%d received=%lu returned=%lu lifecycle=%d connects=%lu\n", (int)isoCisConnected, (unsigned long)isoCisRxCount,
      (unsigned long)returned, isoLifecycle ? 1 : 0, (unsigned long)isoCisConnects
    );
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
    BLEIso iso;  // assigned after BLE.begin(); retained across end()/begin() below
    BLE.end(false);
    delay(500);
    isoBisConnected = false;
    uint32_t sent = 0;
    BTStatus s = BLE.begin(serverName);
    iso = BLE.getIsoController();
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

      BTStatus ib = iso.begin();
      Serial.printf("[SERVER] IsoBis transport: %s\n", ib.toString());

      BLEIso::BigParams params;  // defaults: 120 B SDU, 10 ms, 2M PHY
      BLEIso::Channel bis = iso.createBig(0, params);
      if (bis) {
        bis.onConnected([](BLEIso::Channel &) {
          syncPhaseFromHost();
          isoBisConnected = true;
        });
        Serial.println("[SERVER] IsoBis ready");

        // The channel numbers the SDUs itself (send without a sequence number).
        uint8_t sdu[120];
        beginPhaseWait();
        unsigned long deadline = millis() + 25000;
        while (millis() < deadline && !phaseRelease) {
          syncPhaseFromHost();
          if (isoBisConnected) {
            memset(sdu, (uint8_t)sent, sizeof(sdu));
            if (bis.send(sdu, sizeof(sdu))) {
              sent++;
            }
          }
          delay(10);
        }
        // Leave the BIG connected so BLE.end() must terminate it over HCI and
        // wait for DISCONNECTED. Software-resetting a live BIG panics the controller.
      } else {
        Serial.println("[SERVER] IsoBis create FAILED");
      }
    }
    BLE.end(false);
    bool isoCoreEnded = !iso.isActive();
    delay(500);
    BTStatus restore = BLE.begin(serverName);
    iso = BLE.getIsoController();
    BTStatus isoRetry = restore ? iso.begin() : BTStatus::Fail;
    bool isoLifecycle = isoCoreEnded && restore && isoRetry;
    iso.end();
    Serial.printf("[SERVER] IsoBis result connected=%d sent=%lu lifecycle=%d\n", (int)isoBisConnected, (unsigned long)sent, isoLifecycle ? 1 : 0);
#else
    Serial.println("[SERVER] IsoBis not supported");
#endif
    Serial.println("[SERVER] Phase3 iso_bis done");
  }

  // Phase 4: BAP Unicast transport — the server is the BAP Unicast Server
  // (acceptor). It runs a plain-GATT BLEServer *and* the audio Unicast Server so
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
    bapSrvConn = 0xFFFF;
    bapSrvLinkDown = false;
    bapSrvDisconnectReq = false;
    bapSrvDisconnectDone = false;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      {
        audio.onLinkReady([](uint16_t conn) {
          bapSrvConn = conn;
          bapSrvLinkDown = false;
        });
        audio.onDisconnected([](uint16_t) {
          bapSrvLinkDown = true;
        });
        // An audio role first: lazy engine init must precede plain-GATT staging
        // so the marker service stages into the coordinated (audio-owned)
        // table instead of publishing standalone.
        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1);
        us.setSourceStreams(1);
        us.setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
        for (size_t i = 0; i < us.streamCount(); i++) {
          us.stream(i).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
            if (info.status == BLEAudioSduInfo::Status::Valid) {
              bapSrvRxCount = bapSrvRxCount + 1;
            }
          });
        }
        // Plain-GATT server contributes a marker service to the same GATT table.
        BLEServer srv = BLE.createServer();
        srv.advertiseOnDisconnect(true);
        BLEService svc = srv.createService(SERVICE_UUID);
        auto ch = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
        ch.setValue("bap");
        srv.start();  // stages in audio mode

        if (audio.begin()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.addServiceUUID(SERVICE_UUID);
          adv.start();
          Serial.println("[SERVER] BapUnicast ready");
          // Ceiling covers the client's worst case. Exit as soon as the
          // peripheral-link disconnect (the last client-driven step) finishes
          // or pytest releases the phase.
          beginPhaseWait();
          unsigned long deadline = millis() + 120000;
          while (millis() < deadline && !bapSrvDisconnectDone && !phaseRelease) {
            syncPhaseFromHost();
            if (bapSrvDisconnectReq && !bapSrvDisconnectDone) {
              if (bapSrvConn != 0xFFFF) {
                BTStatus d = audio.disconnect(bapSrvConn);
                unsigned long downWait = millis() + 5000;
                while (!bapSrvLinkDown && millis() < downWait) {
                  syncPhaseFromHost();
                  delay(50);
                }
                Serial.printf("[SERVER] BapUnicast peripheralDisconnect ok=%d down=%d\n", (int)(bool)d, (int)bapSrvLinkDown);
              } else {
                Serial.println("[SERVER] BapUnicast peripheralDisconnect ok=0 down=0");
              }
              bapSrvDisconnectDone = true;
            }
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
    BLEAudio audio;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      audio = BLE.getAudioController();
      {
        BLEAudioBroadcastSource source = audio.createBroadcastSource();
        source.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
        source.setBroadcastId(0x123456);
        source.setName(serverName);
        source.onStarted([&bcStreaming]() {
          bcStreaming = true;
        });
        source.onStopped([&bcStreaming](uint8_t) {
          bcStreaming = false;
        });
        if (audio.begin() && source.start()) {
          Serial.println("[SERVER] BapBroadcast ready");
          beginPhaseWait();
          unsigned long deadline = millis() + 40000;
          uint8_t sdu[40];
          uint8_t n = 0;
          BLEAudioStream src = source.stream(0);
          while (millis() < deadline && !phaseRelease) {
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
          // Leave the BIG up so BLE.end() must stop leftover BAP and wait for ISO idle.
        }
      }
    }
    BLE.end(false);
    bool audioCoreEnded = !audio.isActive();
    delay(500);
    BTStatus restore = BLE.begin(serverName);
    BTStatus retry = BTStatus::Fail;
    if (restore) {
      audio = BLE.getAudioController();
      retry = audio.begin();
      if (retry) {
        (void)audio.end();
      }
    }
    bool audioLifecycle = audioCoreEnded && restore && retry;
    Serial.printf("[SERVER] BapBroadcast result sent=%lu lifecycle=%d\n", (unsigned long)sent, audioLifecycle ? 1 : 0);
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
  if (currentPhase >= 6 && !phaseLC3LoopbackDone) {
    phaseLC3LoopbackDone = true;
#if BLE_AUDIO_LC3_SUPPORTED && BLE_AUDIO_UNICAST_SERVER_SUPPORTED
    for (int lc3Round = 0; lc3Round < 2; lc3Round++) {
      BLE.end(false);
      delay(500);
      lc3RxSamples = 0;
      lc3RxEnergy = 0;
      goertzelCount = 0;
      goertzelS1 = goertzelS2 = 0.0f;
      goertzelBlockEnergy = 0.0;
      goertzelTone = 0.0;
      goertzelTotal = 0.0;
      goertzelBlocks = 0;
      uint32_t plcFrames = 0;
      uint32_t playerUnderruns = 0;
      bool streamSnapOk = false;
      BTStatus s = BLE.begin(serverName);
      if (s) {
        BLEAudio audio = BLE.getAudioController();
        {
          BLEAudioUnicastServer us = audio.createUnicastServer();
          us.setSinkStreams(1);
          us.setSourceStreams(0);
          us.setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
          us.setSupportedPresets(BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_441_2_1));

          // PCM sink instead of I2S pins: decoded frames land here on the codec task.
          BLEAudioPlayer player;
          player.setPCMSink([](const int16_t *pcm, size_t n) {
            lc3RxSamples = lc3RxSamples + (uint32_t)n;
            uint64_t e = 0;
            for (size_t i = 0; i < n; i++) {
              int v = pcm[i];
              e += (v < 0) ? (uint64_t)(-v) : (uint64_t)v;
              float x = (float)v * (1.0f / 32768.0f);
              float ws = x + kGoertzelCoeff * goertzelS1 - goertzelS2;
              goertzelS2 = goertzelS1;
              goertzelS1 = ws;
              goertzelBlockEnergy = goertzelBlockEnergy + (double)x * (double)x;
              if (++goertzelCount >= kGoertzelN) {
                float tp = goertzelS1 * goertzelS1 + goertzelS2 * goertzelS2 - kGoertzelCoeff * goertzelS1 * goertzelS2;
                if (tp > 0.0f) {
                  goertzelTone = goertzelTone + (double)tp;
                }
                goertzelTotal = goertzelTotal + goertzelBlockEnergy;
                goertzelBlocks = goertzelBlocks + 1;
                goertzelS1 = goertzelS2 = 0.0f;
                goertzelBlockEnergy = 0.0;
                goertzelCount = 0;
              }
            }
            lc3RxEnergy = lc3RxEnergy + e;
          });

          if (audio.begin()) {
            // Exercise immediate startPlayback()->stopPlayback()->startPlayback()
            // handshake (regression for BLEAudioDataPath::start()/entry() race).
            BTStatus pb = player.begin();
            BTStatus pa = pb ? player.attach(us.stream(0)) : pb;
            BTStatus psRapid = pa ? player.startPlayback() : pa;
            if (psRapid) {
              player.stopPlayback();
            }
            BTStatus ps = psRapid ? player.startPlayback() : psRapid;
            Serial.printf("[SERVER] LC3Loopback player: %s\n", ps.toString());
            if (ps) {
              BLEAdvertising adv = BLE.getAdvertising();
              adv.reset();
              adv.setType(BLEAdvType::ConnectableScannable);
              adv.setName(serverName);
              adv.start();
              Serial.println("[SERVER] LC3Loopback ready");
              syncPhaseFromHost();
              phaseRelease = false;
              unsigned long deadline = millis() + 60000;
              while (millis() < deadline && !phaseRelease) {
                syncPhaseFromHost();
                if (us.stream(0).isStreaming()) {
                  BLEAudioCodecConfig sc = us.stream(0).codecConfig();
                  BLEAudioQos sq = us.stream(0).qos();
                  const bool snapOk =
                    (lc3Round == 0) ? (sc.samplingRateHz == 16000 && sq.sduIntervalUs == 10000) : (sc.samplingRateHz == 44100 && sq.sduIntervalUs == 10884);
                  if (snapOk) {
                    streamSnapOk = true;
                  }
                }
                delay(20);
              }
            }
            plcFrames = player.plcFrames();
            playerUnderruns = player.underruns();
            player.end();
          }
          audio.end();
        }
        unsigned long long avg = (lc3RxSamples && streamSnapOk) ? (unsigned long long)(lc3RxEnergy / lc3RxSamples) : 0ull;
        unsigned tonePermille = 0;
        if (goertzelBlocks > 0 && goertzelTotal > 0.0) {
          tonePermille = (unsigned)(1000.0 * goertzelTone / goertzelTotal);
          if (tonePermille > 1000) {
            tonePermille = 1000;
          }
        }
        if (lc3Round == 0) {
          Serial.printf(
            "[SERVER] LC3Loopback result samples=%lu meanamp=%llu plc=%lu underruns=%lu tone=%u\n", (unsigned long)lc3RxSamples, avg, (unsigned long)plcFrames,
            (unsigned long)playerUnderruns, tonePermille
          );
        } else {
          Serial.printf(
            "[SERVER] LC3Loopback44 result samples=%lu meanamp=%llu plc=%lu underruns=%lu tone=%u\n", (unsigned long)lc3RxSamples, avg,
            (unsigned long)plcFrames, (unsigned long)playerUnderruns, tonePermille
          );
        }
      }
    }  // end LC3 rounds
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] LC3Loopback not supported");
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
    ctrlMediaCmds = 0;
    ctrlCallReqs = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioCapAcceptor cap = audio.createCapAcceptor();
        cap.setSetSize(2);
        cap.setRank(1);

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
        media.onCommandResult([](BLEAudioMediaCommand cmd, BTStatus st) {
          if (cmd == BLEAudioMediaCommand::Play && st) {
            ctrlMediaCmds = ctrlMediaCmds + 1;
          }
        });

        BLEAudioCallServer call = audio.createCallServer();
        call.setProviderName("ESP Phone");
        call.onOriginate([](uint8_t callIndex, const String &uri) -> bool {
          (void)callIndex;
          (void)uri;
          ctrlCallReqs = ctrlCallReqs + 1;
          return true;
        });

        if (audio.begin()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          Serial.println("[SERVER] ControlProfiles ready");
          waitForRelease(60000);
        }
        audio.end();
      }
      Serial.printf(
        "[SERVER] ControlProfiles result volwrites=%lu micmutes=%lu media=%lu calls=%lu\n", (unsigned long)ctrlVolWrites, (unsigned long)ctrlMicMutes,
        (unsigned long)ctrlMediaCmds, (unsigned long)ctrlCallReqs
      );
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
  // Both sides run the whole cycle twice without a BLE restart: the second
  // re-creation must re-link every profile callback that common_deinit
  // released, or the client's rediscovery observes nothing. Restores a normal
  // stack afterward.
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
      bool reinitStarted = false;
      for (int round = 0; round < 2; round++) {
        // Create roles first (lazy engine init); the commit below may
        // legitimately report Busy while the previous round's link is still
        // tearing down (GAP busy). Busy is retryable, anything else is
        // terminal. Bounded so a stuck stack still fails fast.
        // Underlying roles that back the advertised TMAP/GMAP identities.
        BLEAudioCapAcceptor cap = audio.createCapAcceptor();
        cap.setSetSize(1);
        cap.setRank(1);
        cap.coordinatedSet().onLockChanged([](bool locked, uint16_t connHandle) {
          (void)locked;
          (void)connHandle;
          topSetLocks = topSetLocks + 1;
        });
        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1);
        us.setSourceStreams(1);
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
        gmap.setRoles(BLEAudioGmapRole::UnicastGameTerminal);
        gmap.setFeatures(gf);

        BTStatus bst = audio.begin();  // commit
        for (int att = 0; att < 3 && bst == BTStatus::Busy; att++) {
          delay(500);
          bst = audio.begin();
        }
        if (!bst) {
          audio.end();
          break;
        }
        bool started = (bool)bst;
        if (started) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
        }
        Serial.printf("[SERVER] TopProfiles %sready registered=%d\n", round == 0 ? "" : "reinit ", (int)started);
        if (!started) {
          audio.end();
          break;
        }
        waitForRelease(60000);
        audio.end();
        if (round == 1) {
          reinitStarted = true;
        }
      }
      Serial.printf("[SERVER] TopProfiles result done locks=%lu volume=%u reinit=%d\n", (unsigned long)topSetLocks, (unsigned)topVolume, (int)reinitStarted);
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
      {
        BLEAudioHearingAidDevice ha = audio.createHearingAidDevice();
        ha.setType(BLEHearingAidType::Binaural);
        ha.setPresetSync(false);
        ha.addPreset(1, "Universal");
        ha.addPreset(2, "Outdoor");
        ha.addPreset(3, "Restaurant");
        ha.onPresetSelected([](uint8_t index, bool sync) {
          (void)index;
          (void)sync;
          haSelects = haSelects + 1;
        });

        if (audio.begin()) {
          ha.setActivePreset(1);
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          Serial.println("[SERVER] HearingAid ready");
          waitForRelease(35000);
          // Round 2: the client cycles audio.end() -> re-create roles and
          // reconnects to re-verify discovery after re-registration; the
          // server side needs no reinit for this, just re-advertise.
          adv.start();
          Serial.println("[SERVER] HearingAid reinit ready");
          waitForRelease(35000);
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
    unsigned long runStart = 0;
    uint8_t bis = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioBroadcastSource source = audio.createBroadcastSource();
        source.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
        source.setChannels(2);
        source.setName(serverName);
        source.setBroadcastCode(STEREO_BROADCAST_CODE);
        source.onStarted([&bcStreaming]() {
          bcStreaming = true;
        });
        source.onStopped([&bcStreaming](uint8_t) {
          bcStreaming = false;
        });
        if (audio.begin() && source.start()) {
          bis = (uint8_t)source.streamCount();
          Serial.printf("[SERVER] Stereo ready bis=%u\n", bis);
          uint8_t left[40], right[40];
          memset(left, 0x11, sizeof(left));
          memset(right, 0x22, sizeof(right));
          unsigned long deadline = millis() + 40000;
          while (millis() < deadline) {
            if (bcStreaming && bis == 2) {
              if (runStart == 0) {
                runStart = millis();
              }
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
      unsigned long elapsed = runStart ? millis() - runStart : 0;
      uint32_t rate = elapsed ? (uint32_t)((uint64_t)sent * 2 * 1000 / elapsed) : 0;
      Serial.printf("[SERVER] Stereo result bis=%u sent=%lu ms=%lu rate=%lu\n", bis, (unsigned long)sent, elapsed, (unsigned long)rate);
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
      {
        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setScanDelegator(true);
        sink.setAutoSync(false);
        if (sink && audio.begin()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.addServiceUUID(BLEUUID((uint16_t)0x184F));  // Broadcast Audio Scan Service
          adv.start();
          ready = true;
          Serial.println("[SERVER] BroadcastAssistant ready");
          waitForRelease(70000);
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
      {
        BLEAudioCapAcceptor cap = audio.createCapAcceptor();
        cap.setSetSize(1);
        cap.setRank(1);

        BLEAudioUnicastServer us = audio.createUnicastServer();
        us.setSinkStreams(1);
        us.setSourceStreams(0);
        us.setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
        us.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
          if (info.status == BLEAudioSduInfo::Status::Valid) {
            hoUnicastRx = hoUnicastRx + 1;
          }
        });

        BLEAudioBroadcastSink sink = audio.createBroadcastSink();
        sink.setScanDelegator(true);
        sink.setAutoSync(false);
        sink.onStarted([]() {
          hoBigJoined = true;
        });
        sink.stream(0).onReceive([](BLEAudioStream &, const BLEAudioSduInfo &info, const uint8_t *, uint16_t) {
          if (info.status == BLEAudioSduInfo::Status::Valid) {
            hoBroadcastRx = hoBroadcastRx + 1;
          }
        });

        if (cap && sink && audio.begin()) {
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          Serial.println("[SERVER] CapHandover ready");
          waitForRelease(100000);
          sink.stop();
        }
        audio.end();
      }
      // The CAP initiator builds a PBA for its broadcast advertising; check the
      // same PBP helper round-trips before the phase reports success.
      uint8_t pba[32];
      const uint8_t pbaMeta[] = {3, 0x02, 0x00, 0x00};
      const size_t pbaLen = BLEAudioPublicBroadcast::buildAnnouncement(BLEAudioPublicBroadcast::HighQuality, pbaMeta, sizeof(pbaMeta), pba, sizeof(pba));
      uint8_t pbaFeatures = 0;
      const uint8_t *pbaOut = nullptr;
      size_t pbaOutLen = 0;
      const bool pbaOk = pbaLen > 2 && BLEAudioPublicBroadcast::parseAnnouncement(pba, pbaLen, pbaFeatures, pbaOut, pbaOutLen)
                         && pbaFeatures == BLEAudioPublicBroadcast::HighQuality && pbaOutLen == sizeof(pbaMeta);
      Serial.printf(
        "[SERVER] CapHandover result unicast=%lu joined=%d broadcast=%lu pba=%d\n", (unsigned long)hoUnicastRx, (int)hoBigJoined, (unsigned long)hoBroadcastRx,
        pbaOk ? 1 : 0
      );
    }
    BLE.end(false);
    delay(500);
    (void)BLE.begin(serverName);
#else
    Serial.println("[SERVER] CapHandover not supported");
#endif
    Serial.println("[SERVER] Phase12 cap_handover done");
  }

  // Phase 13: BroadcastSource start()/stop() race — stop() called immediately
  // after start(), before the BIG is up, N times in a row. Regression for the
  // stop()-while-starting fix: stop() must wait out the async start (stopSync),
  // return OK every time, fire onStopped for every terminated BIG, and leave
  // the role able to stream cleanly afterward. Server-side stress; the client
  // only observes.
  if (currentPhase >= 13 && !phaseStopRaceDone) {
    phaseStopRaceDone = true;
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
    BLE.end(false);
    delay(500);
    uint8_t races = 0, okStart = 0, okStop = 0;
    std::atomic<uint32_t> stoppedEvents{0};
    volatile bool bcStreaming = false;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioBroadcastSource source = audio.createBroadcastSource();
        source.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
        source.setChannels(1);
        source.setName(serverName);
        source.onStarted([&bcStreaming]() {
          bcStreaming = true;
        });
        source.onStopped([&bcStreaming, &stoppedEvents](uint8_t) {
          bcStreaming = false;
          stoppedEvents.fetch_add(1, std::memory_order_relaxed);
        });
        if (audio.begin()) {
          for (int i = 0; i < 5; i++) {
            BTStatus st = source.start();
            if (st) {
              okStart++;
            }
            // No wait for onStarted: stop() must resolve the async start itself.
            st = source.stop();
            if (st) {
              okStop++;
            }
            races++;
            delay(300);
            syncPhaseFromHost();
          }
          // A clean session must still work after the interleaved stops, and
          // must handle a >32-octet UTF-8 Broadcast Name straddling byte 32
          // (PBP v1.0 §3.1.1 UTF-8 boundary truncation).
          source.setName("UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU\xC3\xA9");
          bool clean = false;
          bcStreaming = false;
          if (source.start()) {
            unsigned long deadline = millis() + 10000;
            while (millis() < deadline && !bcStreaming) {
              syncPhaseFromHost();
              delay(20);
            }
            uint8_t payload[40];
            memset(payload, 0x33, sizeof(payload));
            clean = bcStreaming && source.stream(0).write(payload, sizeof(payload));
            source.stop();
            deadline = millis() + 6000;
            while (millis() < deadline && bcStreaming) {
              syncPhaseFromHost();
              delay(20);
            }
          }
          audio.end();
          Serial.printf(
            "[SERVER] StopRace result races=%u startOk=%u stopOk=%u stoppedEv=%lu clean=%d\n", races, okStart, okStop, (unsigned long)stoppedEvents.load(),
            clean ? 1 : 0
          );
        } else {
          Serial.println("[SERVER] StopRace audio begin failed");
        }
      }
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    } else {
      Serial.println("[SERVER] StopRace not supported");
    }
#else
    Serial.println("[SERVER] StopRace not supported");
#endif
    Serial.println("[SERVER] Phase13 bcast_stop_race done");
  }

  // Phase 14: CAP initiator startBroadcast()/stopBroadcast() interleave with
  // concurrent stream()/streamCount() snapshot reads from a separate task.
  // Regression for the by-value streams() snapshot and the atomic slot fields:
  // no crash, torn handle or stuck state while the streams vector is rebuilt.
  // Server-side stress; the client only observes.
  if (currentPhase >= 14 && !phaseCapBcDone) {
    phaseCapBcDone = true;
#if BLE_AUDIO_CAP_INITIATOR_SUPPORTED
    BLE.end(false);
    delay(500);
    uint8_t rounds = 0, okStart = 0, okStop = 0;
    std::atomic<uint32_t> stoppedEvents{0};
    std::atomic<uint32_t> snapshotReads{0};
    std::atomic<bool> snapStop{false};
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioCapInitiator cap = audio.createCapInitiator();
        cap.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
        cap.setBroadcastName(serverName);
        cap.onBroadcastStarted([]() {});
        cap.onBroadcastStopped([&stoppedEvents](uint8_t) {
          stoppedEvents.fetch_add(1, std::memory_order_relaxed);
        });
        if (audio.begin()) {
          // Concurrent snapshot reader: hammers streamCount()/stream() while
          // the session vectors are resized between 1 and 2 BIS channels below.
          CapBcReaderArg readerArg = {&cap, &snapStop, &snapshotReads};
          TaskHandle_t reader = nullptr;
          xTaskCreate(capBcSnapshotReader, "snapread", 4096, &readerArg, 5, &reader);
          for (int i = 0; i < 5; i++) {
            BTStatus st = cap.startBroadcast((i % 2) ? 2 : 1);
            if (st) {
              okStart++;
            } else {
              Serial.printf("[SERVER] CapBC round %d start failed: %s\n", i, st.toString());
            }
            delay(400);
            syncPhaseFromHost();
            st = cap.stopBroadcast();
            if (st) {
              okStop++;
            } else {
              Serial.printf("[SERVER] CapBC round %d stop failed: %s streaming=%d\n", i, st.toString(), cap.isBroadcasting() ? 1 : 0);
            }
            rounds++;
            delay(200);
            syncPhaseFromHost();
          }
          // A clean 2-BIS CAP broadcast must stream after the interleaved
          // resizes (also exercises per-BIS QoS on slots[1]).
          bool clean = false;
          BTStatus cleanStart = cap.startBroadcast(2);
          if (cleanStart) {
            unsigned long deadline = millis() + 10000;
            while (millis() < deadline && !cap.isBroadcasting()) {
              syncPhaseFromHost();
              delay(20);
            }
            clean = cap.isBroadcasting() && cap.streamCount() == 2;
            if (!clean) {
              Serial.printf("[SERVER] CapBC clean not streaming (broadcasting=%d streams=%u)\n", cap.isBroadcasting() ? 1 : 0, (unsigned)cap.streamCount());
            }
            cap.stopBroadcast();
          } else {
            Serial.printf("[SERVER] CapBC clean start failed: %s\n", cleanStart.toString());
          }
          snapStop.store(true, std::memory_order_release);
          unsigned long deadline = millis() + 3000;
          while (reader && millis() < deadline && eTaskGetState(reader) != eDeleted) {
            delay(10);
          }
          audio.end();
          Serial.printf(
            "[SERVER] CapBC result rounds=%u startOk=%u stopOk=%u stoppedEv=%lu reads=%lu clean=%d\n", rounds, okStart, okStop,
            (unsigned long)stoppedEvents.load(), (unsigned long)snapshotReads.load(), clean ? 1 : 0
          );
        } else {
          snapStop.store(true, std::memory_order_release);
          Serial.println("[SERVER] CapBC audio begin failed");
        }
      }
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    } else {
      Serial.println("[SERVER] CapBC not supported");
    }
#else
    Serial.println("[SERVER] CapBC not supported");
#endif
    Serial.println("[SERVER] Phase14 cap_bc_interleave done");
  }

  // Phase 15: start() must refuse a retry after a mid-start failure instead of
  // running half-registered roles again. Exhaust the broadcast-sink stream
  // pool so a sink's apply() fails, assert the retry contract, then prove
  // end()/begin() recovers. Server-driven; the client only observes.
  if (currentPhase >= 15 && !phaseStartRetryDone) {
    phaseStartRetryDone = true;
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
    BLE.end(false);
    delay(500);
    int firstRc = 0, retryRc = -99, recovered = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      // Pool sizes vary by build; add sinks until one apply() fails.
      for (int n = 2; n <= 6 && firstRc == 0; n++) {
        BLEAudio audio = BLE.getAudioController();
        for (int i = 0; i < n; i++) {
          BLEAudioBroadcastSink sink = audio.createBroadcastSink();  // lazy init
          sink.setStreams(2);
        }
        if (!audio.isActive()) {
          firstRc = -1;  // Test setup broken; reported as failure below.
          break;
        }
        firstRc = (int)audio.begin().code();
        if (firstRc != 0) {
          retryRc = (int)audio.begin().code();
        }
        audio.end();
      }
      if (firstRc != 0) {
        BLEAudio audio = BLE.getAudioController();
        {
          BLEAudioBroadcastSink sink = audio.createBroadcastSink();  // lazy init
          sink.setStreams(1);
          if (audio.isActive()) {
            recovered = audio.begin() ? 1 : 0;
            audio.end();
          }
        }
      }
#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED
      // PACS boot-pinning: re-begin with different supported contexts must
      // still start (keeps serving the first value, clamps available).
      int pacsFirst = 0, pacsSecond = 0;
      {
        BLEAudio audio = BLE.getAudioController();
        {
          BLEAudioUnicastServer us = audio.createUnicastServer();
          us.setSinkStreams(1);
          us.setSourceStreams(0);
          us.setSinkContexts(BLEAudioContext::Media);
          pacsFirst = audio.begin() ? 1 : 0;
          audio.end();
        }
      }
      {
        BLEAudio audio = BLE.getAudioController();
        {
          BLEAudioUnicastServer us = audio.createUnicastServer();
          us.setSinkStreams(1);
          us.setSourceStreams(0);
          us.setSinkContexts(BLEAudioContext::Conversational);
          pacsSecond = audio.begin() ? 1 : 0;
          audio.end();
        }
      }
#else
      int pacsFirst = -1, pacsSecond = -1;
#endif
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
      // end() contract: safe to call twice, and refused with Busy from a
      // stack callback instead of deadlocking. The in-callback end() runs on
      // the host task when the BIG comes up.
      int endTwice = -1, endFromCb = -99, endRebegin = 0;
      {
        BLEAudio audio = BLE.getAudioController();
        {
          BLEAudioBroadcastSource src = audio.createBroadcastSource();
          src.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
          src.setChannels(1);
          src.setName(serverName);
          volatile bool cbEntered = false;
          src.onStarted([&]() {
            // Host task: must refuse with Busy, never wedge the stack task.
            endFromCb = (int)audio.end().code();
            cbEntered = true;
          });
          if (audio.begin() && src.start()) {
            unsigned long t0 = millis();
            while (!cbEntered && millis() - t0 < 15000) {
              checkSerial();
              delay(50);
            }
            (void)src.stop();
            audio.end();
            BTStatus second = audio.end();
            endTwice = (second == BTStatus::OK || second == BTStatus::InvalidState) ? 1 : 0;
            // Lazy init: no begin() — the factory must initialize the engine
            // (isActive) and begin() must commit without it.
            endRebegin = 0;
            {
              BLEAudioBroadcastSource lazy = audio.createBroadcastSource();
              if (lazy && audio.isActive()) {
                lazy.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
                lazy.setChannels(1);
                lazy.setName(serverName);
                endRebegin = audio.begin() ? 1 : 0;
              }
            }
            audio.end();
          }
        }
      }
#else
      int endTwice = -1, endFromCb = -99, endRebegin = -1;
#endif
#if BLE_AUDIO_CCP_SERVER_SUPPORTED
      // Over-long TBS strings must fail the start instead of truncating.
      int tbsLong = 0;
      {
        BLEAudio audio = BLE.getAudioController();
        {
          BLEAudioCallServer call = audio.createCallServer();
          String big;
          for (int i = 0; i < 100; i++) {
            big += 'P';
          }
          call.setProviderName(big);
          tbsLong = audio.begin() ? 0 : 1;
          audio.end();
        }
      }
#else
      int tbsLong = -1;
#endif
      Serial.printf(
        "[SERVER] StartRetry first=%d retry=%d recovered=%d pacs=%d%d tbslong=%d endtwice=%d endfromcb=%d endrebegin=%d\n", firstRc, retryRc, recovered,
        pacsFirst, pacsSecond, tbsLong, endTwice, endFromCb, endRebegin
      );
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    } else {
      Serial.println("[SERVER] StartRetry not supported");
    }
#else
    Serial.println("[SERVER] StartRetry not supported");
#endif
    Serial.println("[SERVER] Phase15 start_retry_refused done");
  }

  // Phase 16: the audio security policy is explicit. Default on (existing
  // sketches keep working), opt-out via setAutoSecurityPolicy(false) with a
  // warning when nothing configures BLESecurity. Server-driven markers only.
  if (currentPhase >= 16 && !phaseSecPolicyDone) {
    phaseSecPolicyDone = true;
#if BLE_AUDIO_SUPPORTED
    BLE.end(false);
    delay(500);
    int dflt = 0, off = 0, beginOk = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      dflt = audio.autoSecurityPolicy() ? 1 : 0;
      audio.setAutoSecurityPolicy(false);
      off = !audio.autoSecurityPolicy() ? 1 : 0;
      if (audio.begin()) {
        beginOk = 1;
        audio.end();
      }
      audio.setAutoSecurityPolicy(true);
      Serial.printf("[SERVER] SecPolicy default=%d setOff=%d beginOk=%d\n", dflt, off, beginOk);
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    } else {
      Serial.println("[SERVER] SecPolicy not supported");
    }
#else
    Serial.println("[SERVER] SecPolicy not supported");
#endif
    Serial.println("[SERVER] Phase16 security_policy done");
  }

  // Phase 17: ISO handle validation. connectCis() on 0xFFFF or a dead link
  // must return nullptr before any CIG exists (fail closed, no HCI). Needs
  // the CIS central role; server-driven, no link required.
  if (currentPhase >= 17 && !phaseIsoGatesDone) {
    phaseIsoGatesDone = true;
#if BLE_ISO_CIS_CENTRAL_SUPPORTED
    BLE.end(false);
    delay(500);
    int nullFFFF = 0, nullDead = 0;
    BTStatus s = BLE.begin(serverName);
    BLEIso iso = BLE.getIsoController();
    if (s && iso.begin()) {
      BLEIso::CisParams qos;
      nullFFFF = (!iso.connectCis(0xFFFF, qos)) ? 1 : 0;
      nullDead = (!iso.connectCis(0x0042, qos)) ? 1 : 0;
      Serial.printf("[SERVER] IsoGates nullFFFF=%d nullDead=%d\n", nullFFFF, nullDead);
      iso.end();
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    } else {
      Serial.println("[SERVER] IsoGates not supported");
    }
#else
    Serial.println("[SERVER] IsoGates not supported");
#endif
    Serial.println("[SERVER] Phase17 iso_handle_gates done");
  }

  // Phase 18: MICS mute tri-state. This side disables mute up front; the
  // client must observe disabled (not unmuted) on its initial discovery read
  // and its unmute() must fail fast with InvalidState instead of failing at
  // the peer with no signal. Disabling first (rather than mid-link) keeps the
  // test deterministic: no cross-DUT write/notification race.
  if (currentPhase >= 18 && !phaseMicDisableDone) {
    phaseMicDisableDone = true;
#if BLE_AUDIO_MICP_DEVICE_SUPPORTED
    BLE.end(false);
    delay(500);
    int disabled = 0;
    BTStatus s = BLE.begin(serverName);
    if (s) {
      BLEAudio audio = BLE.getAudioController();
      {
        BLEAudioMicDevice mic = audio.createMicDevice();
        if (audio.begin() && mic.disableMute()) {
          disabled = mic.isMuteDisabled() ? 1 : 0;
          BLEAdvertising adv = BLE.getAdvertising();
          adv.reset();
          adv.setType(BLEAdvType::ConnectableScannable);
          adv.setName(serverName);
          adv.start();
          // Hold the link while the client verifies (deadline, then clean up).
          unsigned long t1 = millis();
          while (millis() - t1 < 45000) {
            checkSerial();
            delay(50);
          }
          audio.end();
        }
      }
      Serial.printf("[SERVER] MicDisable disabled=%d\n", disabled);
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    } else {
      Serial.println("[SERVER] MicDisable not supported");
    }
#else
    Serial.println("[SERVER] MicDisable not supported");
#endif
    Serial.println("[SERVER] Phase18 mic_mute_disabled done");
  }

  // Phase 19: Memory release (tail of the suite; runs after every audio phase
  // has restored a normal stack, including the phase 13/14 stress rounds).
  if (currentPhase >= 19 && !phaseMemoryDone) {
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
