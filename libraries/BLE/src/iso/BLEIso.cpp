/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/BLEGuards.h"
#if BLE_ISO_SUPPORTED

#include "BLE.h"
#include "BLEImpl.h"
#include "iso/BLEIso.h"
#include "iso/BLEIsoHost.h"
#include "esp32-hal-log.h"

#include "esp_ble_iso_common_api.h"
#include "core/BLEBackend.h"
#include "core/BLEMutex.h"
#include "core/BLESync.h"
#include "core/BLETask.h"

#include <atomic>
#include <cstring>

/**
 * @file BLEIso.cpp
 * @brief Audio-agnostic ISO transport over the ESP-BLE-ISO engine.
 *
 * Owns a fixed pool of channels, maps the vendor channel ops and GAP callback
 * onto `Channel` handles and `BTStatus`, and implements the host hand-over of
 * `BLEIsoHost.h`. This is the only unit that names `esp_ble_iso_*` types, so
 * the public header stays stack-free.
 *
 * The IDF only compiles each role's entry points when its Kconfig is set
 * (CONFIG_BT_ISO_CENTRAL / PERIPHERAL / BROADCASTER / SYNC_RECEIVER), so each
 * role's code is behind its BLE_ISO_*_SUPPORTED guard.
 *
 * Vendor callbacks run on the ISO host task; everything else runs from the
 * application with the host idle. The few fields both tasks touch
 * (_nextSeq, bigTx) are atomic; callbacks must be registered before a
 * channel connects.
 *
 * API contract is documented on the declarations in `BLEIso.h`; the
 * definitions below carry implementation notes only.
 */

#define ISO_UNICAST (BLE_ISO_CIS_CENTRAL_SUPPORTED || BLE_ISO_CIS_PERIPHERAL_SUPPORTED)

namespace {

/**
 * Protects Channel callbacks, pool slot allocation/reset, and SyncArm state.
 * Never held across esp_ble_iso_* calls (which acquire bt_le_host_lock()) or
 * while invoking user callbacks.
 */
SemaphoreHandle_t isoMtx() {
  static StaticSemaphore_t sStorage;
  static SemaphoreHandle_t sHandle = xSemaphoreCreateRecursiveMutexStatic(&sStorage);
  return sHandle;
}

/** Concurrent ISO channels in the pool; matches the host's own channel limit. */
constexpr int kMaxChan = BLE_ISO_MAX_CHAN;

/** What a slot is used for; set by the operation that configured it. */
enum class Kind : uint8_t {
  None = 0,
  CisCentral,     /*!< connectCis(). */
  CisPeripheral,  /*!< listenCis(). */
  BisBroadcaster, /*!< createBig(). */
  BisReceiver,    /*!< syncBig(). */
};

}  // namespace

// File-scope forward declarations (defined below, after the Impl structs).
// These must NOT live in the anonymous namespace: that would give them
// internal linkage, diverging from the definitions.
uint8_t vendorPhy(BLEIso::Phy phy);
uint8_t packBroadcastCode(const String &code, uint8_t *out);

// The retained transport Impl, set once when the first controller is minted.
// Callbacks and C-linkage helpers use it; null before any controller exists.
std::atomic<BLEIso::Impl *> &transportSingleton();

/**
 * Host ownership is process-global, not per-transport: there is exactly one IDF
 * host and one GAP app callback per boot, and the LE Audio engine can claim
 * that host before any `BLEIso` controller is minted (`audio.begin()` with no
 * `BLE.getIsoController()` call yet). Keeping this state inside `Impl` would
 * make the hand-over silently vanish whenever no controller exists, and the
 * GAP forwarder would then drop every ACL event.
 */
namespace {
std::atomic<bool> sOwnsHost{false};    // This transport called esp_ble_iso_common_init.
std::atomic<bool> sSharedHost{false};  // The LE Audio engine owns the host and ISO.
}  // namespace

// --------------------------------------------------------------------------
// Channel::Impl (per-handle state; callbacks + slot binding)
// --------------------------------------------------------------------------

struct BLEIso::Channel::Impl {
  /**
   * Owning transport; raw because the transport Impl is process-lifetime
   * (retained by the static controller slot, never released), so it always
   * outlives every channel handle. A shared_ptr here would cycle: slots
   * retain channel handles while bound.
   */
  BLEIso::Impl *transport = nullptr;
  std::atomic<int> slot{-1};  ///< Bound pool slot, or -1 once detached.
  uint32_t gen = 0;           ///< Slot generation at bind; immutable, checked lock-free.
  BLEIso::Channel::ConnectedCb onConnected;
  BLEIso::Channel::DisconnectedCb onDisconnected;
  BLEIso::Channel::ReceiveCb onReceive;
  BLEIso::Channel::SentCb onSent;
  std::atomic<uint16_t> _nextSeq{0};  // Atomic: reset on connect (host task), advanced on send (app task).
};

// --------------------------------------------------------------------------
// BLEIso::Impl (transport state; was file-static before the shared-handle API)
// --------------------------------------------------------------------------

struct BLEIso::Impl {
  /** Vendor state of one pool entry; `chan.ops` / `chan.qos` point into the same entry. */
  struct Slot {
    std::atomic<bool> used{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> accepted{false};  // A CIS was handed out to this slot (it may re-listen, but never be released while live).
    std::atomic<uint32_t> gen{0};       // Bumped on every bind; lets stale handles fail closed after slot reuse.
    Kind kind = Kind::None;
    std::atomic<bool> hasTx{false};  // Tx QoS configured (host to controller).
    bool hasRx = false;              // Rx QoS configured (controller to host).
    esp_ble_iso_chan_t chan{};
    esp_ble_iso_chan_ops_t ops{};
    esp_ble_iso_chan_qos_t qos{};
    esp_ble_iso_chan_io_qos_t txQos{};
    esp_ble_iso_chan_io_qos_t rxQos{};
    std::shared_ptr<Channel::Impl> handle;  // Retained while bound; user copies share it.
  };

  Slot slots[kMaxChan];
  std::atomic<uint32_t> genCounter{1};

  std::atomic<bool> active{false};     // begin() succeeded and end() has not run.
  std::atomic<bool> endFailed{false};  // end() refused partway; begin() must retry end() first.
  // Host ownership lives at file scope (sOwnsHost / sSharedHost): it is global
  // to the boot, not to this transport, which need not exist yet.

#if BLE_ISO_CIS_CENTRAL_SUPPORTED
  esp_ble_iso_cig_t *cig = nullptr;  // Single-CIS CIG of the Central slot.
#endif
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
  esp_ble_iso_server_t server{};  // Accepts incoming CIS into listenCis() slots.
  bool serverRegistered = false;
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
  std::atomic<esp_ble_iso_big_t *> bigTx{nullptr};  // Broadcaster BIG (host task clears it on disconnect).
  int bigAdv = -1;                                  // Advertising set registered with the ISO layer, or -1.
#endif

#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  /** BIG sync arm state: the sync itself is issued when a BIGInfo report arrives. */
  struct SyncArm {
    bool armed = false;
    bool syncing = false;  // doBigSync() call in flight on iso_task.
    bool synced = false;   // Sync issued; cleared on PA sync loss or disconnect.
    int slot = -1;
    uint32_t bitfield = 0;  // ESP_BLE_ISO_BIS_INDEX_BIT of the armed BIS.
    uint16_t timeout = 0;   // N * 10 ms.
    uint8_t bcode[ESP_BLE_ISO_BROADCAST_CODE_SIZE] = {};
    esp_ble_iso_big_t *big = nullptr;  // Synced Receiver BIG.
  };
  SyncArm sync;
#endif

