// Combined BLE validation test — SERVER
// Phases: basic lifecycle, BLE5 ext+periodic adv, GATT server, notifications,
//         large writes, descriptors, write-no-response, server disconnect,
//         security, reconnect

#include <Arduino.h>
#include <BLE.h>
#include <atomic>
#include <type_traits>
#include "esp_heap_caps.h"

// Every handler hands the callback a handle it can act on (chr.setValue() from
// onRead, channel.write() from onData, server.startAdvertising() from
// onDisconnect, ...), so each one must accept a callable that takes the handle
// by non-const reference. A const-reference parameter would reject these.
template<typename Handler, typename Fn> constexpr bool acceptsMutableHandle = std::is_constructible_v<Handler, Fn *>;

static_assert(acceptsMutableHandle<BLECharacteristic::ReadHandler, void(BLECharacteristic &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLECharacteristic::WriteHandler, void(BLECharacteristic &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLECharacteristic::NotifyHandler, void(BLECharacteristic &)>);
static_assert(acceptsMutableHandle<BLECharacteristic::SubscribeHandler, void(BLECharacteristic &, const BLEConnInfo &, uint16_t)>);
static_assert(acceptsMutableHandle<BLECharacteristic::StatusHandler, void(BLECharacteristic &, BLECharacteristic::NotifyStatus, uint32_t)>);
static_assert(acceptsMutableHandle<BLEDescriptor::ReadHandler, void(BLEDescriptor &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLEDescriptor::WriteHandler, void(BLEDescriptor &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLEServer::ConnectHandler, void(BLEServer &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLEServer::DisconnectHandler, void(BLEServer &, const BLEConnInfo &, uint8_t)>);
static_assert(acceptsMutableHandle<BLEServer::MtuChangedHandler, void(BLEServer &, const BLEConnInfo &, uint16_t)>);
static_assert(acceptsMutableHandle<BLEServer::ConnParamsHandler, void(BLEServer &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLEServer::IdentityHandler, void(BLEServer &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLEClient::ConnectHandler, void(BLEClient &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLEClient::DisconnectHandler, void(BLEClient &, const BLEConnInfo &, uint8_t)>);
static_assert(acceptsMutableHandle<BLEClient::ConnectFailHandler, void(BLEClient &, int)>);
static_assert(acceptsMutableHandle<BLEClient::MtuChangedHandler, void(BLEClient &, const BLEConnInfo &, uint16_t)>);
static_assert(acceptsMutableHandle<BLEClient::ConnParamsReqHandler, bool(BLEClient &, const BLEConnParams &)>);
static_assert(acceptsMutableHandle<BLEClient::IdentityHandler, void(BLEClient &, const BLEConnInfo &)>);
static_assert(acceptsMutableHandle<BLERemoteCharacteristic::NotifyCallback, void(BLERemoteCharacteristic &, const uint8_t *, size_t, BLERemoteCharacteristic::Delivery)>);
static_assert(acceptsMutableHandle<BLEL2CAPChannel::DataHandler, void(BLEL2CAPChannel &, const uint8_t *, size_t)>);
static_assert(acceptsMutableHandle<BLEL2CAPChannel::DisconnectHandler, void(BLEL2CAPChannel &)>);
static_assert(acceptsMutableHandle<BLEL2CAPServer::AcceptHandler, void(BLEL2CAPChannel &)>);
static_assert(acceptsMutableHandle<BLEL2CAPServer::DataHandler, void(BLEL2CAPChannel &, const uint8_t *, size_t)>);
static_assert(acceptsMutableHandle<BLEL2CAPServer::DisconnectHandler, void(BLEL2CAPChannel &)>);

constexpr BLEUUID kConstexprUuidEmpty;
constexpr BLEUUID kConstexprUuid16 = BLEUUID::from16(0x180D);
constexpr BLEUUID kConstexprUuid128 = BLEUUID::from128(0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
static_assert(!kConstexprUuidEmpty.isValid());
static_assert(kConstexprUuid16.isValid() && kConstexprUuid16.bitSize() == 16);
static_assert(kConstexprUuid128.isValid() && kConstexprUuid128.bitSize() == 128);

static const BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID RW_CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static const BLEUUID NOTIFY_CHAR_UUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
static const BLEUUID INDICATE_CHAR_UUID("d5f782b2-a36e-4d68-947c-0e9a5f2c78e1");
static const BLEUUID SECURE_CHAR_UUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");
static const BLEUUID DESC_CHAR_UUID("a3c87501-8ed3-4bdf-8a39-a01bebede295");
static const BLEUUID WRITENR_CHAR_UUID("1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e");
// Phase 4 / Phase 7: values produced on demand by the onRead handler through
// the handle it receives, so every read returns a fresh value.
static const BLEUUID DYNAMIC_READ_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a35");
static const BLEUUID DYNAMIC_READ_DESC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a36");
// Phase 4 / Phase 7: same idea with a 512-byte value, which needs a Read Blob
// after the first MTU-sized chunk. onRead must run once per read, not per chunk.
static const BLEUUID LONG_READ_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a37");
static const BLEUUID LONG_READ_DESC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a38");
static const BLEUUID SNAPSHOT_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3a");
static const BLEUUID DUAL_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3b");
static const BLEUUID TMP_SVC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3c");
static const BLEUUID TMP_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3d");
static const BLEUUID BUSY_SVC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3e");
static const BLEUUID BUSY_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a3f");
// Phase 35: per-peer CCCD, in-flight removeService, read-timeout recover, notify flood.
static const BLEUUID EDGE_SVC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a40");
static const BLEUUID PEER_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a41");
static const BLEUUID QUEUE_SVC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a42");
static const BLEUUID QUEUE_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a43");
static const BLEUUID TIMEOUT_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a44");
static const BLEUUID WRITABLE_DESC_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a45");
// Phase 3: written through a handle obtained from a const lookup, read back by
// the client in Phase 4.
static const BLEUUID CONST_LOOKUP_CHAR_UUID("7f3a9c2e-5b1d-4e8f-a6c4-2d9b8e1f0a39");
// Phase 15 (introspection_and_permissions): R|W properties but only Read
// permission. Exercises the fail-closed mapping: the Write property must be
// stripped by the backend so the peer sees it as read-only, and writes must
// be rejected at the ATT layer.
static const BLEUUID PERM_FAIL_CLOSED_CHAR_UUID("e8b1d3a1-6b3e-4a5d-9f1d-9b7e5c3a2f41");

// Phase 15 (descriptor permissions): custom-UUID descriptor attached to
// DESC_CHAR with read-only permission. The client reads it (expect success)
// then attempts a write (expect ATT-layer rejection), proving that
// bleValidateDescProps + the BluedroidServer descPerm translation enforce
// the declared descriptor permissions end-to-end.
static const BLEUUID DESC_READONLY_UUID("b5f3e2c1-8a9e-4b7c-9f3d-2e8a5b7c3f91");

// --- New UUIDs introduced by phases 16+ ---------------------------------
// Phase 18: application-level access gating via BLESecurity::onAuthorization.
// Declares ReadAuthorized|WriteAuthorized so every read/write traps through
// the handler. The handler approves reads and specific write payloads and
// denies everything else, exercising both approve and deny branches.
static const BLEUUID AUTHZ_CHAR_UUID("ca11aaaa-1111-4222-8333-444455556666");

// Phase 19: ReadWriteEncrypted permissions. Pre-pair access will fail with
// insufficient-encryption; post-pair access succeeds. Used by both the
// encrypted_perm_enforcement phase and the bond_and_whitelist phase (after
// the bond is erased, the next read must re-trigger pairing).
static const BLEUUID ENCRYPTED_CHAR_UUID("ca11bbbb-1111-4222-8333-444455556666");

// Phase 23 helpers: fake UUIDs that must NOT be discoverable on the server,
// and an extra writable char used to assert ops-after-disconnect failure.
static const BLEUUID UNKNOWN_SVC_UUID("deadbeef-0000-0000-0000-000000000001");
static const BLEUUID UNKNOWN_CHAR_UUID("deadbeef-0000-0000-0000-000000000002");

String serverName;
BLECharacteristic notifyChr;
std::atomic<uint32_t> notifySubEvents{0};
std::atomic<uint32_t> serverIdentityCount{0};
BLECharacteristic indicateChr;
BLECharacteristic descChr;            // exposed to loop() so Phase 15 can
                                      // run construction-time validator
                                      // tests against a real char handle
BLECharacteristic permFailClosedChr;  // same rationale
BLECharacteristic authzChr;           // phase 18
BLECharacteristic encryptedChr;       // phase 19
int notifyStep = 0;
volatile int currentPhase = 0;
bool serverDisconnectDone = false;
bool phase10BondsCleared = false;
bool descPermValidationDone = false;

// Phase 16-28 completion flags (each phase fires once)
bool phase16Done = false;
bool phase17Done = false;
bool phase18Started = false;  // phase 18 is event-driven; no single "done" marker
bool phase19Done = false;
bool phase20Done = false;
bool phase21Done = false;
bool phase22Done = false;
bool phase23Done = false;
bool phase24Done = false;
bool phase25Done = false;
bool phase26Done = false;  // ble5_legacy_over_ext
bool phase27Done = false;  // ble5_legacy_plus_ext
bool phase28Done = false;  // l2cap_bulk_reconnect (HITL)
bool phase29Done = false;  // conn_params_reject (HITL)
bool phase30Done = false;  // deletebond_roundtrip (HITL)
bool phase31Done = false;  // nc_reject (HITL)
bool phase32Done = false;  // passkey_entry (HITL)
bool phase33Done = false;  // indication_busy_retry
volatile bool phase33Continue = false;
bool phase34Done = false;  // periodic_adv_lifecycle (HITL)
bool phase35Done = false;  // pending_send_edges
bool phase36Done = false;  // memory_release (tail of suite)

// Phase 32 (passkey-entry) — the peripheral is DisplayOnly and shows this fixed
// passkey; the central (KeyboardOnly) supplies the same value from its
// onPassKeyRequest handler so the entry pairing succeeds deterministically.
static const uint32_t PASSKEY_ENTRY_STATIC = 398215;

// Phase 28 (L2CAP bulk/reconnect) — accumulate received CoC bytes across the
// stack's SDU callbacks and echo the total once the client's bulk target is
// reached. Touched from the BLE task, read/reset from loop().
volatile uint32_t l2capBulkBytes = 0;
volatile bool l2capBulkReplied = false;
std::atomic<uint32_t> l2capPhase28Accepts{0};
std::atomic<uint32_t> l2capPhase28Disconnects{0};
#if BLE_L2CAP_SUPPORTED
BLEL2CAPServer l2capBulkServer;
#endif
static const uint32_t L2CAP_BULK_TARGET = 2000;

// Phase 16 — server callback aggregation (set from BLE stack task, read from loop).
// Without these we'd race the MTU/conn-param update events against the phase
// coordination token. Storing the last-seen values lets loop() log them
// deterministically AFTER the phase start marker.
volatile bool phase16MtuSeen = false;
volatile uint16_t phase16LastMtu = 0;
volatile bool phase16ConnParamsSeen = false;
volatile uint16_t phase16LastInterval = 0;
volatile uint16_t phase16LastLatency = 0;
volatile uint16_t phase16LastTimeout = 0;

// Phase 18 authorization counters — touched from callback (stack task) and
// read/logged from loop() to avoid interleaving with Serial prints from the
// stack task.
volatile uint32_t authzReadApprovals = 0;
volatile uint32_t authzWriteApprovals = 0;
volatile uint32_t authzWriteDenials = 0;
volatile uint16_t authzLastAttrHandle = 0;
// Per-write toggle used by the Phase 18 authorization handler. The BLE spec
// treats the Write-Authorized bit as a per-(connection, attribute) policy —
// the write payload is NOT available to the handler because the ATT layer
// dispatches authorization before applying the value. We therefore
// alternate "approve / deny" across consecutive writes so the test can
// observe one approval and one denial from the same characteristic.
volatile bool authzApproveNextWrite = true;

// Phase 8 Write-Without-Response burst — the payloads the client sends back to
// back, one distinct byte value repeated per packet so a payload that leaked in
// from another packet in the queue is obvious in the log. Touched only from the
// onWrite callback, which the stack runs on a single task.
static const uint8_t writeNrBurstBytes[] = {0xAA, 0xBB, 0xCC};
static const size_t writeNrBurstLen = 3;
uint8_t writeNrBurstSeen = 0;
bool writeNrBurstOk = true;

// Phase 4 / Phase 7 on-demand reads. Each read handler writes the next value
// into the handle it receives; the stack sends that value in the same response.
uint32_t dynamicChrReads = 0;
uint32_t dynamicDescReads = 0;
uint32_t writableDescWrites = 0;

static void onDynamicChrRead(BLECharacteristic &chr, const BLEConnInfo &conn) {
  chr.setValue(String("dyn_chr_") + String(++dynamicChrReads));
}

// Each long read fills the whole value with one letter that advances per
// onRead call ('A', 'B', ... for the characteristic, 'a', 'b', ... for the
// descriptor). A read stitched from several onRead calls is not uniform.
static const size_t LONG_READ_LEN = 512;
uint32_t longChrReads = 0;
uint32_t longDescReads = 0;

static void fillLongValue(uint8_t *buf, char first, uint32_t n) {
  memset(buf, first + (int)((n - 1) % 26), LONG_READ_LEN);
}

// Phase 4: loop() rewrites this value every few ms, much faster than the
// connection interval between a Read and its Read Blob. Each long read must
// still return one value, not the head of one and the tail of the next.
BLECharacteristic longChr;
std::atomic<bool> longChrMutatePending{false};
BLECharacteristic snapshotChr;

// Phase 5 onNotify/onStatus. Handlers record; loop() logs so the output stays
// ordered relative to the phase markers. notifyPayloadFromCallback makes the
// next onNotify supply the payload, proving it runs before the value is sent.
std::atomic<bool> notifyPayloadFromCallback{false};
std::atomic<uint32_t> notifyCbCount{0};
std::atomic<int> notifyStatus{-1};
std::atomic<uint32_t> notifyStatusCode{0};
std::atomic<uint32_t> indicateCbCount{0};
std::atomic<int> indicateStatus{-1};
std::atomic<uint32_t> indicateStatusCode{0};
bool notifyStatusLogged = false;
bool indicateStatusLogged = false;
bool noSubscriberChecked = false;

// Phase 5: indications sent back to back, before the previous one is
// confirmed. ATT allows one outstanding indication per client, so the library
// must queue them; each one is reported once the client confirms it.
static const int INDICATE_BURST = 4;
static const int INDICATE_OVERFLOW = 12;  // 1 in-flight + 8 queued; the rest must be Busy
std::atomic<uint32_t> indicateOkCount{0};
std::atomic<uint32_t> indicateErrCount{0};
bool indicateBurstSent = false;
bool indicateBurstLogged = false;
unsigned long indicateBurstTime = 0;
bool indicateOverflowSent = false;
bool indicateOverflowLogged = false;
int indicateOverflowAccepted = 0;
int indicateOverflowBusy = 0;
unsigned long indicateOverflowTime = 0;

// Phase 5: a characteristic the client subscribes to for both notifications
// and indications. Its notifications and indications complete through the
// same Bluedroid event, so each status must still land on the right kind.
static const int DUAL_BURST = 4;
BLECharacteristic dualChr;
std::atomic<bool> dualBothEnabled{false};
std::atomic<uint32_t> dualIndOk{0};
std::atomic<uint32_t> dualNotifOk{0};
std::atomic<uint32_t> dualErr{0};
bool dualSent = false;
bool dualLogged = false;
unsigned long dualTime = 0;

// Phase 33: delivery results of the indications sent behind Service Changed.
std::atomic<uint32_t> busyOkCount{0};
std::atomic<uint32_t> busyErrCount{0};

static const char *notifyStatusName(int s) {
  switch (s) {
    case BLECharacteristic::SUCCESS_INDICATE:        return "SUCCESS_INDICATE";
    case BLECharacteristic::SUCCESS_NOTIFY:          return "SUCCESS_NOTIFY";
    case BLECharacteristic::ERROR_INDICATE_DISABLED: return "ERROR_INDICATE_DISABLED";
    case BLECharacteristic::ERROR_NOTIFY_DISABLED:   return "ERROR_NOTIFY_DISABLED";
    case BLECharacteristic::ERROR_GATT:              return "ERROR_GATT";
    case BLECharacteristic::ERROR_NO_CLIENT:         return "ERROR_NO_CLIENT";
    case BLECharacteristic::ERROR_NO_SUBSCRIBER:     return "ERROR_NO_SUBSCRIBER";
    case BLECharacteristic::ERROR_INDICATE_TIMEOUT:  return "ERROR_INDICATE_TIMEOUT";
    case BLECharacteristic::ERROR_INDICATE_FAILURE:  return "ERROR_INDICATE_FAILURE";
    default:                                         return "NONE";
  }
}

// Phase 24 BLE5 advanced — last PHY/DLE state observed by server callbacks.
volatile uint8_t phase24TxPhy = 0;
volatile uint8_t phase24RxPhy = 0;

BLEStream bleStream;
bool bleStreamInitDone = false;
bool bleStreamPhaseDone = false;
int bleStreamStep = 0;
String bleStreamRxLine;
volatile bool bleStreamConnectCb = false;
volatile bool bleStreamDisconnectCb = false;
volatile bool bleStreamWriteToDone = false;
volatile bool bleStreamEndRequested = false;
volatile bool phase13Continue = false;
bool bleStreamPeekDone = false;
bool l2capInitDone = false;

#if BLE_L2CAP_SUPPORTED
BLEL2CAPServer l2capServer;
BLEL2CAPChannel l2capChannel;
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
          Serial.printf("[SERVER] Phase %d started\n", phase);
        }
      } else if (buf == "PHASE13_CONTINUE") {
        phase13Continue = true;
      } else if (buf == "PHASE33_CONTINUE") {
        phase33Continue = true;
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
static void syncPhaseFromHost() {
  checkSerial();
}

void waitForPhase(int n) {
  while (currentPhase < n) {
    checkSerial();
    delay(10);
  }
}

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

// ========================= Phase 1 — Basic Lifecycle =========================

bool phase_basic() {
  // Stack init has previously overrun a heap block and smashed the tail canary,
  // which only surfaced as a corruption panic much later, in unrelated code.
  // Walk every heap region on both sides of the first begin() to catch it here.
  bool heapCleanBefore = heap_caps_check_integrity_all(true);
  BTStatus status = BLE.begin(serverName);
  bool heapCleanAfter = heap_caps_check_integrity_all(true);
  Serial.printf("[SERVER] Phase1 heapIntegrity before=%d after=%d\n", (int)heapCleanBefore, (int)heapCleanAfter);
  if (!status) {
    Serial.printf("[SERVER] Init FAILED: %s\n", status.toString());
    return false;
  }
  Serial.printf("[SERVER] Stack: %s\n", BLE.getStackName());
  Serial.println("[SERVER] Init OK");
  Serial.printf("[SERVER] Device name: %s\n", BLE.getDeviceName().c_str());
  Serial.printf("[SERVER] Address: %s\n", BLE.getAddress().toString().c_str());

  // NVS outlives reflashing, so bonds from an earlier run would otherwise leak
  // into the pairing phases and make them depend on whatever state the boards
  // were left in. Wipe once here, at boot, before any phase pairs.
  {
    BLESecurity sec = BLE.getSecurity();
    size_t stale = sec.getBondedDevices().size();
    (void)sec.deleteAllBonds();
    Serial.printf("[SERVER] Boot bond wipe: stale=%u bonds=%u\n", (unsigned)stale, (unsigned)sec.getBondedDevices().size());
  }

  BLE.end(false);
  Serial.println("[SERVER] Deinit OK");
  delay(1000);

  status = BLE.begin(serverName + "_reinit");
  if (!status) {
    Serial.printf("[SERVER] Reinit FAILED: %s\n", status.toString());
    return false;
  }
  Serial.println("[SERVER] Reinit OK");

  BLE.end(false);
  Serial.println("[SERVER] Final deinit OK");
  return true;
}

// ============== Phase 2 — BLE5 Extended + Periodic Advertising ===============

#if BLE5_SUPPORTED
bool phase_ble5_adv() {
  BTStatus status = BLE.begin(serverName);
  if (!status) {
    return false;
  }

  BLEAdvertising adv = BLE.getAdvertising();

  // Extended advertising set 0: non-connectable (required for periodic),
  // 1M primary / 2M secondary PHY, SID 1. Uses the per-setter API -- the
  // locked public contract (per-setter BLE5 API; see examples/BLE5Advertising).
  adv.setExtType(0, BLEAdvType::NonConnectable);
  adv.setExtPhy(0, BLEPhy::PHY_1M, BLEPhy::PHY_2M);
  adv.setExtSID(0, 1);

  Serial.println("[SERVER] BLE5 init OK");
  Serial.println("[SERVER] Extended adv configured");

  BLEAdvertisementData extData;
  extData.setName(serverName);
  status = adv.setExtAdvertisementData(0, extData);
  if (!status) {
    BLE.end(false);
    return false;
  }

  // Periodic advertising on the same instance (interval in 1.25 ms units).
  adv.setPeriodicAdvInterval(0, 0x20, 0x40);

  BLEAdvertisementData perData;
  perData.setName("PeriodicPayload");
  adv.setPeriodicAdvData(0, perData);

  status = adv.startExtended(0, 0, 0);
  if (!status) {
    BLE.end(false);
    return false;
  }
  Serial.println("[SERVER] Extended adv started");

  adv.startPeriodicAdv(0);
  Serial.println("[SERVER] Periodic adv started");

  delay(20000);

  adv.stopPeriodicAdv(0);
  adv.stopExtended(0);
  BLE.end(false);
  Serial.println("[SERVER] BLE5 adv phase done");
  return true;
}
#endif

// ====================== Phase 3 — GATT Server Setup =========================

bool phase_gatt_setup() {
  size_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[SERVER] Heap before init: %u\n", (unsigned)heapBefore);

  BTStatus status = BLE.begin(serverName);
  if (!status) {
    Serial.println("[SERVER] GATT init FAILED");
    return false;
  }
  Serial.println("[SERVER] GATT init OK");
  BLE.setMTU(512);

  size_t heapAfterInit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[SERVER] Heap after init: %u\n", (unsigned)heapAfterInit);

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLEIOCapability::DisplayYesNo);
  sec.setAuthenticationMode(true, true, true);
  // Passkey can be delivered as display (BLE_SM_IOACT_DISP) on one side and
  // numeric comparison (BLE_SM_IOACT_NUMCMP) on the other; log both.
  sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t passkey) {
    Serial.printf("[SERVER] Passkey: %06lu\n", (unsigned long)passkey);
  });
  sec.onConfirmPassKey([](const BLEConnInfo &conn, uint32_t passkey) -> bool {
    Serial.printf("[SERVER] Passkey: %06lu\n", (unsigned long)passkey);
    return true;
  });
  sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
    // The success flag is appended rather than logged separately so the
    // existing "[SERVER] Authentication complete" substring match still holds,
    // while a server-side pairing failure is no longer invisible in the log.
    Serial.printf("[SERVER] Authentication complete success=%d\n", (int)success);
  });
  // Phase 18: application-level authorization callback. Always approves
  // reads; alternates approve/deny on writes (see authzApproveNextWrite).
  // Counters are drained from loop() to keep output ordered.
  sec.onAuthorization([](const BLEConnInfo &conn, uint16_t attrHandle, bool isRead) -> bool {
    authzLastAttrHandle = attrHandle;
    if (isRead) {
      uint32_t n = authzReadApprovals;
      authzReadApprovals = n + 1;
      return true;
    }
    bool approve = authzApproveNextWrite;
    authzApproveNextWrite = !authzApproveNextWrite;
    if (approve) {
      uint32_t n = authzWriteApprovals;
      authzWriteApprovals = n + 1;
    } else {
      uint32_t n = authzWriteDenials;
      authzWriteDenials = n + 1;
    }
    return approve;
  });
  Serial.println("[SERVER] Security configured");

  BLEServer server = BLE.createServer();
  Serial.printf("[SERVER] Services after begin: %u\n", (unsigned)server.getServices().size());
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    Serial.println("[SERVER] Client connected");
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    syncPhaseFromHost();
    Serial.println("[SERVER] Client disconnected");
  });
  server.onIdentity([](BLEServer, const BLEConnInfo &) {
    syncPhaseFromHost();
    serverIdentityCount.fetch_add(1, std::memory_order_relaxed);
  });
  // Phase 16 — capture MTU/connection-parameter events. Values are logged
  // from loop() after the phase coordination token is applied to guarantee
  // order on the test harness.
  server.onMtuChanged([](BLEServer s, const BLEConnInfo &conn, uint16_t mtu) {
    syncPhaseFromHost();
    phase16LastMtu = mtu;
    phase16MtuSeen = true;
  });
  server.onConnParamsUpdate([](BLEServer s, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    phase16LastInterval = conn.getInterval();
    phase16LastLatency = conn.getLatency();
    phase16LastTimeout = conn.getSupervisionTimeout();
    phase16ConnParamsSeen = true;
  });
  server.advertiseOnDisconnect(true);

  BLEService svc = server.createService(SERVICE_UUID);

  auto rwChr = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadWriteOpen);
  rwChr.setValue("Hello from server!");
  rwChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    if (len <= 50) {
      Serial.printf("[SERVER] Received write: %.*s\n", (int)len, (const char *)data);
    }
    Serial.printf("[SERVER] Received %u bytes\n", (unsigned)len);
  });

  auto dynamicChr = svc.createCharacteristic(DYNAMIC_READ_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  dynamicChr.onRead(onDynamicChrRead);

  auto constLookupChr = svc.createCharacteristic(CONST_LOOKUP_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  constLookupChr.setValue("const_initial");

  longChr = svc.createCharacteristic(LONG_READ_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  longChr.onRead([](BLECharacteristic &chr, const BLEConnInfo &conn) {
    static uint8_t buf[LONG_READ_LEN];  // keep 512 bytes off the BLE task stack
    uint32_t readNum = ++longChrReads;
    fillLongValue(buf, 'A', readNum);
    if (readNum == 3) {
      // Exact (MTU - 1) boundary: the initial ATT Read Response fills MTU - 1
      // bytes, so the client must follow up with a Read Blob at offset = MTU - 1
      // expecting 0 bytes even if loop() rewrites the attribute to 512 bytes
      // before the Read Blob arrives.
      size_t exactLen = conn.getMTU() > 1 ? static_cast<size_t>(conn.getMTU() - 1) : sizeof(buf);
      if (exactLen > sizeof(buf)) {
        exactLen = sizeof(buf);
      }
      chr.setValue(buf, exactLen);
      longChrMutatePending.store(true, std::memory_order_release);
      return;
    }
    chr.setValue(buf, sizeof(buf));
  });

  notifyChr = svc.createCharacteristic(NOTIFY_CHAR_UUID, BLEProperty::Read | BLEProperty::Notify, BLEPermission::ReadOpen);
  notifyChr.onSubscribe([](BLECharacteristic chr, const BLEConnInfo &conn, uint16_t subValue) {
    syncPhaseFromHost();
    notifySubEvents.fetch_add(1, std::memory_order_relaxed);
    Serial.printf("[SERVER] Subscriber count: %u\n", chr.getSubscribedCount());
  });
  notifyChr.onNotify([](BLECharacteristic &chr) {
    notifyCbCount.fetch_add(1, std::memory_order_relaxed);
    if (notifyPayloadFromCallback.exchange(false, std::memory_order_relaxed)) {
      chr.setValue("notify_test_1");
    }
  });
  notifyChr.onStatus([](BLECharacteristic &chr, BLECharacteristic::NotifyStatus status, uint32_t code) {
    notifyStatusCode = code;
    notifyStatus = status;
  });

  indicateChr = svc.createCharacteristic(INDICATE_CHAR_UUID, BLEProperty::Read | BLEProperty::Indicate, BLEPermission::ReadOpen);
  indicateChr.onNotify([](BLECharacteristic &chr) {
    indicateCbCount.fetch_add(1, std::memory_order_relaxed);
  });
  indicateChr.onStatus([](BLECharacteristic &chr, BLECharacteristic::NotifyStatus status, uint32_t code) {
    indicateStatusCode = code;
    indicateStatus = status;
    if (status == BLECharacteristic::SUCCESS_INDICATE) {
      indicateOkCount.fetch_add(1, std::memory_order_relaxed);
    } else {
      indicateErrCount.fetch_add(1, std::memory_order_relaxed);
    }
  });

  dualChr = svc.createCharacteristic(DUAL_CHAR_UUID, BLEProperty::Read | BLEProperty::Notify | BLEProperty::Indicate, BLEPermission::ReadOpen);
  dualChr.setDescription("Dual Characteristic");
  dualChr.onSubscribe([](BLECharacteristic chr, const BLEConnInfo &conn, uint16_t subValue) {
    dualBothEnabled = (subValue == 0x0003);
  });
  dualChr.onStatus([](BLECharacteristic &chr, BLECharacteristic::NotifyStatus status, uint32_t code) {
    if (status == BLECharacteristic::SUCCESS_INDICATE) {
      dualIndOk.fetch_add(1, std::memory_order_relaxed);
    } else if (status == BLECharacteristic::SUCCESS_NOTIFY) {
      dualNotifOk.fetch_add(1, std::memory_order_relaxed);
    } else {
      dualErr.fetch_add(1, std::memory_order_relaxed);
    }
  });

  snapshotChr = svc.createCharacteristic(SNAPSHOT_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  {
    static uint8_t buf[LONG_READ_LEN];
    fillLongValue(buf, 'A', 1);
    snapshotChr.setValue(buf, sizeof(buf));
  }

  auto secureChr = svc.createCharacteristic(SECURE_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadAuthenticated);
  secureChr.setValue("Secure Data!");

  // Phase 7: Descriptor test characteristic with User Description + Presentation Format
  descChr = svc.createCharacteristic(DESC_CHAR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadWriteOpen);
  descChr.setValue("DescTest");
  descChr.setDescription("Test Characteristic");

  auto pfDesc = descChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2904)), BLEPermission::ReadOpen, 7);
  pfDesc.setFormat(BLEDescriptor::FORMAT_UTF8);
  pfDesc.setExponent(-2);
  pfDesc.setUnit(0x2700);
  pfDesc.setNamespace(1);
  pfDesc.setFormatDescription(0x0042);

  // Phase 15: custom-UUID descriptor with read-only permissions. The client
  // will read it successfully and then attempt a write, which must fail at
  // the ATT layer because the declared permissions do not include Write.
  // Using a custom (non-SIG) UUID avoids triggering the reserved-UUID
  // access-profile rules so this is purely a permission-enforcement test.
  auto roDesc = descChr.createDescriptor(DESC_READONLY_UUID, BLEPermission::ReadOpen, 32);
  roDesc.setValue("ro_desc_val");

  auto dynamicDesc = descChr.createDescriptor(DYNAMIC_READ_DESC_UUID, BLEPermission::ReadOpen, 32);
  dynamicDesc.onRead([](BLEDescriptor &desc, const BLEConnInfo &conn) {
    desc.setValue(String("dyn_desc_") + String(++dynamicDescReads));
  });

  auto longDesc = descChr.createDescriptor(LONG_READ_DESC_UUID, BLEPermission::ReadOpen, LONG_READ_LEN);
  longDesc.onRead([](BLEDescriptor &desc, const BLEConnInfo &conn) {
    static uint8_t buf[LONG_READ_LEN];  // keep 512 bytes off the BLE task stack
    fillLongValue(buf, 'a', ++longDescReads);
    desc.setValue(buf, sizeof(buf));
  });

  // Phase 7: Writable custom descriptor with maxLen=510 (< 512) to exercise
  // normal descriptor writes + onWrite, Long Descriptor Writes (>MTU), and
  // Prepared Write maxLen rejection + rollback when writing 512 bytes.
  auto writableDesc = descChr.createDescriptor(WRITABLE_DESC_UUID, BLEPermission::ReadWriteOpen, 510);
  writableDesc.setValue("desc_init");
  writableDesc.onWrite([](BLEDescriptor &desc, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    (void)desc.getValue(&len);
    Serial.printf("[SERVER] Descriptor write count=%lu len=%u\n", (unsigned long)++writableDescWrites, (unsigned)len);
  });

  // Phase 8: WriteNR test characteristic
  auto writeNrChr = svc.createCharacteristic(WRITENR_CHAR_UUID, BLEProperty::Read | BLEProperty::WriteNR, BLEPermission::ReadWriteOpen);
  writeNrChr.setValue("waiting");
  writeNrChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    // Burst packets arrive back to back with no ATT response in between, which is
    // what lets a value buffer shared across queued packets show up: every
    // callback then reads the newest payload instead of its own. Report each one
    // so the host can check the three payloads stayed distinct and in order.
    if (len == writeNrBurstLen && writeNrBurstSeen < sizeof(writeNrBurstBytes)) {
      uint8_t want = writeNrBurstBytes[writeNrBurstSeen];
      bool ok = (data[0] == want && data[1] == want && data[2] == want);
      writeNrBurstOk = writeNrBurstOk && ok;
      writeNrBurstSeen++;
      Serial.printf("[SERVER] Phase8 burst %u bytes=%02X%02X%02X ok=%d\n", (unsigned)writeNrBurstSeen, data[0], data[1], data[2], (int)ok);
      if (writeNrBurstSeen == sizeof(writeNrBurstBytes)) {
        Serial.printf("[SERVER] Phase8 burst done seen=%u ok=%d\n", (unsigned)writeNrBurstSeen, (int)writeNrBurstOk);
      }
      return;
    }
    Serial.printf("[SERVER] WriteNR received: %.*s\n", (int)len, (const char *)data);
  });

  // Phase 15: Fail-closed permission masking. Declares Read|Write but only
  // grants Read, so the backend must drop the Write property from the
  // advertised characteristic and reject any write attempts.
  permFailClosedChr = svc.createCharacteristic(PERM_FAIL_CLOSED_CHAR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadOpen);
  permFailClosedChr.setValue("ro_data");
  permFailClosedChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    Serial.println("[SERVER] FailClosed write callback fired — fail-closed broken!");
  });

  // Phase 18: ReadWriteAuthorized — every read/write traps through the
  // onAuthorization handler registered above. The handler approves reads
  // and approves writes only when the payload is "OK"; this lets the
  // client-side phase walk both approve and deny branches while the
  // counters accumulated from the handler are asserted against the exact
  // number of attempts. onWrite still fires on approved writes so we can
  // confirm the value reached the char.
  authzChr = svc.createCharacteristic(AUTHZ_CHAR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadWriteAuthorized);
  authzChr.setValue("init");
  authzChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    Serial.printf("[SERVER] Authz write applied: %.*s\n", (int)len, (const char *)data);
  });

  // Phase 19 & 22: ReadWriteEncrypted. Access requires a paired link; used
  // to prove BLEConnInfo::isEncrypted/isBonded/isAuthenticated flip the
  // right way and to prove bond-delete forces a re-pair on next access.
  encryptedChr = svc.createCharacteristic(ENCRYPTED_CHAR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadWriteEncrypted);
  encryptedChr.setValue("enc_data");
  encryptedChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    Serial.printf(
      "[SERVER] Encrypted write applied: %.*s enc=%d auth=%d bond=%d\n", (int)len, (const char *)data, (int)conn.isEncrypted(), (int)conn.isAuthenticated(),
      (int)conn.isBonded()
    );
  });

  // Pure lookups must be callable through const handles (e.g. a const
  // BLEServer & parameter in user code).
  {
    const BLEServer &constServer = server;
    const BLEService constSvc = constServer.getService(SERVICE_UUID);
    const BLECharacteristic constChr = constSvc.getCharacteristic(DESC_CHAR_UUID);
    bool ok = constSvc && constChr && constChr.getDescriptor(DYNAMIC_READ_DESC_UUID) && constServer.getAdvertising();
    Serial.printf("[SERVER] Const lookups ok=%d\n", (int)ok);

    // const on a handle is shallow: a lookup through a const handle returns a
    // regular handle to the same shared attribute, so writing through it is
    // visible from every other handle (and to the client in Phase 4).
    BLECharacteristic fromConst = constSvc.getCharacteristic(CONST_LOOKUP_CHAR_UUID);
    fromConst.setValue("set_via_const_lookup");
    bool shared = server.getService(SERVICE_UUID).getCharacteristic(CONST_LOOKUP_CHAR_UUID).getStringValue() == "set_via_const_lookup";
    Serial.printf("[SERVER] Const lookup handle writable=%d\n", (int)shared);
  }

  server.start();

  size_t heapAfterServer = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[SERVER] Heap after server: %u\n", (unsigned)heapAfterServer);
  Serial.println("[SERVER] Server started");

  // Not advertising yet, so nobody is connected: broadcasts must report
  // ERROR_NO_CLIENT (not ERROR_NO_SUBSCRIBER) without calling onNotify.
  {
    uint32_t notifyBefore = notifyCbCount;
    uint32_t indicateBefore = indicateCbCount;
    notifyChr.notify();
    indicateChr.indicate();
    Serial.printf("[SERVER] No-client notify status=%s onNotify=%lu\n", notifyStatusName(notifyStatus), (unsigned long)(notifyCbCount - notifyBefore));
    Serial.printf("[SERVER] No-client indicate status=%s onNotify=%lu\n", notifyStatusName(indicateStatus), (unsigned long)(indicateCbCount - indicateBefore));
    notifyStatus = -1;
    indicateStatus = -1;
    indicateErrCount = 0;
  }

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SERVICE_UUID);
  adv.start();
  Serial.println("[SERVER] Advertising started");
  return true;
}

