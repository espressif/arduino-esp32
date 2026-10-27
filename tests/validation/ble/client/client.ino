// Combined BLE validation test — CLIENT
// Phases: basic lifecycle, BLE5 ext+periodic scan, GATT client, notifications,
//         large writes, descriptors, write-no-response, server disconnect,
//         security, PHY/DLE, reconnect

#include <Arduino.h>
#include <BLE.h>
#include "core/BLEValidationHooks.h"
#include <atomic>

static_assert(BLE_VALIDATION_HOOKS, "validation sketches require -DBLE_VALIDATION_HOOKS");
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <vector>

static const BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID rwCharUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static const BLEUUID notifyCharUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
static const BLEUUID indicateCharUUID("d5f782b2-a36e-4d68-947c-0e9a5f2c78e1");
static const BLEUUID secureCharUUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");
static const BLEUUID descCharUUID("a3c87501-8ed3-4bdf-8a39-a01bebede295");
static const BLEUUID writeNrCharUUID("1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e");
// Phase 4 / Phase 7: the server's onRead handlers produce a fresh value per read.
static const BLEUUID dynamicReadCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a35");
static const BLEUUID dynamicReadDescUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a36");
// Phase 4 / Phase 7: 512-byte values produced per read by the server's onRead.
static const BLEUUID longReadCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a37");
static const BLEUUID longReadDescUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a38");
static const BLEUUID snapshotCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3a");
static const BLEUUID dualCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3b");
static const BLEUUID tmpSvcUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3c");
static const BLEUUID tmpCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3d");
static const BLEUUID busySvcUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3e");
static const BLEUUID busyCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3f");
static const BLEUUID edgeSvcUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a40");
static const BLEUUID peerCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a41");
static const BLEUUID queueSvcUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a42");
static const BLEUUID queueCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a43");
static const BLEUUID timeoutCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a44");
static const BLEUUID writableDescUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a45");
// Defined before any function so the Arduino preprocessor's injected prototypes
// can name this type (it inserts them above the first function).
struct LongReadJob {
  BLERemoteCharacteristic chr;
  String values[8];
  size_t lengths[8] = {};
  char gens[8] = {};
  bool uniform[8] = {};
  int count = 0;
  uint32_t timeoutMs = 3000;
  std::atomic<bool> *go = nullptr;
  std::atomic<bool> done{false};
};
struct GattWriteJob {
  BLERemoteCharacteristic chr;
  const char *value = nullptr;
  int count = 0;
  bool withResponse = true;
  std::atomic<bool> *go = nullptr;
  std::atomic<int> failures{0};
  std::atomic<int> busyRetries{0};
  std::atomic<bool> done{false};
};
struct ServiceDiscoveryRaceJob {
  BLERemoteService service;
  BLEUUID uuid;
  bool aggregate = false;
  std::atomic<bool> *go = nullptr;
  std::atomic<bool> entered{false};
  std::atomic<bool> done{false};
  std::atomic<size_t> resultCount{0};
  BLERemoteCharacteristic result;
};
struct DescriptorDiscoveryRaceJob {
  BLERemoteCharacteristic chr;
  BLEUUID uuid;
  bool aggregate = false;
  std::atomic<bool> *go = nullptr;
  std::atomic<bool> entered{false};
  std::atomic<bool> done{false};
  std::atomic<size_t> resultCount{0};
  BLERemoteDescriptor result;
};
struct BLEStreamShutdownRaceJob {
  BLEStream *stream = nullptr;
  std::atomic<bool> streamEndDone{false};
  std::atomic<bool> hostEndDone{false};
};
struct ScanShutdownJob {
  std::atomic<bool> started{false};
  std::atomic<bool> done{false};
};
struct ScanHostShutdownJob {
  std::atomic<bool> done{false};
};
#if BLE_L2CAP_SUPPORTED
struct L2CAPRaceJob {
  BLEL2CAPChannel channel;
  const uint8_t *data = nullptr;
  size_t len = 0;
  BTStatus status = BTStatus::Fail;
  uint16_t mtu = 0;
  volatile bool started = false;
  volatile bool done = false;
};
#endif
// Phase 4: value the server set through a handle from a const lookup.
static const BLEUUID constLookupCharUUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a39");
static const BLEUUID permFailClosedCharUUID("e8b1d3a1-6b3e-4a5d-9f1d-9b7e5c3a2f41");
// Phase 15: custom-UUID descriptor on DESC_CHAR declared as read-only. The
// client reads it (should succeed) and then writes to it (should be rejected
// at the ATT layer), exercising runtime enforcement of the descriptor
// permissions on both NimBLE and Bluedroid.
static const BLEUUID descReadOnlyUUID("b5f3e2c1-8a9e-4b7c-9f3d-2e8a5b7c3f91");
// Phase 18 (authorization) + Phase 19 (encrypted).
static const BLEUUID authzCharUUID("ca11aaaa-1111-4222-8333-444455556666");
static const BLEUUID encryptedCharUUID("ca11bbbb-1111-4222-8333-444455556666");
// Phase 23 (error paths) — these UUIDs are advertised as absent.
static const BLEUUID unknownSvcUUID("deadbeef-0000-0000-0000-000000000001");
static const BLEUUID unknownCharUUID("deadbeef-0000-0000-0000-000000000002");

String targetName;
BTAddress targetAddr;
volatile int currentPhase = 0;

// Phase 16 — client-side MTU + conn-params callback state. Set from the BLE
// stack task; logged from the Arduino task after the phase start marker.
volatile bool clientMtuChanged = false;
volatile uint16_t clientMtuLast = 0;
volatile bool clientConnParamsUpdated = false;
// Phase 16 — event-ordering guard: onMtuChanged must fire AFTER onConnect on
// both backends (see DESIGN.md backend contract: Client MTU exchange). clientConnectFired
// is set in onConnect; clientMtuAfterConnect snapshots it inside onMtuChanged so
// the harness can catch a regression where the MTU request precedes onConnect
// (the historical Bluedroid CONNECT_EVT behavior).
volatile bool clientConnectFired = false;
volatile bool clientMtuAfterConnect = false;
// Ordering guards: set inside onResult if the device is already in
// getResults() (cache-before-callback), and inside onDisconnect if the
// link state is already torn down (teardown-before-callback).
volatile bool scanCacheInCallbackOk = false;
volatile bool clientDisconnectTornDown = false;
// Phase 10 — onIdentity must fire exactly once per connection on both backends
// (NimBLE surfaces identity on both IDENTITY_RESOLVED and ENC_CHANGE and is
// deduped in the library; Bluedroid fires it once from AUTH_CMPL). Reset before
// the phase-10 pairing connect; asserted == 1 after pairing.
volatile int clientIdentityCount = 0;
volatile uint16_t clientConnParamsReqInterval = 0;
volatile uint16_t clientConnParamsReqLatency = 0;
volatile uint16_t clientConnParamsReqTimeout = 0;
// Phase 29 — when armed, the onConnParamsUpdateRequest pre-accept hook rejects
// the peripheral's request (returns false) instead of accepting it. Also latches
// whether the hook fired at all so the phase can self-skip on Bluedroid, which
// has no pre-accept hook.
volatile bool clientRejectConnParams = false;
volatile bool clientConnParamsReqFired = false;
// Phase 31 (NC reject) / Phase 32 (passkey entry). When clientRejectPairing is
// armed the numeric-comparison confirm handler returns false to reject the
// pairing. clientLastAuthSuccess latches the success flag reported by
// onAuthenticationComplete so the phases can assert failure/success.
volatile bool clientRejectPairing = false;
volatile bool clientAuthCompleteFired = false;
volatile bool clientLastAuthSuccess = false;
volatile bool phase23ReAdvGo = false;
volatile bool phase23ReAdvDisc = false;
volatile bool phase36ConnectGo = false;
volatile bool phase36OobGo = false;
volatile bool phase36OobScGo = false;
// Phase 32 — the central is KeyboardOnly and supplies this fixed passkey (shown
// by the DisplayOnly peripheral via a matching static passkey) so passkey-entry
// pairing succeeds deterministically without a human in the loop.
static const uint32_t PASSKEY_ENTRY_STATIC = 398215;