  static constexpr uint32_t kIdleTimeoutMs = 5000;
  BLESync idleSync{"iso-idle"};

  // -- Slot helpers ---------------------------------------------------------

  bool slotOk(int slot) const {
    return slot >= 0 && slot < kMaxChan && slots[slot].used.load(std::memory_order_acquire);
  }

  /**
   * @brief Whether @p hi still names the live binding of its slot.
   *
   * Lock-free: `used` and `gen` are atomic, and `gen` is never reset (it only
   * grows), so a reused slot always mismatches a stale handle. The vendor
   * `chan` itself is stable while bound.
   */
  bool linkLive(int slot, uint32_t gen) const {
    return slotOk(slot) && slots[slot].gen.load(std::memory_order_acquire) == gen;
  }

  /** @brief Slot owning @p chan, -1 if it is not one of ours. */
  int slotOf(const esp_ble_iso_chan_t *chan) {
    for (int i = 0; i < kMaxChan; i++) {
      if (slots[i].used.load(std::memory_order_acquire) && &slots[i].chan == chan) {
        return i;
      }
    }
    return -1;
  }

  /** @brief First used slot of @p kind, -1 when there is none. */
  int findSlot(Kind kind) {
    for (int i = 0; i < kMaxChan; i++) {
      if (slots[i].used.load(std::memory_order_acquire) && slots[i].kind == kind) {
        return i;
      }
    }
    return -1;
  }

  /**
   * @brief Whether the host holds no live reference to the channel in @p slot.
   *
   * An idle slot may be configured again by the operation of its kind. A
   * Central CIS stays a CIG member after it disconnects; that is still idle,
   * because cisConnect() terminates the old CIG first. Only atomic slot state
   * is read here: the stack-owned chan.state/iso cannot be read safely from
   * this task (see allDisconnected()).
   */
  bool isIdle(int slot) {
    if (!slotOk(slot)) {
      return false;
    }
    const Slot &s = slots[slot];
    if (s.connected.load(std::memory_order_acquire) || s.accepted.load(std::memory_order_acquire)) {
      return false;
    }
    return true;
  }

  /** @brief Detach the handle bound to @p s (its user copies become shells). */
  static void detach(Slot &s) {
    if (s.handle) {
      s.handle->slot.store(-1, std::memory_order_release);
      s.handle.reset();
    }
  }

  /** @brief Reset vendor structs and direction flags, keeping the binding. */
  static void resetVendorState(Slot &s) {
    s.kind = Kind::None;
    s.hasTx.store(false, std::memory_order_relaxed);
    s.hasRx = false;
    s.chan = {};
    s.ops = {};
    s.qos = {};
    s.txQos = {};
    s.rxQos = {};
  }

  void clearSlot(Slot &s) {
    detach(s);
    resetVendorState(s);
    s.used.store(false, std::memory_order_release);
    s.connected.store(false, std::memory_order_release);
    s.accepted.store(false, std::memory_order_release);
  }

  /** @brief Reserve a free slot, -1 if the pool is full. */
  int allocSlot() {
    for (int i = 0; i < kMaxChan; i++) {
      if (!slots[i].used.load(std::memory_order_acquire)) {
        clearSlot(slots[i]);
        slots[i].used.store(true, std::memory_order_release);
        return i;
      }
    }
    return -1;
  }

  /** @brief Return a slot to the pool and detach its Channel. */
  void releaseSlot(int slot) {
    BLELockGuard lock(isoMtx());
    if (slot >= 0 && slot < kMaxChan) {
      clearSlot(slots[slot]);
    }
  }

  /**
   * @brief (Re)configure @p slot for @p kind with fresh vendor structs.
   *
   * The host must hold no reference to the slot's channel (see isIdle()).
   * The slot must already be cleared (allocSlot) or detached (takeChannel
   * reuse): the binding made by takeChannel is kept.
   */
  Slot &prepSlot(int slot, Kind kind) {
    BLELockGuard lock(isoMtx());
    Slot &s = slots[slot];
    resetVendorState(s);
    s.kind = kind;
    s.chan.ops = &s.ops;
    s.chan.qos = &s.qos;
    s.ops.connected = onChanConnected;
    s.ops.disconnected = onChanDisconnected;
    s.ops.recv = onChanRecv;
    s.ops.sent = onChanSent;
    s.used.store(true, std::memory_order_release);
    return s;
  }

  /**
   * @brief Configure the slot's directions; an SDU size of 0 leaves that direction out.
   *
   * PHY (a vendor ESP_BLE_ISO_PHY_* value) and RTN apply to both directions; for
   * a Peripheral CIS and a BIS Receiver the controller reports the values chosen
   * by the other side.
   */
  void setQos(Slot &s, uint16_t txSdu, uint16_t rxSdu, uint8_t phy, uint8_t rtn) {
    BLELockGuard lock(isoMtx());
    s.hasTx.store(txSdu > 0, std::memory_order_relaxed);
    s.hasRx = rxSdu > 0;
    s.txQos.sdu = txSdu;
    s.txQos.phy = phy;
    s.txQos.rtn = rtn;
    s.rxQos.sdu = rxSdu;
    s.rxQos.phy = phy;
    s.rxQos.rtn = rtn;
    s.qos.tx = txSdu > 0 ? &s.txQos : nullptr;
    s.qos.rx = s.hasRx ? &s.rxQos : nullptr;
  }

  /** @brief Drop every slot, group handle and Channel binding; the host side must already be gone. */
  void resetState() {
    BLELockGuard lock(isoMtx());
    for (int i = 0; i < kMaxChan; i++) {
      clearSlot(slots[i]);
    }

    // The host no longer references the CIG: end() terminated it, or the LE
    // Audio engine released the shared ISO host with it. Clearing the pointer
    // is what makes the dropped group really dropped; leaving it behind would
    // make the next connectCis() terminate a freed CIG.
#if BLE_ISO_CIS_CENTRAL_SUPPORTED
    cig = nullptr;
#endif
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
    serverRegistered = false;
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
    bigTx.store(nullptr, std::memory_order_release);
    bigAdv = -1;
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
    sync = SyncArm{};
#endif
  }

  BTStatus begin();
  BTStatus end();

