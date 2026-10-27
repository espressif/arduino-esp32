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

#include "iso/BLEIso.h"
#include "iso/BLEIsoHost.h"
#include "esp32-hal-log.h"

#include "esp_ble_iso_common_api.h"

#include <cstring>

/**
 * @file BLEIso.cpp
 * @brief Audio-agnostic ISO transport over the ESP-BLE-ISO engine.
 *
 * Owns a fixed pool of channels, maps the vendor channel ops and GAP callback
 * onto `Channel` objects and `BTStatus`, and implements the host hand-over of
 * `BLEIsoHost.h`. This is the only unit that names `esp_ble_iso_*` types, so
 * the public header stays stack-free.
 *
 * The IDF only compiles each role's entry points when its Kconfig is set
 * (CONFIG_BT_ISO_CENTRAL / PERIPHERAL / BROADCASTER / SYNC_RECEIVER), so each
 * role's code is behind its BLE_ISO_*_SUPPORTED guard.
 *
 * Vendor callbacks run on the ISO host task; everything else runs from the
 * application with the host idle. There is no locking.
 *
 * API contract is documented on the declarations in `BLEIso.h`; the
 * definitions below carry implementation notes only.
 */

#define ISO_UNICAST (BLE_ISO_CIS_CENTRAL_SUPPORTED || BLE_ISO_CIS_PERIPHERAL_SUPPORTED)

namespace BLEIso {

// --------------------------------------------------------------------------
// Channel internals
// --------------------------------------------------------------------------

/** Private access to Channel for the dispatch below, without public setters. */
struct ChannelAccess {
  /** @brief Attach a fresh channel to @p slot (callbacks from a previous use are dropped). */
  static void bind(Channel &c, int slot) {
    c.resetCallbacks();
    c._slot = slot;
    c._nextSeq = 0;
  }
  static void unbind(Channel &c) {
    c.resetCallbacks();
    c._slot = -1;
  }
  // Every connection numbers its SDUs from 0.
  static void connected(Channel &c) {
    c._nextSeq = 0;
    if (c._onConnected) {
      c._onConnected(c);
    }
  }
  static void disconnected(Channel &c, uint8_t reason) {
    if (c._onDisconnected) {
      c._onDisconnected(c, reason);
    }
  }
  static void receive(Channel &c, const SduInfo &info, const uint8_t *sdu, uint16_t len) {
    if (c._onReceive) {
      c._onReceive(c, info, sdu, len);
    }
  }
  static void sent(Channel &c) {
    if (c._onSent) {
      c._onSent(c);
    }
  }
};

namespace {

/** Concurrent ISO channels in the pool; matches the host's own channel limit. */
#ifdef CONFIG_BT_ISO_MAX_CHAN
constexpr int kMaxChan = CONFIG_BT_ISO_MAX_CHAN;
#else
constexpr int kMaxChan = 1;
#endif

/** What a slot is used for; set by the operation that configured it. */
enum class Kind : uint8_t {
  None = 0,
  CisCentral,      /*!< connectCis(). */
  CisPeripheral,   /*!< listenCis(). */
  BisBroadcaster,  /*!< createBig(). */
  BisReceiver,     /*!< syncBig(). */
};

/** Vendor state of one pool entry; `chan.ops` / `chan.qos` point into the same entry. */
struct Slot {
  bool used;
  Kind kind;
  bool hasTx;  // Tx QoS configured (host to controller).
  bool hasRx;  // Rx QoS configured (controller to host).
  esp_ble_iso_chan_t chan;
  esp_ble_iso_chan_ops_t ops;
  esp_ble_iso_chan_qos_t qos;
  esp_ble_iso_chan_io_qos_t txQos;
  esp_ble_iso_chan_io_qos_t rxQos;
};

/** Pool entry i backs sChannels[i]; i is the Channel's slot(). */
Slot sSlots[kMaxChan];
Channel sChannels[kMaxChan];

bool sActive;      // begin() succeeded and end() has not run.
bool sOwnsHost;    // This transport called esp_ble_iso_common_init.
bool sSharedHost;  // The LE Audio engine owns the host and ISO.

#if BLE_ISO_CIS_CENTRAL_SUPPORTED
esp_ble_iso_cig_t *sCig;  // Single-CIS CIG of the Central slot.
#endif
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
esp_ble_iso_server_t sServer;  // Accepts incoming CIS into listenCis() slots.
bool sServerRegistered;
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
esp_ble_iso_big_t *sBigTx;  // Broadcaster BIG.
int sBigAdv = -1;           // Advertising set registered with the ISO layer, or -1.
#endif

#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
/** BIG sync arm state: the sync itself is issued when a BIGInfo report arrives. */
struct SyncArm {
  bool armed = false;
  bool synced = false;  // Sync issued; cleared on PA sync loss.
  int slot = -1;
  uint32_t bitfield = 0;  // ESP_BLE_ISO_BIS_INDEX_BIT of the armed BIS.
  uint16_t timeout = 0;   // N * 10 ms.
  uint8_t bcode[ESP_BLE_ISO_BROADCAST_CODE_SIZE] = {};
  esp_ble_iso_big_t *big = nullptr;  // Synced Receiver BIG.
};
SyncArm sSync;
#endif

bool slotOk(int slot) {
  return slot >= 0 && slot < kMaxChan && sSlots[slot].used;
}

/** @brief Slot owning @p chan, -1 if it is not one of ours. */
int slotOf(const esp_ble_iso_chan_t *chan) {
  for (int i = 0; i < kMaxChan; i++) {
    if (sSlots[i].used && &sSlots[i].chan == chan) {
      return i;
    }
  }
  return -1;
}

#if BLE_ISO_CIS_CENTRAL_SUPPORTED || BLE_ISO_BROADCASTER_SUPPORTED
uint8_t vendorPhy(Phy phy) {
  switch (phy) {
    case Phy::Phy1M:    return ESP_BLE_ISO_PHY_1M;
    case Phy::PhyCoded: return ESP_BLE_ISO_PHY_CODED;
    default:            return ESP_BLE_ISO_PHY_2M;
  }
}
#endif

// --------------------------------------------------------------------------
// Channel ops (ISO host task): pool slot -> Channel -> std::function
// --------------------------------------------------------------------------

void onChanConnected(esp_ble_iso_chan_t *chan) {
  int slot = slotOf(chan);
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
  if (sSlots[slot].hasTx && (err = esp_ble_iso_setup_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_INPUT, &path)) != ESP_OK) {
    log_e("BLEIso: channel %d: Tx (host to controller) data path setup failed: %d", slot, err);
  }
  if (sSlots[slot].hasRx && (err = esp_ble_iso_setup_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT, &path)) != ESP_OK) {
    log_e("BLEIso: channel %d: Rx (controller to host) data path setup failed: %d", slot, err);
  }
  ChannelAccess::connected(sChannels[slot]);
}