// Cross-stack event-order parity trace. Each client lifecycle callback appends a
// short tag to a fixed-size in-RAM sequence; the phase-10 pairing lifecycle emits
// it as "ORDER=<tag,tag,...>" so test_ble.py can assert the same golden order on
// NimBLE (esp32s3) and Bluedroid (esp32). Allocation-free and callback-safe: tags
// are string literals and appends happen on the single BLE host task; the trace is
// reset from the Arduino task before the phase-10 connect (no callback in flight).
static const char *clientOrderTags[16];
volatile int clientOrderN = 0;
volatile bool phase13MalformedGo = false;
volatile bool phase28StallGo = false;
volatile bool phase28CleanupGo = false;
volatile bool phase28EndGo = false;
volatile bool phase35DisconnectGo = false;
volatile bool phase35ReadRemoveGo = false;
volatile bool phase35StreamReleaseGo = false;
static void recordClientEvent(const char *tag) {
  int n = clientOrderN;
  if (n < (int)(sizeof(clientOrderTags) / sizeof(clientOrderTags[0]))) {
    clientOrderTags[n] = tag;
    clientOrderN = n + 1;
  }
}
static void reportClientOrder(const char *label) {
  String seq;
  int n = clientOrderN;
  for (int i = 0; i < n; i++) {
    if (i) {
      seq += ",";
    }
    seq += clientOrderTags[i];
  }
  Serial.printf("[CLIENT] %s ORDER=%s\n", label, seq.c_str());
}

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
      } else if (buf == "PHASE13_MALFORMED_GO") {
        phase13MalformedGo = true;
      } else if (buf == "PHASE28_STALL_GO") {
        phase28StallGo = true;
      } else if (buf == "PHASE28_CLEANUP_GO") {
        phase28CleanupGo = true;
      } else if (buf == "PHASE28_END_GO") {
        phase28EndGo = true;
      } else if (buf == "PHASE35_DISCONNECT_GO") {
        phase35DisconnectGo = true;
      } else if (buf == "PHASE35_READ_REMOVE_GO") {
        phase35ReadRemoveGo = true;
      } else if (buf == "PHASE35_STREAM_RELEASE_GO") {
        phase35StreamReleaseGo = true;
      } else if (buf == "PHASE23_READV_GO") {
        phase23ReAdvGo = true;
      } else if (buf == "PHASE23_READV_DISC") {
        phase23ReAdvDisc = true;
      } else if (buf == "PHASE36_CONNECT_GO") {
        phase36ConnectGo = true;
      } else if (buf == "PHASE36_OOB_GO") {
        phase36OobGo = true;
      } else if (buf == "PHASE36_OOB_SC_GO") {
        phase36OobScGo = true;
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

// Same ordering guarantee as server: apply START_PHASE_* before BLE callback side effects.
static void syncPhaseFromHost() {
  checkSerial();
}

void waitForPhase(int n) {
  while (currentPhase < n) {
    checkSerial();
    delay(10);
  }
}

// The server fills each long value with a single letter that advances per
// onRead call, so a read assembled from more than one onRead is not uniform.
static void reportLongRead(const char *label, int index, const String &v) {
  bool uniform = v.length() > 0;
  for (size_t i = 1; uniform && i < v.length(); i++) {
    uniform = (v[i] == v[0]);
  }
  Serial.printf("[CLIENT] %s %d: len=%u gen=%c uniform=%d\n", label, index, (unsigned)v.length(), v.length() ? v[0] : '?', (int)uniform);
}

static bool valueIsUniform(const String &v) {
  if (v.length() == 0) {
    return false;
  }
  for (size_t i = 1; i < v.length(); i++) {
    if (v[i] != v[0]) {
      return false;
    }
  }
  return true;
}

static void fillLongReadJob(LongReadJob *job, int index, const String &v) {
  job->values[index] = v;
  job->lengths[index] = v.length();
  job->gens[index] = v.length() ? v[0] : '?';
  job->uniform[index] = valueIsUniform(v);
}

static void longReadJobTask(void *arg) {
  auto *job = static_cast<LongReadJob *>(arg);
  while (job->go && !job->go->load(std::memory_order_acquire)) {
    taskYIELD();
  }
  for (int i = 0; i < job->count; i++) {
    fillLongReadJob(job, i, job->chr.readValue(job->timeoutMs));
  }
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void gattWriteJobTask(void *arg) {
  auto *job = static_cast<GattWriteJob *>(arg);
  while (job->go && !job->go->load(std::memory_order_acquire)) {
    taskYIELD();
  }
  for (int i = 0; i < job->count; i++) {
    BTStatus status = BTStatus::Busy;
    for (int retry = 0; retry < 100 && status == BTStatus::Busy; retry++) {
      status = job->chr.writeValue(String(job->value), job->withResponse);
      if (status == BTStatus::Busy) {
        job->busyRetries.fetch_add(1, std::memory_order_relaxed);
        delay(1);
      }
    }
    if (!status) {
      job->failures.fetch_add(1, std::memory_order_relaxed);
    }
  }
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void serviceDiscoveryRaceTask(void *arg) {
  auto *job = static_cast<ServiceDiscoveryRaceJob *>(arg);
  while (job->go && !job->go->load(std::memory_order_acquire)) {
    taskYIELD();
  }
  job->entered.store(true, std::memory_order_release);
  size_t count = 0;
  if (job->aggregate) {
    auto results = job->service.getCharacteristics();
    count = results.size();
    if (!results.empty()) {
      job->result = results.front();
    }
  } else {
    job->result = job->service.getCharacteristic(job->uuid);
    count = job->result ? 1 : 0;
  }
  job->resultCount.store(count, std::memory_order_relaxed);
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void descriptorDiscoveryRaceTask(void *arg) {
  auto *job = static_cast<DescriptorDiscoveryRaceJob *>(arg);
  while (job->go && !job->go->load(std::memory_order_acquire)) {
    taskYIELD();
  }
  job->entered.store(true, std::memory_order_release);
  size_t count = 0;
  if (job->aggregate) {
    auto results = job->chr.getDescriptors();
    count = results.size();
    if (!results.empty()) {
      job->result = results.front();
    }
  } else {
    job->result = job->chr.getDescriptor(job->uuid);
    count = job->result ? 1 : 0;
  }
  job->resultCount.store(count, std::memory_order_relaxed);
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void bleStreamShutdownRaceEndTask(void *arg) {
  auto *job = static_cast<BLEStreamShutdownRaceJob *>(arg);
  job->stream->end();
  job->streamEndDone.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void bleStreamShutdownRaceHostTask(void *arg) {
  auto *job = static_cast<BLEStreamShutdownRaceJob *>(arg);
  BLE.end(false);
  job->hostEndDone.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void scanShutdownTask(void *arg) {
  auto *job = static_cast<ScanShutdownJob *>(arg);
  job->started.store(true, std::memory_order_release);
  (void)BLE.getScan().startBlocking(0);
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void scanHostShutdownTask(void *arg) {
  auto *job = static_cast<ScanHostShutdownJob *>(arg);
  BLE.end(false);
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

#if BLE_L2CAP_SUPPORTED
static uint8_t phase28RacePayload[4096];
static uint8_t phase28ShutdownPayload[8192];

static void l2capRaceWriteTask(void *arg) {
  auto *job = static_cast<L2CAPRaceJob *>(arg);
  job->started = true;
  job->status = job->channel.write(job->data, job->len);
  job->done = true;
  vTaskDelete(nullptr);
}

static void l2capRaceMtuTask(void *arg) {
  auto *job = static_cast<L2CAPRaceJob *>(arg);
  job->started = true;
  job->mtu = job->channel.getMTU();
  job->done = true;
  vTaskDelete(nullptr);
}

static void l2capRaceDisconnectTask(void *arg) {
  auto *job = static_cast<L2CAPRaceJob *>(arg);
  job->started = true;
  job->status = job->channel.disconnect();
  job->done = true;
  vTaskDelete(nullptr);
}

static bool l2capRaceStatusClean(BTStatus status) {
  return status == BTStatus::OK || status == BTStatus::Busy || status == BTStatus::NotConnected || status == BTStatus::InvalidState;
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

// Scan onComplete runs on BTC / the NimBLE host task. BLE.end() from that
// callback must reject a same-callback begin() with InvalidState, then recover.
static bool phase1DeferredEndCycle(const String &rebeginName, bool injectWorkerFail) {
  std::atomic<bool> completed{false};
  std::atomic<int> during{static_cast<int>(BTStatus::Fail)};
  BLEScan scan = BLE.getScan();
  scan.resetCallbacks();
  scan.onComplete([&](BLEScan::Results &) {
    scan.resetCallbacks();
    if (injectWorkerFail) {
      BLEHooks.lifecycle.endWorkerStartFailures.store(1);
    }
    BLE.end(false);
    during.store(static_cast<int>(BLE.begin(rebeginName + "_during")), std::memory_order_relaxed);
    completed.store(true, std::memory_order_release);
  });
  BTStatus started = scan.start(200);
  unsigned long deadline = millis() + 5000;
  while (started && !completed.load(std::memory_order_acquire) && millis() < deadline) {
    delay(1);
  }
  int beginDuring = during.load(std::memory_order_relaxed);
  scan.resetCallbacks();
  BLE.end(false);
  BTStatus recovered = BTStatus::Fail;
  unsigned long rebeginDeadline = millis() + 8000;
  while (millis() < rebeginDeadline) {
    recovered = BLE.begin(rebeginName);
    if (recovered) {
      break;
    }
    delay(20);
  }
  return started && completed.load(std::memory_order_acquire) && beginDuring == static_cast<int>(BTStatus::InvalidState) && recovered;
}

// ========================= Phase 1 — Basic Lifecycle =========================

bool phase_basic() {
  // Stack init has previously overrun a heap block and smashed the tail canary,
  // which only surfaced as a corruption panic much later, in unrelated code.
  // Walk every heap region on both sides of the first begin() to catch it here.
  bool heapCleanBefore = heap_caps_check_integrity_all(true);
  BTStatus status = BLE.begin(targetName + "_client");
  bool heapCleanAfter = heap_caps_check_integrity_all(true);
  Serial.printf("[CLIENT] Phase1 heapIntegrity before=%d after=%d\n", (int)heapCleanBefore, (int)heapCleanAfter);
  if (!status) {
    Serial.printf("[CLIENT] Init FAILED: %s\n", status.toString());
    return false;
  }
  Serial.printf("[CLIENT] Stack: %s\n", BLE.getStackName());
  Serial.println("[CLIENT] Init OK");
  Serial.printf("[CLIENT] Device name: %s\n", BLE.getDeviceName().c_str());
  Serial.printf("[CLIENT] Address: %s\n", BLE.getAddress().toString().c_str());

  // Exercise end()/begin() before any scan, client, security, or L2CAP role
  // installs its lazy hooks, then prove roles can be installed in the new
  // session.
  BLE.end(false);
  status = BLE.begin(targetName + "_client_bare_reinit");
  if (!status) {
    Serial.printf("[CLIENT] Bare reinit FAILED: %s\n", status.toString());
    return false;
  }
  Serial.println("[CLIENT] Bare lifecycle OK");

  bool deferredDuring = phase1DeferredEndCycle(targetName + "_client_deferred", false);
  bool deferredFailSpawn = phase1DeferredEndCycle(targetName + "_client_deferred_fail", true);
  BLE.end(false);
  BLE.end(true);
  BTStatus afterIdleTrue = BLE.begin(targetName + "_client_idle_true");
  BLE.end(false);
  BTStatus afterIdleTrueEnd = BLE.begin(targetName + "_client_idle_true_end");
  Serial.printf(
    "[CLIENT] Phase1 deferredEnd during=%d failSpawn=%d idleTrue=%d afterIdle=%d heap=%d\n", (int)deferredDuring, (int)deferredFailSpawn,
    (int)(bool)afterIdleTrue, (int)(bool)afterIdleTrueEnd, (int)heap_caps_check_integrity_all(true)
  );
  if (!deferredDuring || !deferredFailSpawn || !afterIdleTrue || !afterIdleTrueEnd) {
    return false;
  }

  // An unlimited blocking scan must be released by BLE.end(). Wait for the
  // public scan state rather than a delay so shutdown overlaps a known active
  // native session on both backends.
  static ScanShutdownJob scanShutdown;
  bool scanTaskStarted = xTaskCreate(scanShutdownTask, "scan-shutdown", 4096, &scanShutdown, 1, nullptr) == pdPASS;
  BLEScan shutdownScan = BLE.getScan();
  unsigned long scanStartDeadline = millis() + 5000;
  while (scanTaskStarted && !shutdownScan.isScanning() && millis() < scanStartDeadline) {
    delay(1);
  }
  bool scanWasActive = shutdownScan.isScanning();
  bool bluedroidScan = strcmp(BLE.getStackName(), "Bluedroid") == 0;
  bool injectCancelFailure = true;
  BLEHooks.scan.cancelFailures.store(1);
  bool shutdownAdmissionClean = true;
  if (bluedroidScan) {
    static ScanHostShutdownJob hostShutdown;
    hostShutdown.done.store(false, std::memory_order_relaxed);
    BLEHooks.scan.bluedroidShutdownEntered.store(false, std::memory_order_relaxed);
    BLEHooks.scan.holdBluedroidShutdown.store(true, std::memory_order_relaxed);
    bool hostTaskStarted = xTaskCreate(scanHostShutdownTask, "scan-host-end", 4096, &hostShutdown, 1, nullptr) == pdPASS;
    unsigned long gateDeadline = millis() + 5000;
    while (hostTaskStarted && !BLEHooks.scan.bluedroidShutdownEntered.load(std::memory_order_acquire) && millis() < gateDeadline) {
      delay(1);
    }
    bool gateClosed = BLEHooks.scan.bluedroidShutdownEntered.load(std::memory_order_acquire);
    BTStatus duringShutdown = gateClosed ? shutdownScan.start(100) : BTStatus::Fail;
    BTStatus stopDuringShutdown = gateClosed ? shutdownScan.stop() : BTStatus::Fail;
    BLEHooks.scan.holdBluedroidShutdown.store(false, std::memory_order_release);
    while (hostTaskStarted && !hostShutdown.done.load(std::memory_order_acquire)) {
      delay(1);
    }
    if (!hostTaskStarted) {
      BLE.end(false);
    }
    BTStatus afterShutdown = shutdownScan.start(100);
    BTStatus stopAfterShutdown = shutdownScan.stop();
    shutdownAdmissionClean = hostTaskStarted && gateClosed && duringShutdown == BTStatus::InvalidState && stopDuringShutdown == BTStatus::OK
                             && afterShutdown == BTStatus::InvalidState && stopAfterShutdown == BTStatus::OK;
  } else {
    BLE.end(false);
    shutdownAdmissionClean = shutdownScan.start(100) == BTStatus::InvalidState && shutdownScan.stop() == BTStatus::OK;
  }
  unsigned long scanEndDeadline = millis() + 5000;
  while (scanTaskStarted && !scanShutdown.done.load() && millis() < scanEndDeadline) {
    delay(1);
  }
  bool scanReleased = scanShutdown.done.load();
  bool cancelFallbackClean = !injectCancelFailure || BLEHooks.scan.cancelFailures.load() == 0;
  Serial.printf(
    "[CLIENT] Blocking scan shutdown task=%d active=%d released=%d cancelFallback=%d admission=%d heap=%d\n", (int)scanTaskStarted, (int)scanWasActive,
    (int)scanReleased, (int)cancelFallbackClean, (int)shutdownAdmissionClean, (int)heap_caps_check_integrity_all(true)
  );
  if (!scanTaskStarted || !scanWasActive || !scanReleased || !cancelFallbackClean || !shutdownAdmissionClean) {
    return false;
  }
  status = BLE.begin(targetName + "_client_scan_reinit");
  if (!status) {
    Serial.printf("[CLIENT] Scan reinit FAILED: %s\n", status.toString());
    return false;
  }

  // NVS outlives reflashing, so bonds from an earlier run would otherwise leak
  // into the pairing phases and make them depend on whatever state the boards
  // were left in. Wipe once here, at boot, before any phase pairs.
  {
    BLESecurity sec = BLE.getSecurity();
    size_t stale = sec.getBondedDevices().size();
    (void)sec.deleteAllBonds();
    Serial.printf("[CLIENT] Boot bond wipe: stale=%u bonds=%u\n", (unsigned)stale, (unsigned)sec.getBondedDevices().size());
  }

  BLE.end(false);
  Serial.println("[CLIENT] Deinit OK");
  delay(1000);

  status = BLE.begin(targetName + "_client_reinit");
  if (!status) {
    Serial.printf("[CLIENT] Reinit FAILED: %s\n", status.toString());
    return false;
  }
  Serial.println("[CLIENT] Reinit OK");

  BLE.end(false);
  Serial.println("[CLIENT] Final deinit OK");
  return true;
}

// ============= Phase 2 — BLE5 Extended Scan + Periodic Sync =================

#if BLE5_SUPPORTED
bool phase_ble5_scan() {
  BTStatus status = BLE.begin("BLE_CLT_Ext");
  if (!status) {
    return false;
  }

  volatile bool found = false;
  volatile bool synced = false;
  volatile bool dataReceived = false;

  BLEScan scan = BLE.getScan();

  scan.onResult([&](BLEAdvertisedDevice dev) {
    syncPhaseFromHost();
    if (!found && dev.getName() == targetName) {
      Serial.println("[CLIENT] Found target via ext scan!");
      targetAddr = dev.getAddress();
      found = true;
      scan.createPeriodicSync(targetAddr, 1);
    }
  });

  scan.onPeriodicSync([&](uint16_t handle, uint8_t sid, const BTAddress &addr, BLEPhy phy, uint16_t interval) {
    syncPhaseFromHost();
    Serial.println("[CLIENT] Synced to periodic adv!");
    synced = true;
  });

  scan.onPeriodicReport([&](uint16_t syncHandle, int8_t rssi, int8_t txPower, const uint8_t *data, size_t len) {
    syncPhaseFromHost();
    if (!dataReceived) {
      Serial.println("[CLIENT] Periodic data received");
      dataReceived = true;
      BLE.getScan().stop();
    }
  });

  Serial.println("[CLIENT] BLE5 init OK");
  Serial.println("[CLIENT] Scanning for periodic adv...");
  scan.startExtended(15000);

  // startExtended() is non-blocking (like start(); only startBlocking() waits),
  // so spin here while the background ext-scan + periodic-sync/report callbacks
  // fire. Bail out early once periodic data has arrived. Up to ~15 s, which
  // overlaps the server's 20 s BLE5 adv window.
  for (int i = 0; i < 150 && !dataReceived; i++) {
    delay(100);
  }

  if (found) {
    Serial.println("[CLIENT] Extended scan found target");
  }
  if (synced) {
    Serial.println("[CLIENT] Periodic sync confirmed");
  }
  if (dataReceived) {
    Serial.println("[CLIENT] Periodic test complete");
  }

  // No explicit periodic-sync teardown here: BLE.end() terminates any syncs the
  // library is tracking while the host is still enabled (see
  // BLEScan::terminateAllPeriodicSyncs). Leaving one active must NOT produce the
  // "failed to terminate periodic sync ... rc=30" (BLE_HS_EDISABLED) log.
  scan.stop();
  BLE.end(false);
  Serial.println("[CLIENT] BLE5 scan phase done");
  return true;
}
#endif

// ======================== Phase 3+ — GATT Client =============================

bool scanForServer() {
  bool found = false;
  BLEScan scan = BLE.getScan();
  scan.onResult([&](BLEAdvertisedDevice dev) {
    syncPhaseFromHost();
    if (!found && dev.haveName() && dev.getName().length() > 0) {
      Serial.printf("[CLIENT] Scan saw name=%s\n", dev.getName().c_str());
    }
    if (!found && dev.getName() == targetName) {
      // DESIGN ordering: results are cached before onResult runs, so the
      // device must already be visible through getResults() here.
      for (const auto &cached : scan.getResults()) {
        if (cached.getAddress() == dev.getAddress()) {
          scanCacheInCallbackOk = true;
          break;
        }
      }
      Serial.println("[CLIENT] Found target server!");
      targetAddr = dev.getAddress();
      found = true;
      BLE.getScan().stop();
    }
  });

  for (int attempt = 1; attempt <= 5 && !found; attempt++) {
    Serial.printf("[CLIENT] Scanning (attempt %d)...\n", attempt);
    (void)scan.startBlocking(5000);
    if (!found) {
      delay(200);
    }
  }
  scan.resetCallbacks();
  return found;
}

static void waitForHostGo(volatile bool &flag, uint32_t timeoutMs) {
  uint32_t deadline = millis() + timeoutMs;
  while (!flag && (int32_t)(deadline - millis()) > 0) {
    checkSerial();
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();

  // ===== Phase 1: Basic lifecycle =====
  waitForPhase(1);
  if (!phase_basic()) {
    return;
  }

  // ===== Phase 2: BLE5 ext adv + periodic =====
  waitForPhase(2);
  {
    bool ble5_ok = false;
#if BLE5_SUPPORTED
    ble5_ok = phase_ble5_scan();
#endif
    if (!ble5_ok) {
      Serial.println("[CLIENT] BLE5 not supported, skipping");
    }
  }

  // ===== Phase 3: Init for GATT tests =====
  waitForPhase(3);

  size_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap before init: %u\n", (unsigned)heapBefore);

  BTStatus status = BLE.begin("BLE_CLT");
  if (!status) {
    Serial.printf("[CLIENT] GATT init FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[CLIENT] GATT init OK");
  BLE.setMTU(512);

  size_t heapAfterInit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap after init: %u\n", (unsigned)heapAfterInit);

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLEIOCapability::DisplayYesNo);
  sec.setAuthenticationMode(true, true, true);
  sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t passkey) {
    Serial.printf("[CLIENT] Passkey: %06lu\n", (unsigned long)passkey);
  });
  sec.onConfirmPassKey([](const BLEConnInfo &conn, uint32_t passkey) -> bool {
    Serial.printf("[CLIENT] Passkey: %06lu\n", (unsigned long)passkey);
    // Phase 31 arms a numeric-comparison reject; every other phase confirms.
    return !clientRejectPairing;
  });
  // Passkey-entry (phase 32): as the KeyboardOnly central, supply the same fixed
  // passkey the DisplayOnly peripheral shows. Harmless in other phases (only
  // invoked when the negotiated method is Passkey Entry).
  sec.onPassKeyRequest([](const BLEConnInfo &conn) -> uint32_t {
    Serial.printf("[CLIENT] PassKeyRequest -> %06lu\n", (unsigned long)PASSKEY_ENTRY_STATIC);
    return PASSKEY_ENTRY_STATIC;
  });
  sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
    Serial.printf("[CLIENT] Authentication complete success=%d\n", (int)success);
    clientAuthCompleteFired = true;
    clientLastAuthSuccess = success;
  });

  // Callback helpers installed on every BLEClient we create so phase 16
  // can assert MTU + conn-params events fired on the client side too.
  auto installClientCallbacks = [](BLEClient c) {
    c.onConnect([](BLEClient, const BLEConnInfo &) {
      syncPhaseFromHost();
      clientConnectFired = true;
      recordClientEvent("CONNECT");
    });
    c.onMtuChanged([](BLEClient, const BLEConnInfo &, uint16_t mtu) {
      syncPhaseFromHost();
      // Snapshot connect-fired state to enforce onConnect-before-onMtuChanged.
      clientMtuAfterConnect = clientConnectFired;
      clientMtuLast = mtu;
      clientMtuChanged = true;
      recordClientEvent("MTU");
    });
    c.onConnParamsUpdateRequest([](BLEClient, const BLEConnParams &p) -> bool {
      syncPhaseFromHost();
      clientConnParamsReqInterval = p.maxInterval;
      clientConnParamsReqLatency = p.latency;
      clientConnParamsReqTimeout = p.timeout;
      clientConnParamsUpdated = true;
      clientConnParamsReqFired = true;
      // Phase 29 arms a rejection; every other phase accepts.
      return !clientRejectConnParams;
    });
    c.onIdentity([](const BLEClient &, const BLEConnInfo &) {
      syncPhaseFromHost();
      clientIdentityCount = clientIdentityCount + 1;
      recordClientEvent("IDENTITY");
    });
    c.onDisconnect([](BLEClient &cl, const BLEConnInfo &, uint8_t) {
      syncPhaseFromHost();
      // DESIGN ordering: connection state and the discovered tree are
      // torn down before onDisconnect is delivered.
      clientDisconnectTornDown = !cl.isConnected() && cl.getServices().empty();
    });
  };

  if (!scanForServer()) {
    Serial.println("[CLIENT] Server not found");
    return;
  }

  uint32_t connectStart = millis();
  BLEClient client = BLE.createClient();
  installClientCallbacks(client);
  status = client.connect(targetAddr);
  uint32_t connectTime = millis() - connectStart;
  if (!status) {
    Serial.printf("[CLIENT] Connect FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[CLIENT] Connected");
  Serial.printf("[CLIENT] ScanCacheInCallback ok=%d\n", (int)scanCacheInCallbackOk);
  Serial.printf("[CLIENT] ConnectCallbackBeforeReturn ok=%d\n", (int)clientConnectFired);
  Serial.printf("[CLIENT] Connect time: %lu ms\n", (unsigned long)connectTime);

  uint16_t mtu = client.getMTU();
  Serial.printf("[CLIENT] Negotiated MTU: %u\n", mtu);

  size_t heapAfterConnect = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap after connect: %u\n", (unsigned)heapAfterConnect);

  BLERemoteService svc = client.getService(serviceUUID);
  if (!svc) {
    Serial.println("[CLIENT] Service not found");
    return;
  }
  Serial.println("[CLIENT] Found service");

  // ===== Phase 4: GATT read/write =====
  waitForPhase(4);

  BLERemoteCharacteristic rwChr = svc.getCharacteristic(rwCharUUID);
  if (!rwChr) {
    Serial.println("[CLIENT] RW characteristic not found");
    return;
  }
  Serial.println("[CLIENT] Found characteristic");

  uint64_t readStart = esp_timer_get_time();
  String val = rwChr.readValue();
  uint64_t readEnd = esp_timer_get_time();
  Serial.printf("[CLIENT] Read value: %s\n", val.c_str());
  Serial.printf("[CLIENT] Read latency: %lu us\n", (unsigned long)(readEnd - readStart));

  status = rwChr.writeValue("Hello from client!");
  if (status) {
    Serial.println("[CLIENT] Write OK");
  } else {
    Serial.printf("[CLIENT] Write FAILED: %s\n", status.toString());
  }

  val = rwChr.readValue();
  Serial.printf("[CLIENT] Read-back: %s\n", val.c_str());

  {
    BLERemoteCharacteristic dynamicChr = svc.getCharacteristic(dynamicReadCharUUID);
    if (!dynamicChr) {
      Serial.println("[CLIENT] Dynamic read char not found");
    } else {
      for (int i = 1; i <= 2; i++) {
        Serial.printf("[CLIENT] Dynamic read %d: %s\n", i, dynamicChr.readValue().c_str());
      }
      const BLERemoteCharacteristic &constChr = dynamicChr;
      size_t rawLen = 0;
      const uint8_t *raw = constChr.readRawData(&rawLen);
      Serial.printf("[CLIENT] Dynamic cached: %.*s\n", (int)rawLen, raw ? (const char *)raw : "");
    }

    BLERemoteCharacteristic longChr = svc.getCharacteristic(longReadCharUUID);
    if (!longChr) {
      Serial.println("[CLIENT] Long read char not found");
    } else {
      for (int i = 1; i <= 2; i++) {
        reportLongRead("Long read", i, longChr.readValue());
      }
      String boundaryVal = longChr.readValue();
      uint16_t expectedBoundaryLen = client.getMTU() > 1 ? (client.getMTU() - 1) : 512;
      Serial.printf(
        "[CLIENT] Long read boundary: len=%u expected=%u gen=%c uniform=%d\n", (unsigned)boundaryVal.length(), (unsigned)expectedBoundaryLen,
        boundaryVal.length() ? boundaryVal[0] : '?', (int)valueIsUniform(boundaryVal)
      );
    }

    // The server rewrites this value continuously during Phase 4.
    BLERemoteCharacteristic snapshotChr = svc.getCharacteristic(snapshotCharUUID);
    if (!snapshotChr) {
      Serial.println("[CLIENT] Snapshot char not found");
    } else {
      for (int i = 1; i <= 10; i++) {
        reportLongRead("Snapshot read", i, snapshotChr.readValue());
      }
    }

    BLERemoteCharacteristic constLookupChr = svc.getCharacteristic(constLookupCharUUID);
    if (!constLookupChr) {
      Serial.println("[CLIENT] Const lookup char not found");
    } else {
      Serial.printf("[CLIENT] Const lookup read: %s\n", constLookupChr.readValue().c_str());

      // Bluedroid has one read and one write completion slot per client. Run
      // same-client callers together to prove each blocking operation retains
      // its slot through completion instead of replacing another caller's
      // pending handle/synchronizer state.
      static std::atomic<bool> readGo{false};
      static LongReadJob readA;
      static LongReadJob readB;
      readA.chr = rwChr;
      readA.count = 4;
      readA.go = &readGo;
      readB.chr = constLookupChr;
      readB.count = 4;
      readB.go = &readGo;
      bool readCreatedA = xTaskCreate(longReadJobTask, "gatt-read-a", 6144, &readA, 1, nullptr) == pdPASS;
      bool readCreatedB = xTaskCreate(longReadJobTask, "gatt-read-b", 6144, &readB, 1, nullptr) == pdPASS;
      readGo.store(true, std::memory_order_release);
      unsigned long raceWait = millis();
      while (((readCreatedA && !readA.done.load(std::memory_order_acquire)) || (readCreatedB && !readB.done.load(std::memory_order_acquire)))
             && millis() - raceWait < 30000) {
        delay(10);
      }
      bool readsClean = readCreatedA && readCreatedB && readA.done.load(std::memory_order_acquire) && readB.done.load(std::memory_order_acquire);
      for (int i = 0; i < 4 && readsClean; i++) {
        readsClean = readA.values[i] == "Hello from client!" && readB.values[i] == "set_via_const_lookup";
      }

      static std::atomic<bool> writeGo{false};
      static GattWriteJob writeA;
      static GattWriteJob writeB;
      writeA.chr = rwChr;
      writeA.value = "race-a";
      writeA.count = 3;
      writeA.go = &writeGo;
      writeB.chr = rwChr;
      writeB.value = "race-b";
      writeB.count = 3;
      writeB.go = &writeGo;
      bool writeCreatedA = xTaskCreate(gattWriteJobTask, "gatt-write-a", 6144, &writeA, 1, nullptr) == pdPASS;
      bool writeCreatedB = xTaskCreate(gattWriteJobTask, "gatt-write-b", 6144, &writeB, 1, nullptr) == pdPASS;
      writeGo.store(true, std::memory_order_release);
      raceWait = millis();
      while (((writeCreatedA && !writeA.done.load(std::memory_order_acquire)) || (writeCreatedB && !writeB.done.load(std::memory_order_acquire)))
             && millis() - raceWait < 70000) {
        delay(10);
      }
      bool writesClean = writeCreatedA && writeCreatedB && writeA.done.load(std::memory_order_acquire) && writeB.done.load(std::memory_order_acquire)
                         && writeA.failures.load() == 0 && writeB.failures.load() == 0;

      static std::atomic<bool> commandGo{false};
      static GattWriteJob commandA;
      static GattWriteJob commandB;
      commandA.chr = rwChr;
      commandA.value = "cmd-a";
      commandA.count = 3;
      commandA.withResponse = false;
      commandA.go = &commandGo;
      commandB.chr = rwChr;
      commandB.value = "cmd-b";
      commandB.count = 3;
      commandB.withResponse = false;
      commandB.go = &commandGo;
      bool commandCreatedA = xTaskCreate(gattWriteJobTask, "gatt-cmd-a", 6144, &commandA, 1, nullptr) == pdPASS;
      bool commandCreatedB = xTaskCreate(gattWriteJobTask, "gatt-cmd-b", 6144, &commandB, 1, nullptr) == pdPASS;
      commandGo.store(true, std::memory_order_release);
      raceWait = millis();
      while (((commandCreatedA && !commandA.done.load(std::memory_order_acquire)) || (commandCreatedB && !commandB.done.load(std::memory_order_acquire)))
             && millis() - raceWait < 70000) {
        delay(10);
      }
      bool commandsClean = commandCreatedA && commandCreatedB && commandA.done.load(std::memory_order_acquire) && commandB.done.load(std::memory_order_acquire)
                           && commandA.failures.load() == 0 && commandB.failures.load() == 0;
      Serial.printf("[CLIENT] Concurrent GATT reads=%d writes=%d commands=%d\n", (int)readsClean, (int)writesClean, (int)commandsClean);
    }

    // Two BLEClient instances on this board share one ACL with the peripheral
    // (one local address). Abandon the in-flight long read by dropping that
    // link, then reconnect so later phases still have a live svc/rwChr.
    static LongReadJob abandon;
    abandon.chr = snapshotChr;
    abandon.count = 1;
    abandon.timeoutMs = 2000;
    abandon.done.store(false, std::memory_order_relaxed);
    if (snapshotChr && xTaskCreate(longReadJobTask, "abandon", 8192, &abandon, 1, nullptr) == pdPASS) {
      delay(40);
      (void)client.disconnect();
      unsigned long wait = millis();
      while (!abandon.done.load(std::memory_order_acquire) && millis() - wait < 5000) {
        delay(10);
      }
    } else {
      abandon.done.store(true, std::memory_order_release);
    }
    bool abandonDone = abandon.done.load(std::memory_order_acquire);
    Serial.printf("[CLIENT] Snapshot abandon empty=%d\n", (int)(abandonDone && abandon.lengths[0] == 0));
    if (abandonDone) {
      abandon.chr = BLERemoteCharacteristic();
    }
    delay(500);
    bool rec = false;
    for (int attempt = 0; attempt < 6 && !rec; attempt++) {
      BTStatus recStatus = client.connect(targetAddr);
      rec = (bool)recStatus;
      Serial.printf("[CLIENT] Snapshot abandon reconnect attempt=%d status=%s\n", attempt + 1, recStatus.toString());
      if (!rec) {
        delay(500);
      }
    }
    bool recUniform = false;
    if (rec) {
      svc = client.getService(serviceUUID);
      rwChr = svc ? svc.getCharacteristic(rwCharUUID) : BLERemoteCharacteristic();
      BLERemoteCharacteristic sc = svc ? svc.getCharacteristic(snapshotCharUUID) : BLERemoteCharacteristic();
      String v = sc ? sc.readValue() : String();
      recUniform = (v.length() == 512) && valueIsUniform(v);
    }
    Serial.printf("[CLIENT] Snapshot abandon reconnect uniform=%d\n", (int)recUniform);
  }

  // ===== Phase 5: Notifications + Indications =====
  waitForPhase(5);

  BLERemoteCharacteristic notifyChr = svc.getCharacteristic(notifyCharUUID);
  BLERemoteCharacteristic indicateChr = svc.getCharacteristic(indicateCharUUID);

  // The BLE stack delivers the notification/indication callback from its own
  // task, which can race with the Arduino task that called subscribe(). To
  // keep logs both truthful and deterministically ordered (and identical
  // across NimBLE and Bluedroid), the callback only captures the payload and
  // signals; all logging is driven from this task in the intended sequence.
  volatile bool notifReceived = false;
  volatile bool indicReceived = false;
  String notifPayload;
  String indicPayload;
  // The first indication is followed by a burst of 4 the server sends back to back.
  static const int kIndicMax = 5;
  String indicLog[kIndicMax];
  std::atomic<int> indicCount{0};

  BTStatus subStatus = notifyChr.subscribe(
    BLESubscription::Notify,
    [&notifReceived, &notifPayload](BLERemoteCharacteristic chr, const uint8_t *data, size_t length, BLESubscription) {
      syncPhaseFromHost();
      notifPayload = String((const char *)data, length);
      notifReceived = true;
    }
  );
  if (subStatus) {
    Serial.println("[CLIENT] Subscribed to notifications");
  } else {
    Serial.printf("[CLIENT] Subscribe to notifications FAILED: %s\n", subStatus.toString());
  }

  {
    unsigned long start = millis();
    while (!notifReceived && (millis() - start < 30000)) {
      delay(100);
    }
  }
  if (notifReceived) {
    Serial.printf("[CLIENT] Notification received: %s\n", notifPayload.c_str());
  }
  BLERemoteDescriptor notifyCccd = notifyChr.getDescriptor(BLEUUID((uint16_t)0x2902));
  uint8_t malformedCccd = 1;
  BTStatus malformedCccdStatus = notifyCccd ? notifyCccd.writeValue(&malformedCccd, 1, true) : BTStatus::NotFound;
  Serial.printf("[CLIENT] Malformed CCCD rejected: %d\n", (int)!malformedCccdStatus);
  // Idempotent CCCD write (0x0001 while already subscribed to notifications)
  // must succeed without firing onSubscribe a second time on the server.
  if (notifyCccd) {
    const uint8_t idempotentCccd[2] = {0x01, 0x00};
    (void)notifyCccd.writeValue(idempotentCccd, sizeof(idempotentCccd), true);
  }

  subStatus = indicateChr.subscribe(
    BLESubscription::Indicate,
    [&indicReceived, &indicPayload, &indicLog, &indicCount](BLERemoteCharacteristic chr, const uint8_t *data, size_t length, BLESubscription) {
      syncPhaseFromHost();
      int n = indicCount;
      if (n == 0) {
        indicPayload = String((const char *)data, length);
        indicReceived = true;
      }
      if (n < kIndicMax) {
        indicLog[n] = String((const char *)data, length);
        indicCount = n + 1;
      }
    }
  );
  if (subStatus) {
    Serial.println("[CLIENT] Subscribed to indications");
  } else {
    Serial.printf("[CLIENT] Subscribe to indications FAILED: %s\n", subStatus.toString());
  }

  {
    unsigned long start = millis();
    while (!indicReceived && (millis() - start < 30000)) {
      delay(100);
    }
  }
  if (indicReceived) {
    Serial.printf("[CLIENT] Indication received: %s\n", indicPayload.c_str());
  }
  {
    unsigned long start = millis();
    while (indicCount < kIndicMax && (millis() - start < 10000)) {
      delay(50);
    }
    String burst;
    for (int i = 1; i < indicCount; i++) {
      burst += (i > 1) ? "," : "";
      burst += indicLog[i];
    }
    Serial.printf("[CLIENT] Indication burst: %s\n", burst.c_str());
  }

  // Enable notifications and indications together on one characteristic; the
  // server then interleaves the two kinds.
  BLERemoteCharacteristic dualChr = svc.getCharacteristic(dualCharUUID);
  {
    static const int kDualMax = 4;
    String dualInd[kDualMax];
    String dualNotif[kDualMax];
    std::atomic<int> dualIndCount{0};
    std::atomic<int> dualNotifCount{0};
    BTStatus ds = dualChr.subscribe(
      BLESubscription::Notify | BLESubscription::Indicate,
      [&dualInd, &dualNotif, &dualIndCount, &dualNotifCount](BLERemoteCharacteristic chr, const uint8_t *data, size_t length, BLESubscription kind) {
        syncPhaseFromHost();
        String v((const char *)data, length);
        if (kind == BLESubscription::Notify) {
          int n = dualNotifCount;
          if (n < kDualMax) {
            dualNotif[n] = v;
            dualNotifCount = n + 1;
          }
        } else {
          int n = dualIndCount;
          if (n < kDualMax) {
            dualInd[n] = v;
            dualIndCount = n + 1;
          }
        }
      }
    );
    if (!ds) {
      Serial.printf("[CLIENT] Dual subscribe FAILED: %s\n", ds.toString());
    }
    unsigned long start = millis();
    while ((dualIndCount < kDualMax || dualNotifCount < kDualMax) && millis() - start < 10000) {
      delay(50);
    }
    String ind, notif;
    for (int i = 0; i < dualIndCount; i++) {
      ind += (i > 0) ? "," : "";
      ind += dualInd[i];
    }
    for (int i = 0; i < dualNotifCount; i++) {
      notif += (i > 0) ? "," : "";
      notif += dualNotif[i];
    }
    Serial.printf("[CLIENT] Dual indications: %s\n", ind.c_str());
    Serial.printf("[CLIENT] Dual notifications: %s\n", notif.c_str());
    // Let the server report its statuses before the unsubscribes below.
    delay(500);
    dualChr.unsubscribe();
  }

  notifyChr.unsubscribe();
  indicateChr.unsubscribe();
  Serial.println("[CLIENT] Unsubscribed");

  // ===== Phase 6: Large ATT write (>MTU) =====
  waitForPhase(6);
  {
    const size_t bigLen = 512;
    uint8_t bigBuf[bigLen];
    for (size_t i = 0; i < bigLen; i++) {
      bigBuf[i] = (uint8_t)(i & 0xFF);
    }

    status = rwChr.writeValue(bigBuf, bigLen);
    if (status) {
      Serial.println("[CLIENT] Large write OK");
    } else {
      Serial.printf("[CLIENT] Large write FAILED: %s\n", status.toString());
    }

    val = rwChr.readValue();
    Serial.printf("[CLIENT] Large read (%u bytes)\n", val.length());

    bool match = (val.length() == bigLen);
    if (match) {
      for (size_t i = 0; i < val.length(); i++) {
        if ((uint8_t)val[i] != (uint8_t)(i & 0xFF)) {
          match = false;
          break;
        }
      }
    }
    if (match) {
      Serial.println("[CLIENT] Large data integrity OK");
    } else {
      Serial.println("[CLIENT] Large data integrity FAILED");
    }

    // Overwrite the 512-byte value with a shorter >MTU Long Write (510 bytes >
    // mtu - 3). Verifies Prepared Write + Execute Write starting at offset 0
    // truncates the previous 512-byte attribute value to 510 bytes.
    const size_t shortLongLen = 510;
    uint8_t shortBuf[shortLongLen];
    for (size_t i = 0; i < shortLongLen; i++) {
      shortBuf[i] = (uint8_t)((i ^ 0xA5) & 0xFF);
    }
    status = rwChr.writeValue(shortBuf, shortLongLen);
    if (status) {
      Serial.println("[CLIENT] Shorter long write OK");
    } else {
      Serial.printf("[CLIENT] Shorter long write FAILED: %s\n", status.toString());
    }
    val = rwChr.readValue();
    Serial.printf("[CLIENT] Shorter long read (%u bytes)\n", val.length());
    bool shortMatch = (val.length() == shortLongLen);
    if (shortMatch) {
      for (size_t i = 0; i < val.length(); i++) {
        if ((uint8_t)val[i] != (uint8_t)((i ^ 0xA5) & 0xFF)) {
          shortMatch = false;
          break;
        }
      }
    }
    if (shortMatch) {
      Serial.println("[CLIENT] Shorter long data integrity OK");
    } else {
      Serial.println("[CLIENT] Shorter long data integrity FAILED");
    }
  }

  // ===== Phase 7: Descriptor read/write =====
  waitForPhase(7);
  {
    BLERemoteCharacteristic descChr = svc.getCharacteristic(descCharUUID);
    if (!descChr) {
      Serial.println("[CLIENT] Descriptor char not found");
    } else {
      Serial.println("[CLIENT] Found descriptor char");
      auto descriptors = descChr.getDescriptors();
      Serial.printf("[CLIENT] Descriptor count: %u\n", (unsigned)descriptors.size());

      // Find User Description (0x2901)
      BLERemoteDescriptor userDesc = descChr.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)));
      if (userDesc) {
        String descVal = userDesc.readValue();
        Serial.printf("[CLIENT] User description: %s\n", descVal.c_str());
      } else {
        Serial.println("[CLIENT] User description not found");
      }

      // Find Presentation Format (0x2904)
      BLERemoteDescriptor pfDesc = descChr.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2904)));
      if (pfDesc) {
        String pfVal = pfDesc.readValue();
        if (pfDesc.isPresentationFormat() && pfVal.length() >= 7) {
          uint8_t format = pfDesc.getFormat();
          int8_t exponent = pfDesc.getExponent();
          uint16_t unit = pfDesc.getUnit();
          uint8_t ns = pfDesc.getNamespace();
          uint16_t pfDescVal = pfDesc.getFormatDescription();
          Serial.printf(
            "[CLIENT] Presentation format: %u exp=%d unit=0x%04X ns=%u desc=0x%04X\n", format, (int)exponent, (unsigned)unit, (unsigned)ns, (unsigned)pfDescVal
          );
          if (format == BLEDescriptor::FORMAT_UTF8 && exponent == -2 && unit == 0x2700 && ns == 1 && pfDescVal == 0x0042) {
            Serial.println("[CLIENT] Format matches UTF8");
          }
        }
      } else {
        Serial.println("[CLIENT] Presentation format not found");
      }

      BLERemoteDescriptor dynamicDesc = descChr.getDescriptor(dynamicReadDescUUID);
      if (dynamicDesc) {
        for (int i = 1; i <= 2; i++) {
          Serial.printf("[CLIENT] Dynamic descriptor read %d: %s\n", i, dynamicDesc.readValue().c_str());
        }
      } else {
        Serial.println("[CLIENT] Dynamic descriptor not found");
      }

      BLERemoteDescriptor longDesc = descChr.getDescriptor(longReadDescUUID);
      if (longDesc) {
        for (int i = 1; i <= 2; i++) {
          reportLongRead("Long descriptor read", i, longDesc.readValue());
        }
      } else {
        Serial.println("[CLIENT] Long descriptor not found");
      }

      BLERemoteDescriptor writableDesc = descChr.getDescriptor(writableDescUUID);
      if (!writableDesc) {
        Serial.println("[CLIENT] Writable descriptor not found");
      } else {
        BTStatus ws1 = writableDesc.writeValue("desc_short_ok");
        String rb1 = writableDesc.readValue();
        Serial.printf("[CLIENT] Writable descriptor short write ok=%d readback=%s\n", (int)(bool)ws1, rb1.c_str());

        static uint8_t longDescWriteBuf[510];
        memset(longDescWriteBuf, 'W', sizeof(longDescWriteBuf));
        BTStatus ws2 = writableDesc.writeValue(longDescWriteBuf, sizeof(longDescWriteBuf));
        String rb2 = writableDesc.readValue();
        bool uniformW = (rb2.length() == sizeof(longDescWriteBuf));
        for (size_t i = 0; uniformW && i < rb2.length(); i++) {
          if (rb2[i] != 'W') {
            uniformW = false;
          }
        }
        Serial.printf("[CLIENT] Writable descriptor long write ok=%d len=%u uniform=%d\n", (int)(bool)ws2, (unsigned)rb2.length(), (int)uniformW);

        // Oversize Long Write (512 > maxLen=510): Prepare Write chunks are queued
        // and Execute Write must reject with INVALID_ATTR_VALUE_LEN and roll back
        // without modifying the previous 510-byte 'W' value or firing onWrite.
        static uint8_t oversizeDescBuf[512];
        memset(oversizeDescBuf, 'X', sizeof(oversizeDescBuf));
        BTStatus ws3 = writableDesc.writeValue(oversizeDescBuf, sizeof(oversizeDescBuf));
        String rb3 = writableDesc.readValue();
        bool rollbackOk = (rb3.length() == sizeof(longDescWriteBuf));
        for (size_t i = 0; rollbackOk && i < rb3.length(); i++) {
          if (rb3[i] != 'W') {
            rollbackOk = false;
          }
        }
        Serial.printf("[CLIENT] Writable descriptor oversize rejected=%d rollbackLen=%u rollbackOk=%d\n", (int)(!ws3), (unsigned)rb3.length(), (int)rollbackOk);
      }
    }
  }

  // ===== Phase 8: Write without response =====
  waitForPhase(8);
  {
    BLERemoteCharacteristic writeNrChr = svc.getCharacteristic(writeNrCharUUID);
    if (!writeNrChr) {
      Serial.println("[CLIENT] WriteNR char not found");
    } else {
      Serial.println("[CLIENT] Found WriteNR char");
      status = writeNrChr.writeValue("WriteNR_OK", false);
      if (status) {
        Serial.println("[CLIENT] WriteNR sent");
      } else {
        Serial.printf("[CLIENT] WriteNR FAILED: %s\n", status.toString());
      }
      delay(500);
      // Read back to verify server received it
      String readBack = writeNrChr.readValue();
      Serial.printf("[CLIENT] WriteNR readback: %s\n", readBack.c_str());

      // Three packets back to back, no delay and no ATT response in between, so
      // they queue up in the stack. The server must see AA/BB/CC and not three
      // copies of the last payload.
      static const uint8_t burst[][3] = {{0xAA, 0xAA, 0xAA}, {0xBB, 0xBB, 0xBB}, {0xCC, 0xCC, 0xCC}};
      bool burstSent = true;
      for (const auto &packet : burst) {
        burstSent = burstSent && (bool)writeNrChr.writeValue(packet, sizeof(packet), false);
      }
      Serial.printf("[CLIENT] Phase8 burst sent=%d\n", (int)burstSent);
      delay(500);
    }
    Serial.println("[CLIENT] Status: write_no_response done");
  }

  // ===== Phase 9: Server-initiated disconnect =====
  waitForPhase(9);
  {
    Serial.println("[CLIENT] Waiting for server disconnect...");
    unsigned long start = millis();
    while (client.isConnected() && (millis() - start < 10000)) {
      delay(100);
    }
    if (!client.isConnected()) {
      Serial.println("[CLIENT] Server disconnected us");
    } else {
      Serial.println("[CLIENT] Server disconnect timeout");
    }
    Serial.printf("[CLIENT] DisconnectTornDownInCallback ok=%d\n", (int)clientDisconnectTornDown);

    // Reconnect after server-initiated disconnect
    delay(2000);
    client = BLE.createClient();
    installClientCallbacks(client);
    status = client.connect(targetAddr);
    if (status) {
      Serial.println("[CLIENT] Reconnected after server disconnect");

      // Best-effort service rediscovery. The next phase deliberately tears this
      // link down to start fresh pairing, so only flag a genuine failure while
      // the link is still up; otherwise this races the phase-10 teardown and
      // prints a misleading "not found".
      svc = client.getService(serviceUUID);
      if (svc) {
        Serial.println("[CLIENT] Services rediscovered after reconnect");
      } else if (client.isConnected()) {
        Serial.println("[CLIENT] Service not found after reconnect");
      }
    } else {
      Serial.printf("[CLIENT] Reconnect FAILED: %s\n", status.toString());
    }
  }

  // ===== Phase 10: Security — encrypted characteristic =====
  waitForPhase(10);
  {
    // Host can deliver START_PHASE_10 to the client before the server has run
    // loop() to process the same line — give the server time to disconnect +
    // deleteAllBonds() so we do not reuse a bond from an earlier run/phase.
    delay(500);
    BLESecurity secWipe = BLE.getSecurity();
    if (secWipe) {
      (void)secWipe.deleteAllBonds();
    }
    if (client.isConnected()) {
      (void)client.disconnect();
    }
    delay(300);
    if (secWipe) {
      (void)secWipe.deleteAllBonds();
    }

    // Fresh connection resets the per-connection onIdentity latch; count how
    // many times onIdentity fires for this pairing (must be exactly 1). The
    // event-order trace is reset here too so ORDER captures exactly this
    // pairing lifecycle (CONNECT -> MTU -> IDENTITY) for cross-stack parity.
    clientIdentityCount = 0;
    clientOrderN = 0;
    client = BLE.createClient();
    installClientCallbacks(client);
    status = client.connect(targetAddr);
    if (!status) {
      Serial.printf("[CLIENT] Phase10 reconnect FAILED: %s\n", status.toString());
    } else {
      Serial.println("[CLIENT] Phase10 reconnected for pairing");
    }
    svc = client.getService(serviceUUID);
    if (!svc) {
      Serial.println("[CLIENT] Service not found after phase10 reconnect");
    } else {
      BLERemoteCharacteristic secureChr = svc.getCharacteristic(secureCharUUID);
      if (!secureChr) {
        Serial.println("[CLIENT] Secure characteristic not found");
      } else {
        delay(1000);
        String sval = secureChr.readValue();
        Serial.printf("[CLIENT] Secure read: %s\n", sval.c_str());
      }
    }
    // Let any trailing identity callbacks settle, then report the dedupe count
    // and the recorded event order for cross-stack parity assertions.
    delay(500);
    Serial.printf("[CLIENT] Phase10 onIdentity count=%d\n", clientIdentityCount);
    reportClientOrder("Phase10");
  }

  // ===== Phase 11: BLE5 PHY + DLE =====
  waitForPhase(11);
  {
    bool phy_ok = false;
#if BLE5_SUPPORTED
    status = client.setPhy(BLEPhy::PHY_2M, BLEPhy::PHY_2M);
    if (status) {
      Serial.println("[CLIENT] PHY update OK");
      phy_ok = true;
    }
    if (phy_ok) {
      status = client.setDataLen(251, 2120);
      if (status) {
        Serial.println("[CLIENT] DLE set OK");
      } else {
        Serial.printf("[CLIENT] DLE failed: %s\n", status.toString());
      }
    }
#endif
    if (!phy_ok) {
      Serial.println("[CLIENT] BLE5 PHY/DLE not supported, skipping");
    }
  }

  // ===== Phase 12: Reconnect (3 cycles) =====
  waitForPhase(12);
  client.disconnect();
  Serial.println("[CLIENT] Disconnected for reconnect test");
  delay(2000);

  for (int i = 0; i < 3; i++) {
    Serial.printf("[CLIENT] Connect cycle %d\n", i + 1);
    BLEClient rc = BLE.createClient();
    installClientCallbacks(rc);
    status = rc.connect(targetAddr);
    if (status) {
      Serial.println("[CLIENT] Connected");
      delay(2000);
      rc.disconnect();
      Serial.println("[CLIENT] Disconnected");
      delay(2000);
    } else {
      Serial.printf("[CLIENT] Connect FAILED cycle %d\n", i + 1);
    }
  }

  Serial.println("[CLIENT] All cycles complete");

  // --- Repeated client create/connect stress (public API only) ---
  // Exercises allocator reuse through the public contract. Internal
  // app-ID wrap-around is a library-internal concern covered by unit
  // tests, not by poking backend Impl state from a sketch.
  {
    static const int STRESS_CYCLES = 15;
    Serial.printf("[CLIENT] AppId stress: cycles=%d\n", STRESS_CYCLES);

    for (int i = 0; i < STRESS_CYCLES; i++) {
      BLEClient sc = BLE.createClient();
      if (!sc) {
        Serial.printf("[CLIENT] AppId stress FAILED cycle %d: createClient\n", i + 1);
        break;
      }
      installClientCallbacks(sc);
      status = sc.connect(targetAddr);
      if (!status) {
        Serial.printf("[CLIENT] AppId stress FAILED cycle %d: connect %s\n", i + 1, status.toString());
        break;
      }
      sc.disconnect();
      delay(500);
      if (i == STRESS_CYCLES - 1) {
        Serial.println("[CLIENT] AppId stress PASSED");
      }
    }
  }

  // NimBLE keepalive must outlive disconnectSync.give() until the host has
  // finished dispatching the terminal event. Re-assigning nimbleRef for a
  // reconnect in that window must not be wiped by the old handler's reset().
#if BLE_NIMBLE && BLE_VALIDATION_HOOKS
  {
    BLEClient raceClient = BLE.createClient();
    installClientCallbacks(raceClient);
    BTStatus raceConnect = raceClient.connect(targetAddr);
    BLEHooks.client.resetWhileConnected.store(false, std::memory_order_relaxed);
    BLEHooks.client.nimbleRefHoldEntered.store(false, std::memory_order_relaxed);
    BLEHooks.client.nimbleRefAssigned.store(false, std::memory_order_relaxed);
    BLEHooks.client.nimbleRefEmptyOnConnect.store(false, std::memory_order_relaxed);
    BLEHooks.client.holdNimbleRefReset.store(true, std::memory_order_relaxed);
    struct NimbleRefRaceJob {
      BLEClient client;
      BTAddress addr;
      std::atomic<bool> disconnectDone{false};
      std::atomic<bool> connectDone{false};
      BTStatus reconnect{BTStatus::Fail};
    };
    static NimbleRefRaceJob nimbleRefJob;
    nimbleRefJob.client = raceClient;
    nimbleRefJob.addr = targetAddr;
    nimbleRefJob.disconnectDone.store(false, std::memory_order_relaxed);
    nimbleRefJob.connectDone.store(false, std::memory_order_relaxed);
    nimbleRefJob.reconnect = BTStatus::Fail;
    UBaseType_t racePriority = uxTaskPriorityGet(nullptr);
    if (racePriority + 1 < configMAX_PRIORITIES) {
      ++racePriority;
    }
    bool raceStarted = raceConnect
                       && xTaskCreate(
                            [](void *arg) {
                              auto *job = static_cast<NimbleRefRaceJob *>(arg);
                              (void)job->client.disconnect();
                              job->disconnectDone.store(true, std::memory_order_release);
                              job->reconnect = job->client.connect(job->addr);
                              job->connectDone.store(true, std::memory_order_release);
                              vTaskDelete(nullptr);
                            },
                            "nimble-ref", 4096, &nimbleRefJob, racePriority, nullptr
                          ) == pdPASS;
    unsigned long holdDeadline = millis() + 3000;
    while (raceStarted && !BLEHooks.client.nimbleRefHoldEntered.load(std::memory_order_acquire) && millis() < holdDeadline) {
      delay(1);
    }
    unsigned long assignedDeadline = millis() + 2000;
    while (raceStarted && !BLEHooks.client.nimbleRefAssigned.load(std::memory_order_acquire) && millis() < assignedDeadline) {
      delay(1);
    }
    BLEHooks.client.holdNimbleRefReset.store(false, std::memory_order_release);
    unsigned long doneDeadline = millis() + 8000;
    while (raceStarted && !nimbleRefJob.connectDone.load(std::memory_order_acquire) && millis() < doneDeadline) {
      delay(1);
    }
    bool stillConnected = nimbleRefJob.client.isConnected();
    bool emptyOnConnect = BLEHooks.client.nimbleRefEmptyOnConnect.load(std::memory_order_relaxed);
    bool resetWhileConnected = BLEHooks.client.resetWhileConnected.load(std::memory_order_relaxed);
    bool raceClean = raceStarted && nimbleRefJob.disconnectDone.load() && nimbleRefJob.connectDone.load() && nimbleRefJob.reconnect && stillConnected
                     && !emptyOnConnect && !resetWhileConnected;
    if (nimbleRefJob.client.isConnected()) {
      (void)nimbleRefJob.client.disconnect();
    }
    nimbleRefJob.client = BLEClient();
    delay(300);
    Serial.printf(
      "[CLIENT] Phase12 nimbleRef race supported=1 clean=%d emptyOnConnect=%d resetWhileConnected=%d reconnect=%d connected=%d\n", (int)raceClean,
      (int)emptyOnConnect, (int)resetWhileConnected, (int)(bool)nimbleRefJob.reconnect, (int)stillConnected
    );
  }
#else
  Serial.println("[CLIENT] Phase12 nimbleRef race supported=0 clean=1 emptyOnConnect=0 resetWhileConnected=0 reconnect=1 connected=1");
#endif

  // ===== Phase 13: BLEStream =====
  waitForPhase(13);
  {
    uint16_t streamPreviousMtu = BLE.getMTU();
    (void)BLE.setMTU(100);
    bool stream_ok = false;
    bool bulkWriteComplete = false;
    bool failedBeginClean = false;
    Serial.println("[CLIENT] BLEStream malformed begin armed");
    while (!phase13MalformedGo) {
      checkSerial();
      delay(10);
    }
    {
      std::atomic<uint32_t> failedConnects{0};
      std::atomic<uint32_t> failedDisconnects{0};
      BLEStream failedStream;
      failedStream.onConnect([&](const BLEConnInfo &) {
        failedConnects.fetch_add(1);
      });
      failedStream.onDisconnect([&](const BLEConnInfo &, uint8_t) {
        failedDisconnects.fetch_add(1);
      });
      BTStatus failedStatus = failedStream.beginClient(targetAddr, 5000);
      failedBeginClean = failedStatus == BTStatus::NotFound && failedConnects.load() == 1 && failedDisconnects.load() == 1;
      Serial.printf(
        "[CLIENT] BLEStream failed begin events connect=%lu disconnect=%lu\n", (unsigned long)failedConnects.load(), (unsigned long)failedDisconnects.load()
      );
    }
    delay(500);
    failedBeginClean = failedBeginClean && heap_caps_check_integrity_all(true);
    Serial.printf("[CLIENT] BLEStream failed begin cleanup=%d\n", (int)failedBeginClean);
    // Don't tear down BLE — BLEStream layers on the existing stack.
    // Give server time to reconfigure advertising with NUS service.
    delay(2000);
    BLEStream stream;
    stream.onPeerData([](const BLEConnInfo &peer, const uint8_t *, size_t) {
      Serial.printf("[CLIENT] BLEStream onPeerData: %s\n", peer.getAddress().toString().c_str());
    });
    for (int attempt = 1; attempt <= 5 && !stream_ok; attempt++) {
      BTStatus s = stream.beginClient(targetAddr, 7000);
      if (!s) {
        Serial.printf("[CLIENT] BLEStream connect attempt %d failed: %s\n", attempt, s.toString());
        delay(500);
        continue;
      }
      Serial.println("[CLIENT] BLEStream init OK");
      stream.setTimeout(10000);

      if (stream.connected()) {
        Serial.println("[CLIENT] BLEStream connected OK");
        auto peerList = stream.peers();
        Serial.printf("[CLIENT] BLEStream peers=%u\n", (unsigned)peerList.size());
        if (!peerList.empty()) {
          Serial.printf("[CLIENT] BLEStream peerAddr=%s\n", peerList[0].getAddress().toString().c_str());
        }
      }

      // writeTo round-trip
      stream.println("stream_write_to_probe");
      Serial.println("[CLIENT] BLEStream writeTo probe sent");
      {
        String rx = stream.readStringUntil('\n');
        rx.trim();
        if (rx == "STREAM_WRITE_TO_ACK") {
          Serial.println("[CLIENT] BLEStream writeTo ack OK");
        } else {
          Serial.printf("[CLIENT] BLEStream writeTo ack FAILED: %s\n", rx.c_str());
        }
      }

      // Basic ping/pong
      stream.println("stream_ping");
      Serial.println("[CLIENT] BLEStream sent");
      {
        String rx = stream.readStringUntil('\n');
        rx.trim();
        Serial.printf("[CLIENT] BLEStream received: %s\n", rx.c_str());
      }

      // Send data for server-side peek() test
      stream.println("peek_test");
      Serial.println("[CLIENT] BLEStream peek_test sent");
      delay(200);

      // Bulk write: 200-byte repeating A-Z pattern (exercises multi-MTU chunking)
      {
        char bulk[201];
        for (int i = 0; i < 200; i++) {
          bulk[i] = 'A' + (i % 26);
        }
        bulk[200] = '\0';
        size_t bulkSent = stream.println(bulk);
        Serial.printf("[CLIENT] BLEStream bulk sent: 200 bytes returned=%u\n", (unsigned)bulkSent);
        bulkWriteComplete = bulkSent == 202;
      }

      // Receive server-initiated message (tests server→client direction)
      {
        String rx = stream.readStringUntil('\n');
        rx.trim();
        Serial.printf("[CLIENT] BLEStream server msg: %s\n", rx.c_str());
      }

      stream.println("end_now");
      unsigned long endWait = millis();
      while (stream.connected() && millis() - endWait < 10000) {
        delay(20);
      }
      Serial.printf("[CLIENT] BLEStream server end disconnected=%d\n", (int)!stream.connected());
      stream_ok = bulkWriteComplete;
      stream.end();

      // Reconnect to the server's restarted stream, then disconnect from the
      // application task. The server's onDisconnect callback calls end() as
      // the outer teardown operation, exercising callback-context deferral.
      std::atomic<uint32_t> callbackEndDisconnects{0};
      BLEStream callbackEndStream;
      callbackEndStream.onDisconnect([&](const BLEConnInfo &, uint8_t) {
        callbackEndDisconnects.fetch_add(1);
      });
      BTStatus callbackConnect = BTStatus::Fail;
      for (int callbackAttempt = 0; callbackAttempt < 10 && !callbackConnect; callbackAttempt++) {
        callbackConnect = callbackEndStream.beginClient(targetAddr, 5000);
        if (!callbackConnect) {
          delay(250);
        }
      }
      if (callbackConnect) {
        delay(300);
        BLEHooks.stream.writeSawClearedHandle.store(false, std::memory_order_relaxed);
        BLEHooks.stream.writeChunksEntered.store(false, std::memory_order_relaxed);
        BLEHooks.stream.holdWriteChunks.store(true, std::memory_order_relaxed);
        struct HandleRaceJob {
          BLEStream *stream;
          std::atomic<bool> done{false};
          uint8_t payload[200];
        };
        static HandleRaceJob handleJob;
        handleJob.stream = &callbackEndStream;
        handleJob.done.store(false, std::memory_order_relaxed);
        for (int i = 0; i < 200; i++) {
          handleJob.payload[i] = static_cast<uint8_t>('A' + (i % 26));
        }
        UBaseType_t writePriority = uxTaskPriorityGet(nullptr);
        if (writePriority + 1 < configMAX_PRIORITIES) {
          ++writePriority;
        }
        bool writeStarted = xTaskCreate(
                              [](void *arg) {
                                auto *job = static_cast<HandleRaceJob *>(arg);
                                (void)job->stream->write(job->payload, sizeof(job->payload));
                                job->done.store(true, std::memory_order_release);
                                vTaskDelete(nullptr);
                              },
                              "stream-wr", 4096, &handleJob, writePriority, nullptr
                            )
                            == pdPASS;
        unsigned long enteredDeadline = millis() + 2000;
        while (writeStarted && !BLEHooks.stream.writeChunksEntered.load(std::memory_order_acquire) && millis() < enteredDeadline) {
          delay(1);
        }
        BLEHooks.stream.disconnectRaces.store(1);
        callbackEndStream.end();
        BLEHooks.stream.holdWriteChunks.store(false, std::memory_order_release);
        unsigned long writeDeadline = millis() + 2000;
        while (writeStarted && !handleJob.done.load(std::memory_order_acquire) && millis() < writeDeadline) {
          delay(1);
        }
      }
      bool handleRaceClean =
        BLEHooks.stream.writeChunksEntered.load(std::memory_order_relaxed) && !BLEHooks.stream.writeSawClearedHandle.load(std::memory_order_relaxed);
      Serial.printf(
        "[CLIENT] BLEStream handle reset race clean=%d sawCleared=%d entered=%d\n", (int)handleRaceClean, (int)BLEHooks.stream.writeSawClearedHandle.load(),
        (int)BLEHooks.stream.writeChunksEntered.load()
      );
      stream_ok = stream_ok && handleRaceClean;
      Serial.printf(
        "[CLIENT] BLEStream callback end cycle connected=%d disconnects=%lu\n", (int)(bool)callbackConnect, (unsigned long)callbackEndDisconnects.load()
      );

      // The server now owns its stream only through a shared_ptr and drops that
      // final public owner from onConnect. The link may disappear before
      // beginClient finishes discovery; callback delivery and clean disconnect
      // are the invariant under test.
      std::atomic<uint32_t> finalOwnerConnects{0};
      std::atomic<uint32_t> finalOwnerDisconnects{0};
      BLEStream finalOwnerClient;
      finalOwnerClient.onConnect([&](const BLEConnInfo &) {
        finalOwnerConnects.fetch_add(1);
      });
      finalOwnerClient.onDisconnect([&](const BLEConnInfo &, uint8_t) {
        finalOwnerDisconnects.fetch_add(1);
      });
      for (int finalAttempt = 0; finalAttempt < 10 && finalOwnerConnects.load() == 0; finalAttempt++) {
        (void)finalOwnerClient.beginClient(targetAddr, 5000);
        if (finalOwnerConnects.load() == 0) {
          delay(250);
        }
      }
      unsigned long finalOwnerDeadline = millis() + 8000;
      while (finalOwnerDisconnects.load() == 0 && millis() < finalOwnerDeadline) {
        delay(20);
      }
      bool finalOwnerClean =
        finalOwnerConnects.load() > 0 && finalOwnerDisconnects.load() > 0 && !finalOwnerClient.connected() && heap_caps_check_integrity_all(true);
      Serial.printf(
        "[CLIENT] BLEStream final owner callback clean=%d connects=%lu disconnects=%lu\n", (int)finalOwnerClean, (unsigned long)finalOwnerConnects.load(),
        (unsigned long)finalOwnerDisconnects.load()
      );
      finalOwnerClient.end();
      stream_ok = stream_ok && finalOwnerClean;
    }
    if (stream_ok) {
      Serial.println("[CLIENT] Status: blestream done");
    } else {
      Serial.println("[CLIENT] BLEStream phase FAILED");
    }
    (void)BLE.setMTU(streamPreviousMtu);
  }

  // ===== Phase 14: L2CAP CoC =====
  waitForPhase(14);
  {
    bool l2cap_supported = false;
#if BLE_L2CAP_SUPPORTED
    l2cap_supported = true;
    BTStatus s = BTStatus::Fail;
    BLEClient l2capClient = BLE.createClient();
    installClientCallbacks(l2capClient);
    for (int attempt = 0; attempt < 6 && !s; attempt++) {
      s = l2capClient.connect(targetAddr);
      if (!s) {
        delay(250);
      }
    }
    if (!s) {
      Serial.printf("[CLIENT] L2CAP connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] L2CAP init OK");
      BLEL2CAPChannel channel = BLE.connectL2CAP(l2capClient.getHandle(), 0x0080, 128);
      if (!channel || !channel.isConnected()) {
        Serial.println("[CLIENT] L2CAP open FAILED");
      } else {
        volatile bool dataReceived = false;
        String l2capRx;
        channel.onData([&](BLEL2CAPChannel ch, const uint8_t *data, size_t len) {
          syncPhaseFromHost();
          l2capRx = String((const char *)data, len);
          dataReceived = true;
        });

        Serial.println("[CLIENT] L2CAP channel connected");
        const char *payload = "L2CAP_PING";
        s = channel.write((const uint8_t *)payload, strlen(payload));
        if (s) {
          Serial.println("[CLIENT] L2CAP sent");
        } else {
          Serial.printf("[CLIENT] L2CAP write FAILED: %s\n", s.toString());
        }

        unsigned long rxStart = millis();
        while (!dataReceived && (millis() - rxStart < 10000)) {
          delay(20);
        }
        if (dataReceived) {
          Serial.printf("[CLIENT] L2CAP received: %s\n", l2capRx.c_str());
          if (l2capRx == "L2CAP_OK") {
            Serial.println("[CLIENT] Status: l2cap done");
          }
        } else {
          Serial.println("[CLIENT] L2CAP receive timeout");
        }
        channel.disconnect();
      }
      l2capClient.disconnect();
    }
#endif
    if (!l2cap_supported) {
      Serial.println("[CLIENT] L2CAP not supported, skipping");
    }
  }

  // ===== Phase 15: Client-side introspection + fail-closed permissions =====
  // Exercises API surface that previous phases didn't touch:
  //   * BLEClient::getServices() / BLERemoteService::getCharacteristics()
  //   * BLEClient::getRSSI()
  //   * BLERemoteCharacteristic::can{Read,Write,WriteNoResponse,Notify,Indicate}()
  //   * BLERemoteCharacteristic::getDescriptors() / getDescriptor() / getHandle()
  // And verifies the new properties/permissions validation end-to-end: a char
  // declared as Read|Write with only Read permission must be advertised
  // without the Write property (fail-closed masking) and must reject writes.
  waitForPhase(15);
  {
    // Reconnect with a fresh client — phase 14's client was disposed. Retry a
    // few times: right after the L2CAP teardown the peer may still be finishing
    // its disconnect / re-advertise cycle, so the first attempt can bounce.
    BLEClient ic = BLE.createClient();
    installClientCallbacks(ic);
    BTStatus s = BTStatus::Fail;
    for (int attempt = 0; attempt < 5; attempt++) {
      s = ic.connect(targetAddr);
      if (s) {
        break;
      }
      delay(500);
    }
    if (!s) {
      Serial.printf("[CLIENT] Introspect connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Reconnected for introspection");

      int8_t rssi = ic.getRSSI();
      Serial.printf("[CLIENT] RSSI: %d\n", (int)rssi);

      // Call the aggregate getters first. The library auto-discovers when the
      // cache is empty, so both backends now behave symmetrically with the
      // targeted getService()/getCharacteristic() calls below.
      auto services = ic.getServices();
      Serial.printf("[CLIENT] Service count: %u\n", (unsigned)services.size());

      BLERemoteService isvc = ic.getService(serviceUUID);
      if (!isvc) {
        Serial.println("[CLIENT] Introspect service not found");
      } else {
        auto chars = isvc.getCharacteristics();
        Serial.printf("[CLIENT] Characteristic count: %u\n", (unsigned)chars.size());

        auto rw = isvc.getCharacteristic(rwCharUUID);
        auto nt = isvc.getCharacteristic(notifyCharUUID);
        auto ind = isvc.getCharacteristic(indicateCharUUID);
        auto wn = isvc.getCharacteristic(writeNrCharUUID);
        auto fc = isvc.getCharacteristic(permFailClosedCharUUID);

        // Property introspection — verifies both backends expose the right
        // property bits to the peer (after fail-closed masking).
        Serial.printf(
          "[CLIENT] RW: canRead=%d canWrite=%d canNotify=%d canIndicate=%d\n", (int)rw.canRead(), (int)rw.canWrite(), (int)rw.canNotify(), (int)rw.canIndicate()
        );
        Serial.printf("[CLIENT] Notify: canNotify=%d canIndicate=%d canRead=%d\n", (int)nt.canNotify(), (int)nt.canIndicate(), (int)nt.canRead());
        Serial.printf("[CLIENT] Indicate: canIndicate=%d canNotify=%d canRead=%d\n", (int)ind.canIndicate(), (int)ind.canNotify(), (int)ind.canRead());
        Serial.printf("[CLIENT] WriteNR: canWriteNoResponse=%d canWrite=%d\n", (int)wn.canWriteNoResponse(), (int)wn.canWrite());

        // Exercise the aggregate descriptor getter FIRST (no prior targeted
        // getDescriptor() call) to confirm it auto-discovers on both backends
        // and stays strictly within each characteristic's handle range.
        auto rwDescriptors = rw.getDescriptors();
        BTStatus rwSubNoCccd = rw.subscribe(BLESubscription::Notify, nullptr);
        BTStatus rwUnsubNoCccd = rw.unsubscribe();
        Serial.printf(
          "[CLIENT] RW descriptor count: %u noCccdNotFound=%d\n", (unsigned)rwDescriptors.size(),
          (int)(rwSubNoCccd == BTStatus::NotFound && rwUnsubNoCccd == BTStatus::NotFound)
        );
        auto ntDescriptors = nt.getDescriptors();
        Serial.printf("[CLIENT] Notify descriptor count: %u\n", (unsigned)ntDescriptors.size());
        auto cccd = nt.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2902)));
        Serial.printf("[CLIENT] Notify CCCD found: %d\n", (int)(bool)cccd);
        auto dualRemote = isvc.getCharacteristic(dualCharUUID);
        auto dualDescriptors = dualRemote ? dualRemote.getDescriptors() : std::vector<BLERemoteDescriptor>{};
        auto dualUd = dualRemote ? dualRemote.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2901))) : BLERemoteDescriptor();
        Serial.printf("[CLIENT] Dual descriptor count: %u userDesc=%s\n", (unsigned)dualDescriptors.size(), dualUd ? dualUd.readValue().c_str() : "none");

        // Fail-closed test: char declares Read|Write but only has Read
        // permission. The backend must strip the Write property bit, so:
        //   - canRead()  -> true, canWrite() -> false
        //   - readValue() succeeds and returns "ro_data"
        //   - writeValue() returns a failure status (server rejects at ATT)
        if (!fc) {
          Serial.println("[CLIENT] FailClosed char not found");
        } else {
          Serial.printf("[CLIENT] FailClosed: canRead=%d canWrite=%d\n", (int)fc.canRead(), (int)fc.canWrite());
          String roVal = fc.readValue();
          Serial.printf("[CLIENT] FailClosed read: %s\n", roVal.c_str());
          BTStatus ws = fc.writeValue("should_fail");
          if (!ws) {
            Serial.printf("[CLIENT] FailClosed write rejected: %s\n", ws.toString());
          } else {
            Serial.println("[CLIENT] FailClosed write UNEXPECTEDLY succeeded");
          }
        }

        // Runtime enforcement of descriptor permissions. On DESC_CHAR the
        // server registers a custom-UUID descriptor with Read-only perms.
        // Expected behavior on both backends:
        //   - getDescriptor() finds it,
        //   - readValue() returns "ro_desc_val",
        //   - writeValue() fails at the ATT layer.
        // Previously Bluedroid hardcoded READ|WRITE for every descriptor,
        // silently ignoring the declared permissions — this write would
        // succeed. The new permToEsp()-based path must reject it.
        auto descChrRemote = isvc.getCharacteristic(descCharUUID);
        if (!descChrRemote) {
          Serial.println("[CLIENT] DescPerm descChr not found");
        } else {
          auto roDesc = descChrRemote.getDescriptor(descReadOnlyUUID);
          if (!roDesc) {
            Serial.println("[CLIENT] DescPerm RO descriptor not found");
          } else {
            Serial.println("[CLIENT] DescPerm RO descriptor found");
            String rv = roDesc.readValue();
            Serial.printf("[CLIENT] DescPerm RO read: %s\n", rv.c_str());
            BTStatus dws = roDesc.writeValue("nope");
            if (!dws) {
              Serial.printf("[CLIENT] DescPerm RO write rejected: %s\n", dws.toString());
            } else {
              Serial.println("[CLIENT] DescPerm RO write UNEXPECTEDLY succeeded");
            }
          }
        }
      }
      ic.disconnect();
      delay(500);
    }
    Serial.println("[CLIENT] Introspection done");
  }

  // ===========================================================================
  // Phase 16: Connection info + MTU/conn-params callbacks
  // ===========================================================================
  // Reconnect with a fresh client, force an MTU renegotiation by calling
  // setMTU before connect, trigger a conn-params update request through
  // BLEClient::updateConnParams, then sample both the client-side callback
  // state (via the globals set from installClientCallbacks()) and the
  // full BLEConnInfo introspection surface.
  waitForPhase(16);
  {
    clientMtuChanged = false;
    clientMtuLast = 0;
    clientConnParamsUpdated = false;
    // Reset ordering guard BEFORE connect: onConnect fires during connect(),
    // so clientConnectFired must start false to prove the callback order.
    clientConnectFired = false;
    clientMtuAfterConnect = false;

    BLEClient pc = BLE.createClient();
    installClientCallbacks(pc);
    pc.setMTU(247);  // non-default to force MTU exchange on connect
    BTStatus s = pc.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Phase16 connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Phase16 connected");
      BLEConnInfo c = pc.getConnInfo();
      uint16_t mtu = pc.getMTU();
      BTAddress peer = pc.getPeerAddress();
      uint16_t h = pc.getHandle();
      Serial.printf("[CLIENT] ConnInfo handle=%u mtu=%u clientMtu=%u\n", (unsigned)c.getHandle(), (unsigned)c.getMTU(), (unsigned)mtu);
      Serial.printf("[CLIENT] ConnInfo peerAddr=%s handle=%u\n", peer.toString().c_str(), (unsigned)h);
      Serial.printf(
        "[CLIENT] ConnInfo sec enc=%d auth=%d bond=%d keySize=%u\n", (int)c.isEncrypted(), (int)c.isAuthenticated(), (int)c.isBonded(),
        (unsigned)c.getSecurityKeySize()
      );
      Serial.printf("[CLIENT] ConnInfo role central=%d peripheral=%d\n", (int)c.isCentral(), (int)c.isPeripheral());
      Serial.printf(
        "[CLIENT] ConnInfo params interval=%u latency=%u timeout=%u\n", (unsigned)c.getInterval(), (unsigned)c.getLatency(), (unsigned)c.getSupervisionTimeout()
      );
      Serial.printf("[CLIENT] ConnInfo phy tx=%u rx=%u\n", (unsigned)c.getTxPhy(), (unsigned)c.getRxPhy());
      Serial.printf("[CLIENT] ConnInfo rssi=%d\n", (int)c.getRSSI());

      // Trigger a conn-params update so the server's onConnParamsUpdate
      // callback has something to fire on. Use a noticeably different
      // interval so both sides see a real change.
      BLEConnParams np;
      np.minInterval = 24;  // 30 ms
      np.maxInterval = 40;  // 50 ms
      np.latency = 0;
      np.timeout = 400;  // 4 s
      BTStatus us = pc.updateConnParams(np);
      Serial.printf("[CLIENT] UpdateConnParams ok=%d\n", (int)(bool)us);
      delay(2500);  // allow the controller to apply the new params

      Serial.printf("[CLIENT] MtuChanged seen=%d last=%u\n", (int)clientMtuChanged, (unsigned)clientMtuLast);
      // Ordering contract: when the MTU callback fired, onConnect must already
      // have fired (afterConnect=1). afterConnect=0 with mtuSeen=1 signals the
      // regression this test guards against.
      Serial.printf("[CLIENT] MtuOrder afterConnect=%d mtuSeen=%d\n", (int)clientMtuAfterConnect, (int)clientMtuChanged);

      pc.disconnect();
      BLE.setMTU(512);
      delay(1000);
    }
    Serial.println("[CLIENT] Phase16 done");
  }

  // ===========================================================================
  // Phase 17: Address types — API surface + connection roundtrip per type
  // ===========================================================================
  // (a) Call BLE.setOwnAddressType with every BTAddress::Type enum value.
  // (b) For each type the server rebuilds advertising with, scan for the
  //     per-type tagged name "T17_<TypeName>_<serverName>", connect, sample
  //     the peer's BTAddress::Type, and disconnect. This is the real
  //     end-to-end verification that BTAddress::Type propagates through
  //     both the adv PDU and the connection establishment path.
  waitForPhase(17);
  {
    BTAddress self = BLE.getAddress();
    Serial.printf("[CLIENT] OwnAddress addr=%s type=%u\n", self.toString().c_str(), (unsigned)self.type());

    // (a) API-surface coverage.
    BTStatus s1 = BLE.setOwnAddressType(BTAddress::Public);
    Serial.printf("[CLIENT] setOwnAddressType(Public) ok=%d\n", (int)(bool)s1);
    BTStatus s2 = BLE.setOwnAddressType(BTAddress::Random);
    Serial.printf("[CLIENT] setOwnAddressType(Random) ok=%d\n", (int)(bool)s2);
    BTStatus s3 = BLE.setOwnAddressType(BTAddress::PublicID);
    Serial.printf("[CLIENT] setOwnAddressType(PublicID) ok=%d\n", (int)(bool)s3);
    BTStatus s4 = BLE.setOwnAddressType(BTAddress::RandomID);
    Serial.printf("[CLIENT] setOwnAddressType(RandomID) ok=%d\n", (int)(bool)s4);
    (void)BLE.setOwnAddressType(self.type());

    // (b) Connection roundtrip per type. The server cycles through Public
    //     and Random, each tagged by a per-type name in the scan response.
    struct TypeSpec {
      const char *name;
    };
    const TypeSpec types[] = {{"Public"}, {"Random"}};
    for (const auto &ts : types) {
      String hitName = String("T17_") + ts.name + "_" + targetName;

      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);  // need scan-response parsing to see the tag
      scn.setFilterDuplicates(false);
      BTAddress hitAddr;
      uint8_t hitType = 0xFF;
      bool hit = false;
      unsigned long deadline = millis() + 10000;
      while (!hit && millis() < deadline) {
        BLEScan::Results res = scn.startBlocking(3000);
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == hitName) {
            hitAddr = d.getAddress();
            hitType = (uint8_t)d.getAddressType();
            hit = true;
            break;
          }
        }
        scn.clearResults();
      }
      Serial.printf("[CLIENT] Phase17 scan type=%s hit=%d advType=%u\n", ts.name, (int)hit, (unsigned)hitType);

      if (!hit) {
        continue;
      }

      BLEClient tc = BLE.createClient();
      installClientCallbacks(tc);
      BTStatus cs = tc.connect(hitAddr);
      Serial.printf("[CLIENT] Phase17 connect type=%s ok=%d\n", ts.name, (int)(bool)cs);
      if (cs) {
        BTAddress peer = tc.getPeerAddress();
        Serial.printf("[CLIENT] Phase17 peer type=%s addr=%s connType=%u\n", ts.name, peer.toString().c_str(), (unsigned)peer.type());
        tc.disconnect();
        delay(800);
      }
    }

    Serial.println("[CLIENT] Phase17 done");
  }

  // ===========================================================================
  // Phase 18: Authorization flow
  // ===========================================================================
  // The AUTHZ char has ReadAuthorized|WriteAuthorized. Every access traps
  // through BLESecurity::onAuthorization on the server. The handler:
  //   - always approves reads,
  //   - approves writes with payload "OK",
  //   - denies writes with any other payload.
  // We exercise all three branches and assert the expected statuses.
  waitForPhase(18);
  {
    // Phase 17 cycled own-address type on the server, so targetAddr is no
    // longer guaranteed to match the current adv. Re-scan by name.
    {
      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);
      scn.setFilterDuplicates(false);
      unsigned long deadline = millis() + 6000;
      while (millis() < deadline) {
        BLEScan::Results res = scn.startBlocking(2000);
        bool found = false;
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            targetAddr = d.getAddress();
            found = true;
            break;
          }
        }
        scn.clearResults();
        if (found) {
          break;
        }
      }
    }
    BLEClient ac = BLE.createClient();
    installClientCallbacks(ac);
    BTStatus s = ac.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Phase18 connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Phase18 connected");
      BLERemoteService as = ac.getService(serviceUUID);
      BLERemoteCharacteristic ach = as.getCharacteristic(authzCharUUID);
      if (!ach) {
        Serial.println("[CLIENT] Authz char not found");
      } else {
        String rv = ach.readValue();
        Serial.printf("[CLIENT] Authz read: %s\n", rv.c_str());
        BTStatus wok = ach.writeValue(String("OK"));
        Serial.printf("[CLIENT] Authz write OK status: %d\n", (int)(bool)wok);
        delay(200);
        BTStatus wno = ach.writeValue(String("NO"));
        Serial.printf("[CLIENT] Authz write NO status: %d\n", (int)(bool)wno);
        delay(200);
        // Readback should return the last APPROVED value ("OK"), not "NO".
        String rb = ach.readValue();
        Serial.printf("[CLIENT] Authz readback: %s\n", rb.c_str());
      }
      ac.disconnect();
      delay(500);
    }
    Serial.println("[CLIENT] Phase18 done");
  }

  // ===========================================================================
  // Phase 19: Encrypted-permission enforcement (post-pair positive control)
  // ===========================================================================
  // Link has been paired since phase 10; the bond persists in NVS across
  // disconnects. The encrypted char should be readable + writable and
  // BLEConnInfo should report encrypted=1 bonded=1 (authenticated=1 when
  // MITM was used by phase 10).
  waitForPhase(19);
  {
    // Refresh the server's address in case earlier phases rotated it.
    {
      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);
      scn.setFilterDuplicates(false);
      unsigned long deadline = millis() + 6000;
      while (millis() < deadline) {
        BLEScan::Results res = scn.startBlocking(2000);
        bool found = false;
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            targetAddr = d.getAddress();
            found = true;
            break;
          }
        }
        scn.clearResults();
        if (found) {
          break;
        }
      }
    }
    BLEClient ec = BLE.createClient();
    installClientCallbacks(ec);
    BTStatus s = ec.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Phase19 connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Phase19 connected");
      BLERemoteService es = ec.getService(serviceUUID);
      BLERemoteCharacteristic ech = es.getCharacteristic(encryptedCharUUID);
      String rv = ech.readValue();
      Serial.printf("[CLIENT] Enc read: %s\n", rv.c_str());
      BTStatus ws = ech.writeValue(String("enc_ok"));
      Serial.printf("[CLIENT] Enc write ok=%d\n", (int)(bool)ws);
      delay(200);
      BLEConnInfo c = ec.getConnInfo();
      Serial.printf("[CLIENT] Phase19 sec enc=%d auth=%d bond=%d\n", (int)c.isEncrypted(), (int)c.isAuthenticated(), (int)c.isBonded());
      ec.disconnect();
      delay(500);
    }
    Serial.println("[CLIENT] Phase19 done");
  }

  // ===========================================================================
  // Phase 20: Advertisement data builders + scan tuning
  // ===========================================================================
  // The server rebuilds its advertisement with every BLEAdvertisementData
  // setter. Configure a passive scan with custom interval/window and duplicate
  // filtering, run startBlocking so we can iterate getResults(), and assert
  // that every parser getter returns the server's advertised value.
  waitForPhase(20);
  {
    BLEScan scn = BLE.getScan();
    // A lingering background scan from earlier phases will steal radio
    // time and drop scan responses; make sure the radio is quiet before
    // we start the measurement window.
    scn.stop();
    scn.resetCallbacks();
    scn.clearResults();
    scn.setActiveScan(true);  // need scan response for short-name
    // Changed same-length primary and scan-response payloads must still replace
    // the cache while byte-identical reports remain filtered.
    scn.setFilterDuplicates(true);
    scn.setInterval(160);  // 100 ms
    scn.setWindow(96);     // 60 ms
    bool sawAdv = false;
    String wantName = String("ADV_") + targetName;
    std::atomic<bool> sawInitialPrimary{false};
    std::atomic<bool> sawRefreshedPrimary{false};
    std::atomic<bool> sawInitialScanRsp{false};
    std::atomic<bool> sawRefreshedScanRsp{false};
    std::atomic<uint32_t> targetReports{0};
    scn.onResult([&](BLEAdvertisedDevice dev) {
      if (dev.getName() != wantName) {
        return;
      }
      targetReports.fetch_add(1, std::memory_order_relaxed);
      String hex = dev.getManufacturerDataString();
      if (hex == "e502455350") {
        sawInitialPrimary = true;
      } else if (hex == "e5024e4557") {
        sawRefreshedPrimary = true;
      }
      size_t svcLen = 0;
      const uint8_t *svcData = dev.getServiceData(0, &svcLen);
      if (svcData && svcLen == 2 && svcData[0] == 0xAB && svcData[1] == 0xCD) {
        sawInitialScanRsp = true;
      } else if (svcData && svcLen == 2 && svcData[0] == 'N' && svcData[1] == 'R') {
        sawRefreshedScanRsp = true;
      }
    });
    auto findTarget = [&](BLEScan::Results &results, BLEAdvertisedDevice &out) -> bool {
      for (const auto &dev : results) {
        String n = dev.getName();
        Serial.printf(
          "[CLIENT] Phase20 seen name=\"%s\" addr=%s payloadLen=%u\n", n.c_str(), dev.getAddress().toString().c_str(), (unsigned)dev.getPayloadLength()
        );
        if (n == wantName) {
          out = dev;
          return true;
        }
      }
      return false;
    };

    BLEAdvertisedDevice d;
    BLEScan::Results results = scn.startBlocking(8000);
    Serial.printf("[CLIENT] Phase20 results=%u isScanning=%d want=%s\n", (unsigned)results.size(), (int)scn.isScanning(), wantName.c_str());
    const BLEScan &constScan = scn;
    Serial.printf("[CLIENT] Phase20 cached results=%u\n", (unsigned)constScan.getResults().size());
    sawAdv = findTarget(results, d);
    // One retry if we only saw the primary ADV (no scan-response fields yet).
    if (sawAdv && !d.haveManufacturerData()) {
      Serial.println("[CLIENT] Phase20 retry scan for scan response");
      scn.clearResults();
      results = scn.startBlocking(4000);
      Serial.printf("[CLIENT] Phase20 retry results=%u\n", (unsigned)results.size());
      BLEAdvertisedDevice retry;
      if (findTarget(results, retry) && retry.getPayloadLength() >= d.getPayloadLength()) {
        d = retry;
      }
    }
    if (sawAdv) {
      Serial.printf("[CLIENT] Phase20 name=%s rssi=%d txPower=%d\n", d.getName().c_str(), (int)d.getRSSI(), (int)d.getTXPower());
      Serial.printf(
        "[CLIENT] Phase20 haveName=%d haveRSSI=%d haveTxPow=%d haveAppearance=%d haveMfg=%d haveSvcData=%d haveSvcUUID=%d\n", (int)d.haveName(),
        (int)d.haveRSSI(), (int)d.haveTXPower(), (int)d.haveAppearance(), (int)d.haveManufacturerData(), (int)d.haveServiceData(), (int)d.haveServiceUUID()
      );
      Serial.printf(
        "[CLIENT] Phase20 appearance=0x%04X addrType=%u connectable=%d legacy=%d\n", (unsigned)d.getAppearance(), (unsigned)d.getAddressType(),
        (int)d.isConnectable(), (int)d.isLegacyAdvertisement()
      );
      Serial.printf("[CLIENT] Phase20 svcUUIDs=%u first=%s\n", (unsigned)d.getServiceUUIDCount(), d.getServiceUUID(0).toString().c_str());
      // Hex of the whole manufacturer field, company id included: an intact
      // 0x02E5 + "ESP" is "e502455350". Reported verbatim so a corrupted field
      // shows what replaced it instead of only a length mismatch.
      String mfgHex = d.getManufacturerDataString();
      Serial.printf(
        "[CLIENT] Phase20 mfgCompany=0x%04X mfgLen=%u mfgHex=%s\n", (unsigned)d.getManufacturerCompanyId(), (unsigned)mfgHex.length(), mfgHex.c_str()
      );
      size_t svcLen = 0;
      const uint8_t *svcRaw = d.getServiceData(0, &svcLen);
      BLEUUID svc16((uint16_t)0x180D);
      Serial.printf(
        "[CLIENT] Phase20 svcDataLen=%u advertisesSvc=%d payloadLen=%u\n", (unsigned)svcLen, (int)d.isAdvertisingService(svc16), (unsigned)d.getPayloadLength()
      );
      (void)svcRaw;
      // Slave Connection Interval Range AD (0x12): parsed directly by
      // BLEAdvertisedDevice::havePreferredParams / getMinPreferred / getMaxPreferred.
      bool havePref = d.havePreferredParams();
      uint16_t prefMin = d.getMinPreferred();
      uint16_t prefMax = d.getMaxPreferred();
      Serial.printf("[CLIENT] Phase20 prefInterval have=%d min=0x%04X max=0x%04X\n", (int)havePref, (unsigned)prefMin, (unsigned)prefMax);
    }
    Serial.printf("[CLIENT] Phase20 primaryRefresh initial=%d refreshed=%d\n", (int)sawInitialPrimary.load(), (int)sawRefreshedPrimary.load());
    Serial.printf("[CLIENT] Phase20 scanRspRefresh initial=%d refreshed=%d\n", (int)sawInitialScanRsp.load(), (int)sawRefreshedScanRsp.load());
    uint32_t reportsBeforeClear = targetReports.load(std::memory_order_relaxed);
    scn.clearDuplicateCache();
    BTStatus redispatchStart = scn.start(1200, true);
    unsigned long redispatchDeadline = millis() + 3200;
    while (scn.isScanning() && millis() < redispatchDeadline) {
      delay(10);
    }
    bool duplicateRedispatched = redispatchStart && targetReports.load(std::memory_order_relaxed) > reportsBeforeClear;
    Serial.printf("[CLIENT] Phase20 duplicateCacheClear redispatched=%d\n", (int)duplicateRedispatched);
    Serial.printf("[CLIENT] Phase20 sawAdv=%d\n", (int)sawAdv);

    // Second scan: server switches to OSZ_<name>, whose scan response holds only
    // malformed AD structures — truncated UUID lists and a zero-length terminator
    // shadowing a well-formed manufacturer AD. None may reach the parsed fields.
    // Server valid window is 14 s; after ADV_ (+ optional retry) wait for OSZ_.
    scn.stop();
    delay(3000);
    scn.clearResults();
    String oszWant = String("OSZ_") + targetName;
    BLEScan::Results oszResults = scn.startBlocking(8000);
    int terminatorHidMfg = -1;
    int parseMalformedUuids = -1;
    for (const auto &dev : oszResults) {
      BLEAdvertisedDevice d = dev;
      if (d.getName() == oszWant) {
        // The well-formed manufacturer AD sits behind a zero-length terminator,
        // so it may only surface if the terminator was ignored.
        terminatorHidMfg = d.haveManufacturerData() ? 0 : 1;
        // The scan response carries three truncated UUID lists and the primary
        // advertisement carries none, so a parser that neither rounds a partial
        // UUID up into a whole one nor walks past the terminator ends up with no
        // service UUID at all.
        parseMalformedUuids = (d.getServiceUUIDCount() == 0) ? 1 : 0;
        Serial.printf(
          "[CLIENT] Phase20 malformedAd name=%s haveMfg=%d svcCount=%u payloadLen=%u\n", d.getName().c_str(), (int)d.haveManufacturerData(),
          (unsigned)d.getServiceUUIDCount(), (unsigned)d.getPayloadLength()
        );
        break;
      }
    }
    Serial.printf("[CLIENT] Phase20 terminatorHidMfg=%d malformedUuidsRejected=%d\n", terminatorHidMfg, parseMalformedUuids);

    scn.clearResults();
    scn.resetCallbacks();
    scn.setActiveScan(true);
    Serial.println("[CLIENT] Phase20 done");
  }

  // ===========================================================================
  // Phase 21: Beacon / Eddystone build + parse
  // ===========================================================================
  // Server rotates iBeacon, URL, encrypted eTLM, then plaintext TLM payloads.
  // We rely on BLEAdvertisedDevice::getFrameType() to route
  // each hit to the matching parser. The payload accessors below (mfgData,
  // serviceData) return the same structures the classifier inspected so we
  // don't need to redo any AD parsing.
  waitForPhase(21);
  {
    bool sawIBeacon = false, sawEddyUrl = false, sawEddyTlm = false, rejectedEncryptedTlm = false;
    int eddy01Frame = -1;
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    scn.setActiveScan(false);
    scn.setFilterDuplicates(false);
    unsigned long deadline = millis() + 25000;
    while (millis() < deadline && (!sawIBeacon || !sawEddyUrl || !sawEddyTlm || !rejectedEncryptedTlm || eddy01Frame < 0)) {
      BLEScan::Results res = scn.startBlocking(3000);
      for (const auto &dev : res) {
        BLEAdvertisedDevice d = dev;
        switch (d.getFrameType()) {
          case BLEAdvertisedDevice::IBeacon:
          {
            if (sawIBeacon) {
              break;
            }
            BLEBeacon b;
            if (d.getBeacon(b)) {
              Serial.printf(
                "[CLIENT] Phase21 iBeacon major=0x%04X minor=0x%04X power=%d uuid=%s\n", (unsigned)b.getMajor(), (unsigned)b.getMinor(),
                (int)b.getSignalPower(), b.getProximityUUID().toString().c_str()
              );
              sawIBeacon = true;
            }
            break;
          }
          case BLEAdvertisedDevice::EddystoneURL:
          {
            if (sawEddyUrl) {
              break;
            }
            BLEEddystoneURL url;
            if (d.getEddystoneURL(url)) {
              Serial.printf("[CLIENT] Phase21 EddyURL url=%s tx=%d\n", url.getURL().c_str(), (int)url.getTxPower());
              sawEddyUrl = true;
            }
            break;
          }
          case BLEAdvertisedDevice::EddystoneTLM:
          {
            if (sawEddyTlm) {
              break;
            }
            BLEEddystoneTLM tlm;
            if (d.getEddystoneTLM(tlm)) {
              Serial.printf(
                "[CLIENT] Phase21 EddyTLM volt=%u temp=%d cnt=%lu up=%lu\n", (unsigned)tlm.getBatteryVoltage(), (int)tlm.getTemperature(),
                (unsigned long)tlm.getAdvertisingCount(), (unsigned long)tlm.getUptime()
              );
              sawEddyTlm = true;
            } else {
              rejectedEncryptedTlm = true;
            }
            break;
          }
          default: break;
        }
        const BLEUUID eddy = BLEEddystoneTLM::serviceUUID();
        for (size_t i = 0; i < d.getServiceDataCount(); i++) {
          size_t sdLen = 0;
          const uint8_t *sd = d.getServiceData(i, &sdLen);
          if (d.getServiceDataUUID(i) == eddy && sd && sdLen >= 1 && sd[0] == 0x01) {
            eddy01Frame = static_cast<int>(d.getFrameType());
          }
        }
      }
      scn.clearResults();
    }
    Serial.printf(
      "[CLIENT] Phase21 sawIBeacon=%d sawEddyURL=%d sawEddyTLM=%d rejectedEncryptedTLM=%d eddy01Frame=%d\n", (int)sawIBeacon, (int)sawEddyUrl, (int)sawEddyTlm,
      (int)rejectedEncryptedTlm, eddy01Frame
    );
    Serial.println("[CLIENT] Phase21 done");
  }

  // ===========================================================================
  // Phase 22: Bond + whitelist management
  // ===========================================================================
  // Log bonds, round-trip the whitelist, then (optionally) exercise
  // deleteBond — but keep the bond intact so later phases that rely on
  // encryption still work without triggering an interactive re-pair.
  waitForPhase(22);
  {
    BLESecurity s = BLE.getSecurity();
    s.setInitiatorKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
    s.setResponderKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
    auto bonds = s.getBondedDevices();
    Serial.printf("[CLIENT] Bond count: %u\n", (unsigned)bonds.size());
    for (size_t i = 0; i < bonds.size() && i < 4; i++) {
      Serial.printf("[CLIENT] Bond[%u]=%s\n", (unsigned)i, bonds[i].toString().c_str());
    }
    BTStatus a = BLE.whiteListAdd(targetAddr);
    bool on = BLE.isOnWhiteList(targetAddr);
    BTStatus r = BLE.whiteListRemove(targetAddr);
    bool after = BLE.isOnWhiteList(targetAddr);
    Serial.printf("[CLIENT] Whitelist add=%d isOn=%d remove=%d after=%d\n", (int)(bool)a, (int)on, (int)(bool)r, (int)after);
    // See server Phase 22 for the matching lines — the Python harness
    // cross-checks local(A) == peer(B) and vice versa.
    String localIrkHex = BLE.getLocalIRKString();
    Serial.flush();
    delay(50);
    Serial.printf("[CLIENT] LocalIRK present=%d hex=%s\n", (int)(localIrkHex.length() > 0), localIrkHex.c_str());
    Serial.flush();
    delay(50);
    if (!bonds.empty()) {
      String peerIrkHex = BLE.getPeerIRKString(bonds[0]);
      Serial.printf("[CLIENT] PeerIRK present=%d hex=%s\n", (int)(peerIrkHex.length() > 0), peerIrkHex.c_str());
      Serial.flush();
      delay(50);
    } else {
      Serial.println("[CLIENT] PeerIRK present=0 hex=");
      Serial.flush();
      delay(50);
    }
    Serial.println("[CLIENT] Phase22 done");
  }

  // ===========================================================================
  // Phase 23: Error paths + miscellaneous coverage
  // ===========================================================================
  // Unknown service/char lookups, ops-after-disconnect, connectAsync/cancel,
  // typed readUInt* and readRawData paths, UUID algebra, operator bool,
  // implicit const char* -> BLEUUID at client/remote GATT API boundaries.
  waitForPhase(23);
  {
    int strPass = 0;
    int strFail = 0;
    auto strCheck = [&](bool ok) {
      if (ok) {
        ++strPass;
      } else {
        ++strFail;
      }
    };
    // Earlier phases (17, 20, 21) have repeatedly torn down adv, rotated
    // own-address type, and rebuilt payloads — any of which can stale the
    // cached targetAddr. Do a short re-scan by name before reconnecting so
    // we pick up the server's current advertised address.
    bool rescanFound = false;
    {
      BLEScan scn = BLE.getScan();
      scn.stop();
      delay(100);
      scn.resetCallbacks();
      scn.clearResults();
      scn.clearDuplicateCache();
      scn.setActiveScan(true);
      scn.setFilterDuplicates(false);
      unsigned long deadline = millis() + 10000;
      while (millis() < deadline) {
        BLEScan::Results res = scn.startBlocking(3000);
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            strCheck(d.isAdvertisingService("4fafc201-1fb5-459e-8fcc-c5c9c331914b"));
            targetAddr = d.getAddress();
            rescanFound = true;
            break;
          }
        }
        scn.clearResults();
        if (rescanFound) {
          break;
        }
      }
    }
    Serial.printf("[CLIENT] Phase23 rescan found=%d addr=%s\n", (int)rescanFound, targetAddr.toString().c_str());
    BLEClient ec = BLE.createClient();
    installClientCallbacks(ec);
    BTStatus cs = ec.connect(targetAddr);
    if (!cs) {
      Serial.printf("[CLIENT] Phase23 connect FAILED: %s\n", cs.toString());
    } else {
      Serial.println("[CLIENT] Phase23 connected");
      BLERemoteService unk = ec.getService(unknownSvcUUID);
      Serial.printf("[CLIENT] Phase23 getService unknown ok=%d\n", (int)(bool)unk);

      BLERemoteService good = ec.getService(serviceUUID);
      BLERemoteCharacteristic unkC = good.getCharacteristic(unknownCharUUID);
      Serial.printf("[CLIENT] Phase23 getChar unknown ok=%d\n", (int)(bool)unkC);

      strCheck(BLEUUID("180D").bitSize() == 16);
      strCheck(BLEUUID("0000180D").bitSize() == 32);
      constexpr BLEUUID cUuidEmpty;
      constexpr BLEUUID cUuid16 = BLEUUID::from16(0x180D);
      constexpr BLEUUID cUuid128 = BLEUUID::from128(0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
      static_assert(!cUuidEmpty.isValid());
      static_assert(cUuid16.isValid() && cUuid16.bitSize() == 16);
      static_assert(cUuid128.isValid() && cUuid128.bitSize() == 128);
      strCheck(cUuid16 == BLEUUID(static_cast<uint16_t>(0x180D)) && cUuid16.toUint16() == 0x180D);
      strCheck(cUuid128 == BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e"));
      BLERemoteService goodStr = ec.getService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
      strCheck(goodStr && good && goodStr.getUUID() == good.getUUID());
      strCheck(good.getClient() && good.getClient().isConnected());
      BLERemoteDescriptor detachedDesc;
      if (goodStr) {
        BLERemoteCharacteristic rwStr = goodStr.getCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8");
        strCheck(rwStr && rwStr.getUUID() == rwCharUUID);
        strCheck(rwStr.getRemoteService() && rwStr.getRemoteService().getUUID() == goodStr.getUUID());
        String valByClient = ec.getValue("4fafc201-1fb5-459e-8fcc-c5c9c331914b", "beb5483e-36e1-4688-b7f5-ea07361b26a8");
        strCheck(valByClient.length() > 0);
        String valBySvc = goodStr.getValue("beb5483e-36e1-4688-b7f5-ea07361b26a8");
        strCheck(valBySvc.length() > 0);
        BTStatus setByClient = ec.setValue("4fafc201-1fb5-459e-8fcc-c5c9c331914b", "beb5483e-36e1-4688-b7f5-ea07361b26a8", "via_client_set");
        String afterClientSet = ec.getValue("4fafc201-1fb5-459e-8fcc-c5c9c331914b", "beb5483e-36e1-4688-b7f5-ea07361b26a8");
        BTStatus setBySvc = goodStr.setValue("beb5483e-36e1-4688-b7f5-ea07361b26a8", "via_svc_set");
        String afterSvcSet = goodStr.getValue("beb5483e-36e1-4688-b7f5-ea07361b26a8");
        bool clientSetOk = (bool)setByClient && (afterClientSet == "via_client_set");
        bool svcSetOk = (bool)setBySvc && (afterSvcSet == "via_svc_set");
        strCheck(clientSetOk);
        strCheck(svcSetOk);
        Serial.printf("[CLIENT] Phase23 setValue clientOk=%d svcOk=%d\n", (int)clientSetOk, (int)svcSetOk);
        BLERemoteCharacteristic descStr = goodStr.getCharacteristic("a3c87501-8ed3-4bdf-8a39-a01bebede295");
        if (descStr) {
          BLERemoteDescriptor roStr = descStr.getDescriptor("b5f3e2c1-8a9e-4b7c-9f3d-2e8a5b7c3f91");
          strCheck(static_cast<bool>(roStr));
          strCheck(roStr.getRemoteCharacteristic() && roStr.getRemoteCharacteristic().getUUID() == descStr.getUUID());
          detachedDesc = roStr;
        }
      }

      BLERemoteCharacteristic rwC = good.getCharacteristic(rwCharUUID);
      // Typed reads — any value fits in a uint8, so we just check that
      // the API returns something without crashing.
      uint8_t u8 = rwC.readUInt8();
      uint16_t u16 = rwC.readUInt16();
      uint32_t u32 = rwC.readUInt32();
      float f32 = rwC.readFloat();
      Serial.printf("[CLIENT] Phase23 typed u8=%u u16=%u u32=%lu f32=%d\n", (unsigned)u8, (unsigned)u16, (unsigned long)u32, (int)f32);

      uint8_t buf[32] = {0};
      size_t rLen = rwC.readValue(buf, sizeof(buf));
      Serial.printf("[CLIENT] Phase23 readBuf len=%u\n", (unsigned)rLen);
      size_t rawLen = 0;
      const uint8_t *raw = rwC.readRawData(&rawLen);
      Serial.printf("[CLIENT] Phase23 readRaw len=%u nonNull=%d\n", (unsigned)rawLen, (int)(raw != nullptr));

      // UUID algebra
      BLEUUID ua(static_cast<uint16_t>(0x180D));
      BLEUUID ub(static_cast<uint16_t>(0x180D));
      BLEUUID uc(static_cast<uint16_t>(0x180E));
      Serial.printf("[CLIENT] Phase23 UUID eq=%d ne=%d lt=%d\n", (int)(ua == ub), (int)(ua != uc), (int)(ua < uc));
      BLEUUID u128 = ua.to128();
      Serial.printf("[CLIENT] Phase23 UUID bitSize16=%u 128=%u toUint16=0x%04X\n", (unsigned)ua.bitSize(), (unsigned)u128.bitSize(), (unsigned)ua.toUint16());
      BLEUUID ubad;
      Serial.printf("[CLIENT] Phase23 UUID valid=%d defaultBool=%d\n", (int)ua.isValid(), (int)(bool)ubad);

      // Race cached-value and parent getters against remote-tree teardown. The
      // retained children must keep their cache lock alive without retaining
      // the parent service/client ownership tree.
      struct RemoteReadRace {
        BLERemoteCharacteristic chr;
        BLERemoteDescriptor desc;
        std::atomic<bool> stop{false};
        std::atomic<bool> done{false};
        std::atomic<uint32_t> iterations{0};
      };
      static RemoteReadRace remoteRace;
      remoteRace.chr = rwC;
      remoteRace.desc = detachedDesc;
      remoteRace.stop.store(false, std::memory_order_relaxed);
      remoteRace.done.store(false, std::memory_order_relaxed);
      remoteRace.iterations.store(0, std::memory_order_relaxed);
      bool remoteRaceCreated = xTaskCreate(
                                 [](void *arg) {
                                   auto *race = static_cast<RemoteReadRace *>(arg);
                                   while (!race->stop.load()) {
                                     size_t len = 0;
                                     (void)race->chr.readRawData(&len);
                                     (void)race->chr.getRemoteService();
                                     (void)race->desc.readRawData(&len);
                                     (void)race->desc.getRemoteCharacteristic();
                                     race->iterations.fetch_add(1);
                                     taskYIELD();
                                   }
                                   race->done.store(true, std::memory_order_release);
                                   vTaskDelete(nullptr);
                                 },
                                 "remote-cache", 4096, &remoteRace, 1, nullptr
                               )
                               == pdPASS;
      delay(20);

      // Leave only the client tree owning the parents. Disconnect now destroys
      // those parents while the task above retains and reads only descendants.
      good = BLERemoteService();
      goodStr = BLERemoteService();

      // Disconnect + op-after-disconnect
      ec.disconnect();
      delay(1500);
      remoteRace.stop = true;
      unsigned long remoteRaceDeadline = millis() + 2000;
      while (remoteRaceCreated && !remoteRace.done.load(std::memory_order_acquire) && millis() < remoteRaceDeadline) {
        delay(1);
      }
      bool remoteRaceClean =
        remoteRaceCreated && remoteRace.done.load(std::memory_order_acquire) && remoteRace.iterations.load() > 0 && heap_caps_check_integrity_all(true);
      Serial.printf("[CLIENT] Phase23 remote cache race clean=%d iterations=%lu\n", (int)remoteRaceClean, (unsigned long)remoteRace.iterations.load());
      strCheck(remoteRaceClean);
      if (remoteRace.done.load(std::memory_order_acquire)) {
        remoteRace.chr = BLERemoteCharacteristic();
        remoteRace.desc = BLERemoteDescriptor();
      }
      Serial.printf("[CLIENT] Phase23 postDisc isConnected=%d\n", (int)ec.isConnected());
      // Disconnect must recursively detach externally retained descendants
      // immediately; dropping the retained service handles must not be what
      // finally clears child-to-parent navigation.
      strCheck(!rwC.getRemoteService());
      strCheck(detachedDesc && !detachedDesc.getRemoteCharacteristic());
      strCheck(!rwC.getRemoteService());
      strCheck(detachedDesc && !detachedDesc.getRemoteCharacteristic() && !detachedDesc.writeValue("x"));
      BTStatus rwAfter = rwC.writeValue(String("after"));
      Serial.printf("[CLIENT] Phase23 writeAfterDisc ok=%d\n", (int)(bool)rwAfter);

      // Exercise the lazy-discovery publication flags directly: aggregate and
      // targeted callers start together on a fresh cache while disconnect
      // invalidates the owning client tree. Outcomes may be populated or empty
      // depending on which operation wins, but both callers must terminate
      // without racing vector publication or detached-parent cleanup.
      BLEClient discoveryClient = BLE.createClient();
      BTStatus discoveryConnect = discoveryClient.connect(targetAddr);
      BLERemoteService discoveryService = discoveryConnect ? discoveryClient.getService(serviceUUID) : BLERemoteService();
      static std::atomic<bool> serviceDiscoveryGo{false};
      static ServiceDiscoveryRaceJob serviceDiscoveryAggregate;
      static ServiceDiscoveryRaceJob serviceDiscoveryTarget;
      serviceDiscoveryAggregate.service = discoveryService;
      serviceDiscoveryAggregate.uuid = rwCharUUID;
      serviceDiscoveryAggregate.aggregate = true;
      serviceDiscoveryAggregate.go = &serviceDiscoveryGo;
      serviceDiscoveryAggregate.entered.store(false, std::memory_order_relaxed);
      serviceDiscoveryAggregate.done.store(false, std::memory_order_relaxed);
      serviceDiscoveryAggregate.result = BLERemoteCharacteristic();
      serviceDiscoveryTarget.service = discoveryService;
      serviceDiscoveryTarget.uuid = rwCharUUID;
      serviceDiscoveryTarget.aggregate = false;
      serviceDiscoveryTarget.go = &serviceDiscoveryGo;
      serviceDiscoveryTarget.entered.store(false, std::memory_order_relaxed);
      serviceDiscoveryTarget.done.store(false, std::memory_order_relaxed);
      serviceDiscoveryTarget.result = BLERemoteCharacteristic();
      bool serviceAggregateCreated =
        discoveryService && xTaskCreate(serviceDiscoveryRaceTask, "svc-disc-all", 6144, &serviceDiscoveryAggregate, 1, nullptr) == pdPASS;
      bool serviceTargetCreated =
        discoveryService && xTaskCreate(serviceDiscoveryRaceTask, "svc-disc-one", 6144, &serviceDiscoveryTarget, 1, nullptr) == pdPASS;
      serviceDiscoveryGo.store(true, std::memory_order_release);
      unsigned long entryDeadline = millis() + 2000;
      while (serviceAggregateCreated && serviceTargetCreated
             && (!serviceDiscoveryAggregate.entered.load(std::memory_order_acquire) || !serviceDiscoveryTarget.entered.load(std::memory_order_acquire))
             && millis() < entryDeadline) {
        taskYIELD();
      }
      if (discoveryConnect) {
        discoveryClient.disconnect();
      }
      unsigned long discoveryDeadline = millis() + 15000;
      while (((serviceAggregateCreated && !serviceDiscoveryAggregate.done.load(std::memory_order_acquire))
              || (serviceTargetCreated && !serviceDiscoveryTarget.done.load(std::memory_order_acquire)))
             && millis() < discoveryDeadline) {
        delay(1);
      }
      bool serviceDiscoveryClean = serviceAggregateCreated && serviceTargetCreated && serviceDiscoveryAggregate.done.load(std::memory_order_acquire)
                                   && serviceDiscoveryTarget.done.load(std::memory_order_acquire)
                                   && (!serviceDiscoveryAggregate.result || !serviceDiscoveryAggregate.result.getRemoteService())
                                   && (!serviceDiscoveryTarget.result || !serviceDiscoveryTarget.result.getRemoteService());

      BTStatus descriptorConnect = discoveryClient.connect(targetAddr);
      BLERemoteService descriptorService = descriptorConnect ? discoveryClient.getService(serviceUUID) : BLERemoteService();
      BLERemoteCharacteristic descriptorChr = descriptorService ? descriptorService.getCharacteristic(descCharUUID) : BLERemoteCharacteristic();
      static std::atomic<bool> descriptorDiscoveryGo{false};
      static DescriptorDiscoveryRaceJob descriptorDiscoveryAggregate;
      static DescriptorDiscoveryRaceJob descriptorDiscoveryTarget;
      descriptorDiscoveryAggregate.chr = descriptorChr;
      descriptorDiscoveryAggregate.uuid = descReadOnlyUUID;
      descriptorDiscoveryAggregate.aggregate = true;
      descriptorDiscoveryAggregate.go = &descriptorDiscoveryGo;
      descriptorDiscoveryAggregate.entered.store(false, std::memory_order_relaxed);
      descriptorDiscoveryAggregate.done.store(false, std::memory_order_relaxed);
      descriptorDiscoveryAggregate.result = BLERemoteDescriptor();
      descriptorDiscoveryTarget.chr = descriptorChr;
      descriptorDiscoveryTarget.uuid = descReadOnlyUUID;
      descriptorDiscoveryTarget.aggregate = false;
      descriptorDiscoveryTarget.go = &descriptorDiscoveryGo;
      descriptorDiscoveryTarget.entered.store(false, std::memory_order_relaxed);
      descriptorDiscoveryTarget.done.store(false, std::memory_order_relaxed);
      descriptorDiscoveryTarget.result = BLERemoteDescriptor();
      bool descriptorAggregateCreated =
        descriptorChr && xTaskCreate(descriptorDiscoveryRaceTask, "dsc-disc-all", 6144, &descriptorDiscoveryAggregate, 1, nullptr) == pdPASS;
      bool descriptorTargetCreated =
        descriptorChr && xTaskCreate(descriptorDiscoveryRaceTask, "dsc-disc-one", 6144, &descriptorDiscoveryTarget, 1, nullptr) == pdPASS;
      descriptorDiscoveryGo.store(true, std::memory_order_release);
      entryDeadline = millis() + 2000;
      while (descriptorAggregateCreated && descriptorTargetCreated
             && (!descriptorDiscoveryAggregate.entered.load(std::memory_order_acquire) || !descriptorDiscoveryTarget.entered.load(std::memory_order_acquire))
             && millis() < entryDeadline) {
        taskYIELD();
      }
      if (descriptorConnect) {
        discoveryClient.disconnect();
      }
      discoveryDeadline = millis() + 15000;
      while (((descriptorAggregateCreated && !descriptorDiscoveryAggregate.done.load(std::memory_order_acquire))
              || (descriptorTargetCreated && !descriptorDiscoveryTarget.done.load(std::memory_order_acquire)))
             && millis() < discoveryDeadline) {
        delay(1);
      }
      bool descriptorDiscoveryClean = descriptorAggregateCreated && descriptorTargetCreated && descriptorDiscoveryAggregate.done.load(std::memory_order_acquire)
                                      && descriptorDiscoveryTarget.done.load(std::memory_order_acquire)
                                      && (!descriptorDiscoveryAggregate.result || !descriptorDiscoveryAggregate.result.getRemoteCharacteristic())
                                      && (!descriptorDiscoveryTarget.result || !descriptorDiscoveryTarget.result.getRemoteCharacteristic());

      BTStatus freshDiscoveryConnect = discoveryClient.connect(targetAddr);
      BLERemoteService freshDiscoveryService = freshDiscoveryConnect ? discoveryClient.getService(serviceUUID) : BLERemoteService();
      auto freshCharacteristics = freshDiscoveryService ? freshDiscoveryService.getCharacteristics() : std::vector<BLERemoteCharacteristic>{};
      BLERemoteCharacteristic freshDescriptorChr = freshDiscoveryService ? freshDiscoveryService.getCharacteristic(descCharUUID) : BLERemoteCharacteristic();
      auto freshDescriptors = freshDescriptorChr ? freshDescriptorChr.getDescriptors() : std::vector<BLERemoteDescriptor>{};
      bool freshDiscoveryClean = !freshCharacteristics.empty() && !freshDescriptors.empty();
      if (freshDiscoveryConnect) {
        discoveryClient.disconnect();
      }
      bool discoveryRaceClean = serviceDiscoveryClean && descriptorDiscoveryClean && freshDiscoveryClean && heap_caps_check_integrity_all(true);
      Serial.printf(
        "[CLIENT] Phase23 lazy discovery race clean=%d serviceResults=%u descriptorResults=%u\n", (int)discoveryRaceClean,
        (unsigned)(serviceDiscoveryAggregate.resultCount.load() + serviceDiscoveryTarget.resultCount.load()),
        (unsigned)(descriptorDiscoveryAggregate.resultCount.load() + descriptorDiscoveryTarget.resultCount.load())
      );
      strCheck(discoveryRaceClean);
      if (serviceDiscoveryAggregate.done.load(std::memory_order_acquire)) {
        serviceDiscoveryAggregate.service = BLERemoteService();
        serviceDiscoveryAggregate.result = BLERemoteCharacteristic();
      }
      if (serviceDiscoveryTarget.done.load(std::memory_order_acquire)) {
        serviceDiscoveryTarget.service = BLERemoteService();
        serviceDiscoveryTarget.result = BLERemoteCharacteristic();
      }
      if (descriptorDiscoveryAggregate.done.load(std::memory_order_acquire)) {
        descriptorDiscoveryAggregate.chr = BLERemoteCharacteristic();
        descriptorDiscoveryAggregate.result = BLERemoteDescriptor();
      }
      if (descriptorDiscoveryTarget.done.load(std::memory_order_acquire)) {
        descriptorDiscoveryTarget.chr = BLERemoteCharacteristic();
        descriptorDiscoveryTarget.result = BLERemoteDescriptor();
      }

      // Reconnect the same client object before touching the retained old
      // characteristic again. Native connection IDs are recyclable; the stale
      // generation must remain detached even if the backend assigns the new
      // link the same ID.
      BTStatus reconnect = ec.connect(targetAddr);
      BLERemoteService freshService = reconnect ? ec.getService(serviceUUID) : BLERemoteService();
      BLERemoteCharacteristic freshRw = freshService ? freshService.getCharacteristic(rwCharUUID) : BLERemoteCharacteristic();
      BTStatus staleOnNewGeneration = rwC.writeValue("stale_generation", false);
      String freshValue = freshRw ? freshRw.readValue() : String();
      bool generationIsolated = reconnect && freshRw && staleOnNewGeneration == BTStatus::InvalidState && freshValue.length() > 0;
      Serial.printf(
        "[CLIENT] Phase23 reconnect generation isolated=%d stale=%s fresh=%d\n", (int)generationIsolated, staleOnNewGeneration.toString(),
        (int)(freshValue.length() > 0)
      );
      strCheck(generationIsolated);
      if (reconnect) {
        ec.disconnect();
        delay(300);
      }

      // connectAsync + cancelConnect
      BLEClient ac = BLE.createClient();
      BTStatus as = ac.connectAsync(targetAddr);
      Serial.printf("[CLIENT] Phase23 connectAsync ok=%d\n", (int)(bool)as);
      delay(100);
      BTStatus cancel = ac.cancelConnect();
      Serial.printf("[CLIENT] Phase23 cancelConnect ok=%d\n", (int)(bool)cancel);
      // The async connect often completes before the cancel lands (cancel then
      // reports failure). Settle and tear down any resulting link so it does not
      // leak into later phases — a lingering connection here previously blocked
      // phase 24's dedicated connect.
      delay(500);
      if (ac.isConnected()) {
        ac.disconnect();
        delay(500);
      }
    }
    Serial.printf("[CLIENT] StrConv pass=%d fail=%d\n", strPass, strFail);
    Serial.println("[CLIENT] Phase23 reAdvRace idle");
    waitForHostGo(phase23ReAdvGo, 25000);
    {
      BLEClient raceCl = BLE.createClient();
      BTStatus raceCs = raceCl.connect(targetAddr);
      Serial.printf("[CLIENT] Phase23 reAdvRace connected=%d\n", (int)(bool)raceCs);
      waitForHostGo(phase23ReAdvDisc, 15000);
      if (raceCl.isConnected()) {
        raceCl.disconnect();
        unsigned long discWait = millis() + 3000;
        while (raceCl.isConnected() && millis() < discWait) {
          delay(20);
        }
      }
      Serial.println("[CLIENT] Phase23 reAdvRace disconnected");
    }
    Serial.println("[CLIENT] Phase23 done");
  }

  // ===========================================================================
  // Phase 24: BLE5 advanced (coded PHY, default PHY)
  // ===========================================================================
  waitForPhase(24);
  {
#if BLE5_SUPPORTED
    BLEPhy tx = BLEPhy::PHY_1M, rx = BLEPhy::PHY_1M;
    BTStatus gd = BLE.getDefaultPhy(tx, rx);
    Serial.printf("[CLIENT] Phase24 getDefaultPhy ok=%d tx=%u rx=%u\n", (int)(bool)gd, (unsigned)tx, (unsigned)rx);
    BTStatus sd = BLE.setDefaultPhy(BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    Serial.printf("[CLIENT] Phase24 setDefaultPhy ok=%d\n", (int)(bool)sd);

    // Exercise coded-PHY path on the scanner. It's OK if the hardware
    // doesn't actually see anything; we only care about the API not
    // faulting.
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    BLEScan::ExtScanConfig coded;
    coded.phy = BLEPhy::PHY_Coded;
    coded.interval = 0x40;
    coded.window = 0x40;
    BTStatus es = scn.startExtended(1500, &coded, nullptr);
    Serial.printf("[CLIENT] Phase24 startExtendedCoded ok=%d\n", (int)(bool)es);
    delay(2000);
    scn.stopExtended();
    delay(200);

    // Establish and hold a GATT connection so the SERVER can deterministically
    // exercise its per-connection setPhy/getPhy/setDataLen every run, instead of
    // relying on a link left over from phase 23. Retry while the server settles
    // back into advertising after that phase's async-connect churn.
    BLEClient pc = BLE.createClient();
    installClientCallbacks(pc);
    BTStatus cs = BTStatus::Fail;
    for (int attempt = 0; attempt < 6; attempt++) {
      cs = pc.connect(targetAddr);
      if (cs) {
        break;
      }
      delay(500);
    }
    Serial.printf("[CLIENT] Phase24 phyConn ok=%d\n", (int)(bool)cs);
    // Hold the link long enough for the server's PHY/DLE ops to complete.
    delay(3000);
    if (pc.isConnected()) {
      pc.disconnect();
    }
    delay(300);
    Serial.println("[CLIENT] Phase24 done");
#else
    Serial.println("[CLIENT] Phase24 BLE5 not supported, skipping");
    Serial.println("[CLIENT] Phase24 done");
#endif
  }

  // ===========================================================================
  // Phase 25: HID-over-GATT spec compliance
  // ===========================================================================
  // Server rebuilds its stack with the HID service. We scan, connect, and
  // verify all HoGP/HIDS spec features:
  //   - HID Service (0x1812) discoverable
  //   - Battery Service (0x180F) discoverable
  //   - Report chars (0x2A4D): at least 2 (input + output)
  //   - Boot Keyboard Input (0x2A22): Read + Notify
  //   - Boot Keyboard Output (0x2A29): Read + Write + WriteNR
  //   - Report Map (0x2A4B): External Report Reference descriptor (0x2907)
  waitForPhase(25);
  {
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    scn.clearResults();
    scn.setActiveScan(true);
    scn.setFilterDuplicates(false);
    bool found = false;
    String hidName = String("HID_") + targetName;
    BTAddress hidAddr;
    unsigned long deadline = millis() + 15000;
    while (!found && millis() < deadline) {
      BLEScan::Results res = scn.startBlocking(3000);
      for (const auto &dev : res) {
        BLEAdvertisedDevice d = dev;
        if (d.getName() == hidName) {
          hidAddr = d.getAddress();
          found = true;
          break;
        }
      }
      scn.clearResults();
    }

    if (!found) {
      Serial.println("[CLIENT] Phase25 HID server not found");
    } else {
      Serial.println("[CLIENT] Phase25 HID found");
      BLEClient hc = BLE.createClient();
      installClientCallbacks(hc);
      BTStatus s = hc.connect(hidAddr, 10000);
      if (!s) {
        Serial.printf("[CLIENT] Phase25 HID connect FAILED: %s\n", s.toString());
      } else {
        auto svcs = hc.getServices();
        BLEUUID hidSvcUUID((uint16_t)0x1812);
        BLEUUID batSvcUUID((uint16_t)0x180F);
        bool haveHid = false;
        bool haveBat = false;
        for (auto &rs : svcs) {
          if (rs.getUUID() == hidSvcUUID) {
            haveHid = true;
          }
          if (rs.getUUID() == batSvcUUID) {
            haveBat = true;
          }
        }
        Serial.printf("[CLIENT] Phase25 services=%u haveHid=%d haveBat=%d\n", (unsigned)svcs.size(), (int)haveHid, (int)haveBat);

        BLERemoteService hidSvc = hc.getService(hidSvcUUID);
        if (hidSvc) {
          auto chars = hidSvc.getCharacteristics();
          Serial.printf("[CLIENT] Phase25 hidChars=%u\n", (unsigned)chars.size());

          BLEUUID reportUUID((uint16_t)0x2A4D);
          BLEUUID bootInUUID((uint16_t)0x2A22);
          BLEUUID bootOutUUID((uint16_t)0x2A32);
          BLEUUID reportMapUUID((uint16_t)0x2A4B);

          int reportCount = 0;
          bool bootInOk = false;
          bool bootOutOk = false;
          bool extRefFound = false;

          for (auto &ch : chars) {
            BLEUUID u = ch.getUUID();
            if (u == reportUUID) {
              reportCount++;
            }
            if (u == bootInUUID) {
              bootInOk = ch.canRead() && ch.canNotify();
              Serial.printf("[CLIENT] Phase25 bootIn canRead=%d canNotify=%d\n", (int)ch.canRead(), (int)ch.canNotify());
            }
            if (u == bootOutUUID) {
              bootOutOk = ch.canRead() && ch.canWrite() && ch.canWriteNoResponse();
              Serial.printf(
                "[CLIENT] Phase25 bootOut canRead=%d canWrite=%d canWriteNR=%d\n", (int)ch.canRead(), (int)ch.canWrite(), (int)ch.canWriteNoResponse()
              );
            }
            if (u == reportMapUUID) {
              BLEUUID extRefUUID((uint16_t)0x2907);
              BLERemoteDescriptor extRef = ch.getDescriptor(extRefUUID);
              extRefFound = (bool)extRef;
              Serial.printf("[CLIENT] Phase25 extRefDesc=%d\n", (int)extRefFound);
            }
          }

          Serial.printf("[CLIENT] Phase25 reportCount=%d\n", reportCount);
          Serial.printf("[CLIENT] Phase25 bootInOk=%d bootOutOk=%d\n", (int)bootInOk, (int)bootOutOk);
        }
        BLERemoteService dynRemoteSvc = hc.getService(tmpSvcUUID);
        String dynVal = dynRemoteSvc ? dynRemoteSvc.getValue(tmpCharUUID) : "";
        Serial.printf("[CLIENT] Phase25 dynSvcRead=%s\n", dynVal.c_str());
        hc.disconnect();
      }
    }
    Serial.println("[CLIENT] Phase25 done");
  }

  // ===========================================================================
  // Phase 26: Legacy advertising over the BLE5 extended engine
  // ===========================================================================
  // The server advertises a connectable legacy ADV_IND (name LEG_<name>) via
  // the public legacy API, which on an ext-adv controller is routed over the
  // extended engine (legacy-over-ext when BLE5). Confirm it is discoverable AND surfaces
  // as a *legacy*, connectable advertisement (the core regression this fixed).
  waitForPhase(26);
  {
#if BLE5_SUPPORTED
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    scn.clearResults();
    scn.setActiveScan(true);
    scn.setFilterDuplicates(false);
    String legName = String("LEG_") + targetName;
    bool sawLegacy = false;
    bool legacyFlag = false;
    bool connectableFlag = false;
    unsigned long deadline = millis() + 10000;
    while (!sawLegacy && millis() < deadline) {
      BLEScan::Results res = scn.startBlocking(2000);
      for (const auto &dev : res) {
        BLEAdvertisedDevice d = dev;
        if (d.getName() == legName) {
          sawLegacy = true;
          legacyFlag = d.isLegacyAdvertisement();
          connectableFlag = d.isConnectable();
          break;
        }
      }
      scn.clearResults();
    }
    Serial.printf("[CLIENT] Phase26 sawLegacy=%d legacy=%d connectable=%d\n", (int)sawLegacy, (int)legacyFlag, (int)connectableFlag);
#else
    Serial.println("[CLIENT] Phase26 BLE5 not supported, skipping");
#endif
    Serial.println("[CLIENT] Phase26 done");
  }

  // ===========================================================================
  // Phase 27: Legacy + extended advertising concurrently
  // ===========================================================================
  // The server broadcasts a connectable legacy set (LEG_<name>) on the reserved
  // instance AND a non-connectable extended set (EXT_<name>) on instance 0 at
  // the same time. A single scan must surface BOTH, with the legacy set flagged
  // legacy=1 and the extended set legacy=0 — proving the reserved-instance
  // concurrency (legacy-over-ext when BLE5).
  waitForPhase(27);
  {
#if BLE5_SUPPORTED
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    scn.clearResults();
    scn.setActiveScan(true);
    scn.setFilterDuplicates(false);
    String legName = String("LEG_") + targetName;
    String extName = String("EXT_") + targetName;
    bool sawLegacy = false, sawExt = false;
    bool legacyIsLegacy = false, extIsLegacy = true;
    unsigned long deadline = millis() + 12000;
    while ((!sawLegacy || !sawExt) && millis() < deadline) {
      BLEScan::Results res = scn.startBlocking(2000);
      for (const auto &dev : res) {
        BLEAdvertisedDevice d = dev;
        if (d.getName() == legName) {
          sawLegacy = true;
          legacyIsLegacy = d.isLegacyAdvertisement();
        } else if (d.getName() == extName) {
          sawExt = true;
          extIsLegacy = d.isLegacyAdvertisement();
        }
      }
      scn.clearResults();
    }
    Serial.printf(
      "[CLIENT] Phase27 sawLegacy=%d sawExt=%d legacyIsLegacy=%d extIsLegacy=%d\n", (int)sawLegacy, (int)sawExt, (int)legacyIsLegacy, (int)extIsLegacy
    );
#else
    Serial.println("[CLIENT] Phase27 BLE5 not supported, skipping");
#endif
    Serial.println("[CLIENT] Phase27 done");
  }

  // ===========================================================================
  // Phase 28: L2CAP CoC bulk across the credit-stall boundary + reconnect
  // ===========================================================================
  // The server rebuilt a fresh connectable stack with an L2CAP listener. Open a
  // channel, push a bulk buffer larger than the peer CoC MTU (write() splits it
  // into MTU-sized SDUs and waits for credits between chunks), verify the server
  // echoed the full byte count, then tear the channel down and open a fresh one
  // to prove the accept path recovers.
  waitForPhase(28);
  {
    bool l2cap_supported = false;
#if BLE_L2CAP_SUPPORTED
    l2cap_supported = true;
    // Server re-init in phase 28 may have rotated its address; rescan by name.
    delay(1500);
    bool found = scanForServer();
    Serial.printf("[CLIENT] Phase28 rescan found=%d\n", (int)found);
    if (found) {
      BLEClient bc = BLE.createClient();
      installClientCallbacks(bc);
      BTStatus cs = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        cs = bc.connect(targetAddr);
        if (cs) {
          break;
        }
        delay(500);
      }
      if (!cs) {
        Serial.printf("[CLIENT] Phase28 connect FAILED: %s\n", cs.toString());
      } else {
        BLERemoteService stallSvc = bc.getService(serviceUUID);
        BLERemoteCharacteristic stallChr = stallSvc ? stallSvc.getCharacteristic(rwCharUUID) : BLERemoteCharacteristic();
        static const uint8_t stallCommand[] = "STALL_L2CAP";
        BTStatus stallWrite = stallChr ? stallChr.writeValue(stallCommand, sizeof(stallCommand) - 1, false) : BTStatus::NotFound;
        Serial.printf("[CLIENT] Phase28 stall armed write=%d\n", (int)(bool)stallWrite);
        while (!phase28StallGo) {
          checkSerial();
          delay(10);
        }
        unsigned long timeoutStart = millis();
        BLEL2CAPChannel abandoned = BLE.connectL2CAP(bc.getHandle(), 0x0080, 256);
        unsigned long timeoutElapsed = millis() - timeoutStart;
        if (abandoned) {
          abandoned.disconnect();
        }
        Serial.printf("[CLIENT] Phase28 timeout returned empty=%d elapsed=%lu\n", (int)!abandoned, timeoutElapsed);
        while (!phase28CleanupGo) {
          checkSerial();
          delay(10);
        }
        bool abandonedClean = stallWrite && !abandoned && timeoutElapsed >= 4500 && timeoutElapsed <= 6500 && heap_caps_check_integrity_all(true);
        Serial.printf("[CLIENT] Phase28 abandoned clean=%d elapsed=%lu write=%d\n", (int)abandonedClean, timeoutElapsed, (int)(bool)stallWrite);

        BLEL2CAPChannel channel;
        for (int attempt = 0; attempt < 4 && !channel; attempt++) {
          channel = BLE.connectL2CAP(bc.getHandle(), 0x0080, 256);
          if (!channel) {
            delay(250);
          }
        }
        if (!channel || !channel.isConnected()) {
          Serial.println("[CLIENT] Phase28 L2CAP open FAILED");
        } else {
          volatile bool got = false;
          String rx;
          channel.onData([&](BLEL2CAPChannel ch, const uint8_t *data, size_t len) {
            syncPhaseFromHost();
            rx = String((const char *)data, len);
            got = true;
          });
          Serial.println("[CLIENT] Phase28 L2CAP channel connected");
          // Bulk payload exceeds the peer CoC MTU (256); write() segments it
          // and waits for COC_TX_UNSTALLED between chunks.
          static uint8_t bulk[2000];
          for (size_t i = 0; i < sizeof(bulk); i++) {
            bulk[i] = (uint8_t)('A' + (i % 26));
          }
          BTStatus ws = channel.write(bulk, sizeof(bulk));
          Serial.printf("[CLIENT] Phase28 bulk sent ok=%d bytes=%u\n", (int)(bool)ws, (unsigned)sizeof(bulk));
          unsigned long rstart = millis();
          while (!got && (millis() - rstart < 15000)) {
            delay(20);
          }
          unsigned long ackTotal = 0;
          bool bulkAck = false;
          if (got) {
            rx.trim();
            if (rx.startsWith("P28BULK:")) {
              ackTotal = (unsigned long)rx.substring(8).toInt();
              bulkAck = (ackTotal == sizeof(bulk));
            }
          }
          Serial.printf("[CLIENT] Phase28 bulkAck ok=%d total=%lu\n", (int)bulkAck, ackTotal);

          // Reconnect: drop this channel and open a fresh one, then ping/pong.
          // Keep the fresh handle for the peer-disconnect operation race and
          // the end()/begin() detached-shell check below.
          channel.disconnect();
          got = false;
          rx = "";
          bool reconnectOk = false;
          bool raceClean = false;
          bool lifecycleClean = false;
          BLEL2CAPChannel ch2 = BLE.connectL2CAP(bc.getHandle(), 0x0080, 256);
          if (ch2 && ch2.isConnected()) {
            ch2.onData([&](BLEL2CAPChannel ch, const uint8_t *data, size_t len) {
              syncPhaseFromHost();
              rx = String((const char *)data, len);
              got = true;
            });
            ch2.write((const uint8_t *)"PING", 4);
            unsigned long p = millis();
            while (!got && (millis() - p < 10000)) {
              delay(20);
            }
            rx.trim();
            reconnectOk = got && (rx == "P28PONG");

            memcpy(phase28RacePayload, "RACE", 4);
            for (size_t i = 4; i < sizeof(phase28RacePayload); i++) {
              phase28RacePayload[i] = (uint8_t)('a' + (i % 26));
            }
            static L2CAPRaceJob writeJob;
            static L2CAPRaceJob mtuJob;
            static L2CAPRaceJob disconnectJob;
            writeJob.channel = ch2;
            writeJob.data = phase28RacePayload;
            writeJob.len = sizeof(phase28RacePayload);
            mtuJob.channel = ch2;
            disconnectJob.channel = ch2;
            writeJob.status = BTStatus::Fail;
            mtuJob.mtu = 0;
            disconnectJob.status = BTStatus::Fail;
            writeJob.started = writeJob.done = false;
            mtuJob.started = mtuJob.done = false;
            disconnectJob.started = disconnectJob.done = false;

            bool writeCreated = xTaskCreate(l2capRaceWriteTask, "l2cap-write", 6144, &writeJob, 1, nullptr) == pdPASS;
            unsigned long startedAt = millis();
            while (writeCreated && !writeJob.started && millis() - startedAt < 1000) {
              delay(1);
            }
            bool mtuCreated = xTaskCreate(l2capRaceMtuTask, "l2cap-mtu", 4096, &mtuJob, 1, nullptr) == pdPASS;
            bool disconnectCreated = xTaskCreate(l2capRaceDisconnectTask, "l2cap-disc", 4096, &disconnectJob, 1, nullptr) == pdPASS;
            bool tasksCreated = writeCreated && mtuCreated && disconnectCreated;
            unsigned long raceAt = millis();
            while (((writeCreated && !writeJob.done) || (mtuCreated && !mtuJob.done) || (disconnectCreated && !disconnectJob.done)) && millis() - raceAt < 15000
            ) {
              delay(10);
            }
            raceClean = tasksCreated && writeJob.done && mtuJob.done && disconnectJob.done && l2capRaceStatusClean(writeJob.status)
                        && (mtuJob.mtu == 0 || mtuJob.mtu == 256) && l2capRaceStatusClean(disconnectJob.status) && heap_caps_check_integrity_all(true);
            Serial.printf(
              "[CLIENT] Phase28 race clean=%d write=%d mtu=%u disconnect=%d\n", (int)raceClean, (int)writeJob.status, (unsigned)mtuJob.mtu,
              (int)disconnectJob.status
            );

            // ESP-IDF's host event pool has only 19 host-side entries before
            // controller additions. Retain 64 sequentially used channel shells
            // so a leaked per-shell event deterministically exhausts the pool.
            static std::vector<BLEL2CAPChannel> churnShells;
            churnShells.clear();
            churnShells.reserve(64);
            bool churnClean = true;
            for (int i = 0; i < 64; i++) {
              BLEL2CAPChannel churn = BLE.connectL2CAP(bc.getHandle(), 0x0080, 256);
              uint16_t churnMtu = churn ? churn.getMTU() : 0;
              BTStatus churnDisconnect = churn ? churn.disconnect() : BTStatus::InvalidState;
              churnClean = churnClean && churn && churnMtu == 256 && churnDisconnect == BTStatus::OK;
              churnShells.push_back(churn);
            }
            churnClean = churnClean && heap_caps_check_integrity_all(true);
            Serial.printf("[CLIENT] Phase28 event churn clean=%d count=%u\n", (int)churnClean, (unsigned)churnShells.size());

            BLEL2CAPChannel shutdownChannel = BLE.connectL2CAP(bc.getHandle(), 0x0080, 256);
            memcpy(phase28ShutdownPayload, "ENDR", 4);
            for (size_t i = 4; i < sizeof(phase28ShutdownPayload); i++) {
              phase28ShutdownPayload[i] = (uint8_t)('A' + (i % 26));
            }
            static L2CAPRaceJob shutdownJob;
            shutdownJob.channel = shutdownChannel;
            shutdownJob.data = phase28ShutdownPayload;
            shutdownJob.len = sizeof(phase28ShutdownPayload);
            shutdownJob.status = BTStatus::Fail;
            shutdownJob.started = shutdownJob.done = false;
            bool shutdownCreated = shutdownChannel && xTaskCreate(l2capRaceWriteTask, "l2cap-end", 6144, &shutdownJob, 1, nullptr) == pdPASS;
            unsigned long shutdownStart = millis();
            while (shutdownCreated && !shutdownJob.started && millis() - shutdownStart < 1000) {
              delay(1);
            }
            unsigned long endGoWait = millis();
            while (!phase28EndGo && millis() - endGoWait < 15000) {
              checkSerial();
              delay(10);
            }
            bool stalledAtEnd = shutdownCreated && phase28EndGo && shutdownJob.started && !shutdownJob.done;
            BLEL2CAPChannel retained = shutdownChannel;
            BLE.end(false);
            unsigned long waiterWait = millis();
            while (shutdownCreated && !shutdownJob.done && millis() - waiterWait < 5000) {
              delay(10);
            }
            bool waiterReleased = stalledAtEnd && shutdownJob.done && shutdownJob.status == BTStatus::NotConnected;
            BTStatus endedWrite = retained.write((const uint8_t *)"x", 1);
            uint16_t endedMtu = retained.getMTU();
            BTStatus endedDisconnect = retained.disconnect();
            bool endedClean = waiterReleased && endedWrite == BTStatus::NotConnected && endedMtu == 0 && endedDisconnect == BTStatus::NotConnected
                              && heap_caps_check_integrity_all(true);
            Serial.printf("[CLIENT] Phase28 end in-flight=%d released=%d status=%d\n", (int)stalledAtEnd, (int)waiterReleased, (int)shutdownJob.status);

            BTStatus rebegin = BLE.begin(targetName + "_client_phase28");
            BTStatus reboundWrite = retained.write((const uint8_t *)"x", 1);
            uint16_t reboundMtu = retained.getMTU();
            BTStatus reboundDisconnect = retained.disconnect();
            bool finalDropClean = true;
            for (auto &shell : churnShells) {
              finalDropClean = finalDropClean && shell && shell.getMTU() == 0;
              shell = BLEL2CAPChannel();
            }
            churnShells.clear();
            delay(50);
            finalDropClean = finalDropClean && heap_caps_check_integrity_all(true);
            Serial.printf("[CLIENT] Phase28 final handle drop clean=%d\n", (int)finalDropClean);
            lifecycleClean = churnClean && rebegin && reboundWrite == BTStatus::NotConnected && reboundMtu == 0 && reboundDisconnect == BTStatus::NotConnected
                             && finalDropClean && heap_caps_check_integrity_all(true);
            Serial.printf(
              "[CLIENT] Phase28 retained clean=%d end=%d/%u/%d rebegin=%d after=%d/%u/%d\n", (int)(endedClean && lifecycleClean), (int)endedWrite,
              (unsigned)endedMtu, (int)endedDisconnect, (int)(bool)rebegin, (int)reboundWrite, (unsigned)reboundMtu, (int)reboundDisconnect
            );
          }
          Serial.printf("[CLIENT] Phase28 reconnect ok=%d\n", (int)reconnectOk);
          Serial.printf("[CLIENT] Phase28 operation race ok=%d lifecycle=%d\n", (int)raceClean, (int)lifecycleClean);
        }
      }
    }
#endif
    if (!l2cap_supported) {
      Serial.println("[CLIENT] Phase28 L2CAP not supported, skipping");
    }
    Serial.println("[CLIENT] Phase28 done");
  }

  // ===========================================================================
  // Phase 29: Connection-parameter update request denial (NimBLE-only)
  // ===========================================================================
  // Arm the pre-accept hook to REJECT the peripheral's conn-param update, then
  // let the server request one. On NimBLE the hook fires and returns false, so
  // the live interval must stay put; on Bluedroid the hook never fires (no
  // pre-accept surface) and the phase self-skips.
  waitForPhase(29);
  {
    clientRejectConnParams = true;
    clientConnParamsReqFired = false;
    // Give the server time to rebuild its stack and re-advertise serverName.
    delay(1500);
    bool found = scanForServer();
#if defined(CONFIG_NIMBLE_ENABLED)
    Serial.printf("[CLIENT] Phase29 rescan found=%d nimble=1\n", (int)found);
#else
    Serial.printf("[CLIENT] Phase29 rescan found=%d nimble=0\n", (int)found);
#endif
    if (found) {
      BLEClient pc = BLE.createClient();
      installClientCallbacks(pc);
      BTStatus s = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        s = pc.connect(targetAddr);
        if (s) {
          break;
        }
        delay(500);
      }
      if (!s) {
        Serial.printf("[CLIENT] Phase29 connect FAILED: %s\n", s.toString());
      } else {
        delay(500);
        uint16_t beforeInterval = pc.getConnInfo().getInterval();
        // The server issues the update request ~1 s in; wait for it to land and
        // for our rejection (or Bluedroid's silent accept) to settle.
        delay(5000);
        uint16_t afterInterval = pc.getConnInfo().getInterval();
        bool fired = clientConnParamsReqFired;
        bool held = (afterInterval == beforeInterval);
        Serial.printf(
          "[CLIENT] Phase29 hookFired=%d beforeInterval=%u afterInterval=%u held=%d\n", (int)fired, (unsigned)beforeInterval, (unsigned)afterInterval, (int)held
        );
        pc.disconnect();
        delay(500);
      }
    }
    clientRejectConnParams = false;
    Serial.println("[CLIENT] Phase29 done");
  }

  // ===========================================================================
  // Phase 30: deleteBond round-trip (delete -> verify gone -> re-pair)
  // ===========================================================================
  // The server rebuilt the full secured stack. Delete the client bond, confirm
  // the store shrank, then reconnect and read the authenticated characteristic
  // to force a fresh pairing — proving deleteBond fully round-trips and a new
  // bond is re-established on both stacks.
  waitForPhase(30);
  {
    BLESecurity sec = BLE.getSecurity();
    auto bondsBefore = sec.getBondedDevices();
    Serial.printf("[CLIENT] Phase30 bondsBefore=%u\n", (unsigned)bondsBefore.size());
    // Delete while a finite scan is active. NimBLE preempts discovery to update
    // its resolving list; the resumed scan must honor the original deadline
    // rather than starting another full-duration interval.
    BLEScan deadlineScan = BLE.getScan();
    deadlineScan.stop();
    deadlineScan.resetCallbacks();
    deadlineScan.clearResults();
    std::atomic<bool> deadlineScanComplete{false};
    deadlineScan.onComplete([&](BLEScan::Results) {
      deadlineScanComplete = true;
    });
    const uint32_t deadlineDurationMs = 4000;
    unsigned long deadlineStart = millis();
    BTStatus deadlineStartStatus = deadlineScan.start(deadlineDurationMs, false);
    delay(2500);
    // Exercise the singular deleteBond() API on the one stored bond.
    BTStatus delStatus = bondsBefore.empty() ? BTStatus::NotFound : sec.deleteBond(bondsBefore[0]);
    while (!deadlineScanComplete.load() && millis() - deadlineStart < 8000) {
      delay(10);
    }
    unsigned long deadlineElapsed = millis() - deadlineStart;
    if (!deadlineScanComplete.load()) {
      deadlineScan.stop();
    }
    deadlineScan.resetCallbacks();
    Serial.printf(
      "[CLIENT] Phase30 preemptScan start=%d complete=%d elapsed=%lu\n", (int)(bool)deadlineStartStatus, (int)deadlineScanComplete.load(), deadlineElapsed
    );

    // Supersede one active native session with another. A late completion from
    // the canceled session must not complete or signal the replacement.
    std::atomic<uint32_t> replacementCompletions{0};
    deadlineScan.onComplete([&](BLEScan::Results) {
      replacementCompletions.fetch_add(1);
    });
    BTStatus supersededStart = deadlineScan.start(3000, false);
    delay(100);
    unsigned long replacementStartMs = millis();
    BTStatus replacementStartStatus = deadlineScan.start(800, false);
    while (replacementCompletions.load() == 0 && millis() - replacementStartMs < 2500) {
      delay(10);
    }
    unsigned long replacementElapsed = millis() - replacementStartMs;
    delay(100);
    bool replacementStopped = !deadlineScan.isScanning();
    deadlineScan.stop();
    deadlineScan.resetCallbacks();
    Serial.printf(
      "[CLIENT] Phase30 sessionReplace first=%d replacement=%d completions=%lu elapsed=%lu stopped=%d\n", (int)(bool)supersededStart,
      (int)(bool)replacementStartStatus, (unsigned long)replacementCompletions.load(), replacementElapsed, (int)replacementStopped
    );
#if BLE_NIMBLE && BLE_VALIDATION_HOOKS
    BLEHooks.scan.heldContexts.store(0, std::memory_order_relaxed);
    BTStatus preemptOwnedStart = deadlineScan.start(150, false);
    BLEHooks.scan.forcePreemptedStop.store(true, std::memory_order_relaxed);
    BTStatus preemptOwnedStop = deadlineScan.stop();
    bool contextRetained = BLEHooks.scan.heldContexts.load(std::memory_order_relaxed) == 1;
    unsigned long contextDeadline = millis() + 1500;
    while (BLEHooks.scan.heldContexts.load(std::memory_order_relaxed) != 0 && millis() < contextDeadline) {
      delay(1);
    }
    bool preemptStopClean = preemptOwnedStart && preemptOwnedStop && contextRetained && BLEHooks.scan.heldContexts.load(std::memory_order_relaxed) == 0
                            && heap_caps_check_integrity_all(true);
    Serial.printf("[CLIENT] Phase30 preemptStop clean=%d retained=%d\n", (int)preemptStopClean, (int)contextRetained);
#else
    Serial.println("[CLIENT] Phase30 preemptStop clean=1 retained=0");
#endif
    size_t scanHeapBefore = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    bool scanChurnOk = true;
    for (int i = 0; i < 24; i++) {
      scanChurnOk = scanChurnOk && (bool)deadlineScan.start(0, false);
      scanChurnOk = scanChurnOk && (bool)deadlineScan.stop();
    }
#if BLE_NIMBLE && BLE_VALIDATION_HOOKS
    unsigned long retireDeadline = millis() + 2000;
    while (BLEHooks.scan.retiredContexts.load(std::memory_order_relaxed) != 0 && millis() < retireDeadline) {
      delay(1);
    }
    scanChurnOk = scanChurnOk && BLEHooks.scan.retiredContexts.load(std::memory_order_relaxed) == 0;
#else
    delay(50);
#endif
    size_t scanHeapAfter = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int32_t scanHeapDelta = static_cast<int32_t>(scanHeapBefore) - static_cast<int32_t>(scanHeapAfter);
    scanChurnOk = scanChurnOk && scanHeapDelta <= 64 && heap_caps_check_integrity_all(true);
    Serial.printf("[CLIENT] Phase30 scanChurn clean=%d heapDelta=%ld\n", (int)scanChurnOk, (long)scanHeapDelta);
#if defined(CONFIG_NIMBLE_ENABLED)
    std::atomic<bool> hostChurnEntered{false};
    std::atomic<bool> hostChurnDone{false};
    std::atomic<bool> hostChurnClean{true};
    deadlineScan.onResult([&](BLEAdvertisedDevice) {
      if (hostChurnEntered.exchange(true)) {
        return;
      }
      bool clean = true;
      for (int i = 0; i < 48; i++) {
        clean = clean && (bool)deadlineScan.stop();
        clean = clean && (bool)deadlineScan.start(0, false);
      }
      clean = clean && (bool)deadlineScan.stop() && heap_caps_check_integrity_all(true);
      hostChurnClean = clean;
      hostChurnDone = true;
    });
    BTStatus hostChurnStart = deadlineScan.start(0, false);
    unsigned long hostChurnDeadline = millis() + 8000;
    while (!hostChurnDone.load() && millis() < hostChurnDeadline) {
      delay(10);
    }
    deadlineScan.stop();
    deadlineScan.resetCallbacks();
    bool hostChurnOk = hostChurnStart && hostChurnDone.load() && hostChurnClean.load();
    Serial.printf("[CLIENT] Phase30 hostScanChurn clean=%d cycles=%d\n", (int)hostChurnOk, hostChurnDone.load() ? 48 : 0);
#else
    Serial.println("[CLIENT] Phase30 hostScanChurn clean=1 cycles=0");
#endif
    Serial.printf("[CLIENT] Phase30 deleteOk=%d\n", (int)(bool)delStatus);
    auto bondsAfterDelete = sec.getBondedDevices();
    Serial.printf("[CLIENT] Phase30 bondsAfterDelete=%u\n", (unsigned)bondsAfterDelete.size());

    delay(1500);
    bool found = scanForServer();
    Serial.printf("[CLIENT] Phase30 rescan found=%d\n", (int)found);
    if (!found) {
      // Diagnostic: dump whatever the radio actually heard so a silent
      // advertiser and a name mismatch are distinguishable in the log.
      BLEScan dumpScan = BLE.getScan();
      BLEScan::Results dump = dumpScan.startBlocking(4000);
      Serial.printf("[CLIENT] Phase30 scanDump count=%u\n", (unsigned)dump.size());
      for (const auto &d : dump) {
        Serial.printf("[CLIENT] Phase30 scanDump addr=%s name=%s\n", d.getAddress().toString().c_str(), d.getName().c_str());
      }
    }
    if (found) {
      BLEClient rc = BLE.createClient();
      installClientCallbacks(rc);
      BTStatus cs = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        cs = rc.connect(targetAddr);
        if (cs) {
          break;
        }
        delay(500);
      }
      if (!cs) {
        Serial.printf("[CLIENT] Phase30 reconnect FAILED: %s\n", cs.toString());
      } else {
        BLERemoteService svc30 = rc.getService(serviceUUID);
        if (!svc30) {
          Serial.println("[CLIENT] Phase30 service not found");
        } else {
          BLERemoteCharacteristic sc = svc30.getCharacteristic(secureCharUUID);
          if (!sc) {
            Serial.println("[CLIENT] Phase30 secure char not found");
          } else {
            // Reading a ReadAuthenticated characteristic forces a fresh pairing
            // now that the bond was deleted on both ends.
            String v = sc.readValue();
            Serial.printf("[CLIENT] Phase30 secureRead=%s\n", v.c_str());
          }
        }
        delay(1500);
        BLEConnInfo ci = rc.getConnInfo();
        auto bondsAfterRepair = sec.getBondedDevices();
        Serial.printf(
          "[CLIENT] Phase30 reEnc=%d reBond=%d bondsAfterRepair=%u\n", (int)ci.isEncrypted(), (int)ci.isBonded(), (unsigned)bondsAfterRepair.size()
        );
        rc.disconnect();
        delay(500);
      }
    }
    Serial.println("[CLIENT] Phase30 done");
  }

  // ===========================================================================
  // Phase 31: Numeric-Comparison reject
  // ===========================================================================
  // Arm a reject in the numeric-comparison confirm handler, then reconnect and
  // read the authenticated characteristic to force a fresh pairing. The reject
  // must abort pairing: the link stays unencrypted/unbonded, the secure read
  // fails, and no bond is stored. Runs on both stacks (NC is the default method
  // when both ends are DisplayYesNo).
  waitForPhase(31);
  {
    clientRejectPairing = true;
    clientAuthCompleteFired = false;
    clientLastAuthSuccess = false;
    clientIdentityCount = 0;
    BLESecurity sec = BLE.getSecurity();
    (void)sec.deleteAllBonds();
    delay(1500);
    bool found = scanForServer();
    Serial.printf("[CLIENT] Phase31 rescan found=%d\n", (int)found);
    if (found) {
      BLEClient rc = BLE.createClient();
      installClientCallbacks(rc);
      BTStatus cs = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        cs = rc.connect(targetAddr);
        if (cs) {
          break;
        }
        delay(500);
      }
      if (!cs) {
        Serial.printf("[CLIENT] Phase31 connect FAILED: %s\n", cs.toString());
      } else {
        bool readOk = false;
        BLERemoteService svc = rc.getService(serviceUUID);
        if (svc) {
          BLERemoteCharacteristic sc = svc.getCharacteristic(secureCharUUID);
          if (sc) {
            // Reading the ReadAuthenticated characteristic starts pairing, which
            // we reject; the read must therefore fail (empty result).
            String v = sc.readValue();
            readOk = (v.length() > 0);
          }
        }
        delay(2000);
        BLEConnInfo ci = rc.getConnInfo();
        auto bonds = sec.getBondedDevices();
        Serial.printf(
          "[CLIENT] Phase31 ncReject readOk=%d enc=%d bond=%d bonds=%u authFired=%d authSuccess=%d identityCount=%d\n", (int)readOk, (int)ci.isEncrypted(),
          (int)ci.isBonded(), (unsigned)bonds.size(), (int)clientAuthCompleteFired, (int)clientLastAuthSuccess, (int)clientIdentityCount
        );
        rc.disconnect();
        delay(500);
      }
    }
    clientRejectPairing = false;
    Serial.println("[CLIENT] Phase31 done");
  }

  // ===========================================================================
  // Phase 32: Passkey-entry pairing (central KeyboardOnly, peripheral DisplayOnly)
  // ===========================================================================
  // Switch the central to KeyboardOnly so the SMP method negotiates Passkey
  // Entry against the DisplayOnly peripheral. Supply the fixed passkey from the
  // onPassKeyRequest handler; the authenticated read must succeed and the link
  // must end up encrypted + bonded.
  waitForPhase(32);
  {
    clientRejectPairing = false;
    clientAuthCompleteFired = false;
    clientLastAuthSuccess = false;
    BLESecurity sec = BLE.getSecurity();
    (void)sec.deleteAllBonds();
    sec.setIOCapability(BLEIOCapability::KeyboardOnly);
    delay(1500);
    bool found = scanForServer();
    Serial.printf("[CLIENT] Phase32 rescan found=%d\n", (int)found);
    if (found) {
      BLEClient pc = BLE.createClient();
      installClientCallbacks(pc);
      BTStatus cs = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        cs = pc.connect(targetAddr);
        if (cs) {
          break;
        }
        delay(500);
      }
      if (!cs) {
        Serial.printf("[CLIENT] Phase32 connect FAILED: %s\n", cs.toString());
      } else {
        BLERemoteService svc = pc.getService(serviceUUID);
        if (!svc) {
          Serial.println("[CLIENT] Phase32 service not found");
        } else {
          BLERemoteCharacteristic sc = svc.getCharacteristic(secureCharUUID);
          if (!sc) {
            Serial.println("[CLIENT] Phase32 secure char not found");
          } else {
            String v = sc.readValue();
            Serial.printf("[CLIENT] Phase32 secureRead=%s\n", v.c_str());
          }
        }
        delay(2000);
        BLEConnInfo ci = pc.getConnInfo();
        auto bonds = sec.getBondedDevices();
        Serial.printf(
          "[CLIENT] Phase32 passkeyEntry enc=%d bond=%d bonds=%u authFired=%d authSuccess=%d\n", (int)ci.isEncrypted(), (int)ci.isBonded(),
          (unsigned)bonds.size(), (int)clientAuthCompleteFired, (int)clientLastAuthSuccess
        );
        pc.disconnect();
        delay(500);
      }
    }
    // Restore the numeric-comparison default for any later work.
    sec.setIOCapability(BLEIOCapability::DisplayYesNo);
    Serial.println("[CLIENT] Phase32 done");
  }

  // ===========================================================================
  // Phase 33: Indications right behind one the stack sent itself
  // ===========================================================================
  // Subscribe to the server's Phase 33 characteristic and stay connected while
  // it deletes a service (Bluedroid sends Service Changed) and indicates.
  // The Service Changed handling may drop this subscription, so only the
  // server's confirmation counts are asserted.
  waitForPhase(33);
  {
    (void)BLE.getSecurity().deleteAllBonds();
    // The server rebuilds its database before it advertises again.
    delay(2000);
    bool found = scanForServer();
    bool subscribed = false;
    if (found) {
      BLEClient pc = BLE.createClient();
      installClientCallbacks(pc);
      BTStatus cs = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        cs = pc.connect(targetAddr);
        if (cs) {
          break;
        }
        delay(500);
      }
      if (!cs) {
        Serial.printf("[CLIENT] Phase33 connect FAILED: %s\n", cs.toString());
      } else {
        std::atomic<int> svcChangedRx{0};
        std::atomic<bool> svcChangedPayload4{false};
        std::atomic<bool> svcChangedFullRange{false};
        std::atomic<int> busyIndCount{0};
        String busyIndLog[3];
        BTStatus svcSubStatus = BTStatus::NotFound;
        BLERemoteService gattSvc = pc.getService(BLEUUID((uint16_t)0x1801));
        if (gattSvc) {
          BLERemoteCharacteristic svcChanged = gattSvc.getCharacteristic(BLEUUID((uint16_t)0x2A05));
          if (svcChanged) {
            svcSubStatus = svcChanged.subscribe(
              BLESubscription::Indicate,
              [&svcChangedRx, &svcChangedPayload4, &svcChangedFullRange](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLESubscription) {
                svcChangedRx.fetch_add(1, std::memory_order_relaxed);
                svcChangedPayload4 = len == 4;
                svcChangedFullRange = len == 4 && data[0] == 0x01 && data[1] == 0x00 && data[2] == 0xFF && data[3] == 0xFF;
              }
            );
          }
        }
        BLERemoteService svc = pc.getService(busySvcUUID);
        BLERemoteCharacteristic bc = svc ? svc.getCharacteristic(busyCharUUID) : BLERemoteCharacteristic();
        BTStatus sub = bc ? bc.subscribe(
                              BLESubscription::Indicate,
                              [&busyIndCount, &busyIndLog](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLESubscription) {
                                int n = busyIndCount.load(std::memory_order_relaxed);
                                if (n < 3) {
                                  busyIndLog[n] = String((const char *)data, len);
                                  busyIndCount.store(n + 1, std::memory_order_release);
                                }
                              }
                            )
                          : BTStatus::NotFound;
        subscribed = (bool)sub;
        if (!sub) {
          Serial.printf("[CLIENT] Phase33 subscribe FAILED: %s\n", sub.toString());
        }
        unsigned long waitStart = millis();
        while (svcChangedRx.load(std::memory_order_relaxed) == 0 && millis() - waitStart < 10000) {
          delay(20);
        }
        BTStatus staleStatus = bc ? bc.subscribe(BLESubscription::Indicate, nullptr) : BTStatus::InvalidState;
        bool staleRejected = staleStatus == BTStatus::InvalidState;
        BTStatus rediscovered = pc.discoverServices();
        svc = rediscovered ? pc.getService(busySvcUUID) : BLERemoteService();
        bc = svc ? svc.getCharacteristic(busyCharUUID) : BLERemoteCharacteristic();
        BTStatus resub = bc ? bc.subscribe(
                                BLESubscription::Indicate,
                                [&busyIndCount, &busyIndLog](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLESubscription) {
                                  int n = busyIndCount.load(std::memory_order_relaxed);
                                  if (n < 3) {
                                    busyIndLog[n] = String((const char *)data, len);
                                    busyIndCount.store(n + 1, std::memory_order_release);
                                  }
                                }
                              )
                            : BTStatus::NotFound;
        subscribed = (bool)resub;
        Serial.printf(
          "[CLIENT] Phase33 stack=%s staleRejected=%d rediscovered=%d resubscribed=%d payload4=%d fullRange=%d\n", BLE.getStackName(), (int)staleRejected,
          (int)(bool)rediscovered, (int)(bool)resub, (int)svcChangedPayload4.load(), (int)svcChangedFullRange.load()
        );
        waitStart = millis();
        while (busyIndCount.load(std::memory_order_acquire) < 3 && millis() - waitStart < 8000) {
          delay(50);
        }
        String received;
        int n = busyIndCount.load(std::memory_order_acquire);
        for (int i = 0; i < n; i++) {
          received += (i > 0) ? "," : "";
          received += busyIndLog[i];
        }
        pc.disconnect();
        delay(500);
        Serial.printf(
          "[CLIENT] Phase33 svcChangedRx=%d svcSubOk=%d busyPayloads=%s\n", svcChangedRx.load(std::memory_order_relaxed), (int)(bool)svcSubStatus,
          received.c_str()
        );
      }
    }
    Serial.printf("[CLIENT] Phase33 found=%d subscribed=%d\n", (int)found, (int)subscribed);
    Serial.println("[CLIENT] Phase33 done");
  }

  // ===========================================================================
  // Phase 34: Periodic advertising lifecycle — sync, terminate, sync-lost (BLE5)
  // ===========================================================================
  // Re-sync to the server's periodic train (as in phase 2), then exercise the
  // teardown transitions the happy path never reached: an explicit
  // terminatePeriodicSync and an onPeriodicLost after the server stops the
  // train. Restores a normal stack afterward so memory_release still measures a
  // meaningful free. Self-skips without BLE5.
  waitForPhase(34);
  {
    bool ble5_ok = false;
#if BLE5_SUPPORTED
    BLE.end(false);
    delay(500);
    BTStatus st = BLE.begin("BLE_CLT_P34");
    if (st) {
      volatile bool found = false;
      volatile bool synced = false;
      volatile bool dataRx = false;
      volatile bool lost = false;
      volatile uint16_t syncHandle = 0;
      BLEScan scan = BLE.getScan();
      scan.onResult([&](BLEAdvertisedDevice dev) {
        syncPhaseFromHost();
        if (!found && dev.getName() == targetName) {
          found = true;
          targetAddr = dev.getAddress();
          scan.createPeriodicSync(targetAddr, 1);
        }
      });
      scan.onPeriodicSync([&](uint16_t handle, uint8_t sid, const BTAddress &addr, BLEPhy phy, uint16_t interval) {
        syncPhaseFromHost();
        synced = true;
        syncHandle = handle;
      });
      scan.onPeriodicReport([&](uint16_t h, int8_t rssi, int8_t txPower, const uint8_t *data, size_t len) {
        syncPhaseFromHost();
        dataRx = true;
      });
      scan.onPeriodicLost([&](uint16_t h) {
        syncPhaseFromHost();
        lost = true;
      });
      scan.startExtended(26000);
      // Wait for sync + first report.
      for (int i = 0; i < 150 && !(synced && dataRx); i++) {
        delay(100);
      }
      Serial.printf("[CLIENT] Phase34 synced=%d dataRx=%d\n", (int)synced, (int)dataRx);
      ble5_ok = synced && dataRx;

      // Observe supervision-timeout sync-lost once the server stops the train.
      for (int i = 0; i < 120 && !lost; i++) {
        delay(100);
      }
      // Explicit terminate transition (returns fail if the sync was already
      // lost above; both outcomes are reported for the teardown-coverage gate).
      BTStatus tt = BTStatus::Fail;
      if (synced) {
        tt = scan.terminatePeriodicSync(syncHandle);
      }
      Serial.printf("[CLIENT] Phase34 lost=%d terminateOk=%d\n", (int)lost, (int)(bool)tt);
      scan.stop();

      // Restore a normal stack so memory_release measures a meaningful free.
      BLE.end(false);
      delay(500);
      (void)BLE.begin("BLE_CLT");
    }
#endif
    if (!ble5_ok) {
      Serial.println("[CLIENT] Phase34 BLE5 not supported, skipping");
    }
    Serial.println("[CLIENT] Phase34 done");
  }

  // ===========================================================================
  // Phase 35: CCCD bit routing, in-flight removeService, read-timeout recover,
  //           notify flood
  // ===========================================================================
  waitForPhase(35);
  {
    delay(2000);
    bool found = scanForServer();
    std::atomic<int> peerNotify{0};
    std::atomic<int> peerIndicate{0};
    std::atomic<int> floodCount{0};
    std::atomic<int> queueIndicate{0};
    std::atomic<bool> callbackCommandAttempted{false};
    std::atomic<int> callbackCommandAccepted{0};
    std::atomic<int> callbackCommandBusy{0};
    std::atomic<int> callbackCommandOther{0};
    bool phase35Bluedroid = strcmp(BLE.getStackName(), "Bluedroid") == 0;
    BLERemoteCharacteristic::NotifyCallback onPeer = [&peerNotify, &peerIndicate, &floodCount, &callbackCommandAttempted, &callbackCommandAccepted,
                                                      &callbackCommandBusy, &callbackCommandOther,
                                                      phase35Bluedroid](BLERemoteCharacteristic chr, const uint8_t *, size_t, BLESubscription kind) {
      if (kind == BLESubscription::Notify) {
        bool expected = false;
        if (callbackCommandAttempted.compare_exchange_strong(expected, true)) {
          const int attempts = phase35Bluedroid ? 10 : 9;
          for (int i = 0; i < attempts; i++) {
            BTStatus status = chr.writeValue("callback-command", false);
            if (status) {
              callbackCommandAccepted.fetch_add(1, std::memory_order_relaxed);
            } else if (status == BTStatus::Busy) {
              callbackCommandBusy.fetch_add(1, std::memory_order_relaxed);
            } else {
              callbackCommandOther.fetch_add(1, std::memory_order_relaxed);
            }
          }
          if (!phase35Bluedroid) {
            BLEHooks.remote.holdWriteResponseRetirement.store(false, std::memory_order_relaxed);
          }
        }
        if (peerNotify.fetch_add(1, std::memory_order_relaxed) > 0) {
          floodCount.fetch_add(1, std::memory_order_relaxed);
        }
      } else {
        peerIndicate.fetch_add(1, std::memory_order_relaxed);
      }
    };
    bool connected = false;
    BLEClient c1;
    BLERemoteCharacteristic peer;
    BLERemoteCharacteristic queue;
    if (found) {
      c1 = BLE.createClient();
      installClientCallbacks(c1);
      BTStatus cs = BTStatus::Fail;
      for (int attempt = 0; attempt < 6; attempt++) {
        cs = c1.connect(targetAddr);
        if (cs) {
          break;
        }
        delay(500);
      }
      if (cs) {
        BLERemoteService edge = c1.getService(edgeSvcUUID);
        peer = edge ? edge.getCharacteristic(peerCharUUID) : BLERemoteCharacteristic();
        BLERemoteService qsvc = c1.getService(queueSvcUUID);
        queue = qsvc ? qsvc.getCharacteristic(queueCharUUID) : BLERemoteCharacteristic();
        // Arm the passive indication channel first. The peer notification
        // callback deliberately submits Write Commands from the stack task; it
        // must not race this unrelated blocking CCCD write.
        if (queue) {
          (void)queue.subscribe(BLESubscription::Indicate, [&queueIndicate](BLERemoteCharacteristic, const uint8_t *, size_t, BLESubscription) {
            queueIndicate.fetch_add(1, std::memory_order_relaxed);
          });
        }
        BLEHooks.remote.writeCommandOverlappedWaiter.store(false, std::memory_order_relaxed);
        BLEHooks.remote.holdWriteResponseRetirement.store(false, std::memory_order_relaxed);
        BLEHooks.remote.writeCommandSubmitFailures.store(phase35Bluedroid ? 1 : 0, std::memory_order_relaxed);
        BLEHooks.remote.writeCommandCompletionFailures.store(phase35Bluedroid ? 1 : 0, std::memory_order_relaxed);
        BLEHooks.remote.writeResponseStatusPreserved.store(!phase35Bluedroid, std::memory_order_relaxed);
        BLEHooks.remote.combinedWriteOwnerObserved.store(!phase35Bluedroid, std::memory_order_relaxed);
        BLEHooks.remote.combinedWriteOwnerHeld.store(true, std::memory_order_relaxed);
        connected = peer && (bool)peer.subscribe(BLESubscription::Notify, onPeer);
        bool failOk = false;
#if BLE_VALIDATION_HOOKS
        if (connected) {
          (void)peer.unsubscribe();
          BLEHooks.remote.injectCccdWriteFail.store(true, std::memory_order_relaxed);
          failOk = peer.subscribe(BLESubscription::Notify, onPeer) != BTStatus::OK;
        }
        Serial.printf("[CLIENT] Phase35 rollbackReady fail=%d\n", (int)failOk);
#else
        failOk = true;
        Serial.printf("[CLIENT] Phase35 rollbackReady fail=1\n");
#endif
        BLEHooks.remote.holdWriteResponseRetirement.store(phase35Bluedroid, std::memory_order_relaxed);
        BTStatus resub = BTStatus::Fail;
        if (peer) {
          resub = peer.subscribe(BLESubscription::Notify, onPeer);
          connected = (bool)resub;
        }
        Serial.printf("[CLIENT] Phase35 subscribeRollback fail=%d resub=%d\n", (int)failOk, (int)connected);
        BLEHooks.remote.holdWriteResponseRetirement.store(false, std::memory_order_relaxed);
      }
    }
    unsigned long start = millis();
    while (peerNotify.load() == 0 && millis() - start < 15000) {
      delay(50);
    }
    bool bluedroidCommands = phase35Bluedroid;
    bool callbackOverlap = !bluedroidCommands || BLEHooks.remote.writeCommandOverlappedWaiter.load(std::memory_order_relaxed);
    bool callbackStateClean = BLEHooks.remote.writeResponseStatusPreserved.load(std::memory_order_relaxed)
                              && BLEHooks.remote.combinedWriteOwnerObserved.load(std::memory_order_relaxed)
                              && BLEHooks.remote.combinedWriteOwnerHeld.load(std::memory_order_relaxed);
    bool callbackCommandsClean =
      callbackOverlap && callbackStateClean && callbackCommandAttempted.load()
      && ((bluedroidCommands && callbackCommandAccepted.load() == 8 && callbackCommandBusy.load() == 1
           && callbackCommandOther.load() == 1)
          || (!bluedroidCommands && callbackCommandAccepted.load() == 9 && callbackCommandBusy.load() == 0
              && callbackCommandOther.load() == 0));
    Serial.printf(
      "[CLIENT] Phase35 callback commands clean=%d overlap=%d state=%d accepted=%d busy=%d other=%d\n", (int)callbackCommandsClean, (int)callbackOverlap,
      (int)callbackStateClean, callbackCommandAccepted.load(), callbackCommandBusy.load(), callbackCommandOther.load()
    );
    start = millis();
    while (floodCount.load() < 32 && millis() - start < 20000) {
      if (peer) {
        (void)peer.readValue(500);
      }
      delay(20);
    }
    if (peer) {
      (void)peer.subscribe(BLESubscription::Indicate, onPeer);
    }
    start = millis();
    while (peerIndicate.load() == 0 && millis() - start < 15000) {
      delay(50);
    }
    Serial.printf(
      "[CLIENT] Phase35 found=%d connected=%d notify=%d indicate=%d\n", (int)found, (int)connected, (int)peerNotify.load(), (int)peerIndicate.load()
    );
    if (peer) {
      (void)peer.subscribe(BLESubscription::Notify | BLESubscription::Indicate, onPeer);
    }
    Serial.println("[CLIENT] Phase35 mixedCccd armed=1");
    Serial.printf("[CLIENT] Phase35 flood=%d\n", (int)floodCount.load());

    // Wait for all queued indications before the harness arms the deterministic
    // read/remove barrier on the server.
    start = millis();
    while (queueIndicate.load() < 9 && millis() - start < 10000) {
      delay(20);
    }
    delay(200);
    Serial.printf("[CLIENT] Phase35 queueRx=%d\n", (int)queueIndicate.load());
    while (!phase35ReadRemoveGo) {
      checkSerial();
      delay(1);
    }

    struct LocalReadRemoveRace {
      BLERemoteCharacteristic chr;
      std::atomic<bool> done{false};
      std::atomic<uint32_t> attempts{0};
    };
    static LocalReadRemoveRace readRemoveRace;
    readRemoveRace.chr = queue;
    readRemoveRace.done.store(false, std::memory_order_relaxed);
    readRemoveRace.attempts.store(0, std::memory_order_relaxed);
    bool readRemoveCreated = queue
                             && xTaskCreate(
                                  [](void *arg) {
                                    auto *race = static_cast<LocalReadRemoveRace *>(arg);
                                    (void)race->chr.readValue(5000);
                                    race->attempts = 1;
                                    race->done.store(true, std::memory_order_release);
                                    vTaskDelete(nullptr);
                                  },
                                  "gatt-read-rm", 4096, &readRemoveRace, 1, nullptr
                                ) == pdPASS;
    Serial.printf("[CLIENT] Phase35 localReadRemove armed=%d\n", (int)readRemoveCreated);
    unsigned long readRemoveDeadline = millis() + 3000;
    while (readRemoveCreated && !readRemoveRace.done.load(std::memory_order_acquire) && millis() < readRemoveDeadline) {
      delay(1);
    }
    bool readRemoveClean =
      readRemoveCreated && readRemoveRace.done.load(std::memory_order_acquire) && readRemoveRace.attempts.load() > 0 && heap_caps_check_integrity_all(true);
    Serial.printf("[CLIENT] Phase35 localReadRemove clean=%d attempts=%lu\n", (int)readRemoveClean, (unsigned long)readRemoveRace.attempts.load());
    if (readRemoveRace.done.load(std::memory_order_acquire)) {
      readRemoveRace.chr = BLERemoteCharacteristic();
    }

    bool timeoutEmpty = false;
    bool timeoutOverlapRejected = false;
    bool rereadOk = false;
    bool writeCommandLateIsolated = true;
    bool writeLateIsolated = true;
    if (connected) {
      {
        BLERemoteService edge = c1.getService(edgeSvcUUID);
        BLERemoteCharacteristic tc = edge ? edge.getCharacteristic(timeoutCharUUID) : BLERemoteCharacteristic();
        String first = tc ? tc.readValue(200) : String();
        timeoutEmpty = first.length() == 0;
        String overlap = tc ? tc.readValue(200) : String();
        timeoutOverlapRejected = strcmp(BLE.getStackName(), "Bluedroid") != 0 || overlap.length() == 0;
      }
      delay(600);
      (void)c1.discoverServices();
      BLERemoteService edge = c1.getService(edgeSvcUUID);
      BLERemoteCharacteristic tc = edge ? edge.getCharacteristic(timeoutCharUUID) : BLERemoteCharacteristic();
      peer = edge ? edge.getCharacteristic(peerCharUUID) : BLERemoteCharacteristic();
      String second = tc ? tc.readValue(3000) : String();
      rereadOk = second == "timeout_ok";
      if (tc && strcmp(BLE.getStackName(), "Bluedroid") == 0) {
        BLEHooks.remote.holdWriteCommandCompletion.store(true, std::memory_order_relaxed);
        BTStatus firstCommand = tc.writeValue("late-command-1", false);
        BTStatus overlapCommand = tc.writeValue("late-command-2", false);
        BLEHooks.remote.releaseWriteCommandCompletion.store(true, std::memory_order_relaxed);
        BTStatus recoveredCommand = BTStatus::Busy;
        for (int retry = 0; retry < 1000 && recoveredCommand == BTStatus::Busy; retry++) {
          recoveredCommand = tc.writeValue("late-command-3", false);
          if (recoveredCommand == BTStatus::Busy) {
            delay(1);
          }
        }
        writeCommandLateIsolated = firstCommand == BTStatus::OK && overlapCommand == BTStatus::Busy && recoveredCommand == BTStatus::OK;

        BLEHooks.remote.writeTimeoutMs.store(200, std::memory_order_relaxed);
        BTStatus firstWrite = BTStatus::Busy;
        for (int retry = 0; retry < 1000 && firstWrite == BTStatus::Busy; retry++) {
          firstWrite = tc.writeValue("late-write-1", true);
          if (firstWrite == BTStatus::Busy) {
            delay(1);
          }
        }
        BTStatus overlapWrite = tc.writeValue("late-write-2", true);
        BTStatus recoveredWrite = BTStatus::Busy;
        for (int retry = 0; retry < 2000 && recoveredWrite == BTStatus::Busy; retry++) {
          recoveredWrite = tc.writeValue("late-write-3", true);
          if (recoveredWrite == BTStatus::Busy) {
            delay(1);
          }
        }
        writeLateIsolated = firstWrite == BTStatus::Timeout && overlapWrite == BTStatus::Busy && recoveredWrite == BTStatus::OK;
      }
    }
    Serial.printf(
      "[CLIENT] Phase35 timeoutEmpty=%d overlapRejected=%d reread=%d commandLateIsolated=%d writeLateIsolated=%d\n", (int)timeoutEmpty,
      (int)timeoutOverlapRejected, (int)rereadOk, (int)writeCommandLateIsolated, (int)writeLateIsolated
    );
    BTStatus restoreNotify = peer ? peer.subscribe(BLESubscription::Notify, onPeer) : BTStatus::NotFound;
    BTStatus dropStatus = peer ? peer.writeValue("drop_pending", false) : BTStatus::NotFound;
    Serial.printf("[CLIENT] Phase35 notifyTeardown trigger=%d\n", (int)(restoreNotify && dropStatus));
    while (!phase35DisconnectGo) {
      checkSerial();
      delay(1);
    }
    bool pendingOwnerArmed = true;
    bool disconnected = false;
    if (peer && strcmp(BLE.getStackName(), "Bluedroid") == 0) {
      static std::atomic<bool> pendingOwnerDisconnected{false};
      pendingOwnerDisconnected.store(false, std::memory_order_relaxed);
      c1.onDisconnect([](BLEClient, const BLEConnInfo &, uint8_t) {
        pendingOwnerDisconnected.store(true, std::memory_order_release);
      });
      BLEHooks.remote.holdWriteCommandCompletion.store(true, std::memory_order_relaxed);
      pendingOwnerArmed = (bool)peer.writeValue("owner-drop", false);
      peer = BLERemoteCharacteristic();
      queue = BLERemoteCharacteristic();
      c1 = BLEClient();
      Serial.println("[CLIENT] Phase35 owner handles dropped=1");
      unsigned long ownerDropDeadline = millis() + 10000;
      while (!pendingOwnerDisconnected.load(std::memory_order_acquire) && millis() < ownerDropDeadline) {
        delay(1);
      }
      disconnected = pendingOwnerDisconnected.load(std::memory_order_acquire);
    } else {
      BTStatus dropLink = c1.disconnect();
      disconnected = dropLink && !c1.isConnected();
    }
    Serial.printf(
      "[CLIENT] Phase35 notifyTeardown armed=%d pendingOwner=%d disconnected=%d\n", (int)(restoreNotify && dropStatus), (int)pendingOwnerArmed,
      (int)disconnected
    );

    bool terminalShutdownSupported = strcmp(BLE.getStackName(), "Bluedroid") == 0;
    bool terminalShutdownClean = !terminalShutdownSupported;
    if (terminalShutdownSupported) {
      BLEClient terminalClient = BLE.createClient();
      BTStatus terminalConnect = BTStatus::Fail;
      for (int retry = 0; retry < 5 && !terminalConnect; retry++) {
        terminalConnect = terminalClient.connect(targetAddr);
        if (!terminalConnect) {
          delay(100);
        }
      }
      BLERemoteService terminalService = terminalConnect ? terminalClient.getService(edgeSvcUUID) : BLERemoteService();
      BLERemoteCharacteristic terminalChar = terminalService ? terminalService.getCharacteristic(timeoutCharUUID) : BLERemoteCharacteristic();

      // Force xTaskCreate failure once. The synchronous GAP-disconnect fallback
      // must retain the client until its native disconnect callback arrives.
      BLEHooks.gatt.terminalWorkerStartFailures.store(1);
      BLEHooks.gatt.forceTerminalClose.store(true);
      BTStatus fallbackWrite = terminalChar ? terminalChar.writeValue("terminal-fallback", false) : BTStatus::NotFound;
      unsigned long fallbackDeadline = millis() + 5000;
      while (terminalClient.isConnected() && millis() < fallbackDeadline) {
        delay(1);
      }
      bool fallbackClean = fallbackWrite && !terminalClient.isConnected() && BLEHooks.gatt.terminalWorkerStartFailures.load() == 0;
      BLEHooks.gatt.forceTerminalClose.store(false);
      terminalService = BLERemoteService();
      terminalChar = BLERemoteCharacteristic();
      terminalConnect = BTStatus::Fail;
      for (int retry = 0; retry < 5 && !terminalConnect; retry++) {
        terminalConnect = terminalClient.connect(targetAddr);
        if (!terminalConnect) {
          delay(100);
        }
      }
      terminalService = terminalConnect ? terminalClient.getService(edgeSvcUUID) : BLERemoteService();
      terminalChar = terminalService ? terminalService.getCharacteristic(timeoutCharUUID) : BLERemoteCharacteristic();

      BLEHooks.gatt.terminalWorkerEntered.store(false);
      BLEHooks.gatt.terminalShutdownEntered.store(false);
      BLEHooks.gatt.holdTerminalWorker.store(true);
      BLEHooks.gatt.forceTerminalClose.store(true);
      BTStatus terminalWrite = terminalChar ? terminalChar.writeValue("terminal-shutdown", false) : BTStatus::NotFound;
      unsigned long terminalWorkerDeadline = millis() + 5000;
      while (!BLEHooks.gatt.terminalWorkerEntered.load() && millis() < terminalWorkerDeadline) {
        delay(1);
      }
      bool workerEntered = BLEHooks.gatt.terminalWorkerEntered.load();
      BLEHooks.gatt.forceTerminalClose.store(false);
      terminalChar = BLERemoteCharacteristic();
      terminalService = BLERemoteService();
      terminalClient = BLEClient();

      static BLEStreamShutdownRaceJob terminalShutdownRace;
      terminalShutdownRace.hostEndDone.store(false);
      bool hostTaskStarted = workerEntered && xTaskCreate(bleStreamShutdownRaceHostTask, "gatt-host-end", 6144, &terminalShutdownRace, 1, nullptr) == pdPASS;
      unsigned long terminalShutdownDeadline = millis() + 5000;
      while (hostTaskStarted && !BLEHooks.gatt.terminalShutdownEntered.load() && millis() < terminalShutdownDeadline) {
        delay(1);
      }
      bool shutdownEntered = BLEHooks.gatt.terminalShutdownEntered.load();
      bool shutdownBlocked = shutdownEntered && !terminalShutdownRace.hostEndDone.load();
      BLEHooks.gatt.holdTerminalWorker.store(false);
      while (hostTaskStarted && !terminalShutdownRace.hostEndDone.load()) {
        delay(1);
      }
      if (!hostTaskStarted) {
        BLE.end(false);
      }
      BTStatus terminalRebegin = BLE.begin(targetName + "_client_terminal_reinit");
      terminalShutdownClean = fallbackClean && terminalWrite && workerEntered && hostTaskStarted && shutdownBlocked && terminalShutdownRace.hostEndDone.load()
                              && terminalRebegin && heap_caps_check_integrity_all(true);
    }
    Serial.printf("[CLIENT] Phase35 terminalShutdown supported=%d clean=%d\n", (int)terminalShutdownSupported, (int)terminalShutdownClean);

    BLEStream shutdownClient;
    BTStatus shutdownConnect = BTStatus::Fail;
    for (int shutdownAttempt = 0; shutdownAttempt < 10 && !shutdownConnect; shutdownAttempt++) {
      shutdownConnect = shutdownClient.beginClient(targetAddr, 5000);
      if (!shutdownConnect) {
        delay(250);
      }
    }
    Serial.printf("[CLIENT] Phase35 BLEStream final release connected=%d\n", (int)(bool)shutdownConnect);
    while (shutdownConnect && !phase35StreamReleaseGo) {
      checkSerial();
      delay(1);
    }
    static BLEStreamShutdownRaceJob streamShutdownRace;
    streamShutdownRace.stream = &shutdownClient;
    streamShutdownRace.streamEndDone.store(false);
    streamShutdownRace.hostEndDone.store(false);
    BLEHooks.stream.finalRoleReleaseReached.store(false);
    BLEHooks.stream.hostShutdownEntered.store(false);
    BLEHooks.stream.holdFinalRoleRelease.store(true);
    bool streamTaskStarted = shutdownConnect && xTaskCreate(bleStreamShutdownRaceEndTask, "stream-final-end", 6144, &streamShutdownRace, 1, nullptr) == pdPASS;
    unsigned long releaseDeadline = millis() + 5000;
    while (streamTaskStarted && !BLEHooks.stream.finalRoleReleaseReached.load() && millis() < releaseDeadline) {
      delay(1);
    }
    bool finalReleaseReached = BLEHooks.stream.finalRoleReleaseReached.load();
    bool streamHostTaskStarted =
      finalReleaseReached && xTaskCreate(bleStreamShutdownRaceHostTask, "stream-host-end", 6144, &streamShutdownRace, 1, nullptr) == pdPASS;
    unsigned long hostDeadline = millis() + 5000;
    while (streamHostTaskStarted && !BLEHooks.stream.hostShutdownEntered.load() && millis() < hostDeadline) {
      delay(1);
    }
    bool streamHostEntered = BLEHooks.stream.hostShutdownEntered.load();
    bool streamHostBlocked = streamHostEntered && !streamShutdownRace.hostEndDone.load() && !streamShutdownRace.streamEndDone.load();
    BLEHooks.stream.holdFinalRoleRelease.store(false);
    while (streamTaskStarted && !streamShutdownRace.streamEndDone.load()) {
      delay(1);
    }
    while (streamHostTaskStarted && !streamShutdownRace.hostEndDone.load()) {
      delay(1);
    }
    if (!streamTaskStarted) {
      shutdownClient.end();
    }
    if (!streamHostTaskStarted) {
      BLE.end(false);
    }
    BTStatus streamShutdownRebegin = BLE.begin(targetName + "_client_stream_reinit");
    bool streamShutdownClean = shutdownConnect && streamTaskStarted && streamHostTaskStarted && finalReleaseReached && streamHostBlocked
                               && streamShutdownRace.streamEndDone.load() && streamShutdownRace.hostEndDone.load() && streamShutdownRebegin
                               && heap_caps_check_integrity_all(true);
    Serial.printf("[CLIENT] Phase35 BLEStream final release clean=%d\n", (int)streamShutdownClean);
    delay(300);
    Serial.println("[CLIENT] Phase35 done");
  }

  // ===== Phase 36: Directed high-duty advertising + OOB reject =====
  waitForPhase(36);
  {
    Serial.printf("[CLIENT] Phase36 addr=%s\n", BLE.getAddress().toString().c_str());
    waitForHostGo(phase36ConnectGo, 15000);
    clientAuthCompleteFired = false;
    clientLastAuthSuccess = false;
    BLEClient dc = BLE.createClient();
    installClientCallbacks(dc);
    BTStatus dcs = dc.connect(targetAddr);
    Serial.printf("[CLIENT] Phase36 directed connected=%d\n", (int)(bool)dcs);
    if (dcs) {
      dc.disconnect();
      delay(300);
    }
    waitForHostGo(phase36OobGo, 20000);
#if BLE_VALIDATION_HOOKS
    BLEHooks.security.advertiseOob.store(true, std::memory_order_relaxed);
#endif
    BLESecurity sec = BLE.getSecurity();
    (void)sec.deleteAllBonds();
    sec.setAuthenticationMode(true, true, false);
    auto oobAttempt = [&](const char *tag) {
      clientAuthCompleteFired = false;
      clientLastAuthSuccess = false;
      (void)sec.deleteAllBonds();
      delay(300);
      bool found = scanForServer();
      bool connected = false;
      bool readOk = false;
      bool enc = false;
      bool bond = false;
      if (found) {
        BLEClient rc = BLE.createClient();
        installClientCallbacks(rc);
        BTStatus cs = rc.connect(targetAddr);
        connected = (bool)cs;
        if (cs) {
          (void)rc.secureConnection();
          uint32_t deadline = millis() + 10000;
          while (!clientAuthCompleteFired && (int32_t)(deadline - millis()) > 0) {
            delay(20);
          }
          BLERemoteService svc = rc.getService(serviceUUID);
          if (svc) {
            BLERemoteCharacteristic sc = svc.getCharacteristic(secureCharUUID);
            if (sc) {
              String v = sc.readValue();
              readOk = (v.length() > 0);
            }
          }
          BLEConnInfo ci = rc.getConnInfo();
          enc = ci.isEncrypted();
          bond = ci.isBonded();
          rc.disconnect();
          delay(500);
        }
      }
      Serial.printf(
        "[CLIENT] Phase36 %s found=%d connected=%d readOk=%d enc=%d bond=%d authFired=%d authSuccess=%d\n", tag, (int)found, (int)connected, (int)readOk,
        (int)enc, (int)bond, (int)clientAuthCompleteFired, (int)clientLastAuthSuccess
      );
    };
    oobAttempt("oobLegacy");
    waitForHostGo(phase36OobScGo, 20000);
    sec.setAuthenticationMode(true, true, true);
    oobAttempt("oobSc");
#if BLE_VALIDATION_HOOKS
    BLEHooks.security.advertiseOob.store(false, std::memory_order_relaxed);
#endif
    sec.setAuthenticationMode(true, true, true);
    Serial.println("[CLIENT] Phase36 done");
  }

  // ===== Phase 37: Memory release + reinit guard =====
  waitForPhase(37);

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