  /**
   * @brief Common factory prologue: check the transport, detach any idle
   * previous binding, and mint a fresh handle.
   *
   * Single-instance roles (CIS Central, BIS Broadcaster, BIS Receiver) reuse
   * their existing slot once the host released it, so going down and up again
   * never exhausts the pool. The previous handle, if any, is detached: its
   * user copies become shells instead of aliasing the new channel.
   *
   * @param op    Factory name, for the log.
   * @param reuse Kind whose existing slot is reused, or Kind::None for a new slot.
   */
  Channel takeChannel(const char *op, Kind reuse) {
    if (!active.load(std::memory_order_acquire) || !BLE.isInitialized() || BLE.isEnding()) {
      log_e("BLEIso: %s() called before begin()", op);
      return Channel();
    }
    BLELockGuard lock(isoMtx());
    int slot = reuse != Kind::None ? findSlot(reuse) : -1;
    if (slot >= 0 && !isIdle(slot)) {
      log_e("BLEIso: %s(): its channel is still up; disconnect() it and wait for onDisconnected", op);
      return Channel();
    }
    if (slot < 0) {
      slot = allocSlot();
    }
    if (slot < 0) {
      log_w("BLEIso: %s(): all %u ISO channels in use (BLE_ISO_MAX_CHAN)", op, (unsigned)kMaxChan);
      if (reuse == Kind::None) {
        log_w("BLEIso: a listenCis() channel re-listens on its own after a disconnect; "
              "calling listenCis() again reserves an *additional* concurrent channel");
      }
      return Channel();
    }
    // The slot is idle: drop any previous binding, then mint a fresh handle
    // so stale copies fail closed instead of aliasing the new channel.
    detach(slots[slot]);
    auto hi = std::make_shared<Channel::Impl>();
    hi->transport = this;
    hi->gen = genCounter.fetch_add(1, std::memory_order_relaxed);
    hi->slot.store(slot, std::memory_order_release);
    slots[slot].gen.store(hi->gen, std::memory_order_release);
    slots[slot].handle = hi;
    slots[slot].used.store(true, std::memory_order_release);
    return Channel(hi);
  }

  /** @brief Common factory epilogue: on error, log and give the slot back. */
  Channel finish(Channel ch, esp_err_t err, const char *op) {
    if (err == ESP_OK) {
      return ch;
    }
    log_e("BLEIso: %s failed (err=%d)", op, err);
    if (ch) {
      releaseSlot(ch._impl->slot.load(std::memory_order_acquire));
    }
    return Channel();
  }

  /**
   * @brief Drop every live-completing channel when audio releases the host.
   *
   * Only completes slots that actually held a CIS/BIS; idle never-accepted
   * listen slots are dropped silently (disconnect() on them is also silent).
   */
  void attachSharedDone() {
    for (int i = 0; i < kMaxChan; i++) {
      if (slots[i].used.load(std::memory_order_acquire)
          && (slots[i].connected.load(std::memory_order_acquire) || slots[i].accepted.load(std::memory_order_acquire))) {
        dispatchDisconnected(i, 0x16);
      }
    }
    resetState();
    active.store(false, std::memory_order_release);
  }

  bool allDisconnected() {
    // Runs on waitDisconnected()'s waiter and on notifyIsoIdle() from the host
    // task; isoMtx() serializes both against slot teardown. An armed-but-never-
    // synced Receiver still holds periodic-sync resources, so it counts as busy.
    BLELockGuard lock(isoMtx());
    for (int i = 0; i < kMaxChan; i++) {
      if (slots[i].used.load(std::memory_order_acquire)
          && (slots[i].connected.load(std::memory_order_acquire) || slots[i].accepted.load(std::memory_order_acquire))) {
        return false;
      }
    }
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
    if (sync.armed || sync.syncing || sync.synced) {
      return false;
    }
#endif
    return true;
  }

  void notifyIsoIdle() {
    if (allDisconnected()) {
      idleSync.give(BTStatus::OK);
    }
  }

  bool waitDisconnected() {
    if (allDisconnected()) {
      return true;
    }
    if (BLETask::inStackCallbackTask()) {
      log_e("BLEIso: cannot wait for ISO disconnect from a stack callback");
      return false;
    }
    idleSync.take();
    if (allDisconnected()) {
      idleSync.give(BTStatus::OK);
    }
    BTStatus st = idleSync.wait(kIdleTimeoutMs);
    if (st != BTStatus::OK || !allDisconnected()) {
      log_e("BLEIso: timed out waiting for ISO channels to disconnect");
      return false;
    }
    return true;
  }

#if BLE_ISO_CIS_CENTRAL_SUPPORTED
  /**
   * @brief Create a single-CIS CIG with this QoS and connect its CIS on @p connHandle.
   *
   * A CIG left by a previous CIS is terminated first, so an idle Central slot
   * can reconnect with new QoS. Completion arrives via onChanConnected().
   */
  esp_err_t cisConnect(int slot, uint16_t connHandle, const BLEIso::CisParams &p) {
    // The CIG holds this slot's channel from a previous CIS: free it before the slot is reset.
    if (cig) {
      esp_err_t err = esp_ble_iso_cig_terminate(cig);
      if (err != ESP_OK) {
        log_e("BLEIso: terminating the previous CIG failed: %d (is its CIS still up?)", err);
        return err;
      }
      cig = nullptr;
    }
    Slot &s = prepSlot(slot, Kind::CisCentral);
    setQos(s, p.sduSize, p.returnSduSize, vendorPhy(p.phy), p.rtn);

    esp_ble_iso_chan_t *channels[1] = {&s.chan};
    esp_ble_iso_cig_param_t cigParam = {};
    cigParam.cis_channels = channels;
    cigParam.num_cis = 1;
    cigParam.sca = ESP_BLE_ISO_SCA_UNKNOWN;
    cigParam.packing = p.packing;
    cigParam.framing = p.framing;
    cigParam.c_to_p_latency = p.latencyMs;
    cigParam.p_to_c_latency = p.latencyMs;
    cigParam.c_to_p_interval = p.sduIntervalUs;
    cigParam.p_to_c_interval = p.sduIntervalUs;
    esp_err_t err = esp_ble_iso_cig_create(&cigParam, &cig);
    if (err != ESP_OK) {
      log_e("BLEIso: CIG create (%u us, %u/%u octets) failed: %d", (unsigned)p.sduIntervalUs, p.sduSize, p.returnSduSize, err);
      cig = nullptr;
      return err;
    }

    esp_ble_iso_connect_param_t connectParam = {};
    connectParam.iso_chan = &s.chan;
    connHandle = BLEIsoInternal::resolveAclHandle(connHandle);
    /* Mark busy before the async connect so allDisconnected() cannot report idle
     * in the window before onChanConnected (chan.state is stack-owned and cannot
     * be read safely from this task). Cleared on failure here and on disconnect. */
    s.connected.store(true, std::memory_order_release);
    err = esp_ble_iso_chan_connect(&connectParam, connHandle, 1);
    if (err != ESP_OK) {
      // Drop the CIG too: the caller releases the slot, whose channel it references.
      log_e("BLEIso: CIS connect on ACL 0x%04x failed: %d", connHandle, err);
      s.connected.store(false, std::memory_order_release);
      esp_ble_iso_cig_terminate(cig);
      cig = nullptr;
    }
    return err;
  }
#endif /* BLE_ISO_CIS_CENTRAL_SUPPORTED */

#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
  // Incoming CIS request: hand out the first listening slot not yet connected, or reject.
  static int onCisAccept(const esp_ble_iso_accept_info_t *info, esp_ble_iso_chan_t **chan) {
    (void)info;
    Impl *self = transportSingleton().load(std::memory_order_acquire);
    if (self == nullptr) {
      return -1;
    }
    BLELockGuard lock(isoMtx());
    for (int i = 0; i < kMaxChan; i++) {
      if (self->slots[i].used.load(std::memory_order_acquire) && self->slots[i].kind == Kind::CisPeripheral && self->slots[i].chan.iso == nullptr) {
        self->slots[i].accepted.store(true, std::memory_order_release);
        *chan = &self->slots[i].chan;
        return 0;
      }
    }
    log_w("BLEIso: incoming CIS rejected: no listenCis() channel free");
    return -1;
  }