// =============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();

  waitForPhase(1);
  if (!phase_basic()) {
    return;
  }

  waitForPhase(2);
  {
    bool ble5_ok = false;
#if BLE5_SUPPORTED
    ble5_ok = phase_ble5_adv();
#endif
    if (!ble5_ok) {
      Serial.println("[SERVER] BLE5 not supported, skipping");
    }
  }

  waitForPhase(3);
  if (!phase_gatt_setup()) {
    return;
  }
}

void loop() {
  checkSerial();

  // Phase 10: clear bond store so encrypted read forces a fresh pairing (passkey
  // lines). Runs once on the first loop after START_PHASE_10; disconnect first
  // so the stack can drop the old link before ble_store is cleared.
  if (currentPhase >= 10 && !phase10BondsCleared) {
    phase10BondsCleared = true;
    syncPhaseFromHost();
    {
      BLEServer srv = BLE.createServer();
      for (const auto &c : srv.getConnections()) {
        (void)srv.disconnect(c.getHandle());
      }
    }
    delay(200);
    BLESecurity se = BLE.getSecurity();
    if (se) {
      (void)se.deleteAllBonds();
    }
  }

  if (currentPhase == 4) {
    static uint8_t buf[LONG_READ_LEN];
    uint32_t n = 1;
    while (currentPhase == 4) {
      if (longChrMutatePending.exchange(false, std::memory_order_acq_rel)) {
        // Wait briefly so the BTC/NimBLE host task finishes snapshotting the
        // offset=0 read response after onRead returns before overwriting the
        // value prior to the peer's follow-up Read Blob request.
        delay(5);
        memset(buf, 'Z', sizeof(buf));
        longChr.setValue(buf, sizeof(buf));
      }
      fillLongValue(buf, 'A', ++n);
      snapshotChr.setValue(buf, sizeof(buf));
      delay(2);
      checkSerial();
    }
  }

  static unsigned long notifyTime = 0;

  // Phase 5: Send notification then indication. The notification payload comes
  // from onNotify: the client receiving "notify_stale" instead of
  // "notify_test_1" means onNotify ran after the value was captured.
  if (currentPhase >= 5 && notifyStep == 0 && notifyChr.getSubscribedCount() > 0) {
    notifyChr.setValue("notify_stale");
    notifyPayloadFromCallback = true;
    BTStatus s = notifyChr.notify();
    if (s) {
      Serial.println("[SERVER] Notification sent");
    }
    notifyStep = 1;
    notifyTime = millis();
  }
  if (notifyStep >= 1 && !notifyStatusLogged && notifyStatus >= 0) {
    notifyStatusLogged = true;
    Serial.printf(
      "[SERVER] Notify status=%s code=%lu onNotify=%lu\n", notifyStatusName(notifyStatus), (unsigned long)notifyStatusCode, (unsigned long)notifyCbCount
    );
  }
  if (notifyStep == 1 && millis() - notifyTime >= 1000 && indicateChr.getSubscribedCount() > 0) {
    indicateChr.setValue("indicate_test_1");
    BTStatus s = indicateChr.indicate();
    if (s) {
      Serial.println("[SERVER] Indication sent");
    }
    notifyStep = 2;
  }
  if (notifyStep >= 2 && !indicateStatusLogged && indicateStatus >= 0) {
    indicateStatusLogged = true;
    Serial.printf(
      "[SERVER] Indicate status=%s code=%lu onNotify=%lu\n", notifyStatusName(indicateStatus), (unsigned long)indicateStatusCode, (unsigned long)indicateCbCount
    );
  }
  if (indicateStatusLogged && !indicateBurstSent && indicateChr.getSubscribedCount() > 0) {
    indicateBurstSent = true;
    indicateOkCount = 0;
    indicateErrCount = 0;
    int accepted = 0;
    for (int i = 1; i <= INDICATE_BURST; i++) {
      char payload[16];
      int len = snprintf(payload, sizeof(payload), "burst_%d", i);
      if (indicateChr.indicate(reinterpret_cast<const uint8_t *>(payload), len)) {
        accepted++;
      }
    }
    Serial.printf("[SERVER] Indicate burst accepted=%d\n", accepted);
    indicateBurstTime = millis();
  }
  if (indicateBurstSent && !indicateBurstLogged && (indicateOkCount + indicateErrCount >= INDICATE_BURST || millis() - indicateBurstTime >= 10000)) {
    indicateBurstLogged = true;
    Serial.printf("[SERVER] Indicate burst confirmed=%lu failed=%lu\n", (unsigned long)indicateOkCount, (unsigned long)indicateErrCount);
  }
  // Queue capacity is 1 in-flight + 8 queued. Twelve back-to-back indicates
  // must accept at least 8 and refuse the rest with Busy.
  if (indicateBurstLogged && !indicateOverflowSent && indicateChr.getSubscribedCount() > 0) {
    indicateOverflowSent = true;
    indicateOkCount = 0;
    indicateErrCount = 0;
    indicateOverflowAccepted = 0;
    indicateOverflowBusy = 0;
    for (int i = 1; i <= INDICATE_OVERFLOW; i++) {
      char payload[16];
      int len = snprintf(payload, sizeof(payload), "ovf_%d", i);
      BTStatus s = indicateChr.indicate(reinterpret_cast<const uint8_t *>(payload), len);
      if (s == BTStatus::Busy) {
        indicateOverflowBusy++;
      } else if (s) {
        indicateOverflowAccepted++;
      }
    }
    indicateOverflowTime = millis();
  }
  if (indicateOverflowSent && !indicateOverflowLogged
      && (indicateOkCount + indicateErrCount >= (uint32_t)(indicateOverflowAccepted + indicateOverflowBusy) || millis() - indicateOverflowTime >= 10000)) {
    indicateOverflowLogged = true;
    Serial.printf(
      "[SERVER] Indicate overflow accepted=%d busy=%d confirmed=%lu failed=%lu\n", indicateOverflowAccepted, indicateOverflowBusy,
      (unsigned long)indicateOkCount, (unsigned long)indicateErrCount
    );
  }
  // Every notification after the first indication is sent while it is still
  // unconfirmed, on the same characteristic and connection.
  if (indicateOverflowLogged && !dualSent && dualBothEnabled) {
    dualSent = true;
    for (int i = 1; i <= DUAL_BURST; i++) {
      char payload[16];
      int len = snprintf(payload, sizeof(payload), "di_%d", i);
      dualChr.indicate(reinterpret_cast<const uint8_t *>(payload), len);
      len = snprintf(payload, sizeof(payload), "dn_%d", i);
      dualChr.notify(reinterpret_cast<const uint8_t *>(payload), len);
    }
    dualTime = millis();
  }
  if (dualSent && !dualLogged && (dualIndOk + dualNotifOk + dualErr >= 2 * DUAL_BURST || millis() - dualTime >= 10000)) {
    dualLogged = true;
    Serial.printf("[SERVER] Dual confirmed ind=%lu notif=%lu failed=%lu\n", (unsigned long)dualIndOk, (unsigned long)dualNotifOk, (unsigned long)dualErr);
  }
  // Phase 6 start: the client unsubscribed at the end of Phase 5, so a
  // broadcast notify() must report ERROR_NO_SUBSCRIBER without calling onNotify.
  if (currentPhase >= 6 && notifyStep >= 2 && !noSubscriberChecked && notifyChr.getSubscribedCount() == 0) {
    noSubscriberChecked = true;
    notifyStatus = -1;
    uint32_t before = notifyCbCount;
    notifyChr.notify();
    Serial.printf(
      "[SERVER] No-subscriber notify status=%s onNotify=%lu subEvents=%lu\n", notifyStatusName(notifyStatus), (unsigned long)(notifyCbCount - before),
      (unsigned long)notifySubEvents.load(std::memory_order_relaxed)
    );
  }

  // Phase 9: Server-initiated disconnect
  if (currentPhase >= 9 && !serverDisconnectDone) {
    serverDisconnectDone = true;
    BLEServer server = BLE.createServer();
    auto conns = server.getConnections();
    if (!conns.empty()) {
      BTStatus s = server.disconnect(conns[0].getHandle());
      if (s) {
        Serial.println("[SERVER] Server-initiated disconnect OK");
      } else {
        Serial.println("[SERVER] Server-initiated disconnect FAILED");
      }
    }
  }

  // Phase 13: BLEStream (server side)
  if (currentPhase >= 13 && !bleStreamInitDone) {
    bleStreamInitDone = true;
    BLEServer malformedServer = BLE.createServer();
    BLEService malformedNus = malformedServer.createService(BLEStream::nusServiceUUID());
    malformedNus.createCharacteristic(BLEStream::nusTxCharUUID(), BLEProperty::Read, BLEPermission::ReadOpen).setValue("no_cccd");
    malformedNus.createCharacteristic(
      BLEStream::nusRxCharUUID(), BLEProperty::Write | BLEProperty::WriteNR, BLEPermission::WriteOpen
    );
    BLEAdvertising malformedAdv = BLE.getAdvertising();
    malformedAdv.stop();
    BTStatus malformedStart = malformedServer.start();
    malformedAdv.reset();
    malformedAdv.addServiceUUID(BLEStream::nusServiceUUID());
    BTStatus malformedAdvStart = malformedAdv.start();
    Serial.printf("[SERVER] BLEStream malformed NUS ready=%d\n", (int)(malformedStart && malformedAdvStart));
    BLEStream conflictStream;
    BTStatus conflictStatus = conflictStream.begin(serverName);
    Serial.printf("[SERVER] BLEStream existing NUS rejected=%d\n", (int)(conflictStatus == BTStatus::Busy));
    while (!phase13Continue) {
      checkSerial();
      delay(10);
    }
    malformedAdv.stop();
    BTStatus malformedRemoved = malformedServer.removeService(malformedNus);
    Serial.printf("[SERVER] BLEStream malformed NUS removed=%d\n", (int)(bool)malformedRemoved);

    bleStream.onConnect([](const BLEConnInfo &connInfo) {
      syncPhaseFromHost();
      bleStreamConnectCb = true;
    });
    bleStream.onDisconnect([](const BLEConnInfo &connInfo, uint8_t reason) {
      syncPhaseFromHost();
      bleStreamDisconnectCb = true;
    });
    bleStream.onPeerData([](const BLEConnInfo &peer, const uint8_t *data, size_t len) {
      syncPhaseFromHost();
      if (len >= 19 && memcmp(data, "stream_write_to_probe", 19) == 0) {
        Serial.printf("[SERVER] BLEStream onPeerData: %s\n", peer.getAddress().toString().c_str());
        bleStream.writeTo(peer, (const uint8_t *)"STREAM_WRITE_TO_ACK\n", 20);
        bleStreamWriteToDone = true;
      } else if (len >= 7 && memcmp(data, "end_now", 7) == 0) {
        bleStreamEndRequested = true;
      }
    });
    BTStatus s = bleStream.begin(serverName);
    if (s) {
      Serial.println("[SERVER] BLEStream init OK");
    } else {
      Serial.printf("[SERVER] BLEStream init FAILED: %s\n", s.toString());
    }
  }

  if (currentPhase >= 13 && bleStreamInitDone && !bleStreamPhaseDone) {
    // Step 0: Wait for onConnect callback
    if (bleStreamStep == 0 && bleStreamConnectCb) {
      Serial.println("[SERVER] BLEStream onConnect fired");
      if (bleStream.connected()) {
        Serial.println("[SERVER] BLEStream connected OK");
        Serial.printf("[SERVER] BLEStream peerCount=%u\n", bleStream.peerCount());
        auto peerList = bleStream.peers();
        Serial.printf("[SERVER] BLEStream peers=%u\n", (unsigned)peerList.size());
        if (!peerList.empty()) {
          Serial.printf("[SERVER] BLEStream peerAddr=%s\n", peerList[0].getAddress().toString().c_str());
        }
      }
      bleStreamStep = 1;
    }

    // Step 1: writeTo round-trip via onData
    if (bleStreamStep == 1 && bleStreamWriteToDone) {
      Serial.println("[SERVER] BLEStream writeTo ack sent");
      bleStreamStep = 2;
    }

    // Step 2: Receive "stream_ping", reply "STREAM_PONG"
    if (bleStreamStep == 2 && bleStream.available()) {
      while (bleStream.available()) {
        int c = bleStream.read();
        if (c < 0) {
          break;
        }
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          bleStreamRxLine.trim();
          if (bleStreamRxLine.length() == 0 || bleStreamRxLine == "stream_write_to_probe") {
            bleStreamRxLine = "";
            continue;
          }
          if (bleStreamRxLine != "stream_ping") {
            bleStreamRxLine = "";
            continue;
          }
          Serial.printf("[SERVER] BLEStream received: %s\n", bleStreamRxLine.c_str());
          bleStream.println("STREAM_PONG");
          Serial.println("[SERVER] BLEStream reply sent");
          bleStreamRxLine = "";
          bleStreamStep = 3;
          break;
        }
        bleStreamRxLine += (char)c;
      }
    }

    // Step 3: peek() test — peek first byte, then read full "peek_test" line
    if (bleStreamStep == 3 && bleStream.available()) {
      if (!bleStreamPeekDone) {
        int p = bleStream.peek();
        Serial.printf("[SERVER] BLEStream peek: %d\n", p);
        bleStreamPeekDone = true;
      }
      while (bleStream.available()) {
        int c = bleStream.read();
        if (c < 0) {
          break;
        }
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          bleStreamRxLine.trim();
          if (bleStreamRxLine != "peek_test") {
            bleStreamRxLine = "";
            continue;
          }
          Serial.printf("[SERVER] BLEStream peek read: %s\n", bleStreamRxLine.c_str());
          bleStreamRxLine = "";
          bleStreamStep = 4;
          break;
        }
        bleStreamRxLine += (char)c;
      }
    }

    // Step 4: Receive bulk data (200-char pattern), verify integrity
    if (bleStreamStep == 4 && bleStream.available()) {
      while (bleStream.available()) {
        int c = bleStream.read();
        if (c < 0) {
          break;
        }
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          bleStreamRxLine.trim();
          if (bleStreamRxLine.length() != 200) {
            bleStreamRxLine = "";
            continue;
          }
          size_t len = bleStreamRxLine.length();
          Serial.printf("[SERVER] BLEStream bulk received: %u bytes\n", (unsigned)len);
          bool ok = true;
          for (size_t i = 0; i < len && ok; i++) {
            if (bleStreamRxLine[i] != (char)('A' + (i % 26))) {
              ok = false;
            }
          }
          Serial.println(ok ? "[SERVER] BLEStream bulk integrity OK" : "[SERVER] BLEStream bulk integrity FAILED");
          bleStreamRxLine = "";
          bleStreamStep = 5;
          break;
        }
        bleStreamRxLine += (char)c;
      }
    }

    // Step 5: Server-initiated write to client
    if (bleStreamStep == 5) {
      bleStream.println("server_says_hi");
      Serial.println("[SERVER] BLEStream server msg sent");
      bleStreamStep = 6;
    }

    // Step 6: end() while the peer is still connected. It must block through
    // disconnect callback delivery, stop advertising, remove NUS, and permit
    // a clean re-begin without disturbing the BLE stack.
    if (bleStreamStep == 6 && bleStreamEndRequested) {
      bleStream.end();
      Serial.printf("[SERVER] BLEStream onDisconnect fired=%d\n", (int)bleStreamDisconnectCb);
      delay(100);
      BLEAdvertising stoppedAdv = BLE.getAdvertising();
      Serial.printf("[SERVER] BLEStream end advertisingStopped=%d\n", (int)!stoppedAdv.isAdvertising());
      BTStatus restart = bleStream.begin(serverName);
      Serial.printf("[SERVER] BLEStream rebegin ok=%d\n", (int)(bool)restart);
      bleStreamPhaseDone = true;
    }
  }

  // Phase 14: L2CAP CoC (server side)
  if (currentPhase >= 14 && !l2capInitDone) {
#if BLE_L2CAP_SUPPORTED
    l2capInitDone = true;
    l2capServer = BLE.createL2CAPServer(0x0080, 128);
    if (!l2capServer) {
      Serial.println("[SERVER] L2CAP init FAILED");
    } else {
      l2capServer.onAccept([](BLEL2CAPChannel channel) {
        syncPhaseFromHost();
        l2capChannel = channel;
        Serial.println("[SERVER] L2CAP channel accepted");
      });
      l2capServer.onData([](BLEL2CAPChannel &channel, const uint8_t *data, size_t len) {
        syncPhaseFromHost();
        String msg((const char *)data, len);
        Serial.printf("[SERVER] L2CAP received: %s\n", msg.c_str());
        const char *reply = "L2CAP_OK";
        BTStatus s = channel.write((const uint8_t *)reply, strlen(reply));
        if (s) {
          Serial.println("[SERVER] L2CAP reply sent");
        } else {
          Serial.printf("[SERVER] L2CAP reply FAILED: %s\n", s.toString());
        }
      });
      Serial.println("[SERVER] L2CAP init OK");
    }
#else
    l2capInitDone = true;
    Serial.println("[SERVER] L2CAP not supported, skipping");
#endif
  }

  // Phase 15: Construction-time descriptor permission validation.
  //
  // Runs bleValidateDescProps through BLECharacteristic::createDescriptor for
  // a handful of malformed specs and verifies each one is rejected (empty
  // BLEDescriptor handle → `!desc` is true). These calls happen after
  // server.start() has already registered the live GATT table, so any
  // descriptor that *would* have been accepted here won't be advertised to
  // the peer — that's intentional: this block tests the validator itself,
  // independent of the live GATT state. The runtime enforcement counterpart
  // (reading/writing the read-only custom descriptor on DESC_CHAR) is
  // driven by the client.
  if (currentPhase >= 15 && !descPermValidationDone) {
    descPermValidationDone = true;
    Serial.println("[SERVER] DescPerm validation start");

    // 1. BLEPermission::None — fail-closed: a descriptor with no access is
    //    inaccessible by construction and must be rejected.
    auto d1 = permFailClosedChr.createDescriptor(BLEUUID("a0000000-0000-4000-8000-000000000001"), BLEPermission::None);
    Serial.printf("[SERVER] DescPerm None rejected: %d\n", (int)(!d1));

    // 2. Presentation Format (0x2904) declared writable — the spec says
    //    this descriptor must be read-only.
    auto d2 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2904)), BLEPermission::ReadOpen | BLEPermission::WriteOpen, 7);
    Serial.printf("[SERVER] DescPerm 0x2904+Write rejected: %d\n", (int)(!d2));

    // 3. Extended Properties (0x2900) with Write — must be read-only.
    auto d3 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2900)), BLEPermission::WriteOpen, 2);
    Serial.printf("[SERVER] DescPerm 0x2900+Write rejected: %d\n", (int)(!d3));

    // 4. CCCD (0x2902) with read-only — must have both R+W so clients can
    //    subscribe.
    auto d4 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2902)), BLEPermission::ReadOpen, 2);
    Serial.printf("[SERVER] DescPerm 0x2902 read-only rejected: %d\n", (int)(!d4));

    // 5. User Description (0x2901) with no read permission — must be
    //    readable (even when writable is gated by 0x2900).
    auto d5 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)), BLEPermission::WriteOpen, 16);
    Serial.printf("[SERVER] DescPerm 0x2901 no-read rejected: %d\n", (int)(!d5));

    // 6. WriteAuthorized without any write direction — authorization must
    //    attach to a base direction.
    auto d6 = permFailClosedChr.createDescriptor(BLEUUID("a0000000-0000-4000-8000-000000000002"), BLEPermission::ReadOpen | BLEPermission::WriteAuthorized);
    Serial.printf("[SERVER] DescPerm WriteAuthorized-no-write rejected: %d\n", (int)(!d6));

    // 7. Control: a valid descriptor spec must NOT be rejected. Uses a
    //    custom UUID so no reserved-UUID rule fires.
    auto d7 = permFailClosedChr.createDescriptor(BLEUUID("a0000000-0000-4000-8000-000000000003"), BLEPermission::ReadOpen);
    Serial.printf("[SERVER] DescPerm valid accepted: %d\n", (int)((bool)d7));

    // Characteristic construction-time validation (bleValidateCharProps) and
    // included-service removeService() Busy guard.
    BLEServer valSrv = BLE.createServer();
    BLEService valSvc = valSrv.createService(BLEUUID("a0000000-0000-4000-8000-000000000010"));
    auto c1 = valSvc.createCharacteristic(BLEUUID("a0000000-0000-4000-8000-000000000011"), static_cast<BLEProperty>(0), BLEPermission::ReadOpen);
    auto c2 = valSvc.createCharacteristic(BLEUUID("a0000000-0000-4000-8000-000000000012"), BLEProperty::Write, BLEPermission::ReadOpen);
    auto c3 = valSvc.createCharacteristic(BLEUUID("a0000000-0000-4000-8000-000000000013"), BLEProperty::Read, BLEPermission::WriteOpen);
    auto c4 = valSvc.createCharacteristic(BLEUUID("a0000000-0000-4000-8000-000000000014"), BLEProperty::Read, BLEPermission::WriteAuthorized);
    auto c5 = valSvc.createCharacteristic(BLEUUID("a0000000-0000-4000-8000-000000000015"), BLEProperty::Read, BLEPermission::ReadOpen);
    int charRej = (int)(!c1) + (int)(!c2) + (int)(!c3) + (int)(!c4);
    (void)valSrv.removeService(valSvc);
    Serial.printf("[SERVER] CharProp validation rejected=%d/4 validAccepted=%d\n", charRej, (int)((bool)c5));

    BLEService incTarget = valSrv.createService(BLEUUID("a0000000-0000-4000-8000-000000000016"));
    BLEService incHost = valSrv.createService(BLEUUID("a0000000-0000-4000-8000-000000000017"));
    incHost.addIncludedService(incTarget);
    BTStatus remBusy = valSrv.removeService(incTarget);
    BTStatus remHost = valSrv.removeService(incHost);
    BTStatus remTarget = valSrv.removeService(incTarget);
    BTStatus remAgain = valSrv.removeService(incTarget);
    BTStatus remNull = valSrv.removeService(BLEService());
    Serial.printf(
      "[SERVER] IncludedService removeBusy=%d removeOrderOk=%d removeInvalidOk=%d\n", (int)(remBusy == BTStatus::Busy), (int)((bool)remHost && (bool)remTarget),
      (int)(remAgain == BTStatus::InvalidState && remNull == BTStatus::InvalidState)
    );

    BLEDescriptor udHelper = BLEDescriptor::createUserDescription("ud_init");
    udHelper.setUserDescription("ud_updated");
    BLEDescriptor cccdHelper = BLEDescriptor::createCCCD();
    cccdHelper.setNotifications(true);
    cccdHelper.setIndications(true);
    BLEDescriptor pfHelper = BLEDescriptor::createPresentationFormat();
    pfHelper.setFormat(BLEDescriptor::FORMAT_UINT16);
    pfHelper.setExponent(-3);
    pfHelper.setUnit(0x2703);
    pfHelper.setNamespace(1);
    pfHelper.setFormatDescription(0x0010);
    BLEDescriptor nullDesc;
    bool helpersOk = udHelper.isUserDescription() && !udHelper.isCCCD() && udHelper.getUserDescription() == "ud_updated" && cccdHelper.isCCCD()
                     && cccdHelper.getNotifications() && cccdHelper.getIndications() && pfHelper.isPresentationFormat() && pfHelper.getLength() == 7
                     && pfHelper.getFormat() == BLEDescriptor::FORMAT_UINT16 && pfHelper.getExponent() == -3 && pfHelper.getUnit() == 0x2703
                     && pfHelper.getNamespace() == 1 && pfHelper.getFormatDescription() == 0x0010 && nullDesc.getLength() == 0 && !nullDesc.getNotifications()
                     && dualChr.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2901))).getUserDescription() == "Dual Characteristic";
    Serial.printf("[SERVER] DescriptorHelpers ok=%d\n", (int)helpersOk);

    Serial.println("[SERVER] DescPerm validation done");
  }

  // =========================================================================
  // Phase 16: Connection info + MTU / conn-params callbacks
  // =========================================================================
  // The client reconnects for this phase and issues an updateConnParams
  // request. By the time we observe phase16 on the host, both the MTU and
  // conn-params callbacks should have fired at least once (MTU during
  // reconnect, conn-params after the client's request round-trips through
  // the controller). We log them here rather than from the callbacks to
  // guarantee ordering after the phase start marker.
  if (currentPhase >= 16 && !phase16Done) {
    phase16Done = true;
    // Clear any MTU state left over from earlier phases so the seen/last we
    // report reflect ONLY this phase's reconnect exchange. The peer is
    // disconnected entering phase 16 and reconnects below, so the fresh
    // onMtuChanged fires after this reset.
    phase16MtuSeen = false;
    phase16LastMtu = 0;
    Serial.println("[SERVER] Phase16 waiting for peer");
    unsigned long startWait = millis();
    while ((millis() - startWait) < 10000) {
      BLEServer srv = BLE.createServer();
      if (srv.getConnectedCount() > 0) {
        break;
      }
      delay(50);
    }
    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    if (conns.empty()) {
      Serial.println("[SERVER] Phase16 no peer connected");
    } else {
      const BLEConnInfo &c = conns[0];
      uint16_t peerMtu = srv.getPeerMTU(c.getHandle());
      Serial.printf("[SERVER] ConnInfo handle=%u mtu=%u\n", (unsigned)c.getHandle(), (unsigned)c.getMTU());
      Serial.printf("[SERVER] ConnInfo peerMtu=%u\n", (unsigned)peerMtu);
      Serial.printf(
        "[SERVER] ConnInfo sec enc=%d auth=%d bond=%d keySize=%u\n", (int)c.isEncrypted(), (int)c.isAuthenticated(), (int)c.isBonded(),
        (unsigned)c.getSecurityKeySize()
      );
      Serial.printf("[SERVER] ConnInfo role central=%d peripheral=%d\n", (int)c.isCentral(), (int)c.isPeripheral());
      Serial.printf(
        "[SERVER] ConnInfo params interval=%u latency=%u timeout=%u\n", (unsigned)c.getInterval(), (unsigned)c.getLatency(), (unsigned)c.getSupervisionTimeout()
      );
      BTAddress ota = c.getAddress();
      BTAddress id = c.getIdAddress();
      Serial.printf("[SERVER] ConnInfo addr ota=%s id=%s\n", ota.toString().c_str(), id.toString().c_str());
      // Wait up to 10 s for the client's updateConnParams to round-trip. The
      // controller may take several intervals before the new values propagate.
      unsigned long paramWait = millis();
      while (!phase16ConnParamsSeen && (millis() - paramWait) < 10000) {
        delay(50);
      }
      Serial.printf("[SERVER] MtuChanged seen=%d last=%u\n", (int)phase16MtuSeen, (unsigned)phase16LastMtu);
      Serial.printf(
        "[SERVER] ConnParamsUpdate seen=%d interval=%u latency=%u timeout=%u\n", (int)phase16ConnParamsSeen, (unsigned)phase16LastInterval,
        (unsigned)phase16LastLatency, (unsigned)phase16LastTimeout
      );

      // getConnInfo(handle) on-demand refresh check. The snapshot 'c' above was
      // taken before the MTU exchange and conn-params update round-tripped, so a
      // FRESH getConnInfo() for the same handle must show the new MTU/timeout,
      // while a bogus handle must yield an invalid (false) snapshot. This must
      // behave identically on NimBLE and Bluedroid (shared connection table).
      uint16_t h = c.getHandle();
      BLEConnInfo fresh = srv.getConnInfo(h);
      BLEConnInfo bogus = srv.getConnInfo(0xFFFF);
      bool freshValid = (bool)fresh && fresh.getHandle() == h;
      // Fresh snapshot must match the MTU/params the callbacks observed (above
      // the 23-byte default), proving it refreshed rather than returning stale.
      bool refreshedMtu = (fresh.getMTU() == phase16LastMtu) && (phase16LastMtu > 23);
      bool refreshedParams = !phase16ConnParamsSeen || fresh.getSupervisionTimeout() == phase16LastTimeout;
      Serial.printf(
        "[SERVER] Phase16 getConnInfo valid=%d mtu=%u timeout=%u bogusValid=%d refreshedMtu=%d refreshedParams=%d\n", (int)freshValid, (unsigned)fresh.getMTU(),
        (unsigned)fresh.getSupervisionTimeout(), (int)(bool)bogus, (int)refreshedMtu, (int)refreshedParams
      );
    }
    Serial.println("[SERVER] Phase16 done");
  }

  // =========================================================================
  // Phase 17: Address types — API surface + connection roundtrip per type
  // =========================================================================
  // Two halves:
  //   (a) API-surface coverage — call BLE.setOwnAddressType with every
  //       BTAddress::Type enum value and log the BTStatus.
  //   (b) Connection roundtrip — for Public + Random, stop advertising,
  //       switch the own-address type, restart adv with a per-type nonce
  //       in the scan response, and wait for the client to scan + connect
  //       + disconnect. This proves the full stack propagates the type
  //       through adv PDUs, not just the API.
  //
  // Types that the controller rejects (typically PublicID/RandomID when
  // LL privacy is off) are still exercised in the API-surface pass so the
  // return value is observable; the connection roundtrip covers the two
  // types that are guaranteed to work on every ESP32 variant.
  if (currentPhase >= 17 && !phase17Done) {
    phase17Done = true;
    BTAddress self = BLE.getAddress();
    Serial.printf("[SERVER] OwnAddress addr=%s type=%u\n", self.toString().c_str(), (unsigned)self.type());

    // (a) API-surface coverage over every enum value.
    BTStatus s1 = BLE.setOwnAddressType(BTAddress::Public);
    Serial.printf("[SERVER] setOwnAddressType(Public) ok=%d\n", (int)(bool)s1);
    BTStatus s2 = BLE.setOwnAddressType(BTAddress::Random);
    Serial.printf("[SERVER] setOwnAddressType(Random) ok=%d\n", (int)(bool)s2);
    BTStatus s3 = BLE.setOwnAddressType(BTAddress::PublicID);
    Serial.printf("[SERVER] setOwnAddressType(PublicID) ok=%d\n", (int)(bool)s3);
    BTStatus s4 = BLE.setOwnAddressType(BTAddress::RandomID);
    Serial.printf("[SERVER] setOwnAddressType(RandomID) ok=%d\n", (int)(bool)s4);

    // Drop any existing connection before swapping the identity type.
    {
      BLEServer sv = BLE.createServer();
      auto cs = sv.getConnections();
      for (auto &c : cs) {
        (void)sv.disconnect(c.getHandle());
      }
    }
    delay(500);

    // (b) Connection roundtrip per type.
    struct TypeSpec {
      const char *name;
      BTAddress::Type type;
    };
    const TypeSpec types[] = {
      {"Public", BTAddress::Public},
      {"Random", BTAddress::Random},
    };
    BLEAdvertising adv = BLE.getAdvertising();
    for (const auto &ts : types) {
      adv.stop();
      delay(150);
      BTStatus st = BLE.setOwnAddressType(ts.type);
      Serial.printf("[SERVER] Phase17 set type=%s ok=%d\n", ts.name, (int)(bool)st);

      // Per-type name in the primary ADV payload lets the client
      // unambiguously pair a scan hit with the current type.
      adv.reset();
      adv.setName(String("T17_") + ts.name + "_" + serverName);
      BTStatus as = adv.start();
      Serial.printf("[SERVER] Phase17 adv type=%s ok=%d\n", ts.name, (int)(bool)as);

      // Wait up to 12s for the client to connect+disconnect using this type.
      unsigned long startWait = millis();
      bool observedConnect = false;
      bool observedDisconnect = false;
      uint16_t lastHandle = 0;
      while ((millis() - startWait) < 12000) {
        BLEServer sv = BLE.createServer();
        auto cs = sv.getConnections();
        if (!cs.empty()) {
          if (!observedConnect) {
            observedConnect = true;
            lastHandle = cs[0].getHandle();
          }
        } else if (observedConnect) {
          observedDisconnect = true;
          break;
        }
        delay(50);
      }
      Serial.printf(
        "[SERVER] Phase17 type=%s connObserved=%d discObserved=%d handle=%u\n", ts.name, (int)observedConnect, (int)observedDisconnect, (unsigned)lastHandle
      );
    }

    // Restore a stable, connectable default for subsequent phases. The
    // controller-selected default (`self.type()`) is preferred, falling
    // back to Public if that type is somehow rejected here.
    adv.stop();
    delay(150);
    BTStatus rst = BLE.setOwnAddressType(self.type());
    if (!rst) {
      (void)BLE.setOwnAddressType(BTAddress::Public);
    }
    adv.reset();
    adv.setType(BLEAdvType::ConnectableScannable);
    adv.addServiceUUID(SERVICE_UUID);
    (void)adv.start();

    Serial.println("[SERVER] Phase17 done");
  }

  // =========================================================================
  // Phase 18: Authorization handler — runtime access gating
  // =========================================================================
  // The client reads the AUTHZ char (approved), then writes "OK" (approved),
  // then writes "NO" (denied by handler — controller returns an ATT error
  // and onWrite must NOT fire for the denied attempt). We log the counters
  // that the handler incremented from the stack task. Because reads and
  // writes are driven by the client, this phase just samples the counters
  // once the client signals it has finished exercising them.
  if (currentPhase >= 18 && !phase18Started) {
    phase18Started = true;
    Serial.println("[SERVER] Phase18 authz ready");
    // Observer loop: let the client drive for up to 15 s, then report.
    unsigned long startObs = millis();
    uint32_t lastReads = 0, lastApprovals = 0, lastDenials = 0;
    while ((millis() - startObs) < 15000) {
      if (authzReadApprovals != lastReads || authzWriteApprovals != lastApprovals || authzWriteDenials != lastDenials) {
        lastReads = authzReadApprovals;
        lastApprovals = authzWriteApprovals;
        lastDenials = authzWriteDenials;
        // Wait a little longer for the client to finish any remaining ops.
        delay(500);
      }
      // Exit early once we've seen at least one of each.
      if (authzReadApprovals >= 1 && authzWriteApprovals >= 1 && authzWriteDenials >= 1) {
        delay(500);
        break;
      }
      delay(50);
    }
    Serial.printf(
      "[SERVER] Authz counters reads=%lu writesOK=%lu writesDeny=%lu\n", (unsigned long)authzReadApprovals, (unsigned long)authzWriteApprovals,
      (unsigned long)authzWriteDenials
    );
    Serial.printf("[SERVER] Authz lastAttrHandle=%u\n", (unsigned)authzLastAttrHandle);
    Serial.println("[SERVER] Phase18 done");
  }

  // =========================================================================
  // Phase 19: Encrypted-permission enforcement
  // =========================================================================
  // Link has been paired since phase 10; the client reads + writes the
  // encrypted char and we log the current connection security view from
  // the server side. This is the positive-control pairing exists, the
  // encrypted ops succeed.
  if (currentPhase >= 19 && !phase19Done) {
    phase19Done = true;
    // The client reconnects asynchronously for this phase. Poll for a peer
    // before sampling so we don't race the connection handshake; otherwise
    // getConnections() can be empty even though the client is about to
    // complete the setup. A 10s budget is generous enough for pairing
    // restoration over a flaky radio environment.
    // BLEConnInfo is a point-in-time snapshot, and the client tears the link
    // down as soon as it finishes the encrypted ops. Poll from the moment the
    // peer connects and latch the security view the instant encryption is in
    // place, so we report the true post-pairing state rather than a
    // pre-encryption capture. A 10s budget covers pairing over a flaky radio.
    unsigned long startWait = millis();
    bool havePeer = false;
    bool enc = false, auth = false, bonded = false;
    while ((millis() - startWait) < 10000) {
      BLEServer srv = BLE.createServer();
      auto conns = srv.getConnections();
      if (!conns.empty()) {
        havePeer = true;
        BLEConnInfo ci = conns[0];
        if (ci.isEncrypted()) {
          enc = true;
          auth = ci.isAuthenticated();
          bonded = ci.isBonded();
          break;
        }
      }
      delay(20);
    }
    if (!havePeer) {
      Serial.println("[SERVER] Phase19 no peer connected");
    } else {
      Serial.printf("[SERVER] Phase19 encSec enc=%d auth=%d bond=%d\n", (int)enc, (int)auth, (int)bonded);
    }
    Serial.println("[SERVER] Phase19 done");
  }

  // =========================================================================
  // Phase 20: Advertisement data builders + scan tuning (server side)
  // =========================================================================
  // Requires tearing down the current connection: building a new legacy adv
  // payload with every BLEAdvertisementData setter, pushing it via
  // setAdvertisementData(), and letting the client scan+parse it. A second
  // window (OSZ_<name>) carries a truncated manufacturer AD for the
  // parsePayload bounds regression (issue #12801). The server stays
  // initialized but the connection is dropped and a new advertising
  // configuration is applied.
  if (currentPhase >= 20 && !phase20Done) {
    phase20Done = true;
    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    for (auto &c : conns) {
      (void)srv.disconnect(c.getHandle());
    }
    delay(500);

    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    adv.reset();
    adv.setType(BLEAdvType::ConnectableScannable);
    adv.setInterval(100, 200);
    adv.setMinPreferred(0x06);
    adv.setMaxPreferred(0x12);
    adv.setAppearance(0x0340);  // Generic Sensor
    adv.setScanResponse(true);
    std::atomic<uint32_t> manualAdvCompletions{0};
    adv.onComplete([&manualAdvCompletions](uint8_t) {
      manualAdvCompletions.fetch_add(1);
    });

    // Split across primary (31 B max) and scan response (31 B max).
    //   primary  -> flags (3) + name "ADV_xxxx" (~12) + appearance (4) + txPower (3)
    //   scan rsp -> manufacturer data (7) + svc data (6) + 16-bit svc UUID (4)
    String advName = String("ADV_") + serverName;
    BLEAdvertisementData d;
    d.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
    d.setName(advName);
    d.setAppearance(0x0340);
    d.setTxPower(-4);
    adv.setAdvertisementData(d);

    BLEAdvertisementData sr;
    uint8_t mfg[] = {'E', 'S', 'P'};
    sr.setManufacturerData(0x02E5, mfg, sizeof(mfg));
    uint8_t svcData[] = {0xAB, 0xCD};
    sr.setServiceData(BLEUUID((uint16_t)0x180D), svcData, sizeof(svcData));
    sr.addServiceUUID(BLEUUID((uint16_t)0x180D));
    // Slave Connection Interval Range AD (0x12): 6..18 in 1.25 ms units.
    // Mirrors setMinPreferred/setMaxPreferred so the client can parse it back.
    sr.setPreferredParams(0x0006, 0x0012);
    // Issue #12801, kept last: the length byte claims 20 octets when only 2
    // follow. Rejecting it must not disturb the real fields parsed above — a
    // parser missing the bounds check reads type 0xFF with a 19-byte length and
    // overwrites the manufacturer data above with out-of-bounds bytes, so the
    // client asserting company 0x02E5 / length 3 is what catches it.
    const uint8_t oversizeAd[] = {20, 0xFF, 0x01, 0x02};
    sr.addRaw(oversizeAd, sizeof(oversizeAd));
    adv.setScanResponseData(sr);

    BTStatus s = adv.start();
    Serial.printf("[SERVER] Phase20 advReady ok=%d isAdv=%d\n", (int)(bool)s, (int)adv.isAdvertising());
    // Must outlast the client's active scan (+ optional scan-response retry).
    delay(14000);
    // Intentionally do NOT call adv.stop() here: calling adv.start() while
    // already advertising must stop and restart advertising with the updated
    // OSZ_ advertisement and scan-response payloads on both backends.

    // Parser regression window: every AD structure here is malformed in a
    // different way and none of them may reach the client's parsed fields.
    // Distinct name so the client does not confuse this with the ADV_ window.
    // The client concatenates the advertisement and the scan response before
    // parsing, so the primary payload must stay free of anything that halts
    // parsing or the whole scan response below would be skipped untested.
    String oszName = String("OSZ_") + serverName;
    BLEAdvertisementData od;
    od.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
    od.setName(oszName);
    adv.setAdvertisementData(od);

    BLEAdvertisementData osr;
    // A UUID list must hold a whole number of UUIDs. Each of these is one octet
    // group short, so a parser that rounds down would invent a UUID from the
    // trailing bytes or read past the field. None of them halts parsing.
    const uint8_t partial16Ad[] = {2, 0x03, 0x0D};                      // 16-bit list, 1 of 2 octets
    const uint8_t partial32Ad[] = {4, 0x05, 0x0D, 0x18, 0x00};          // 32-bit list, 3 of 4 octets
    const uint8_t short128Ad[] = {8, 0x07, 0x0D, 0x18, 0, 0, 0, 0, 0};  // 128-bit UUID, 7 of 16 octets
    // A zero-length AD terminates the payload (Core Spec Vol 3, Part C, §11).
    // The manufacturer AD behind it is deliberately well formed, so nothing
    // except correct terminator handling can keep it out of the parsed fields.
    // A parser missing that check treats the following length byte as the AD
    // type and underflows its own length to 255: type 0x04 then walks a 32-bit
    // UUID list far past the buffer, so the client sees a burst of garbage
    // service UUIDs instead of none.
    const uint8_t terminatorAd[] = {0x00};
    const uint8_t hiddenMfgAd[] = {4, 0xFF, 0xE5, 0x02, 0x42};
    osr.addRaw(partial16Ad, sizeof(partial16Ad));
    osr.addRaw(partial32Ad, sizeof(partial32Ad));
    osr.addRaw(short128Ad, sizeof(short128Ad));
    osr.addRaw(terminatorAd, sizeof(terminatorAd));
    osr.addRaw(hiddenMfgAd, sizeof(hiddenMfgAd));
    adv.setScanResponseData(osr);

    BTStatus os = adv.start();
    Serial.printf("[SERVER] Phase20 malformedAdv ok=%d isAdv=%d\n", (int)(bool)os, (int)adv.isAdvertising());
    // Client may still be finishing the ADV_ retry; keep this window wide.
    delay(12000);
    adv.stop();
    Serial.printf("[SERVER] Phase20 manualAdvCompletions=%lu\n", (unsigned long)manualAdvCompletions.load());
    adv.resetCallbacks();
    std::atomic<uint32_t> timedAdvCompletions{0};
    adv.onComplete([&timedAdvCompletions](uint8_t) {
      timedAdvCompletions.fetch_add(1);
    });
    unsigned long timedStartMs = millis();
    BTStatus timedStart = adv.start(300);
    while (timedAdvCompletions.load() == 0 && millis() - timedStartMs < 3000) {
      delay(10);
    }
    unsigned long timedElapsedMs = millis() - timedStartMs;
    Serial.printf(
      "[SERVER] Phase20 timedAdv start=%d completions=%lu isAdv=%d elapsed=%lu\n", (int)(bool)timedStart,
      (unsigned long)timedAdvCompletions.load(), (int)adv.isAdvertising(), timedElapsedMs
    );
    adv.stop();
    adv.resetCallbacks();
    std::atomic<uint32_t> cancelledAdvCompletions{0};
    adv.onComplete([&cancelledAdvCompletions](uint8_t) {
      cancelledAdvCompletions.fetch_add(1);
    });
    BTStatus cancelStart = adv.start(600);
    delay(100);
    BTStatus cancelStop = adv.stop();
    BTStatus replacementStart = adv.start();
    delay(700);
    Serial.printf(
      "[SERVER] Phase20 cancelledAdv timedStart=%d stop=%d replacement=%d completions=%lu isAdv=%d\n", (int)(bool)cancelStart,
      (int)(bool)cancelStop, (int)(bool)replacementStart, (unsigned long)cancelledAdvCompletions.load(), (int)adv.isAdvertising()
    );
    adv.stop();
    adv.resetCallbacks();
    int boundaryRestarts = 0;
    for (int i = 0; i < 12; i++) {
      BTStatus boundaryStart = adv.start(20);
      delay(18 + (i % 5));
      BTStatus boundaryReplacement = adv.start();
      delay(30);
      if (boundaryStart && boundaryReplacement && adv.isAdvertising()) {
        boundaryRestarts++;
      }
      adv.stop();
    }
    Serial.printf("[SERVER] Phase20 boundaryRestarts=%d/12\n", boundaryRestarts);
    adv.reset();
    adv.addServiceUUID(SERVICE_UUID);
    adv.start();

    // Local AD payload-limit regression checks (no peer needed): guards the
    // legacy 31-octet cap enforcement and the shortened-name fallback.
    String longName = "ThisNameIsWayTooLongToFitInThirtyOneByteAdv";  // 43 chars
    BLEAdvertisementData cap;
    cap.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
    cap.setName(longName);  // must degrade to a Shortened Local Name, not overflow
    bool nameFit = cap.length() <= BLEAdvertisementData::LEGACY_MAX_PAYLOAD;
    // A single field that cannot fit must be dropped whole (payload unchanged).
    size_t beforeLen = cap.length();
    uint8_t big[64] = {0};
    cap.setServiceData(BLEUUID((uint16_t)0x180D), big, sizeof(big));
    bool oversizeRejected = (cap.length() == beforeLen) && (cap.length() <= BLEAdvertisementData::LEGACY_MAX_PAYLOAD);
    // An extended payload accepts the full name (>31 octets allowed).
    BLEAdvertisementData ext(BLEAdvertisementData::EXTENDED_MAX_PAYLOAD);
    ext.setName(longName);
    bool extFullName = ext.length() >= (longName.length() + 2);
    Serial.printf("[SERVER] Phase20 adCap nameFit=%d oversizeRejected=%d extFullName=%d\n", (int)nameFit, (int)oversizeRejected, (int)extFullName);

    // Slave Connection Interval Range AD (0x12) encoding guard (no peer needed):
    // setPreferredParams must emit exactly [04 12 <min_lo min_hi> <max_lo max_hi>]
    // in little-endian 1.25 ms units. This is the field setMinPreferred /
    // setMaxPreferred route through the legacy AD builder.
    BLEAdvertisementData pref;
    pref.setPreferredParams(0x0006, 0x0012);
    const uint8_t *pp = pref.data();
    size_t ppLen = pref.length();
    bool prefOk = false;
    for (size_t i = 0; i + 1 < ppLen;) {
      uint8_t flen = pp[i];
      if (flen == 0) {
        break;
      }
      if (pp[i + 1] == 0x12 && flen == 5 && (i + 1 + flen) <= ppLen) {
        uint16_t mn = (uint16_t)pp[i + 2] | ((uint16_t)pp[i + 3] << 8);
        uint16_t mx = (uint16_t)pp[i + 4] | ((uint16_t)pp[i + 5] << 8);
        prefOk = (mn == 0x0006) && (mx == 0x0012);
        break;
      }
      i += flen + 1;
    }
    Serial.printf("[SERVER] Phase20 prefIntervalAD ok=%d\n", (int)prefOk);

    Serial.println("[SERVER] Phase20 done");
  }

  // =========================================================================
  // Phase 21: iBeacon + Eddystone (URL + TLM) frame build
  // =========================================================================
  // Three non-connectable advertising payloads, one after another, so the
  // client can classify each via BLEAdvertisedDevice::getFrameType() and
  // parse it back to the original fields.
  if (currentPhase >= 21 && !phase21Done) {
    phase21Done = true;
    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    for (auto &c : conns) {
      (void)srv.disconnect(c.getHandle());
    }
    delay(500);
    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    adv.reset();
    adv.setType(BLEAdvType::NonConnectable);

    // iBeacon frame.
    BLEBeacon beacon;
    beacon.setProximityUUID(BLEUUID("a1b2c3d4-e5f6-1122-3344-556677889900"));
    beacon.setMajor(0x1234);
    beacon.setMinor(0x5678);
    beacon.setSignalPower(-59);
    adv.setAdvertisementData(beacon.getAdvertisementData());
    BTStatus s = adv.start();
    Serial.printf("[SERVER] Phase21 iBeacon adv ok=%d\n", (int)(bool)s);
    delay(4000);
    adv.stop();

    // Eddystone URL.
    BLEEddystoneURL url;
    url.setURL("https://espressif.com");
    url.setTxPower(-20);
    adv.setAdvertisementData(url.getAdvertisementData());
    s = adv.start();
    Serial.printf("[SERVER] Phase21 EddystoneURL ok=%d\n", (int)(bool)s);
    delay(4000);
    adv.stop();

    // Eddystone TLM.
    BLEEddystoneTLM tlm;
    tlm.setBatteryVoltage(3000);
    tlm.setTemperature(25.5f);
    tlm.setAdvertisingCount(42);
    tlm.setUptime(1234);
    adv.setAdvertisementData(tlm.getAdvertisementData());
    s = adv.start();
    Serial.printf("[SERVER] Phase21 EddystoneTLM ok=%d\n", (int)(bool)s);
    delay(4000);
    adv.stop();

    adv.reset();
    adv.setType(BLEAdvType::ConnectableScannable);
    adv.addServiceUUID(SERVICE_UUID);
    adv.start();
    Serial.println("[SERVER] Phase21 done");
  }

  // =========================================================================
  // Phase 22: Bond management + whitelist
  // =========================================================================
  // Enumerate bonds (phase 10 established at least one), log IRK, round-trip
  // the whitelist API. deleteAllBonds is left for the client-driven
  // re-pair test on encryptedChr — after delete the client must re-pair
  // on the next encrypted read. The server lists bonds before and after
  // deletion so both branches are observable.
  if (currentPhase >= 22 && !phase22Done) {
    phase22Done = true;
    BLESecurity s = BLE.getSecurity();
    auto bonds = s.getBondedDevices();
    Serial.printf("[SERVER] Bond count: %u\n", (unsigned)bonds.size());
    for (size_t i = 0; i < bonds.size() && i < 4; i++) {
      Serial.printf("[SERVER] Bond[%u]=%s type=%u\n", (unsigned)i, bonds[i].toString().c_str(), (unsigned)bonds[i].type());
    }

    // Whitelist round-trip uses the first bonded peer if any, otherwise a
    // fixed fake address. The goal is to exercise add/query/remove paths.
    BTAddress wlAddr = bonds.empty() ? BTAddress("AA:BB:CC:DD:EE:FF") : bonds[0];
    BTStatus a = BLE.whiteListAdd(wlAddr);
    bool on = BLE.isOnWhiteList(wlAddr);
    BTStatus r = BLE.whiteListRemove(wlAddr);
    bool after = BLE.isOnWhiteList(wlAddr);
    Serial.printf("[SERVER] Whitelist add=%d isOn=%d remove=%d after=%d\n", (int)(bool)a, (int)on, (int)(bool)r, (int)after);

    // Local IRK + peer IRK: the client exports its local IRK on the line
    // "[CLIENT] LocalIRK hex=<32 hex>" and its peer IRK (= our local IRK)
    // on "[CLIENT] PeerIRK hex=<32 hex>". We emit the mirror lines here so
    // the Python harness can assert local(server)==peer(client) and
    // peer(server)==local(client) — proving identity keys round-tripped
    // correctly across the SMP pairing.
    String localIrkHex = BLE.getLocalIRKString();
    Serial.flush();
    delay(50);
    Serial.printf("[SERVER] LocalIRK present=%d hex=%s\n", (int)(localIrkHex.length() > 0), localIrkHex.c_str());
    Serial.flush();
    delay(50);
    if (!bonds.empty()) {
      String peerIrkHex = BLE.getPeerIRKString(bonds[0]);
      Serial.printf("[SERVER] PeerIRK present=%d hex=%s\n", (int)(peerIrkHex.length() > 0), peerIrkHex.c_str());
      Serial.flush();
      delay(50);
    } else {
      Serial.println("[SERVER] PeerIRK present=0 hex=");
      Serial.flush();
      delay(50);
    }

    Serial.println("[SERVER] Phase22 done");
  }

  // =========================================================================
  // Phase 23: Error paths + miscellaneous API coverage
  // =========================================================================
  // - BLEServer::getServices()/getService(uuid)/getService(unknown)
  // - BLEService::getCharacteristics()/getCharacteristic(uuid)/removeCharacteristic
  // - BLECharacteristic::notify(connHandle), isSubscribed(conn)
  // - BLEUUID ==, bitSize, toUint16, to128 round-trip
  // - Implicit const char* -> BLEUUID at every GATT/adv/beacon API boundary
  // Then trigger a notify targeted at the specific connection handle.
  if (currentPhase >= 23 && !phase23Done) {
    phase23Done = true;
    // Ensure we're advertising so the client can reconnect for this phase.
    BLEAdvertising adv23 = BLE.getAdvertising();
    if (!adv23.isAdvertising()) {
      adv23.start();
    }
    BLEServer srv = BLE.createServer();

    // Server-local API surface tests (no connection needed).
    auto svcs = srv.getServices();
    Serial.printf("[SERVER] Services: %u\n", (unsigned)svcs.size());
    BLEService primarySvc = srv.getService(SERVICE_UUID);
    Serial.printf("[SERVER] GetService known ok=%d started=%d\n", (int)(bool)primarySvc, (int)primarySvc.isStarted());
    BLEService unknownSvc = srv.getService(UNKNOWN_SVC_UUID);
    Serial.printf("[SERVER] GetService unknown ok=%d\n", (int)(bool)unknownSvc);

    if (primarySvc) {
      auto chars = primarySvc.getCharacteristics();
      Serial.printf("[SERVER] Characteristics: %u\n", (unsigned)chars.size());
      auto known = primarySvc.getCharacteristic(RW_CHAR_UUID);
      Serial.printf("[SERVER] GetCharacteristic known ok=%d\n", (int)(bool)known);
      auto unknown = primarySvc.getCharacteristic(UNKNOWN_CHAR_UUID);
      Serial.printf("[SERVER] GetCharacteristic unknown ok=%d\n", (int)(bool)unknown);
      Serial.printf("[SERVER] Characteristic handle=%u\n", (unsigned)known.getHandle());
    }

    BLEUUID u16(static_cast<uint16_t>(0x180D));
    BLEUUID u128 = u16.to128();
    bool eq = (u16 == u128);
    Serial.printf(
      "[SERVER] UUID bitSize16=%u bitSize128=%u to16=0x%04X eq=%d\n", (unsigned)u16.bitSize(), (unsigned)u128.bitSize(), (unsigned)u16.toUint16(), (int)eq
    );
    BLEUUID u32(static_cast<uint32_t>(0x0000180DUL));
    Serial.printf("[SERVER] UUID bitSize32=%u to32=0x%08lX\n", (unsigned)u32.bitSize(), (unsigned long)u32.toUint32());

    // Implicit const char* -> BLEUUID (string literals at API boundaries).
    int strPass = 0;
    int strFail = 0;
    auto strCheck = [&](bool ok) {
      if (ok) {
        ++strPass;
      } else {
        ++strFail;
      }
    };

    strCheck(BLEUUID("180D").bitSize() == 16);
    strCheck(BLEUUID("0000180D").bitSize() == 32);
    strCheck(BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b") == SERVICE_UUID);
    strCheck(kConstexprUuid16 == u16 && kConstexprUuid16.toUint16() == 0x180D && kConstexprUuid16.to128() == u128);
    strCheck(kConstexprUuid128 == BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e"));

    BLEService svcByStr = srv.getService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    strCheck(svcByStr && primarySvc && svcByStr.getHandle() == primarySvc.getHandle());

    if (primarySvc) {
      BLECharacteristic chrByStr = primarySvc.getCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8");
      strCheck(chrByStr && chrByStr.getUUID() == RW_CHAR_UUID);

      if (descChr) {
        BLEDescriptor descByStr = descChr.getDescriptor("b5f3e2c1-8a9e-4b7c-9f3d-2e8a5b7c3f91");
        strCheck(static_cast<bool>(descByStr));
      }
    }

    BLEService ephemeral = srv.createService("cccccccc-0000-4000-8000-000000000099");
    strCheck(static_cast<bool>(ephemeral));
    if (ephemeral) {
      BLECharacteristic epChr = ephemeral.createCharacteristic("cccccccc-0000-4000-8000-00000000009a", BLEProperty::Read, BLEPermission::ReadOpen);
      strCheck(static_cast<bool>(epChr));
      if (epChr) {
        BLEDescriptor epDesc = epChr.createDescriptor("2901", BLEPermission::ReadOpen, 16);
        strCheck(static_cast<bool>(epDesc));
      }
    }

    BLEAdvertisementData adStr;
    adStr.addServiceUUID("180D");
    adStr.addServiceUUID("180F");
    adStr.setPartialServices("1810");
    uint8_t svcDataStub[1] = {0x01};
    adStr.setServiceData("180D", svcDataStub, sizeof(svcDataStub));
    strCheck(adStr.length() > 0);

    BLE.getAdvertising().addServiceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");

    BLEBeacon beaconStr;
    beaconStr.setProximityUUID("a1b2c3d4-e5f6-1122-3344-556677889900");
    strCheck(beaconStr.getProximityUUID().isValid());

    Serial.printf("[SERVER] StrConv pass=%d fail=%d\n", strPass, strFail);

    // Wait for the client to connect (it rescans before connecting).
    {
      unsigned long deadline = millis() + 20000;
      bool sawConnect = false;
      while (millis() < deadline) {
        auto conns = srv.getConnections();
        if (!conns.empty()) {
          sawConnect = true;
          uint16_t h = conns[0].getHandle();
          // The client is connected but has not subscribed, so targeted sends
          // must be refused; a handle that is not connected reports NO_CLIENT.
          notifyChr.setValue("targeted");
          notifyStatus = -1;
          BTStatus s = notifyChr.notify(h);
          Serial.printf("[SERVER] NotifyHandle ok=%d ret=%s status=%s\n", (int)(bool)s, s.toString(), notifyStatusName(notifyStatus));
          indicateStatus = -1;
          s = indicateChr.indicate(h);
          Serial.printf("[SERVER] IndicateHandle ok=%d ret=%s status=%s\n", (int)(bool)s, s.toString(), notifyStatusName(indicateStatus));
          notifyStatus = -1;
          s = notifyChr.notify(static_cast<uint16_t>(h + 0x100));
          Serial.printf("[SERVER] NotifyBadHandle ok=%d ret=%s status=%s\n", (int)(bool)s, s.toString(), notifyStatusName(notifyStatus));
          Serial.printf("[SERVER] IsSubscribed handle=%u sub=%d\n", (unsigned)h, (int)notifyChr.isSubscribed(h));
          auto subs = notifyChr.getSubscribedConnections();
          Serial.printf("[SERVER] SubscribedConnections: %u\n", (unsigned)subs.size());
          break;
        }
        delay(100);
      }
      if (!sawConnect) {
        Serial.println("[SERVER] Phase23 no peer for notify(handle)");
      }
    }
    Serial.println("[SERVER] Phase23 done");
  }

  // =========================================================================
  // Phase 24: BLE5 advanced (coded PHY, server setPhy/setDataLen, defaultPhy)
  // =========================================================================
  // Self-skipping when BLE5 is unavailable. Only BLE.getDefaultPhy/
  // setDefaultPhy and per-connection setPhy/setDataLen are exercised from
  // the server side; the client drives coded-PHY extended adv/scan in its
  // own phase 24 block.
  if (currentPhase >= 24 && !phase24Done) {
    phase24Done = true;
#if BLE5_SUPPORTED
    BLEPhy tx = BLEPhy::PHY_1M;
    BLEPhy rx = BLEPhy::PHY_1M;
    BTStatus gd = BLE.getDefaultPhy(tx, rx);
    Serial.printf("[SERVER] Phase24 getDefaultPhy ok=%d tx=%u rx=%u\n", (int)(bool)gd, (unsigned)tx, (unsigned)rx);
    BTStatus sd = BLE.setDefaultPhy(BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    Serial.printf("[SERVER] Phase24 setDefaultPhy ok=%d\n", (int)(bool)sd);

    BLEServer srv = BLE.createServer();
    // Make sure we're advertising so the client's dedicated phase-24 connection
    // can be established, then wait for it. This exercises the per-connection
    // PHY/DLE path every run rather than only when a stale link lingers from
    // phase 23.
    BLEAdvertising adv24 = BLE.getAdvertising();
    if (!adv24.isAdvertising()) {
      adv24.start();
    }
    unsigned long startWait = millis();
    while (srv.getConnectedCount() == 0 && (millis() - startWait) < 10000) {
      delay(50);
    }
    auto conns = srv.getConnections();
    if (!conns.empty()) {
      uint16_t h = conns[0].getHandle();
      BTStatus sp = srv.setPhy(h, BLEPhy::PHY_2M, BLEPhy::PHY_2M);
      Serial.printf("[SERVER] Phase24 setPhy(2M) ok=%d\n", (int)(bool)sp);
      BLEPhy txp = BLEPhy::PHY_1M, rxp = BLEPhy::PHY_1M;
      BTStatus gp = srv.getPhy(h, txp, rxp);
      Serial.printf("[SERVER] Phase24 getPhy ok=%d tx=%u rx=%u\n", (int)(bool)gp, (unsigned)txp, (unsigned)rxp);
      BTStatus dl = srv.setDataLen(h, 251, 2120);
      Serial.printf("[SERVER] Phase24 setDataLen ok=%d\n", (int)(bool)dl);
    } else {
      Serial.println("[SERVER] Phase24 no peer for setPhy");
    }
#else
    Serial.println("[SERVER] Phase24 BLE5 not supported, skipping");
#endif
    Serial.println("[SERVER] Phase24 done");
  }

  // =========================================================================
  // Phase 25: HID-over-GATT spec compliance
  // =========================================================================
  // Tears down the existing server/service state, rebuilds a fresh
  // BLEHIDDevice and exercises every HoGP/HIDS spec addition:
  //   - Included Service (Battery in HID)
  //   - External Report Reference descriptor on Report Map
  //   - Multiple Report characteristics (same UUID 0x2A4D)
  //   - Boot Keyboard Input/Output with correct properties + caching
  //   - Auto-advertised HID Service UUID
  if (currentPhase >= 25 && !phase25Done) {
    phase25Done = true;

    BLEServer endedServer = BLE.createServer();
    auto conns = endedServer.getConnections();
    for (auto &c : conns) {
      (void)endedServer.disconnect(c.getHandle());
    }
    delay(500);
    BLE.end(false);
    BTStatus endedNotify = notifyChr.notify();
    bool teardownOk = endedServer.getServices().empty() && !endedServer.isStarted() && notifyChr.getHandle() == 0 && !notifyChr.getService()
                      && notifyChr.getDescriptors().empty() && endedNotify == BTStatus::InvalidState;
    Serial.printf("[SERVER] Phase25 teardown shells=%d\n", (int)teardownOk);
    delay(500);

    BTStatus s = BLE.begin(String("HID_") + serverName);
    if (!s) {
      Serial.printf("[SERVER] Phase25 HID init FAILED: %s\n", s.toString());
    } else {
      BLEServer hidServer = BLE.createServer();
      Serial.printf("[SERVER] Phase25 services after begin: %u\n", (unsigned)hidServer.getServices().size());

      // Database-building removal APIs must detach externally retained handles
      // instead of leaving raw parent pointers behind.
      BLEService removedSvc = hidServer.createService(TMP_SVC_UUID);
      BLECharacteristic removedChr = removedSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadWriteOpen);
      BLEDescriptor removedDesc = removedChr.createDescriptor(BLEUUID((uint16_t)0xA001), BLEPermission::ReadWriteOpen);
      removedChr.removeDescriptor(removedDesc);
      bool descriptorShell = removedDesc.getHandle() == 0 && !removedDesc.getCharacteristic();
      removedDesc = removedChr.createDescriptor(BLEUUID((uint16_t)0xA002), BLEPermission::ReadWriteOpen);
      removedSvc.removeCharacteristic(removedChr);
      bool characteristicShell = removedChr.getHandle() == 0 && !removedChr.getService() && removedChr.getDescriptors().empty() && removedDesc.getHandle() == 0
                                 && !removedDesc.getCharacteristic();
      removedChr = removedSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
      removedDesc = removedChr.createDescriptor(BLEUUID((uint16_t)0xA003), BLEPermission::ReadOpen);
      bool serviceRemoved = (bool)hidServer.removeService(removedSvc);
      bool serviceShell = removedSvc.getHandle() == 0 && !removedSvc.getServer() && removedSvc.getCharacteristics().empty() && removedChr.getHandle() == 0
                          && !removedChr.getService() && removedDesc.getHandle() == 0 && !removedDesc.getCharacteristic();
      Serial.printf("[SERVER] Phase25 removal shells=%d\n", (int)(descriptorShell && characteristicShell && serviceRemoved && serviceShell));

      // Pre-registration characteristic validation (bleValidateCharForRegistration):
      //   1. ExtendedProps without 0x2900 descriptor -> InvalidState
      //   2. Broadcast without 0x2903 descriptor -> InvalidState
      //   3. 0x2904 descriptor with length != 7 -> InvalidState
      //   4. Descriptor with permissions mutated to None post-creation -> InvalidState
      int preRegRej = 0;
      {
        BLEService badSvc = hidServer.createService(TMP_SVC_UUID);
        (void)badSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read | BLEProperty::ExtendedProps, BLEPermission::ReadOpen);
        if (hidServer.start() == BTStatus::InvalidState) {
          preRegRej++;
        }
        (void)hidServer.removeService(badSvc);
      }
      {
        BLEService badSvc = hidServer.createService(TMP_SVC_UUID);
        (void)badSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read | BLEProperty::Broadcast, BLEPermission::ReadOpen);
        if (hidServer.start() == BTStatus::InvalidState) {
          preRegRej++;
        }
        (void)hidServer.removeService(badSvc);
      }
      {
        BLEService badSvc = hidServer.createService(TMP_SVC_UUID);
        BLECharacteristic badChr = badSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
        BLEDescriptor badPf = badChr.createDescriptor(BLEUUID((uint16_t)0x2904), BLEPermission::ReadOpen, 7);
        const uint8_t shortPf[3] = {0x01, 0x00, 0x00};
        badPf.setValue(shortPf, sizeof(shortPf));
        if (hidServer.start() == BTStatus::InvalidState) {
          preRegRej++;
        }
        (void)hidServer.removeService(badSvc);
      }
      {
        BLEService badSvc = hidServer.createService(TMP_SVC_UUID);
        BLECharacteristic badChr = badSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
        BLEDescriptor mutDesc = badChr.createDescriptor(BLEUUID((uint16_t)0xA004), BLEPermission::ReadOpen, 8);
        mutDesc.setPermissions(BLEPermission::None);
        if (hidServer.start() == BTStatus::InvalidState) {
          preRegRej++;
        }
        (void)hidServer.removeService(badSvc);
      }
      Serial.printf("[SERVER] Phase25 preRegRejected=%d/4\n", preRegRej);

      BLEHIDDevice hid(hidServer);
      hid.manufacturer("Espressif");
      hid.pnp(0x02, 0x303A, 0x1001, 0x0100);
      hid.hidInfo(0x00, 0x01);
      static const uint8_t kbdReportMap[] = {0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
                                             0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x75, 0x08, 0x95, 0x01, 0x81, 0x03, 0xC0};
      hid.reportMap(kbdReportMap, sizeof(kbdReportMap));

      BLECharacteristic inRpt = hid.inputReport(1);
      BLECharacteristic outRpt = hid.outputReport(1);

      BLECharacteristic bootIn1 = hid.bootInput();
      BLECharacteristic bootIn2 = hid.bootInput();

      BLECharacteristic bootOut = hid.bootOutput();

      hid.setBatteryLevel(84);

      auto hidChars = hid.hidService().getCharacteristics();
      Serial.printf("[SERVER] Phase25 hidChars=%u\n", (unsigned)hidChars.size());

      hidServer.start();

      // Handle comparisons must happen after start() — handles are 0 before registration.
      bool multiReport = (inRpt && outRpt && inRpt.getHandle() != outRpt.getHandle());
      Serial.printf("[SERVER] Phase25 multiReport=%d\n", (int)multiReport);

      bool bootCached = (bootIn1 && bootIn2 && bootIn1.getHandle() == bootIn2.getHandle());
      Serial.printf("[SERVER] Phase25 bootCached=%d\n", (int)bootCached);

      Serial.printf("[SERVER] Phase25 bootOut=%d\n", (int)(bool)bootOut);

      // Dynamic service addition after server.start(): add a new service +
      // characteristic and call start() again so the peer can discover and
      // read it alongside the initially started HID and Battery services.
      BLEService dynSvc = hidServer.createService(TMP_SVC_UUID);
      BLECharacteristic dynChr = dynSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
      dynChr.setValue("dyn_svc_ok");
      BTStatus dynStart = hidServer.start();
      Serial.printf("[SERVER] Phase25 dynSvcStarted=%d handle=%u\n", (int)((bool)dynStart && dynChr.getHandle() != 0), (unsigned)dynChr.getHandle());

      BLEAdvertising adv = BLE.getAdvertising();
      // BLE.end() above already reset the advertising state (cleared all
      // accumulated serviceUUIDs, appearance, etc.), so no adv.reset() is
      // needed here.  BLEHIDDevice's constructor automatically added the HID
      // Service UUID (0x1812) per HoGP §4.3.1; it is already present.
      adv.setType(BLEAdvType::ConnectableScannable);
      adv.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
      BTStatus as = adv.start();
      Serial.printf("[SERVER] Phase25 HID adv ok=%d\n", (int)(bool)as);
      Serial.println("[SERVER] Phase25 HID ready");
      delay(10000);
      adv.stop();
    }
    Serial.println("[SERVER] Phase25 done");
  }

  // =========================================================================
  // Phase 26: Legacy advertising over the BLE5 extended engine
  // =========================================================================
  // On an ext-adv-enabled controller the public "legacy" advertising API is
  // routed over the extended API using a legacy PDU on a reserved instance
  // (legacy-over-ext when BLE5). Re-init a clean stack, stand up a minimal connectable
  // GATT server, and advertise a connectable legacy ADV_IND (name LEG_<name>).
  // The client confirms the set is discoverable AND flagged as a *legacy*
  // connectable advertisement — the core regression this plan fixed.
  if (currentPhase >= 26 && !phase26Done) {
    phase26Done = true;
#if BLE5_SUPPORTED
    // Phase 25 left the stack running as HID_<name>; start fresh.
    BLE.end(false);
    delay(500);
    BTStatus s = BLE.begin(serverName);
    if (!s) {
      Serial.printf("[SERVER] Phase26 init FAILED: %s\n", s.toString());
    } else {
      BLEServer srv = BLE.createServer();
      srv.advertiseOnDisconnect(false);
      BLEService svc = srv.createService(SERVICE_UUID);
      auto ch = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
      ch.setValue("legacy_over_ext");
      srv.start();

      BLEAdvertising adv = BLE.getAdvertising();
      adv.reset();
      adv.setType(BLEAdvType::ConnectableScannable);  // ADV_IND legacy PDU
      adv.setName(String("LEG_") + serverName);
      adv.addServiceUUID(SERVICE_UUID);
      adv.setScanResponse(true);
      BTStatus as = adv.start();
      Serial.printf("[SERVER] Phase26 legacyAdv ok=%d isAdv=%d\n", (int)(bool)as, (int)adv.isAdvertising());
      // Outlast the client's scan windows (client retries for ~10 s).
      delay(13000);
    }
#else
    Serial.println("[SERVER] Phase26 BLE5 not supported, skipping");
#endif
    Serial.println("[SERVER] Phase26 done");
  }

  // =========================================================================
  // Phase 27: Legacy + extended advertising concurrently
  // =========================================================================
  // The reserved legacy instance must broadcast alongside a user extended set
  // (legacy-over-ext when BLE5). Advertise a connectable legacy ADV_IND (LEG_<name>) on
  // the reserved instance AND a non-connectable extended set (EXT_<name>) on
  // instance 0 at the same time. The client must see BOTH names in one scan,
  // with the legacy set flagged legacy=1 and the extended set legacy=0.
  if (currentPhase >= 27 && !phase27Done) {
    phase27Done = true;
#if BLE5_SUPPORTED
    // Reuses the stack begun in phase 26.
    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    adv.reset();
    adv.setType(BLEAdvType::ConnectableScannable);
    adv.setName(String("LEG_") + serverName);
    adv.setScanResponse(true);
    BTStatus ls = adv.start();
    Serial.printf("[SERVER] Phase27 legacyAdv ok=%d\n", (int)(bool)ls);

    // Concurrent user extended set on instance 0 (reserved legacy is the top
    // instance). Non-connectable, legacy=0 so the client can distinguish it.
    adv.setExtType(0, BLEAdvType::NonConnectable);
    adv.setExtPhy(0, BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    adv.setExtSID(0, 2);
    BLEAdvertisementData extData;
    extData.setName(String("EXT_") + serverName);
    BTStatus eds = adv.setExtAdvertisementData(0, extData);
    BTStatus es = adv.startExtended(0, 0, 0);
    Serial.printf("[SERVER] Phase27 extAdv ok=%d data=%d\n", (int)(bool)es, (int)(bool)eds);
    Serial.printf("[SERVER] Phase27 concurrent legacy=%d ext=%d\n", (int)(bool)ls, (int)(bool)es);
    // Outlast the client's scan windows.
    delay(13000);
    // Stop the extended set first, then the legacy set; per-instance tracking
    // ensures stopExtended(0) does not suppress stop() on the legacy instance.
    adv.stopExtended(0);
    adv.stop();
#else
    Serial.println("[SERVER] Phase27 BLE5 not supported, skipping");
#endif
    Serial.println("[SERVER] Phase27 done");
  }

  // =========================================================================
  // Phase 28: L2CAP CoC bulk transfer across the credit-stall boundary + reconnect
  // =========================================================================
  // Phases 25-27 left the stack running as a minimal/BLE5 server. Rebuild a
  // clean connectable server that also stands up a fresh L2CAP CoC listener,
  // then let the client push a bulk payload larger than the peer CoC MTU
  // (write() segments + waits for credits) followed by a channel disconnect + a
  // fresh channel to prove the CoC accept path recovers. Self-skips without L2CAP.
  if (currentPhase >= 28 && !phase28Done) {
    phase28Done = true;
#if BLE_L2CAP_SUPPORTED
    BLE.end(false);
    delay(500);
    BTStatus s = BLE.begin(serverName);
    if (!s) {
      Serial.printf("[SERVER] Phase28 init FAILED: %s\n", s.toString());
    } else {
      BLEServer srv = BLE.createServer();
      srv.advertiseOnDisconnect(true);
      BLEService svc = srv.createService(SERVICE_UUID);
      auto ch = svc.createCharacteristic(RW_CHAR_UUID, BLEProperty::Read | BLEProperty::WriteNR, BLEPermission::ReadWriteOpen);
      ch.setValue("l2cap_bulk");
      ch.onWrite([](BLECharacteristic &chr, const BLEConnInfo &) {
        if (chr.getStringValue() == "STALL_L2CAP") {
          // Deliberately hold the peer host task past connectL2CAP's 5 s
          // timeout. The late COC_CONNECTED must disconnect its abandoned
          // channel and retain the callback context through COC_DISCONNECTED.
          Serial.println("[SERVER] Phase28 delayed L2CAP accept entered");
          delay(7000);
          Serial.println("[SERVER] Phase28 delayed L2CAP accept released");
        }
      });
      srv.start();

      l2capBulkBytes = 0;
      l2capBulkReplied = false;
      l2capPhase28Accepts = 0;
      l2capPhase28Disconnects = 0;
      l2capBulkServer = BLE.createL2CAPServer(0x0080, 256);
      if (!l2capBulkServer) {
        Serial.println("[SERVER] Phase28 L2CAP init FAILED");
      } else {
        {
          BLEL2CAPServer droppedServer = BLE.createL2CAPServer(0x0081, 128);
          droppedServer.onData([](BLEL2CAPChannel &channel, const uint8_t *, size_t) {
            channel.write((const uint8_t *)"P28DROP_OK", 10);
          });
        }
        Serial.println("[SERVER] Phase28 dropped server handle");
        l2capBulkServer.onData([](BLEL2CAPChannel &channel, const uint8_t *data, size_t len) {
          syncPhaseFromHost();
          if (len >= 4 && memcmp(data, "ENDR", 4) == 0) {
            // Hold receive processing long enough for the peer's multi-SDU
            // write to stall, then let its BLE.end() release the waiter.
            Serial.println("[SERVER] Phase28 shutdown stall entered");
            delay(7000);
            Serial.println("[SERVER] Phase28 shutdown stall released");
            return;
          }
          if (len >= 4 && memcmp(data, "RACE", 4) == 0) {
            // This callback is on the NimBLE host task. disconnect() must
            // submit directly without blocking while the peer concurrently
            // runs write/getMTU/disconnect from application tasks.
            delay(20);
            BTStatus status = channel.disconnect();
            Serial.printf("[SERVER] Phase28 peer race disconnect=%d\n", (int)status);
            return;
          }
          if (!l2capBulkReplied) {
            // Bulk accumulation phase: sum SDU lengths until the target is met,
            // then echo the total so the client can verify integrity of size.
            l2capBulkBytes += (uint32_t)len;
            if (l2capBulkBytes >= L2CAP_BULK_TARGET) {
              l2capBulkReplied = true;
              char reply[32];
              int n = snprintf(reply, sizeof(reply), "P28BULK:%lu", (unsigned long)l2capBulkBytes);
              channel.write((const uint8_t *)reply, (size_t)n);
              Serial.printf("[SERVER] Phase28 bulk got=%lu\n", (unsigned long)l2capBulkBytes);
            }
          } else {
            // Post-reconnect ping/pong on the fresh channel.
            channel.write((const uint8_t *)"P28PONG", 7);
            Serial.println("[SERVER] Phase28 reconnect pong sent");
          }
        });
        l2capBulkServer.onAccept([](BLEL2CAPChannel channel) {
          syncPhaseFromHost();
          uint32_t count = l2capPhase28Accepts.fetch_add(1) + 1;
          Serial.printf("[SERVER] Phase28 L2CAP channel accepted count=%lu\n", (unsigned long)count);
        });
        l2capBulkServer.onDisconnect([](BLEL2CAPChannel &) {
          uint32_t count = l2capPhase28Disconnects.fetch_add(1) + 1;
          Serial.printf("[SERVER] Phase28 L2CAP channel disconnected count=%lu\n", (unsigned long)count);
        });
        Serial.println("[SERVER] Phase28 L2CAP init OK");
        BLEL2CAPServer oversizedL2cap = BLE.createL2CAPServer(0x0082, UINT16_MAX);
        Serial.printf(
          "[SERVER] Phase28 oversized MTU rejected=%d heap=%d\n", (int)!oversizedL2cap, (int)heap_caps_check_integrity_all(true)
        );

        BLEAdvertising adv = BLE.getAdvertising();
        adv.reset();
        adv.setType(BLEAdvType::ConnectableScannable);
        adv.addServiceUUID(SERVICE_UUID);
        adv.start();
        Serial.println("[SERVER] Phase28 ready");
        // Outlast late-connect, bulk, race, 64-shell churn, and in-flight end.
        unsigned long phase28Wait = millis();
        while (l2capPhase28Disconnects.load() < 68 && millis() - phase28Wait < 120000) {
          delay(20);
        }
      }
    }
#else
    Serial.println("[SERVER] Phase28 L2CAP not supported, skipping");
#endif
    Serial.println("[SERVER] Phase28 done");
  }

  // =========================================================================
  // Phase 29: Connection-parameter update request denial (NimBLE-only)
  // =========================================================================
  // The peripheral (server) requests a connection-parameter change; the central
  // (client) rejects it from its onConnParamsUpdateRequest pre-accept hook. This
  // hook only exists on NimBLE — Bluedroid negotiates conn-param updates inside
  // L2CAP and never surfaces a pre-accept callback, so the client self-skips
  // there. The server simply issues the request and reports status.
  if (currentPhase >= 29 && !phase29Done) {
    phase29Done = true;
    // Rebuild a fresh secured server so the client reliably rediscovers the
    // plain serverName advertisement — earlier phases (e.g. HID in phase 25)
    // can leave a different advertising payload active.
    BLE.end(false);
    delay(500);
    if (!phase_gatt_setup()) {
      Serial.println("[SERVER] Phase29 rebuild FAILED");
    } else {
      BLEServer srv = BLE.createServer();
      // Wait for the client's dedicated phase-29 connection.
      unsigned long startWait = millis();
      while (srv.getConnectedCount() == 0 && (millis() - startWait) < 12000) {
        delay(50);
      }
      auto conns = srv.getConnections();
      if (conns.empty()) {
        Serial.println("[SERVER] Phase29 no peer connected");
      } else {
        uint16_t h = conns[0].getHandle();
        // Give the client a moment to arm its rejecting hook before requesting.
        delay(500);
        BLEConnParams req;
        req.minInterval = 36;  // 45 ms — distinct from the default so a change is observable
        req.maxInterval = 56;  // 70 ms
        req.latency = 0;
        req.timeout = 600;  // 6 s
        BTStatus us = srv.updateConnParams(h, req);
        Serial.printf("[SERVER] Phase29 requestSent ok=%d\n", (int)(bool)us);
        // Let the request round-trip and the client's reject (or Bluedroid's
        // internal accept) settle before the client samples the live interval.
        delay(3000);
      }
    }
    Serial.println("[SERVER] Phase29 done");
  }

  // =========================================================================
  // Phase 30: deleteBond round-trip (delete -> verify gone -> re-pair)
  // =========================================================================
  // Rebuild the full secured server (secure characteristic + bonding), drop any
  // existing bond on the server side, and let the client delete its bond, verify
  // the bond store is empty, then re-read the authenticated characteristic to
  // force a fresh pairing — proving deleteBond fully round-trips on both stacks.
  if (currentPhase >= 30 && !phase30Done) {
    phase30Done = true;
    BLE.end(false);
    delay(500);
    if (!phase_gatt_setup()) {
      Serial.println("[SERVER] Phase30 rebuild FAILED");
    } else {
      Serial.printf("[SERVER] Phase30 isAdvAfterSetup=%d\n", (int)BLE.getAdvertising().isAdvertising());
      BLESecurity sec = BLE.getSecurity();
      auto before = sec.getBondedDevices();
      Serial.printf("[SERVER] Phase30 bondsBefore=%u\n", (unsigned)before.size());
      // Delete the server-side bond via the singular deleteBond() API so the
      // re-pair is genuinely fresh on both ends.
      BTStatus delStatus = before.empty() ? BTStatus::NotFound : sec.deleteBond(before[0]);
      Serial.printf("[SERVER] Phase30 deleteOk=%d\n", (int)(bool)delStatus);
      serverIdentityCount.store(0, std::memory_order_relaxed);
      auto after = sec.getBondedDevices();
      Serial.printf("[SERVER] Phase30 bondsAfterDelete=%u\n", (unsigned)after.size());
      // Wait for the client to reconnect and re-pair against the secure char.
      unsigned long startWait = millis();
      bool rebonded = false;
      Serial.printf("[SERVER] Phase30 isAdvAfterDelete=%d\n", (int)BLE.getAdvertising().isAdvertising());
      while ((millis() - startWait) < 20000) {
        BLEServer srv = BLE.createServer();
        auto conns = srv.getConnections();
        if (!conns.empty() && conns[0].isBonded()) {
          rebonded = true;
          break;
        }
        delay(100);
      }
      // getBondedDevices reflects the persisted store after the SMP exchange.
      auto rebond = sec.getBondedDevices();
      Serial.printf(
        "[SERVER] Phase30 rebonded=%d bondsAfterRepair=%u identityCount=%lu\n", (int)rebonded, (unsigned)rebond.size(),
        (unsigned long)serverIdentityCount.load(std::memory_order_relaxed)
      );
    }
    Serial.println("[SERVER] Phase30 done");
  }

  // =========================================================================
  // Phase 31: Numeric-Comparison reject
  // =========================================================================
  // Reuses the phase-30 secured server. The client arms a reject in its
  // numeric-comparison confirm handler and attempts the authenticated read; the
  // pairing must fail, leaving the link unencrypted and no bond on either side.
  // The server just wipes any prior bond, keeps advertising, and confirms no
  // bond formed.
  if (currentPhase >= 31 && !phase31Done) {
    phase31Done = true;
    BLESecurity sec = BLE.getSecurity();
    (void)sec.deleteAllBonds();
    {
      BLEServer srv = BLE.createServer();
      for (auto &c : srv.getConnections()) {
        (void)srv.disconnect(c.getHandle());
      }
    }
    serverIdentityCount.store(0, std::memory_order_relaxed);
    delay(300);
    BLEAdvertising adv = BLE.getAdvertising();
    if (!adv.isAdvertising()) {
      adv.start();
    }
    // The client connects, arms the reject, and drives the secure read. Watch
    // for it to connect then drop after the failed pairing, then report bonds.
    unsigned long startWait = millis();
    bool sawPeer = false;
    while ((millis() - startWait) < 20000) {
      BLEServer srv = BLE.createServer();
      if (srv.getConnectedCount() > 0) {
        sawPeer = true;
      } else if (sawPeer) {
        break;
      }
      delay(100);
    }
    auto bonds = sec.getBondedDevices();
    Serial.printf(
      "[SERVER] Phase31 ncReject sawPeer=%d bonds=%u identityCount=%lu\n", (int)sawPeer, (unsigned)bonds.size(),
      (unsigned long)serverIdentityCount.load(std::memory_order_relaxed)
    );
    Serial.println("[SERVER] Phase31 done");
  }

  // =========================================================================
  // Phase 32: Passkey-entry pairing (peripheral DisplayOnly, central KeyboardOnly)
  // =========================================================================
  // Reconfigure the peripheral as DisplayOnly with a fixed static passkey so the
  // SMP method negotiates Passkey Entry (not Just Works / Numeric Comparison).
  // The central (KeyboardOnly) supplies the same passkey from its
  // onPassKeyRequest handler; the authenticated read must then succeed and bond.
  if (currentPhase >= 32 && !phase32Done) {
    phase32Done = true;
    BLESecurity sec = BLE.getSecurity();
    (void)sec.deleteAllBonds();
    sec.setIOCapability(BLEIOCapability::DisplayOnly);
    sec.setStaticPassKey(PASSKEY_ENTRY_STATIC);
    {
      BLEServer srv = BLE.createServer();
      for (auto &c : srv.getConnections()) {
        (void)srv.disconnect(c.getHandle());
      }
    }
    delay(300);
    BLEAdvertising adv = BLE.getAdvertising();
    if (!adv.isAdvertising()) {
      adv.start();
    }
    unsigned long startWait = millis();
    bool rebonded = false;
    while ((millis() - startWait) < 25000) {
      BLEServer srv = BLE.createServer();
      auto conns = srv.getConnections();
      if (!conns.empty() && conns[0].isBonded()) {
        rebonded = true;
        break;
      }
      delay(100);
    }
    auto bonds = sec.getBondedDevices();
    Serial.printf("[SERVER] Phase32 passkeyEntry rebonded=%d bonds=%u\n", (int)rebonded, (unsigned)bonds.size());
    // Restore the numeric-comparison default; later phases don't pair.
    sec.setIOCapability(BLEIOCapability::DisplayYesNo);
    sec.setRandomPassKey();
    Serial.println("[SERVER] Phase32 done");
  }

  // =========================================================================
  // Phase 33: Indications right behind one the stack sent itself
  // =========================================================================
  // On Bluedroid, deleting a started service makes the stack indicate Service
  // Changed to the client. Indications sent right after it must wait for its
  // confirmation and then go out, not fail with ESP_GATT_BUSY. NimBLE rebuilds
  // its whole database to delete a service, so there the indications go out
  // on their own.
  if (currentPhase >= 33 && !phase33Done) {
    phase33Done = true;
    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    (void)BLE.getSecurity().deleteAllBonds();
    BLE.end(false);
    delay(500);
    BTStatus init = BLE.begin(serverName);
    if (!init) {
      Serial.printf("[SERVER] Phase33 init FAILED: %s\n", init.toString());
      Serial.println("[SERVER] Phase33 done");
      return;
    }
    BLEServer srv = BLE.createServer();
    adv = BLE.getAdvertising();
    // A fresh database makes Bluedroid's Service Changed (sent when the
    // temporary started service below is deleted) the only indication holding
    // the slot. Without a bond, the client receives no stale database change
    // from an earlier session when it reconnects.
    BLEService busySvc = srv.createService(BUSY_SVC_UUID);
    BLECharacteristic busyChr = busySvc.createCharacteristic(BUSY_CHAR_UUID, BLEProperty::Read | BLEProperty::Indicate, BLEPermission::ReadOpen);
    busyChr.onStatus([](BLECharacteristic &chr, BLECharacteristic::NotifyStatus status, uint32_t code) {
      if (status == BLECharacteristic::SUCCESS_INDICATE) {
        busyOkCount.fetch_add(1, std::memory_order_relaxed);
      } else {
        busyErrCount.fetch_add(1, std::memory_order_relaxed);
      }
    });
    BLEService tmpSvc = srv.createService(TMP_SVC_UUID);
    tmpSvc.createCharacteristic(TMP_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadOpen).setValue("tmp");
    srv.start();
    adv.start();

    unsigned long startWait = millis();
    while (busyChr.getSubscribedCount() == 0 && millis() - startWait < 30000) {
      delay(50);
    }
    if (busyChr.getSubscribedCount() == 0) {
      Serial.println("[SERVER] Phase33 no subscriber");
    } else {
      // The client's subscribe() is still waiting for the CCCD write confirm.
      // Sending Service Changed immediately aborts that GATT procedure, so
      // subscribe() returns Fail even though the CCCD write landed.
      delay(500);
      BTStatus rm = srv.removeService(tmpSvc);
      const char *serviceChanged = rm ? "1" : "0";
      while (!phase33Continue) {
        checkSerial();
        delay(10);
      }
      for (int i = 1; i <= 3; i++) {
        char payload[16];
        int len = snprintf(payload, sizeof(payload), "busy_%d", i);
        busyChr.indicate(reinterpret_cast<const uint8_t *>(payload), len);
      }
      unsigned long sent = millis();
      while (busyOkCount + busyErrCount < 3 && millis() - sent < 10000) {
        delay(20);
      }
      Serial.printf("[SERVER] Phase33 serviceChanged=%s confirmed=%lu failed=%lu\n", serviceChanged, (unsigned long)busyOkCount, (unsigned long)busyErrCount);
    }
    Serial.println("[SERVER] Phase33 done");
  }

  // =========================================================================
  // Phase 34: Periodic advertising lifecycle — sync, terminate, sync-lost (BLE5)
  // =========================================================================
  // Extends the phase-2 periodic-sync happy path with the teardown transitions:
  // the client terminates its sync explicitly (terminatePeriodicSync) and also
  // observes onPeriodicLost after the server stops the periodic train. Re-inits
  // BLE5 advertising, then restores a normal stack so the memory_release phase
  // still measures a meaningful free. Self-skips without BLE5.
  if (currentPhase >= 34 && !phase34Done) {
    phase34Done = true;
#if BLE5_SUPPORTED
    BLE.end(false);
    delay(500);
    BTStatus s = BLE.begin(serverName);
    if (!s) {
      Serial.printf("[SERVER] Phase34 init FAILED: %s\n", s.toString());
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
      perData.setName("PeriodicPayload");
      adv.setPeriodicAdvData(0, perData);
      (void)adv.startExtended(0, 0, 0);
      adv.startPeriodicAdv(0);
      Serial.println("[SERVER] Phase34 periodic started");
      // Let the client sync + terminate its own sync.
      delay(12000);
      // Stop the periodic train so the client observes a supervision-timeout
      // sync-lost on any sync it re-established.
      adv.stopPeriodicAdv(0);
      adv.stopExtended(0);
      Serial.println("[SERVER] Phase34 periodic stopped");
      delay(6000);
      // Restore a normal connectable stack so memory_release frees real state.
      BLE.end(false);
      delay(500);
      (void)BLE.begin(serverName);
    }
#else
    Serial.println("[SERVER] Phase34 BLE5 not supported, skipping");
#endif
    Serial.println("[SERVER] Phase34 done");
  }

  // =========================================================================
  // Phase 35: CCCD bit routing, in-flight removeService, read-timeout recover,
  //           notify flood / congestion
  // =========================================================================
  // One ACL (two boards share a single local address pair). Per-peer CCCD and
  // EATT need a third DUT / a public EATT API. The host auto-confirms
  // indications, so a withheld-CONF / 30 s indication timeout cannot be forced.
  if (currentPhase >= 35 && !phase35Done) {
    phase35Done = true;
    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    BLE.end(false);
    delay(500);
    BTStatus init = BLE.begin(serverName);
    if (!init) {
      Serial.printf("[SERVER] Phase35 init FAILED: %s\n", init.toString());
      Serial.println("[SERVER] Phase35 done");
    } else {
      BLEServer srv = BLE.createServer();
      srv.advertiseOnDisconnect(true);

      std::atomic<uint16_t> peerCccd{0};
      std::atomic<uint16_t> peerConn{0};
      std::atomic<bool> havePeer{false};
      std::atomic<uint32_t> peerNotifyOk{0};
      std::atomic<uint32_t> peerIndicateOk{0};
      std::atomic<uint32_t> peerNotifyDisabled{0};
      std::atomic<uint32_t> peerIndicateDisabled{0};
      std::atomic<uint32_t> queueOk{0};
      std::atomic<uint32_t> queueErr{0};
      std::atomic<uint32_t> floodOk{0};
      std::atomic<uint32_t> floodErr{0};
      std::atomic<bool> flooding{false};
      std::atomic<bool> teardownFlooding{false};
      std::atomic<bool> teardownDisconnected{false};
      std::atomic<uint32_t> teardownAccepted{0};
      std::atomic<uint32_t> teardownSuccess{0};
      std::atomic<uint32_t> teardownNoClient{0};
      std::atomic<uint32_t> teardownOther{0};
      std::atomic<uint32_t> teardownTerminalAtDisconnect{0};
      std::atomic<uint32_t> teardownLate{0};
      std::atomic<uint32_t> timeoutAttempts{0};
      std::atomic<bool> timeoutAllowed{false};
      uint16_t timeoutHandle = 0;

      BLEService edgeSvc = srv.createService(EDGE_SVC_UUID);
      BLECharacteristic peerChr =
        edgeSvc.createCharacteristic(
          PEER_CHAR_UUID, BLEProperty::Read | BLEProperty::Write | BLEProperty::WriteNR | BLEProperty::Notify | BLEProperty::Indicate,
          BLEPermission::ReadWriteOpen
        );
      peerChr.onSubscribe([&peerCccd, &peerConn, &havePeer](BLECharacteristic, const BLEConnInfo &info, uint16_t subValue) {
        peerCccd = subValue;
        peerConn = info.getHandle();
        havePeer = true;
      });
      peerChr.onStatus([&peerNotifyOk, &peerIndicateOk, &peerNotifyDisabled, &peerIndicateDisabled, &floodOk, &floodErr,
                        &flooding, &teardownFlooding, &teardownDisconnected, &teardownSuccess, &teardownNoClient, &teardownOther,
                        &teardownLate](BLECharacteristic &, BLECharacteristic::NotifyStatus status, uint32_t) {
        if (teardownFlooding.load(std::memory_order_relaxed)) {
          if (teardownDisconnected.load(std::memory_order_relaxed)) {
            teardownLate.fetch_add(1, std::memory_order_relaxed);
          }
          if (status == BLECharacteristic::SUCCESS_NOTIFY) {
            teardownSuccess.fetch_add(1, std::memory_order_relaxed);
          } else if (status == BLECharacteristic::ERROR_NO_CLIENT) {
            teardownNoClient.fetch_add(1, std::memory_order_relaxed);
          } else {
            teardownOther.fetch_add(1, std::memory_order_relaxed);
          }
          return;
        }
        if (flooding.load(std::memory_order_relaxed)) {
          if (status == BLECharacteristic::SUCCESS_NOTIFY) {
            floodOk.fetch_add(1, std::memory_order_relaxed);
          } else {
            floodErr.fetch_add(1, std::memory_order_relaxed);
          }
          return;
        }
        if (status == BLECharacteristic::SUCCESS_NOTIFY) {
          peerNotifyOk.fetch_add(1, std::memory_order_relaxed);
        } else if (status == BLECharacteristic::SUCCESS_INDICATE) {
          peerIndicateOk.fetch_add(1, std::memory_order_relaxed);
        } else if (status == BLECharacteristic::ERROR_NOTIFY_DISABLED) {
          peerNotifyDisabled.fetch_add(1, std::memory_order_relaxed);
        } else if (status == BLECharacteristic::ERROR_INDICATE_DISABLED) {
          peerIndicateDisabled.fetch_add(1, std::memory_order_relaxed);
        }
      });
      peerChr.onWrite([&teardownFlooding, &teardownAccepted](BLECharacteristic &chr, const BLEConnInfo &info) {
        if (chr.getStringValue() != "drop_pending") {
          return;
        }
        teardownFlooding = true;
        Serial.println("[SERVER] Phase35 notifyTeardown burst entered");
        for (int i = 0; i < 64; i++) {
          uint8_t value = static_cast<uint8_t>(i);
          if (chr.notify(info.getHandle(), &value, 1)) {
            teardownAccepted.fetch_add(1, std::memory_order_relaxed);
          }
          delay(2);
        }
      });
      srv.onDisconnect(
        [&teardownDisconnected, &teardownTerminalAtDisconnect, &teardownSuccess, &teardownNoClient,
         &teardownOther](BLEServer &, const BLEConnInfo &, uint8_t) {
          teardownTerminalAtDisconnect =
            teardownSuccess.load(std::memory_order_relaxed) + teardownNoClient.load(std::memory_order_relaxed)
            + teardownOther.load(std::memory_order_relaxed);
          teardownDisconnected = true;
        }
      );

      BLECharacteristic timeoutChr = edgeSvc.createCharacteristic(TIMEOUT_CHAR_UUID, BLEProperty::Read, BLEPermission::ReadAuthorized);
      timeoutChr.setValue("timeout_ok");

      BLEService queueSvc = srv.createService(QUEUE_SVC_UUID);
      BLECharacteristic queueChr = queueSvc.createCharacteristic(QUEUE_CHAR_UUID, BLEProperty::Read | BLEProperty::Indicate, BLEPermission::ReadOpen);
      queueChr.onStatus([&queueOk, &queueErr](BLECharacteristic &, BLECharacteristic::NotifyStatus status, uint32_t) {
        if (status == BLECharacteristic::SUCCESS_INDICATE) {
          queueOk.fetch_add(1, std::memory_order_relaxed);
        } else {
          queueErr.fetch_add(1, std::memory_order_relaxed);
        }
      });

      srv.start();
      timeoutHandle = timeoutChr.getHandle();
      BLESecurity sec = BLE.getSecurity();
      sec.onAuthorization([&timeoutAllowed, &timeoutAttempts, timeoutHandle](const BLEConnInfo &, uint16_t attrHandle, bool isRead) -> bool {
        if (!isRead || attrHandle != timeoutHandle) {
          return true;
        }
        uint32_t n = timeoutAttempts.fetch_add(1, std::memory_order_relaxed);
        if (n == 0) {
          // A short delay keeps the ATT read outstanding so the client's
          // 200 ms timeout expires before the error response is sent.
          delay(400);
          timeoutAllowed = true;
          return false;
        }
        return timeoutAllowed.load();
      });

      adv = BLE.getAdvertising();
      adv.addServiceUUID(EDGE_SVC_UUID);
      adv.start();

      unsigned long startWait = millis();
      while ((peerCccd.load() & 0x0001) == 0 && millis() - startWait < 30000) {
        delay(50);
      }
      // Bluedroid's first GATT conn_id is 0; do not treat a zero handle as missing.
      // Targeted send: broadcast indicate with notify-only CCCD reports
      // ERROR_NO_SUBSCRIBER, not ERROR_INDICATE_DISABLED.
      if (havePeer.load()) {
        peerChr.indicate(peerConn.load(), reinterpret_cast<const uint8_t *>("peer_i"), 6);
      }
      startWait = millis();
      while (peerIndicateDisabled.load() == 0 && millis() - startWait < 2000) {
        delay(10);
      }
      if (havePeer.load()) {
        peerChr.notify(peerConn.load(), reinterpret_cast<const uint8_t *>("peer_n"), 6);
      }
      startWait = millis();
      while (peerNotifyOk.load() == 0 && millis() - startWait < 10000) {
        delay(20);
      }
      flooding.store(true, std::memory_order_relaxed);
      const int kFlood = 32;
      int floodSent = 0;
      for (int i = 1; i <= kFlood; i++) {
        char payload[16];
        int len = snprintf(payload, sizeof(payload), "flood_%d", i);
        peerChr.setValue(reinterpret_cast<const uint8_t *>(payload), len);
        BTStatus s = (i & 1) ? peerChr.notify(reinterpret_cast<const uint8_t *>(payload), len) : peerChr.notify();
        if (s) {
          floodSent++;
        }
        delay(5);
      }
      startWait = millis();
      while (floodOk.load() + floodErr.load() < (uint32_t)floodSent && millis() - startWait < 15000) {
        delay(20);
      }
      const uint32_t floodConfirmed = floodOk.load();
      const uint32_t floodFailed = floodErr.load();
      flooding.store(false, std::memory_order_relaxed);
      startWait = millis();
      while ((peerCccd.load() & 0x0002) == 0 && millis() - startWait < 15000) {
        delay(50);
      }
      if (havePeer.load()) {
        peerChr.notify(peerConn.load(), reinterpret_cast<const uint8_t *>("peer_n"), 6);
      }
      startWait = millis();
      while (peerNotifyDisabled.load() == 0 && millis() - startWait < 2000) {
        delay(10);
      }
      if (havePeer.load()) {
        peerChr.indicate(peerConn.load(), reinterpret_cast<const uint8_t *>("peer_i"), 6);
      }
      startWait = millis();
      while (peerIndicateOk.load() == 0 && millis() - startWait < 10000) {
        delay(20);
      }
      Serial.printf(
        "[SERVER] Phase35 cccd notifyOk=%lu indicateDisabled=%lu indicateOk=%lu notifyDisabled=%lu\n", (unsigned long)peerNotifyOk.load(),
        (unsigned long)peerIndicateDisabled.load(), (unsigned long)peerIndicateOk.load(), (unsigned long)peerNotifyDisabled.load()
      );

      startWait = millis();
      while (queueChr.getSubscribedCount() == 0 && millis() - startWait < 10000) {
        delay(50);
      }
      int qAccepted = 0;
      int qBusy = 0;
      for (int i = 1; i <= 10; i++) {
        char payload[16];
        int len = snprintf(payload, sizeof(payload), "q_%d", i);
        BTStatus s = queueChr.indicate(reinterpret_cast<const uint8_t *>(payload), len);
        if (s == BTStatus::Busy) {
          qBusy++;
        } else if (s) {
          qAccepted++;
        }
      }
      BTStatus removeBusy = srv.removeService(queueSvc);
      startWait = millis();
      while (queueOk.load() + queueErr.load() < (uint32_t)(qAccepted + qBusy) && millis() - startWait < 10000) {
        delay(20);
      }
      BTStatus removeIdle = srv.removeService(queueSvc);
      Serial.printf(
        "[SERVER] Phase35 removeBusy=%d accepted=%d busy=%d removeIdle=%d\n", (int)(removeBusy == BTStatus::Busy), qAccepted, qBusy, (int)(bool)removeIdle
      );
      Serial.printf("[SERVER] Phase35 flood sent=%d confirmed=%lu failed=%lu\n", floodSent, (unsigned long)floodConfirmed, (unsigned long)floodFailed);

      startWait = millis();
      while (timeoutAttempts.load() == 0 && millis() - startWait < 15000) {
        delay(50);
      }
      startWait = millis();
      while (!teardownDisconnected.load() && millis() - startWait < 15000) {
        delay(20);
      }
      delay(500);
      uint32_t finalTerminal = teardownSuccess + teardownNoClient + teardownOther;
      Serial.printf(
        "[SERVER] Phase35 notifyTeardown stack=%s attempted=64 accepted=%lu atDisconnect=%lu final=%lu success=%lu noClient=%lu other=%lu late=%lu\n",
        BLE.getStackName(), (unsigned long)teardownAccepted.load(), (unsigned long)teardownTerminalAtDisconnect.load(),
        (unsigned long)finalTerminal, (unsigned long)teardownSuccess.load(), (unsigned long)teardownNoClient.load(),
        (unsigned long)teardownOther.load(), (unsigned long)teardownLate.load()
      );
      peerChr.resetCallbacks();
      timeoutChr.resetCallbacks();
      queueChr.resetCallbacks();
      sec.onAuthorization(nullptr);
      Serial.println("[SERVER] Phase35 done");
    }
  }

  // Phase 36: Memory release (tail of the suite; runs after every other phase
  // so phases 16-35 can exercise the BLE stack while it's still alive).
  if (currentPhase >= 36 && !phase36Done) {
    phase36Done = true;
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