void onChanDisconnected(esp_ble_iso_chan_t *chan, uint8_t reason) {
  int slot = slotOf(chan);
  if (slot < 0) {
    return;
  }
  const Slot &s = sSlots[slot];
  // A Central CIS keeps its handle and data path across disconnection (Core
  // 6.0 Vol 4 Part E 7.7.5); remove it or the next setup returns Command
  // Disallowed. Peripheral CIS and BIS paths are torn down by the controller.
  if (s.kind == Kind::CisCentral) {
    if (s.hasTx) {
      esp_ble_iso_remove_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_INPUT);
    }
    if (s.hasRx) {
      esp_ble_iso_remove_data_path(chan, ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT);
    }
  }
#if BLE_ISO_BROADCASTER_SUPPORTED
  // Single-BIS BIG: its only BIS going down (terminate, or a failed create) ends the BIG.
  if (s.kind == Kind::BisBroadcaster) {
    sBigTx = nullptr;
  }
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  // A lost or failed BIG sync is re-issued on the next BIGInfo while armed.
  if (slot == sSync.slot) {
    sSync.synced = false;
    sSync.big = nullptr;
  }
#endif
  ChannelAccess::disconnected(sChannels[slot], reason);
}

// Hot path (once per SDU interval): decode the flags and forward, no logging.
void onChanRecv(esp_ble_iso_chan_t *chan, const esp_ble_iso_recv_info_t *info, const uint8_t *data, uint16_t len) {
  int slot = slotOf(chan);
  if (slot < 0) {
    return;
  }
  SduInfo sdu;
  if (info) {
    sdu.timestampUs = info->ts;
    sdu.seqNum = info->seq_num;
    sdu.valid = (info->flags & ESP_BLE_ISO_FLAGS_VALID) != 0;
    sdu.timestampValid = (info->flags & ESP_BLE_ISO_FLAGS_TS) != 0;
  }
  ChannelAccess::receive(sChannels[slot], sdu, data, len);
}