  /**
   * @brief Accept the next incoming CIS on @p slot, registering the ISO server on first use.
   *
   * Each incoming CIS takes the first listening slot that is not connected
   * yet; the slot keeps listening after its CIS disconnects.
   */
  esp_err_t cisListen(int slot, const BLEIso::CisParams &p) {
    Slot &s = prepSlot(slot, Kind::CisPeripheral);
    setQos(s, p.returnSduSize, p.sduSize, ESP_BLE_ISO_PHY_2M, 0);
    if (!serverRegistered) {
      server = esp_ble_iso_server_t{};
      server.accept = onCisAccept;
      esp_err_t err = esp_ble_iso_server_register(&server);
      if (err != ESP_OK) {
        log_e("BLEIso: ISO server registration failed: %d", err);
        return err;
      }
      serverRegistered = true;
    }
    return ESP_OK;
  }
#endif /* BLE_ISO_CIS_PERIPHERAL_SUPPORTED */

#if BLE_ISO_BROADCASTER_SUPPORTED
  /**
   * @brief Create a single-BIS BIG on @p advHandle, whose extended + periodic advertising already runs.
   *
   * Registers the advertising set with the ISO layer (kept across BIGs) and
   * issues the create; completion arrives via onChanConnected().
   */
  esp_err_t bigCreate(int slot, uint8_t advHandle, const BLEIso::BigParams &p) {
    if (bigTx) {
      log_e("BLEIso: a BIG is already broadcasting; disconnect() it first");
      return ESP_ERR_INVALID_STATE;
    }
    // The ISO layer refuses a second registration of the same set: keep it across BIGs.
    esp_err_t err;
    if (bigAdv != advHandle) {
      esp_ble_iso_ext_adv_info_t advInfo = {};
      if (bigAdv >= 0) {
        advInfo.adv_handle = (uint8_t)bigAdv;
        esp_ble_iso_big_ext_adv_delete(&advInfo);
        bigAdv = -1;
      }
      advInfo.adv_handle = advHandle;
      err = esp_ble_iso_big_ext_adv_add(&advInfo);
      if (err != ESP_OK) {
        log_e("BLEIso: registering advertising set %u for the BIG failed: %d", advHandle, err);
        return err;
      }
      bigAdv = advHandle;
    }

    Slot &s = prepSlot(slot, Kind::BisBroadcaster);
    setQos(s, p.sduSize, 0, vendorPhy(p.phy), p.rtn);

    esp_ble_iso_chan_t *bis[1] = {&s.chan};
    esp_ble_iso_big_create_param_t param = {};
    param.bis_channels = bis;
    param.num_bis = 1;
    param.interval = p.sduIntervalUs;
    param.latency = p.latencyMs;
    param.packing = p.packing;
    param.framing = p.framing;
    param.encryption = packBroadcastCode(p.broadcastCode, param.bcode) > 0;
    bigTx = nullptr;
    esp_ble_iso_big_t *big = nullptr;
    err = esp_ble_iso_big_create(advHandle, &param, &big);
    if (err != ESP_OK) {
      log_e("BLEIso: BIG create on advertising set %u (%u us, %u octets) failed: %d", advHandle, (unsigned)p.sduIntervalUs, p.sduSize, err);
      bigTx = nullptr;
    } else {
      bigTx = big;
      // Mark busy before any async completion runs: until onDisconnected clears
      // it, end()/allDisconnected() treat the BIG as live. Cleared on failure
      // (no BIG) and on disconnect, mirroring the CIS connect window.
      s.connected.store(true, std::memory_order_release);
    }
    return err;
  }
#endif /* BLE_ISO_BROADCASTER_SUPPORTED */

#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  /**
   * @brief Arm a BIG sync for BIS @p bisIndex; no host call here.
   *
   * doBigSync() issues the sync on the next BIGInfo report. Only one sync can
   * be armed; arming again replaces it.
   *
   * @param syncTimeout BIG sync timeout (N * 10 ms).
   */
  esp_err_t bigSyncArm(int slot, uint8_t bisIndex, const BLEIso::BigParams &p, uint16_t syncTimeout) {
    setQos(prepSlot(slot, Kind::BisReceiver), 0, p.sduSize, ESP_BLE_ISO_PHY_2M, 0);
    BLELockGuard lock(isoMtx());
    sync = SyncArm{};
    sync.slot = slot;
    sync.bitfield = ESP_BLE_ISO_BIS_INDEX_BIT(bisIndex);
    sync.timeout = syncTimeout;
    packBroadcastCode(p.broadcastCode, sync.bcode);
    sync.armed = true;
    return ESP_OK;
  }

  /**
   * @brief Issue the armed BIG sync from a BIGInfo report (once per arm).
   *
   * @param nse        Subevents per BIS event, used as the max subevents to receive.
   * @param encryption From the BIG's BIGInfo; the armed code is applied only when set.
   */
  void doBigSync(uint16_t syncHandle, uint8_t nse, bool encryption) {
    int slot = -1;
    uint32_t bitfield = 0;
    uint16_t timeout = 0;
    uint8_t bcode[ESP_BLE_ISO_BROADCAST_CODE_SIZE] = {};
    {
      BLELockGuard lock(isoMtx());
      if (!sync.armed || sync.syncing || sync.synced || !slotOk(sync.slot)) {
        return;
      }
      slot = sync.slot;
      bitfield = sync.bitfield;
      timeout = sync.timeout;
      memcpy(bcode, sync.bcode, sizeof(bcode));
      sync.syncing = true;
    }

    esp_ble_iso_chan_t *bis[1] = {&slots[slot].chan};
    esp_ble_iso_big_sync_param_t param = {};
    param.bis_channels = bis;
    param.num_bis = 1;
    param.bis_bitfield = bitfield;
    param.mse = nse;
    param.sync_timeout = timeout;
    // The BIG's advertised encryption flag is authoritative; the armed
    // Broadcast Code is only applied when the group is actually encrypted.
    param.encryption = encryption;
    if (encryption) {
      memcpy(param.bcode, bcode, ESP_BLE_ISO_BROADCAST_CODE_SIZE);
    }
    esp_ble_iso_big_t *big = nullptr;
    esp_err_t err = esp_ble_iso_big_sync(syncHandle, &param, &big);
    esp_ble_iso_big_t *orphan = nullptr;
    {
      BLELockGuard lock(isoMtx());
      sync.syncing = false;
      if (err != ESP_OK) {
        // Left armed: the next BIGInfo report retries.
        log_e("BLEIso: BIG sync to BIS bitfield 0x%08lx on PA sync 0x%04x failed: %d", (unsigned long)bitfield, syncHandle, err);
        return;
      }
      if (!sync.armed || sync.slot != slot) {
        // Concurrent disconnect()/end() disarmed while esp_ble_iso_big_sync was in flight.
        orphan = big;
      } else {
        sync.big = big;
        sync.synced = true;
      }
    }
    if (orphan) {
      (void)esp_ble_iso_big_terminate(orphan);
    }
  }
#endif /* BLE_ISO_SYNC_RECEIVER_SUPPORTED */

  // -- Vendor dispatch (ISO host task): slot -> retained handle -> user callback --

  void dispatchConnected(int slot) {
    Channel::ConnectedCb cb;
    std::shared_ptr<Channel::Impl> hi;
    {
      BLELockGuard lock(isoMtx());
      if (!slotOk(slot)) {
        return;
      }
      hi = slots[slot].handle;
      if (!hi) {
        return;
      }
      hi->_nextSeq.store(0, std::memory_order_relaxed);
      cb = hi->onConnected;
    }
    if (cb) {
      Channel ch(hi);
      cb(ch);
    }
  }

  void dispatchDisconnected(int slot, uint8_t reason) {
    Channel::DisconnectedCb cb;
    std::shared_ptr<Channel::Impl> hi;
    {
      BLELockGuard lock(isoMtx());
      if (!slotOk(slot)) {
        return;
      }
      hi = slots[slot].handle;
      if (!hi) {
        return;
      }
      cb = hi->onDisconnected;
    }
    if (cb) {
      Channel ch(hi);
      cb(ch, reason);
    }
  }

  void dispatchReceive(int slot, const BLEIso::SduInfo &info, const uint8_t *sdu, uint16_t len) {
    Channel::ReceiveCb cb;
    std::shared_ptr<Channel::Impl> hi;
    {
      BLELockGuard lock(isoMtx());
      if (!slotOk(slot)) {
        return;
      }
      hi = slots[slot].handle;
      if (!hi) {
        return;
      }
      cb = hi->onReceive;
    }
    if (cb) {
      Channel ch(hi);
      cb(ch, info, sdu, len);
    }
  }

  void dispatchSent(int slot) {
    Channel::SentCb cb;
    std::shared_ptr<Channel::Impl> hi;
    {
      BLELockGuard lock(isoMtx());
      if (!slotOk(slot)) {
        return;
      }
      hi = slots[slot].handle;
      if (!hi) {
        return;
      }
      cb = hi->onSent;
    }
    if (cb) {
      Channel ch(hi);
      cb(ch);
    }
  }

  static void onChanConnected(esp_ble_iso_chan_t *chan) {
    Impl *self = transportSingleton().load(std::memory_order_acquire);
    if (self == nullptr) {
      return;
    }
    int slot = self->slotOf(chan);
    if (slot < 0) {
      return;
    }
    // Set up the HCI data path here, as every esp_ble_iso example does, so the
    // application only deals in connected/receive/sent events. A failure still
    // reports the channel as connected: it is up, only its SDUs will not flow.
    esp_ble_iso_chan_path_t path = {};
    path.pid = ESP_BLE_ISO_DATA_PATH_HCI;
    path.format = ESP_BLE_ISO_CODING_FORMAT_TRANSPARENT;
    esp_err_t err;
    if (self->slots[slot].hasTx.load(std::memory_order_relaxed)
        && (err = esp_ble_iso_setup_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_INPUT, &path)) != ESP_OK) {
      log_e("BLEIso: channel %d: Tx (host to controller) data path setup failed: %d", slot, err);
    }
    if (self->slots[slot].hasRx && (err = esp_ble_iso_setup_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT, &path)) != ESP_OK) {
      log_e("BLEIso: channel %d: Rx (controller to host) data path setup failed: %d", slot, err);
    }
    self->slots[slot].connected.store(true, std::memory_order_release);
    self->dispatchConnected(slot);
  }

  static void onChanDisconnected(esp_ble_iso_chan_t *chan, uint8_t reason) {
    Impl *self = transportSingleton().load(std::memory_order_acquire);
    if (self == nullptr) {
      return;
    }
    int slot = self->slotOf(chan);
    if (slot < 0) {
      return;
    }
    Slot &s = self->slots[slot];
    s.connected.store(false, std::memory_order_release);
    // Clear the accept flag alongside connected: otherwise end() would keep
    // treating a disconnected-but-accepted Peripheral slot as live and wedge
    // waiting for a release that never comes.
    s.accepted.store(false, std::memory_order_release);
    // A Central CIS keeps its handle and data path across disconnection (Core
    // 6.0 Vol 4 Part E 7.7.5); remove it or the next setup returns Command
    // Disallowed. Peripheral CIS and BIS paths are torn down by the controller.
    if (s.kind == Kind::CisCentral) {
      if (s.hasTx.load(std::memory_order_relaxed)) {
        esp_ble_iso_remove_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_INPUT);
      }
      if (s.hasRx) {
        esp_ble_iso_remove_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT);
      }
    }
#if BLE_ISO_BROADCASTER_SUPPORTED
    // Single-BIS BIG: its only BIS going down (terminate, or a failed create) ends the BIG.
    if (s.kind == Kind::BisBroadcaster) {
      self->bigTx.store(nullptr, std::memory_order_release);
    }
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
    // A lost or failed BIG sync is re-issued on the next BIGInfo while armed.
    {
      BLELockGuard lock(isoMtx());
      if (slot == self->sync.slot) {
        self->sync.synced = false;
        self->sync.big = nullptr;
      }
    }
#endif
    self->dispatchDisconnected(slot, reason);
    self->notifyIsoIdle();
  }

  // Hot path (once per SDU interval): decode the flags and forward, no logging.
  static void onChanRecv(esp_ble_iso_chan_t *chan, const esp_ble_iso_recv_info_t *info, const uint8_t *data, uint16_t len) {
    Impl *self = transportSingleton().load(std::memory_order_acquire);
    if (self == nullptr) {
      return;
    }
    int slot = self->slotOf(chan);
    if (slot < 0) {
      return;
    }
    BLEIso::SduInfo sdu;
    if (info) {
      sdu.timestampUs = info->ts;
      sdu.seqNum = info->seq_num;
      sdu.valid = (info->flags & ESP_BLE_ISO_FLAGS_VALID) != 0 && (info->flags & (ESP_BLE_ISO_FLAGS_ERROR | ESP_BLE_ISO_FLAGS_LOST)) == 0;
      sdu.timestampValid = (info->flags & ESP_BLE_ISO_FLAGS_TS) != 0;
    }
    self->dispatchReceive(slot, sdu, data, len);
  }

  static void onChanSent(esp_ble_iso_chan_t *chan, void *userData) {
    (void)userData;
    Impl *self = transportSingleton().load(std::memory_order_acquire);
    if (self == nullptr) {
      return;
    }
    int slot = self->slotOf(chan);
    if (slot >= 0) {
      self->dispatchSent(slot);
    }
  }

  /*
   * GAP app callback: installed with esp_ble_iso_common_init when this transport
   * owns the host, or fed by bleIsoOnGapEvent() when audio owns it. Only the
   * Receiver needs it: the armed sync is issued on the first BIGInfo report.
   * A lost BIG sync clears `synced` (onChanDisconnected), so it is issued again
   * on the next BIGInfo once the host is synced to the train again.
   */
  static void onGapEvent(esp_ble_iso_gap_app_event_t *event) {
    Impl *self = transportSingleton().load(std::memory_order_acquire);
    if (event == nullptr || self == nullptr || !self->active.load(std::memory_order_acquire)) {
      return;
    }
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
    if (event->type == ESP_BLE_ISO_GAP_EVENT_BIGINFO_RECV) {
      self->doBigSync(event->biginfo_recv.sync_handle, event->biginfo_recv.nse, event->biginfo_recv.encryption != 0);
    }
#endif
  }
};  // struct BLEIso::Impl