void onChanSent(esp_ble_iso_chan_t *chan, void *userData) {
  (void)userData;
  int slot = slotOf(chan);
  if (slot >= 0) {
    ChannelAccess::sent(sChannels[slot]);
  }
}

// --------------------------------------------------------------------------
// Pool
// --------------------------------------------------------------------------

/** @brief First used slot of @p kind, -1 when there is none. */
int findSlot(Kind kind) {
  for (int i = 0; i < kMaxChan; i++) {
    if (sSlots[i].used && sSlots[i].kind == kind) {
      return i;
    }
  }
  return -1;
}

/**
 * @brief Whether the host holds no live reference to the channel in @p slot.
 *
 * An idle slot may be configured again by the operation of its kind. A
 * Central CIS stays a CIG member (it keeps `chan.iso`) after it disconnects;
 * that is still idle, because cisConnect() terminates the old CIG first.
 */
bool isIdle(int slot) {
  if (!slotOk(slot)) {
    return false;
  }
  const Slot &s = sSlots[slot];
  if (s.chan.state != BT_ISO_STATE_DISCONNECTED) {
    return false;
  }
  return s.kind == Kind::CisCentral || s.chan.iso == nullptr;
}

/** @brief Reserve a free slot, -1 if the pool is full. */
int allocSlot() {
  for (int i = 0; i < kMaxChan; i++) {
    if (!sSlots[i].used) {
      sSlots[i] = Slot{};
      sSlots[i].used = true;
      return i;
    }
  }
  return -1;
}

/** @brief Return a slot to the pool and unbind its Channel. */
void releaseSlot(int slot) {
  sSlots[slot] = Slot{};
  ChannelAccess::unbind(sChannels[slot]);
}

/**
 * @brief (Re)configure @p slot for @p kind with fresh vendor structs.
 *
 * The host must hold no reference to the slot's channel (see isIdle()).
 */
Slot &prepSlot(int slot, Kind kind) {
  Slot &s = sSlots[slot];
  s = Slot{};
  s.used = true;
  s.kind = kind;
  s.chan.ops = &s.ops;
  s.chan.qos = &s.qos;
  s.ops.connected = onChanConnected;
  s.ops.disconnected = onChanDisconnected;
  s.ops.recv = onChanRecv;
  s.ops.sent = onChanSent;
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
  s.hasTx = txSdu > 0;
  s.hasRx = rxSdu > 0;
  s.txQos.sdu = txSdu;
  s.txQos.phy = phy;
  s.txQos.rtn = rtn;
  s.rxQos.sdu = rxSdu;
  s.rxQos.phy = phy;
  s.rxQos.rtn = rtn;
  s.qos.tx = s.hasTx ? &s.txQos : nullptr;
  s.qos.rx = s.hasRx ? &s.rxQos : nullptr;
}

/** @brief Drop every slot, group handle and Channel binding; the host side must already be gone. */
void resetState() {
  for (int i = 0; i < kMaxChan; i++) {
    sSlots[i] = Slot{};
    ChannelAccess::unbind(sChannels[i]);
  }
#if BLE_ISO_CIS_CENTRAL_SUPPORTED
  sCig = nullptr;
#endif
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
  sServerRegistered = false;
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
  sBigTx = nullptr;
  sBigAdv = -1;
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  sSync = SyncArm{};
#endif
}

/**
 * @brief Common factory prologue: check the transport, take a slot, bind its Channel.
 *
 * Single-instance roles (CIS Central, BIS Broadcaster, BIS Receiver) reuse
 * their existing slot once the host released it, so going down and up again
 * never exhausts the pool.
 *
 * @param op    Factory name, for the log.
 * @param reuse Kind whose existing slot is reused, or Kind::None for a new slot.
 */