uint8_t vendorPhy(BLEIso::Phy phy) {
  switch (phy) {
    case BLEIso::Phy::Phy1M:    return ESP_BLE_ISO_PHY_1M;
    case BLEIso::Phy::PhyCoded: return ESP_BLE_ISO_PHY_CODED;
    default:                    return ESP_BLE_ISO_PHY_2M;
  }
}

#if BLE_ISO_BROADCASTER_SUPPORTED || BLE_ISO_SYNC_RECEIVER_SUPPORTED
/**
 * @brief Pack a Broadcast Code string into the fixed-size key input (zero-padded).
 * @param out ESP_BLE_ISO_BROADCAST_CODE_SIZE bytes.
 * @return Number of code characters copied (0: no code).
 */
uint8_t packBroadcastCode(const String &code, uint8_t *out) {
  memset(out, 0, ESP_BLE_ISO_BROADCAST_CODE_SIZE);
  size_t n = code.length();
  if (n > ESP_BLE_ISO_BROADCAST_CODE_SIZE) {
    log_w("BLEIso: Broadcast Code truncated to %u characters", (unsigned)ESP_BLE_ISO_BROADCAST_CODE_SIZE);
    n = ESP_BLE_ISO_BROADCAST_CODE_SIZE;
  }
  memcpy(out, code.c_str(), n);
  return (uint8_t)n;
}
#endif

// --------------------------------------------------------------------------
// Channel (shared handle; thin wrapper over Channel::Impl)
// --------------------------------------------------------------------------

BLEIso::Channel::operator bool() const {
  return _impl && _impl->slot.load(std::memory_order_acquire) >= 0;
}

int BLEIso::Channel::slot() const {
  if (!_impl) {
    return -1;
  }
  return _impl->slot.load(std::memory_order_acquire);
}

bool BLEIso::Channel::isConnected() const {
  if (!_impl) {
    return false;
  }
  const int s = _impl->slot.load(std::memory_order_acquire);
  BLEIso::Impl *t = _impl->transport;
  return t->linkLive(s, _impl->gen) && t->slots[s].connected.load(std::memory_order_acquire);
}

bool BLEIso::Channel::canSend() const {
  if (!_impl) {
    return false;
  }
  const int s = _impl->slot.load(std::memory_order_acquire);
  BLEIso::Impl *t = _impl->transport;
  return t->linkLive(s, _impl->gen) && t->slots[s].hasTx.load(std::memory_order_relaxed);
}

BTStatus BLEIso::Channel::send(const uint8_t *sdu, uint16_t len) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  return send(sdu, len, _impl->_nextSeq.load(std::memory_order_relaxed));
}

// Hot path: no logging, the caller decides what a refused SDU means.
// A receive-only build (CONFIG_BT_ISO_TX unset) has no Tx path.
BTStatus BLEIso::Channel::send(const uint8_t *sdu, uint16_t len, uint16_t seqNum) {
  if (!_impl || !BLE.isInitialized() || BLE.isEnding()) {
    return BTStatus::InvalidState;
  }
  BLEIso::Impl *t = _impl->transport;
  const int slot = _impl->slot.load(std::memory_order_acquire);
  if (!t->linkLive(slot, _impl->gen)) {
    return BTStatus::InvalidState;
  }
  BLEIso::Impl::Slot &s = t->slots[slot];
  if (!s.hasTx.load(std::memory_order_relaxed) || !s.connected.load(std::memory_order_acquire)) {
    return BTStatus::Fail;
  }
#if ISO_UNICAST || BLE_ISO_BROADCASTER_SUPPORTED
  if (esp_ble_iso_chan_send(&s.chan, sdu, len, seqNum) != ESP_OK) {
    return BTStatus::Fail;
  }
  _impl->_nextSeq.store((uint16_t)(seqNum + 1), std::memory_order_relaxed);
  return BTStatus::OK;
#else
  (void)sdu;
  (void)len;
  return BTStatus::Fail;
#endif
}