Channel *takeChannel(const char *op, Kind reuse) {
  if (!sActive) {
    log_e("BLEIso: %s() called before BLEIso::begin()", op);
    return nullptr;
  }
  int slot = reuse != Kind::None ? findSlot(reuse) : -1;
  if (slot >= 0 && !isIdle(slot)) {
    log_e("BLEIso: %s(): its channel is still up; disconnect() it and wait for onDisconnected", op);
    return nullptr;
  }
  if (slot < 0) {
    slot = allocSlot();
  }
  if (slot < 0) {
    log_w("BLEIso: %s(): all %u ISO channels in use (CONFIG_BT_ISO_MAX_CHAN)", op, (unsigned)kMaxChan);
    return nullptr;
  }
  ChannelAccess::bind(sChannels[slot], slot);
  return &sChannels[slot];
}

/** @brief Common factory epilogue: on error, log and give the slot back. */
Channel *finish(Channel *c, esp_err_t err, const char *op) {
  if (err == ESP_OK) {
    return c;
  }
  log_e("BLEIso: %s failed (err=%d)", op, err);
  releaseSlot(c->slot());
  return nullptr;
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
// Roles
// --------------------------------------------------------------------------

#if BLE_ISO_CIS_CENTRAL_SUPPORTED
/**
 * @brief Create a single-CIS CIG with this QoS and connect its CIS on @p connHandle.
 *
 * A CIG left by a previous CIS is terminated first, so an idle Central slot
 * can reconnect with new QoS. Completion arrives via onChanConnected().
 */
esp_err_t cisConnect(int slot, uint16_t connHandle, const CisParams &p) {
  // The CIG holds this slot's channel from a previous CIS: free it before the slot is reset.
  if (sCig) {
    esp_err_t err = esp_ble_iso_cig_terminate(sCig);
    if (err != ESP_OK) {
      log_e("BLEIso: terminating the previous CIG failed: %d (is its CIS still up?)", err);
      return err;
    }
    sCig = nullptr;
  }
  Slot &s = prepSlot(slot, Kind::CisCentral);
  setQos(s, p.sduSize, p.returnSduSize, vendorPhy(p.phy), p.rtn);

  esp_ble_iso_chan_t *channels[1] = {&s.chan};
  esp_ble_iso_cig_param_t cig = {};
  cig.cis_channels = channels;
  cig.num_cis = 1;
  cig.sca = ESP_BLE_ISO_SCA_UNKNOWN;
  cig.packing = p.packing;
  cig.framing = p.framing;
  cig.c_to_p_latency = p.latencyMs;
  cig.p_to_c_latency = p.latencyMs;
  cig.c_to_p_interval = p.sduIntervalUs;
  cig.p_to_c_interval = p.sduIntervalUs;
  esp_err_t err = esp_ble_iso_cig_create(&cig, &sCig);
  if (err != ESP_OK) {
    log_e("BLEIso: CIG create (%u us, %u/%u octets) failed: %d", (unsigned)p.sduIntervalUs, p.sduSize, p.returnSduSize, err);
    sCig = nullptr;
    return err;
  }

  esp_ble_iso_connect_param_t connectParam = {};
  connectParam.iso_chan = &s.chan;
  err = esp_ble_iso_chan_connect(&connectParam, connHandle, 1);
  if (err != ESP_OK) {
    // Drop the CIG too: the caller releases the slot, whose channel it references.
    log_e("BLEIso: CIS connect on ACL 0x%04x failed: %d", connHandle, err);
    esp_ble_iso_cig_terminate(sCig);
    sCig = nullptr;
  }
  return err;
}
#endif /* BLE_ISO_CIS_CENTRAL_SUPPORTED */

#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
// Incoming CIS request: hand out the first listening slot not yet connected, or reject.
int onCisAccept(const esp_ble_iso_accept_info_t *info, esp_ble_iso_chan_t **chan) {
  (void)info;
  for (int i = 0; i < kMaxChan; i++) {
    if (sSlots[i].used && sSlots[i].kind == Kind::CisPeripheral && sSlots[i].chan.iso == nullptr) {
      *chan = &sSlots[i].chan;
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
esp_err_t cisListen(int slot, const CisParams &p) {
  Slot &s = prepSlot(slot, Kind::CisPeripheral);
  setQos(s, p.returnSduSize, p.sduSize, ESP_BLE_ISO_PHY_2M, 0);
  if (!sServerRegistered) {
    sServer = esp_ble_iso_server_t{};
    sServer.accept = onCisAccept;
    esp_err_t err = esp_ble_iso_server_register(&sServer);
    if (err != ESP_OK) {
      log_e("BLEIso: ISO server registration failed: %d", err);
      return err;
    }
    sServerRegistered = true;
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
esp_err_t bigCreate(int slot, uint8_t advHandle, const BigParams &p) {
  if (sBigTx) {
    log_e("BLEIso: a BIG is already broadcasting; disconnect() it first");
    return ESP_ERR_INVALID_STATE;
  }
  // The ISO layer refuses a second registration of the same set: keep it across BIGs.
  esp_err_t err;
  if (sBigAdv != advHandle) {
    esp_ble_iso_ext_adv_info_t advInfo = {};
    if (sBigAdv >= 0) {
      advInfo.adv_handle = (uint8_t)sBigAdv;
      esp_ble_iso_big_ext_adv_delete(&advInfo);
      sBigAdv = -1;
    }
    advInfo.adv_handle = advHandle;
    err = esp_ble_iso_big_ext_adv_add(&advInfo);
    if (err != ESP_OK) {
      log_e("BLEIso: registering advertising set %u for the BIG failed: %d", advHandle, err);
      return err;
    }
    sBigAdv = advHandle;
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
  sBigTx = nullptr;
  err = esp_ble_iso_big_create(advHandle, &param, &sBigTx);
  if (err != ESP_OK) {
    log_e("BLEIso: BIG create on advertising set %u (%u us, %u octets) failed: %d", advHandle, (unsigned)p.sduIntervalUs, p.sduSize, err);
    sBigTx = nullptr;
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
esp_err_t bigSyncArm(int slot, uint8_t bisIndex, const BigParams &p, uint16_t syncTimeout) {
  setQos(prepSlot(slot, Kind::BisReceiver), 0, p.sduSize, ESP_BLE_ISO_PHY_2M, 0);
  sSync = SyncArm{};
  sSync.slot = slot;
  sSync.bitfield = ESP_BLE_ISO_BIS_INDEX_BIT(bisIndex);
  sSync.timeout = syncTimeout;
  packBroadcastCode(p.broadcastCode, sSync.bcode);
  sSync.armed = true;
  return ESP_OK;
}

/**
 * @brief Issue the armed BIG sync from a BIGInfo report (once per arm).
 *
 * @param nse        Subevents per BIS event, used as the max subevents to receive.
 * @param encryption From the BIGInfo; the armed code is applied only when set.
 */
void doBigSync(uint16_t syncHandle, uint8_t nse, bool encryption) {
  if (!sSync.armed || sSync.synced || !slotOk(sSync.slot)) {
    return;
  }
  esp_ble_iso_chan_t *bis[1] = {&sSlots[sSync.slot].chan};
  esp_ble_iso_big_sync_param_t param = {};
  param.bis_channels = bis;
  param.num_bis = 1;
  param.bis_bitfield = sSync.bitfield;
  param.mse = nse;
  param.sync_timeout = sSync.timeout;
  // The BIG's advertised encryption flag is authoritative; the armed
  // Broadcast Code is only applied when the group is actually encrypted.
  param.encryption = encryption;
  if (encryption) {
    memcpy(param.bcode, sSync.bcode, ESP_BLE_ISO_BROADCAST_CODE_SIZE);
  }
  esp_err_t err = esp_ble_iso_big_sync(syncHandle, &param, &sSync.big);
  if (err != ESP_OK) {
    // Left armed: the next BIGInfo report retries.
    log_e("BLEIso: BIG sync to BIS bitfield 0x%08lx on PA sync 0x%04x failed: %d", (unsigned long)sSync.bitfield, syncHandle, err);
    return;
  }
  sSync.synced = true;
}
#endif /* BLE_ISO_SYNC_RECEIVER_SUPPORTED */

/*
 * GAP app callback: installed with esp_ble_iso_common_init when this transport
 * owns the host, or fed by bleIsoOnGapEvent() when audio owns it. Only the
 * Receiver needs it: the armed sync is issued on the first BIGInfo report.
 * A lost BIG sync clears `synced` (onChanDisconnected), so it is issued again
 * on the next BIGInfo once the host is synced to the train again.
 */
void onGapEvent(esp_ble_iso_gap_app_event_t *event) {
  if (event == nullptr || !sActive) {
    return;
  }
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  if (event->type == ESP_BLE_ISO_GAP_EVENT_BIGINFO_RECV) {
    doBigSync(event->biginfo_recv.sync_handle, event->biginfo_recv.nse, event->biginfo_recv.encryption != 0);
  }
#endif
}

}  // namespace

// --------------------------------------------------------------------------
// Channel
// --------------------------------------------------------------------------

bool Channel::isConnected() const {
  return slotOk(_slot) && sSlots[_slot].chan.state == BT_ISO_STATE_CONNECTED;
}

bool Channel::canSend() const {
  return slotOk(_slot) && sSlots[_slot].hasTx;
}

BTStatus Channel::send(const uint8_t *sdu, uint16_t len) {
  return send(sdu, len, _nextSeq);
}

// Hot path: no logging, the caller decides what a refused SDU means.
// A receive-only build (CONFIG_BT_ISO_TX unset) has no Tx path.
BTStatus Channel::send(const uint8_t *sdu, uint16_t len, uint16_t seqNum) {
  if (!slotOk(_slot)) {
    return BTStatus::InvalidState;
  }
  Slot &s = sSlots[_slot];
  if (!s.hasTx || s.chan.state != BT_ISO_STATE_CONNECTED) {
    return BTStatus::Fail;
  }
#if ISO_UNICAST || BLE_ISO_BROADCASTER_SUPPORTED
  if (esp_ble_iso_chan_send(&s.chan, sdu, len, seqNum) != ESP_OK) {
    return BTStatus::Fail;
  }
  _nextSeq = (uint16_t)(seqNum + 1);
  return BTStatus::OK;
#else
  (void)sdu;
  (void)len;
  return BTStatus::Fail;
#endif
}

BTStatus Channel::disconnect() {
  if (!slotOk(_slot)) {
    return BTStatus::InvalidState;
  }
  esp_err_t err = ESP_ERR_INVALID_STATE;
  switch (sSlots[_slot].kind) {
#if ISO_UNICAST
    case Kind::CisCentral:
    case Kind::CisPeripheral: err = esp_ble_iso_chan_disconnect(&sSlots[_slot].chan); break;
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
    case Kind::BisBroadcaster:
      if (sBigTx) {
        err = esp_ble_iso_big_terminate(sBigTx);
        if (err == ESP_OK) {
          sBigTx = nullptr;
        }
      }
      break;
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
    case Kind::BisReceiver:
      // Disarm first so the next BIGInfo does not sync again.
      sSync.armed = false;
      err = ESP_OK;
      if (sSync.big) {
        err = esp_ble_iso_big_terminate(sSync.big);
        if (err == ESP_OK) {
          sSync.big = nullptr;
          sSync.synced = false;
        }
      }
      break;
#endif
    default: break;
  }
  if (err != ESP_OK) {
    log_w("BLEIso: channel %d: disconnect failed: %d", _slot, err);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

void Channel::resetCallbacks() {
  _onConnected = nullptr;
  _onDisconnected = nullptr;
  _onReceive = nullptr;
  _onSent = nullptr;
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

// Claims the host unless the LE Audio engine already owns it (then ISO rides on it).
BTStatus begin() {
  if (sActive) {
    return BTStatus::OK;
  }
  if (!sSharedHost) {
    esp_ble_iso_init_info_t info = {};
    info.gap_cb = onGapEvent;
    esp_err_t err = esp_ble_iso_common_init(&info);
    if (err != ESP_OK) {
      log_e("BLEIso: ISO transport init failed: esp_ble_iso_common_init returned %d", err);
      return BTStatus::Fail;
    }
    sOwnsHost = true;
  }
  resetState();
  sActive = true;
  log_i("BLEIso: transport up (%s host)", sOwnsHost ? "own" : "shared LE Audio");
  return BTStatus::OK;
}

/*
 * Group teardown results are ignored: a group that already ended is fine, and
 * common_deinit reports anything that really blocks the teardown. When it
 * fails the transport stays active with its channels bound, so the
 * application can disconnect them and call end() again.
 */
void end() {
  if (!sActive) {
    return;
  }
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
  if (sServerRegistered) {
    esp_ble_iso_server_unregister(&sServer);
  }
#endif
#if BLE_ISO_BROADCASTER_SUPPORTED
  if (sBigTx) {
    esp_ble_iso_big_terminate(sBigTx);
  }
  if (sBigAdv >= 0) {
    esp_ble_iso_ext_adv_info_t advInfo = {};
    advInfo.adv_handle = (uint8_t)sBigAdv;
    esp_ble_iso_big_ext_adv_delete(&advInfo);
  }
#endif
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  if (sSync.big) {
    esp_ble_iso_big_terminate(sSync.big);
  }
#endif
#if BLE_ISO_CIS_CENTRAL_SUPPORTED
  if (sCig) {
    esp_ble_iso_cig_terminate(sCig);
  }
#endif
  if (sOwnsHost) {
    esp_err_t err = esp_ble_iso_common_deinit(nullptr);
    if (err != ESP_OK) {
      log_e("BLEIso: ISO deinit failed: %d (disconnect every CIS/BIS first)", err);
      return;
    }
    sOwnsHost = false;
  }
  resetState();
  sActive = false;
}

bool isActive() {
  return sActive;
}

// --------------------------------------------------------------------------
// Factories
// --------------------------------------------------------------------------

Channel *connectCis(uint16_t connHandle, const CisParams &p) {
#if BLE_ISO_CIS_CENTRAL_SUPPORTED
  Channel *c = takeChannel("connectCis", Kind::CisCentral);
  if (!c) {
    return nullptr;
  }
  return finish(c, cisConnect(c->slot(), connHandle, p), "CIS connect");
#else
  (void)connHandle;
  (void)p;
  log_e("BLEIso: connectCis() is not enabled in this build (CONFIG_BT_ISO_CENTRAL)");
  return nullptr;
#endif
}

Channel *listenCis(const CisParams &p) {
#if BLE_ISO_CIS_PERIPHERAL_SUPPORTED
  Channel *c = takeChannel("listenCis", Kind::None);
  if (!c) {
    return nullptr;
  }
  return finish(c, cisListen(c->slot(), p), "CIS listen");
#else
  (void)p;
  log_e("BLEIso: listenCis() is not enabled in this build (CONFIG_BT_ISO_PERIPHERAL)");
  return nullptr;
#endif
}

Channel *createBig(uint8_t advHandle, const BigParams &p) {
#if BLE_ISO_BROADCASTER_SUPPORTED
  Channel *c = takeChannel("createBig", Kind::BisBroadcaster);
  if (!c) {
    return nullptr;
  }
  return finish(c, bigCreate(c->slot(), advHandle, p), "BIG create");
#else
  (void)advHandle;
  (void)p;
  log_e("BLEIso: createBig() is not enabled in this build (CONFIG_BT_ISO_BROADCASTER)");
  return nullptr;
#endif
}

// Sync timeout 100 x 10 ms = 1 s without a BIS PDU before the sync is lost.
Channel *syncBig(uint8_t bisIndex, const BigParams &p) {
#if BLE_ISO_SYNC_RECEIVER_SUPPORTED
  Channel *c = takeChannel("syncBig", Kind::BisReceiver);
  if (!c) {
    return nullptr;
  }
  return finish(c, bigSyncArm(c->slot(), bisIndex, p, /*syncTimeout=*/100), "BIG sync arm");
#else
  (void)bisIndex;
  (void)p;
  log_e("BLEIso: syncBig() is not enabled in this build (CONFIG_BT_ISO_SYNC_RECEIVER)");
  return nullptr;
#endif
}

}  // namespace BLEIso

// --------------------------------------------------------------------------
// Host hand-over (BLEIsoHost.h, C linkage)
// --------------------------------------------------------------------------

bool bleIsoOwnsHost(void) {
  return BLEIso::sOwnsHost;
}

bool bleIsoHostUp(void) {
  return BLEIso::sOwnsHost || BLEIso::sSharedHost;
}

void bleIsoAttachShared(bool attached) {
  BLEIso::sSharedHost = attached;
  if (!attached && BLEIso::sActive && !BLEIso::sOwnsHost) {
    // The audio engine's deinit already released the ISO layer.
    BLEIso::resetState();
    BLEIso::sActive = false;
  }
}

// Ignored unless shared: when this transport owns the host, onGapEvent is installed directly.
void bleIsoOnGapEvent(void *event) {
  if (BLEIso::sSharedHost) {
    BLEIso::onGapEvent(static_cast<esp_ble_iso_gap_app_event_t *>(event));
  }
}

#endif /* BLE_ISO_SUPPORTED */