BTStatus BLEIso::Channel::disconnect() {
  if (!_impl || !BLE.isInitialized() || BLE.isEnding()) {
    return BTStatus::InvalidState;
  }
  BLEIso::Impl *t = _impl->transport;
  const int slot = _impl->slot.load(std::memory_order_acquire);
  if (!t->linkLive(slot, _impl->gen)) {
    return BTStatus::InvalidState;
  }
  BLEIso::Impl::Slot &sl = t->slots[slot];
  esp_err_t err = ESP_ERR_INVALID_STATE;
  switch (sl.kind) {
#if ISO_UNICAST
    case Kind::CisCentral: err = esp_ble_iso_chan_disconnect(&sl.chan); break;
    case Kind::CisPeripheral:
    {
      bool neverAccepted = false;
      {
        // Probe under the lock, act outside it: the ISO contract forbids
        // holding isoMtx() across esp_ble_iso_* calls, so the disconnect
        // below runs unlocked (see "Never held across esp_ble_iso_*" above).
        BLELockGuard lock(isoMtx());
        if (sl.chan.iso == nullptr && !sl.accepted.load(std::memory_order_acquire)) {
          // Never accepted anything: stop listening and free the slot instead
          // of erroring on an idle channel. A slot that already served (or is
          // serving) a CIS keeps re-listening by design; see listenCis().
          neverAccepted = true;
        }
      }
      if (neverAccepted) {
        t->releaseSlot(slot);
        return BTStatus::OK;
      }
      err = esp_ble_iso_chan_disconnect(&sl.chan);
      break;
    }
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
    case Kind::BisBroadcaster:
    {
      esp_ble_iso_big_t *big = t->bigTx.load(std::memory_order_acquire);
      if (big) {
        err = esp_ble_iso_big_terminate(big);
        if (err == ESP_OK) {
          t->bigTx.store(nullptr, std::memory_order_release);
        }
      }
      break;
    }
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
    case Kind::BisReceiver:
    {
      // Disarm first so the next BIGInfo does not sync again (and an in-flight
      // doBigSync() terminates its newly created BIG on completion).
      esp_ble_iso_big_t *toTerminate = nullptr;
      {
        BLELockGuard lock(isoMtx());
        t->sync.armed = false;
        toTerminate = t->sync.big;
        t->sync.big = nullptr;
        t->sync.synced = false;
      }
      err = toTerminate ? esp_ble_iso_big_terminate(toTerminate) : ESP_OK;
      break;
    }
#endif
    default: break;
  }
  if (err != ESP_OK) {
    log_w("BLEIso: channel %d: disconnect failed: %d", slot, err);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

void BLEIso::Channel::onConnected(ConnectedCb cb) {
  if (_impl) {
    BLELockGuard lock(isoMtx());
    _impl->onConnected = std::move(cb);
  }
}

void BLEIso::Channel::onDisconnected(DisconnectedCb cb) {
  if (_impl) {
    BLELockGuard lock(isoMtx());
    _impl->onDisconnected = std::move(cb);
  }
}

void BLEIso::Channel::onReceive(ReceiveCb cb) {
  if (_impl) {
    BLELockGuard lock(isoMtx());
    _impl->onReceive = std::move(cb);
  }
}

void BLEIso::Channel::onSent(SentCb cb) {
  if (_impl) {
    BLELockGuard lock(isoMtx());
    _impl->onSent = std::move(cb);
  }
}

void BLEIso::Channel::resetCallbacks() {
  if (!_impl) {
    return;
  }
  BLELockGuard lock(isoMtx());
  _impl->onConnected = nullptr;
  _impl->onDisconnected = nullptr;
  _impl->onReceive = nullptr;
  _impl->onSent = nullptr;
}

// --------------------------------------------------------------------------
// BLEIso (controller; owns the transport Impl)
// --------------------------------------------------------------------------

BLEIso::operator bool() const {
  return (bool)_impl;
}

// Claims the host unless the LE Audio engine already owns it (then ISO rides on it).
BTStatus BLEIso::begin() {
  if (!_impl || !BLE.isInitialized() || BLE.isEnding()) {
    return BTStatus::InvalidState;
  }
  return _impl->begin();
}

BTStatus BLEIso::end() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  return _impl->end();
}

bool BLEIso::isActive() const {
  return _impl && _impl->active.load(std::memory_order_acquire);
}

BTStatus BLEIso::Impl::begin() {
  if (!BLE.isInitialized() || BLE.isEnding()) {
    return BTStatus::InvalidState;
  }
  if (active.load(std::memory_order_acquire)) {
    if (!endFailed.load(std::memory_order_acquire)) {
      return BTStatus::OK;
    }
    // The previous end() left ISO resources behind; retry the teardown
    // instead of reporting OK over a half-torn-down session.
    log_w("BLEIso: retrying the incomplete end() before begin()");
    const BTStatus st = end();
    if (!st) {
      return st;
    }
  }
  if (!sSharedHost.load(std::memory_order_acquire)) {
    esp_ble_iso_init_info_t info = {};
    info.gap_cb = onGapEvent;
    esp_err_t err = esp_ble_iso_common_init(&info);
    if (err != ESP_OK) {
      log_e("BLEIso: ISO transport init failed: esp_ble_iso_common_init returned %d", err);
      return BTStatus::Fail;
    }
    sOwnsHost.store(true, std::memory_order_release);
  }
  resetState();
  active.store(true, std::memory_order_release);
  endFailed.store(false, std::memory_order_release);
  log_i("BLEIso: transport up (%s host)", sOwnsHost.load(std::memory_order_relaxed) ? "own" : "shared LE Audio");
  return BTStatus::OK;
}

BTStatus BLEIso::Impl::end() {
  if (!active.load(std::memory_order_acquire)) {
    return BTStatus::OK;
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("BLEIso: end() cannot run from a stack callback; defer it to loop()");
    return BTStatus::Busy;
  }

#if ISO_UNICAST
  for (int i = 0; i < kMaxChan; i++) {
    // Atomics only: stack-owned chan.state/iso cannot be read safely from this
    // (user) task while the host task may mutate them; connected/accepted are
    // the cross-task liveness bits (see isIdle()).
    if (!slots[i].used.load(std::memory_order_acquire) || (slots[i].kind != Kind::CisCentral && slots[i].kind != Kind::CisPeripheral)
        || (!slots[i].connected.load(std::memory_order_acquire) && !slots[i].accepted.load(std::memory_order_acquire))) {
      continue;
    }
    esp_err_t err = esp_ble_iso_chan_disconnect(&slots[i].chan);
    if (err != ESP_OK) {
      log_e("BLEIso: channel %d disconnect during end failed: %d", i, err);
    }
  }
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
  if (esp_ble_iso_big_t *big = bigTx.load(std::memory_order_acquire)) {
    esp_err_t err = esp_ble_iso_big_terminate(big);
    if (err != ESP_OK) {
      log_e("BLEIso: BIG terminate during end failed: %d", err);
    }
  }
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  {
    esp_ble_iso_big_t *toTerminate = nullptr;
    {
      BLELockGuard lock(isoMtx());
      sync.armed = false;
      toTerminate = sync.big;
      sync.big = nullptr;
      sync.synced = false;
    }
    if (toTerminate) {
      esp_err_t err = esp_ble_iso_big_terminate(toTerminate);
      if (err != ESP_OK) {
        log_e("BLEIso: BIG sync terminate during end failed: %d", err);
      }
    }
  }
#endif

  if (!waitDisconnected()) {
    endFailed.store(true, std::memory_order_release);
    return BTStatus::Timeout;
  }

#if BLE_ISO_CIS_CENTRAL_SUPPORTED
  if (cig) {
    esp_err_t err = esp_ble_iso_cig_terminate(cig);
    if (err != ESP_OK) {
      log_e("BLEIso: CIG terminate during end failed: %d", err);
      endFailed.store(true, std::memory_order_release);
      return BTStatus::Fail;
    }
    cig = nullptr;
  }
#endif
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
  if (serverRegistered) {
    esp_err_t err = esp_ble_iso_server_unregister(&server);
    if (err != ESP_OK) {
      log_e("BLEIso: server unregister during end failed: %d", err);
      endFailed.store(true, std::memory_order_release);
      return BTStatus::Fail;
    }
  }
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
  if (bigAdv >= 0) {
    esp_ble_iso_ext_adv_info_t advInfo = {};
    advInfo.adv_handle = (uint8_t)bigAdv;
    esp_err_t err = esp_ble_iso_big_ext_adv_delete(&advInfo);
    if (err != ESP_OK) {
      log_e("BLEIso: advertising set release during end failed: %d", err);
      endFailed.store(true, std::memory_order_release);
      return BTStatus::Fail;
    }
  }
#endif
  if (sOwnsHost.load(std::memory_order_acquire)) {
    esp_err_t err = esp_ble_iso_common_deinit(nullptr);
    if (err != ESP_OK) {
      log_e("BLEIso: ISO deinit failed: %d (disconnect every CIS/BIS first)", err);
      endFailed.store(true, std::memory_order_release);
      return BTStatus::Fail;
    }
    sOwnsHost.store(false, std::memory_order_release);
  }
  resetState();
  active.store(false, std::memory_order_release);
  endFailed.store(false, std::memory_order_release);
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Factories (enabled roles)
// --------------------------------------------------------------------------

#if BLE_ISO_CIS_CENTRAL_SUPPORTED
BLEIso::Channel BLEIso::connectCis(uint16_t connHandle, const CisParams &p) {
  if (!_impl) {
    return Channel();
  }
  // Validate before the CIG exists: creating it first would waste HCI and
  // race end(), and a recycled handle could address the wrong peer.
  if (connHandle == 0xFFFF || !BLEIsoInternal::aclHandleLive(connHandle)) {
    log_e("BLEIso: connectCis(): no live ACL for handle %u (passed a disconnected client?)", connHandle);
    return Channel();
  }
  Channel c = _impl->takeChannel("connectCis", Kind::CisCentral);
  if (!c) {
    return Channel();
  }
  return _impl->finish(c, _impl->cisConnect(c._impl->slot.load(std::memory_order_acquire), connHandle, p), "CIS connect");
}
#endif

#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
BLEIso::Channel BLEIso::listenCis(const CisParams &p) {
  if (!_impl) {
    return Channel();
  }
  Channel c = _impl->takeChannel("listenCis", Kind::None);
  if (!c) {
    return Channel();
  }
  return _impl->finish(c, _impl->cisListen(c._impl->slot.load(std::memory_order_acquire), p), "CIS listen");
}
#endif

#if BLE_ISO_BROADCASTER_SUPPORTED
BLEIso::Channel BLEIso::createBig(uint8_t advHandle, const BigParams &p) {
  if (!_impl) {
    return Channel();
  }
  Channel c = _impl->takeChannel("createBig", Kind::BisBroadcaster);
  if (!c) {
    return Channel();
  }
  return _impl->finish(c, _impl->bigCreate(c._impl->slot.load(std::memory_order_acquire), advHandle, p), "BIG create");
}
#endif

// Sync timeout 100 x 10 ms = 1 s without a BIS PDU before the sync is lost.
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
BLEIso::Channel BLEIso::syncBig(uint8_t bisIndex, const BigParams &p) {
  if (!_impl) {
    return Channel();
  }
  if (bisIndex == 0 || bisIndex > 31) {
    // ESP_BLE_ISO_BIS_INDEX_BIT(bisIndex) is 1UL << (bisIndex - 1); reject the
    // reserved index 0 and anything that would shift out of range (UB).
    log_e("BLEIso: syncBig() BIS index %u out of range (1..31)", bisIndex);
    return Channel();
  }
  Channel c = _impl->takeChannel("syncBig", Kind::BisReceiver);
  if (!c) {
    return Channel();
  }
  return _impl->finish(c, _impl->bigSyncArm(c._impl->slot.load(std::memory_order_acquire), bisIndex, p, /*syncTimeout=*/100), "BIG sync arm");
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

BLEIso::Channel BLEIso::connectCis(uint16_t connHandle) {
  return connectCis(connHandle, CisParams{});
}

#if !BLE_ISO_CIS_CENTRAL_SUPPORTED
BLEIso::Channel BLEIso::connectCis(uint16_t, const CisParams &) {
  log_e("BLEIso: connectCis() is not enabled in this build (CONFIG_BT_ISO_CENTRAL)");
  return Channel();
}
#endif

BLEIso::Channel BLEIso::listenCis() {
  return listenCis(CisParams{});
}

#if !BLE_ISO_CIS_PERIPHERAL_SUPPORTED
BLEIso::Channel BLEIso::listenCis(const CisParams &) {
  log_e("BLEIso: listenCis() is not enabled in this build (CONFIG_BT_ISO_PERIPHERAL)");
  return Channel();
}
#endif

BLEIso::Channel BLEIso::createBig(uint8_t advHandle) {
  return createBig(advHandle, BigParams{});
}

#if !BLE_ISO_BROADCASTER_SUPPORTED
BLEIso::Channel BLEIso::createBig(uint8_t, const BigParams &) {
  log_e("BLEIso: createBig() is not enabled in this build (CONFIG_BT_ISO_BROADCASTER)");
  return Channel();
}
#endif

BLEIso::Channel BLEIso::syncBig(uint8_t bisIndex) {
  return syncBig(bisIndex, BigParams{});
}

#if !BLE_ISO_SYNC_RECEIVER_SUPPORTED
BLEIso::Channel BLEIso::syncBig(uint8_t, const BigParams &) {
  log_e("BLEIso: syncBig() is not enabled in this build (CONFIG_BT_ISO_SYNC_RECEIVER)");
  return Channel();
}
#endif

// --------------------------------------------------------------------------
// Controller singleton (mirrors BLEAudio's audioControllerSlot)
// --------------------------------------------------------------------------

namespace {

BLEIso &controllerSlot() {
  static BLEIso s;
  return s;
}

}  // namespace

std::atomic<BLEIso::Impl *> &transportSingleton() {
  static std::atomic<BLEIso::Impl *> t{nullptr};
  return t;
}

BLEIso BLEClass::getIsoController() {
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || !_impl || _impl->ending) {
    log_e("getIsoController: BLE not initialized");
    return BLEIso();
  }
  BLEIso &slot = controllerSlot();
  if (!slot) {
    slot = BLEIso(std::make_shared<BLEIso::Impl>());
    transportSingleton().store(slot.impl(), std::memory_order_release);
  }
  return slot;
}

// Called from the ISO pre-end hook (see isoPreEndTrampoline in BLE.cpp);
// silent when no controller was ever minted.
void BLEIsoInternal::preEnd() {
  BLEIso retained = controllerSlot();
  if (retained) {
    (void)retained.end();
  }
}

bool BLEIsoInternal::isActive() {
  BLEIso retained = controllerSlot();
  return retained && retained.isActive();
}

// --------------------------------------------------------------------------
// Host hand-over (BLEIsoHost.h, C linkage)
// --------------------------------------------------------------------------

bool bleIsoOwnsHost(void) {
  return sOwnsHost.load(std::memory_order_acquire);
}

bool bleIsoHostUp(void) {
  return sOwnsHost.load(std::memory_order_acquire) || sSharedHost.load(std::memory_order_acquire);
}

void bleIsoAttachShared(bool attached) {
  sSharedHost.store(attached, std::memory_order_release);
  // Detach only what exists: with no minted controller there can be no ISO
  // channel, so detaching while the LE Audio engine owns the host is a no-op.
  BLEIso::Impl *t = transportSingleton().load(std::memory_order_acquire);
  if (!attached && t != nullptr && t->active.load(std::memory_order_acquire) && !sOwnsHost.load(std::memory_order_acquire)) {
    // The audio engine's deinit already released the ISO layer. Complete the
    // attached raw channels with a reason first: unlike end(), which
    // disconnects natively (real reasons), and begin(), whose documented
    // contract drops previous-session channels, the audio-end() path would
    // otherwise lose them silently while the app still holds the channels.
    // 0x16 = terminated by the local host (no controller event follows).
    t->attachSharedDone();
  }
}

// Ignored unless shared: when this transport owns the host, onGapEvent is installed directly.
void bleIsoOnGapEvent(void *event) {
  if (sSharedHost.load(std::memory_order_acquire)) {
    BLEIso::Impl::onGapEvent(static_cast<esp_ble_iso_gap_app_event_t *>(event));
  }
}

#endif /* BLE_ISO_SUPPORTED */
