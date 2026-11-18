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
#if BLE_AUDIO_SUPPORTED

#include <cstring>
#include "BLEAudioBroadcastAssistant.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioStreamImpl.h"
#include "engine/BLEAudioEngineAssistant.h"
#include "BLE.h"
#include "scan/BLEScan.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioBroadcastAssistant.cpp
 * @brief BAP Broadcast Assistant role handle.
 *
 * Wraps the assistant engine unit (BLEAudioEngineAssistant) for one remote
 * Scan Delegator at a time. A remote scan uses the application-facing
 * BLEScan (passive, extended); the engine filters its reports into
 * onSourceFound(). The handle remembers each remote source's subgroup count
 * because a Modify Source must list every subgroup.
 *
 * API contract is documented on the declarations in
 * `BLEAudioBroadcastAssistant.h`; the definitions below carry implementation
 * notes only.
 */

struct BLEAudioBroadcastAssistant::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (role list); same ownership as every other role.
  BLESharedMutex mtx;                     ///< Protects the callbacks and the state below.
  uint16_t conn = BLE_AUDIO_CONN_NONE;    ///< The Scan Delegator being driven.
  bool remoteScanning = false;            ///< startRemoteScan() is in effect (remote + local scan).

  // Subgroup count per remote source, needed to build a Modify Source.
  struct Known {
    bool used;
    uint8_t srcId;
    uint8_t numSubgroups;
  };
  Known known[4] = {};

  DiscoverCallback discoverCb;
  SourceFoundCallback sourceFoundCb;
  ReceiveStateCallback receiveStateCb;
  ResultCallback resultCb;

  /** @return a copy of the remembered entry for @p srcId; false when unknown. */
  bool find(uint8_t srcId, Known &out) {
    BLELockGuard lock(mtx.get());
    for (auto &k : known) {
      if (k.used && k.srcId == srcId) {
        out = k;
        return true;
      }
    }
    return false;
  }

  /** Drop the remembered entry for @p srcId, if any. */
  void forget(uint8_t srcId) {
    BLELockGuard lock(mtx.get());
    for (auto &k : known) {
      if (k.used && k.srcId == srcId) {
        k.used = false;
        return;
      }
    }
  }

  /** Record or update a source's subgroup count (dropped silently when all slots are used). */
  void remember(uint8_t srcId, uint8_t numSubgroups) {
    BLELockGuard lock(mtx.get());
    Known *k = nullptr;
    for (auto &slot : known) {
      if (slot.used && slot.srcId == srcId) {
        k = &slot;
        break;
      }
      if (!k && !slot.used) {
        k = &slot;
      }
    }
    if (k) {
      *k = {true, srcId, numSubgroups};
    }
  }

  /**
   * Engine event. A disconnect of the bound peer forgets it; assistant events
   * pass for the bound peer, plus scan reports (which carry no connection).
   */
  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED) {
      BLELockGuard lock(mtx.get());
      if (e.conn_handle == conn) {
        // Keep remoteScanning: the local scan is released by stopRemoteScan()
        // from the role teardown, which cannot run in a stack callback.
        conn = BLE_AUDIO_CONN_NONE;
        memset(known, 0, sizeof(known));
      }
      return;
    }
    DiscoverCallback dcb;
    SourceFoundCallback scb;
    ReceiveStateCallback rcb;
    ResultCallback rescb;
    {
      BLELockGuard lock(mtx.get());
      if (BLE_AUDIO_EVT_GROUP(e.type) != BLE_AUDIO_GRP_BROADCAST_ASSISTANT || (e.conn_handle != BLE_AUDIO_CONN_NONE && e.conn_handle != conn)) {
        return;
      }
      dcb = discoverCb;
      scb = sourceFoundCb;
      rcb = receiveStateCb;
      rescb = resultCb;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_BA_DISCOVERED:
        if (dcb) {
          auto *d = static_cast<const ble_audio_ba_discovered_t *>(e.data);
          dcb(bleAudioStatus(e.err), d ? d->recv_states : 0);
        }
        break;
      case BLE_AUDIO_EVT_BA_SOURCE_FOUND:
        if (scb && e.data) {
          auto *s = static_cast<const ble_audio_ba_source_t *>(e.data);
          BLEAudioBroadcastSourceInfo info;
          info.address = BTAddress(s->addr, static_cast<BTAddress::Type>(s->addr_type & 0x03));
          info.sid = s->sid;
          info.broadcastId = s->broadcast_id;
          info.name = s->name;
          info.rssi = s->rssi;
          info.paInterval = s->pa_interval;
          scb(info);
        }
        break;
      case BLE_AUDIO_EVT_BA_RECV_STATE:
        if (e.data) {
          auto *r = static_cast<const ble_audio_ba_recv_state_t *>(e.data);
          remember(r->src_id, r->num_subgroups);
          if (rcb) {
            BLEAudioReceiveState st;
            st.sourceId = r->src_id;
            st.address = BTAddress(r->addr, static_cast<BTAddress::Type>(r->addr_type & 0x03));
            st.sid = r->sid;
            st.broadcastId = r->broadcast_id;
            st.paState = static_cast<BLEAudioReceiveState::PaState>(r->pa_sync_state);
            st.encryption = static_cast<BLEAudioReceiveState::Encryption>(r->encrypt_state);
            memcpy(st.badCode, r->bad_code, sizeof(st.badCode));
            st.numSubgroups = r->num_subgroups;
            memcpy(st.bisSync, r->bis_sync, sizeof(st.bisSync));
            rcb(st);
          }
        }
        break;
      case BLE_AUDIO_EVT_BA_RECV_STATE_REMOVED:
        if (e.data) {
          auto *r = static_cast<const ble_audio_ba_result_t *>(e.data);
          forget(r->src_id);
          if (rcb) {
            BLEAudioReceiveState st;
            st.sourceId = r->src_id;
            st.removed = true;
            rcb(st);
          }
        }
        break;
      case BLE_AUDIO_EVT_BA_RESULT:
        if (e.data) {
          auto *r = static_cast<const ble_audio_ba_result_t *>(e.data);
          BTStatus st = bleAudioStatus(e.err);
          if (!st) {
            log_e("BroadcastAssistant: control point op %u failed (%s)", r->op, st.toString());
          }
          if (rescb) {
            rescb(static_cast<Op>(r->op), st);
          }
        }
        break;
      default: break;
    }
  }
};

BLEAudioBroadcastAssistant::BLEAudioBroadcastAssistant() = default;

BLEAudioBroadcastAssistant::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioBroadcastAssistant::discover(uint16_t connHandle) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t old;
  {
    BLELockGuard lock(_impl->mtx.get());
    old = _impl->conn;
  }
  /* Source_IDs are per remote: forget them when switching peers. */
  if (connHandle != old) {
    /* Release a scan started for the previous peer before rebinding. Runs
     * without the lock: it touches the shared scanner and the engine. */
    (void)stopRemoteScan();
    BLELockGuard lock(_impl->mtx.get());
    /* Bind only if nobody rebound concurrently; otherwise leave the winner's
     * binding alone (engine discovery below is per-connection anyway). */
    if (_impl->conn == old) {
      memset(_impl->known, 0, sizeof(_impl->known));
      _impl->conn = connHandle;
    }
  }
  return bleAudioStatus(bleAudioBaDiscover(connHandle));
}

uint16_t BLEAudioBroadcastAssistant::getConnHandle() const {
  if (!_impl) {
    return BLE_AUDIO_CONN_NONE;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->conn;
}

BTStatus BLEAudioBroadcastAssistant::startRemoteScan() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t conn;
  {
    BLELockGuard lock(_impl->mtx.get());
    conn = _impl->conn;
    if (conn == BLE_AUDIO_CONN_NONE) {
      log_e("BroadcastAssistant: startRemoteScan without a peer");
      return BTStatus::NotConnected;
    }
    if (_impl->remoteScanning) {
      // Repeated starts are a no-op: the remote is already scanning for us.
      return BTStatus::OK;
    }
    // Claim the scan now so concurrent starts cannot run twice.
    _impl->remoteScanning = true;
  }
  BLEScan scan = BLE.getScan();
  if (scan.isScanning()) {
    // The scanner is a single shared session; do not steal another role's scan.
    log_e("BroadcastAssistant: another extended scan is already active");
    BLELockGuard lock(_impl->mtx.get());
    _impl->remoteScanning = false;
    return BTStatus::Busy;
  }
  /* Tell the remote first (engine starts reporting), then run our own scanner for it. */
  BTStatus st = bleAudioStatus(bleAudioBaScanStart(conn, false));
  if (!st) {
    log_e("BroadcastAssistant: remote scan start on conn %u failed (%s)", conn, st.toString());
    BLELockGuard lock(_impl->mtx.get());
    _impl->remoteScanning = false;
    return st;
  }
  // Passive and unfiltered: sources are reported once per remote scan by the engine.
  scan.setActiveScan(false);
  scan.setFilterDuplicates(false);
  st = scan.startExtended(0);
  if (!st) {
    log_e("BroadcastAssistant: local extended scan failed to start (%s); remote scan canceled", st.toString());
    {
      BLELockGuard lock(_impl->mtx.get());
      _impl->remoteScanning = false;
    }
    bleAudioBaScanStop(conn);
  }
  return st;
}

BTStatus BLEAudioBroadcastAssistant::stopRemoteScan() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t conn;
  bool wasScanning;
  {
    BLELockGuard lock(_impl->mtx.get());
    conn = _impl->conn;
    wasScanning = _impl->remoteScanning;
    if (wasScanning) {
      // Release the shared scan first; a failed remote write must not leave it on.
      _impl->remoteScanning = false;
    }
  }
  if (wasScanning) {
    BLEScan scan = BLE.getScan();
    if (scan.isScanning()) {
      scan.stopExtended();
    }
  }
  if (conn == BLE_AUDIO_CONN_NONE) {
    // The peer is gone; the local scan was still released above.
    return BTStatus::NotConnected;
  }
  if (!wasScanning) {
    // Already stopped (or never started): no redundant Scan Stopped write, so
    // a role teardown after an explicit stop stays silent.
    return BTStatus::OK;
  }
  BTStatus st = bleAudioStatus(bleAudioBaScanStop(conn));
  if (!st) {
    // The write never reached the sink; let a later call retry the remote scan end.
    BLELockGuard lock(_impl->mtx.get());
    _impl->remoteScanning = true;
  }
  return st;
}

BTStatus BLEAudioBroadcastAssistant::addSource(const BLEAudioBroadcastSourceInfo &source, bool syncPa, uint32_t bisMask) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t conn;
  {
    BLELockGuard lock(_impl->mtx.get());
    conn = _impl->conn;
  }
  if (conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  ble_audio_ba_add_src_t p = {};
  p.addr_type = static_cast<uint8_t>(source.address.type());
  memcpy(p.addr, source.address.data(), 6);
  p.sid = source.sid;
  p.broadcast_id = source.broadcastId;
  p.pa_sync = syncPa;
  p.pa_interval = source.paInterval;
  p.num_subgroups = 1;
  p.bis_sync[0] = bisMask;
  return bleAudioStatus(bleAudioBaAddSource(conn, &p));
}

BTStatus BLEAudioBroadcastAssistant::modifySource(uint8_t sourceId, bool syncPa, uint32_t bisMask) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t conn;
  {
    BLELockGuard lock(_impl->mtx.get());
    conn = _impl->conn;
  }
  if (conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  Impl::Known k;
  const bool known = _impl->find(sourceId, k);
  ble_audio_ba_mod_src_t p = {};
  p.src_id = sourceId;
  p.pa_sync = syncPa;
  p.pa_interval = BLE_AUDIO_BA_PA_INTERVAL_UNKNOWN;
  p.num_subgroups = known && k.numSubgroups ? k.numSubgroups : 1;
  // A specific BIS mask names BISes of the first subgroup; "none" / "no preference" apply to all.
  const bool forAll = bisMask == 0 || bisMask == BLE_AUDIO_BA_BIS_NO_PREF;
  for (uint8_t i = 0; i < p.num_subgroups; i++) {
    p.bis_sync[i] = (i == 0 || forAll) ? bisMask : 0;
  }
  return bleAudioStatus(bleAudioBaModifySource(conn, &p));
}

BTStatus BLEAudioBroadcastAssistant::removeSource(uint8_t sourceId) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t conn;
  {
    BLELockGuard lock(_impl->mtx.get());
    conn = _impl->conn;
  }
  if (conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  return bleAudioStatus(bleAudioBaRemoveSource(conn, sourceId));
}

BTStatus BLEAudioBroadcastAssistant::setBroadcastCode(uint8_t sourceId, const String &code) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t conn;
  {
    BLELockGuard lock(_impl->mtx.get());
    conn = _impl->conn;
  }
  if (conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  if (code.length() > BLE_AUDIO_BA_CODE_SIZE) {
    log_e("BroadcastAssistant: Broadcast Code is %u characters, the maximum is %u", (unsigned)code.length(), BLE_AUDIO_BA_CODE_SIZE);
    return BTStatus::InvalidParam;
  }
  /* Shorter codes are zero-padded to 16 octets, as BAP requires. */
  uint8_t buf[BLE_AUDIO_BA_CODE_SIZE] = {};
  memcpy(buf, code.c_str(), code.length());
  return bleAudioStatus(bleAudioBaSetBroadcastCode(conn, sourceId, buf));
}

void BLEAudioBroadcastAssistant::onDiscovered(DiscoverCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoverCb = std::move(cb);
}

void BLEAudioBroadcastAssistant::onSourceFound(SourceFoundCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.sourceFoundCb = std::move(cb);
}

void BLEAudioBroadcastAssistant::onReceiveState(ReceiveStateCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.receiveStateCb = std::move(cb);
}

void BLEAudioBroadcastAssistant::onResult(ResultCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.resultCb = std::move(cb);
}

void BLEAudioBroadcastAssistant::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoverCb = nullptr;
  impl.sourceFoundCb = nullptr;
  impl.receiveStateCb = nullptr;
  impl.resultCb = nullptr;
}

#if BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED
BLEAudioBroadcastAssistant BLEAudio::createBroadcastAssistant() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioBroadcastAssistant();  // begin() already logged the cause
  }
  auto ba = std::make_shared<BLEAudioBroadcastAssistant::Impl>();
  ba->audio = _impl;
  if (_impl->started) {
    /* Created after the commit: the staged apply below would never run
     * (start() already drained roleApplies), so initialize eagerly. The
     * engine unit also initializes lazily on first use; both are idempotent. */
    const BTStatus st = bleAudioStatus(bleAudioBaInit());
    if (!st) {
      log_e("createBroadcastAssistant(): engine init failed (%s)", st.toString());
      return BLEAudioBroadcastAssistant();
    }
  } else {
    _impl->roleApplies.push_back([]() -> BTStatus {
      return bleAudioStatus(bleAudioBaInit());
    });
  }
  _impl->roles.push_back(ba);
  /* `roles` owns the role until end(); a strong capture here would form a cycle. */
  std::weak_ptr<BLEAudioBroadcastAssistant::Impl> weak = ba;
  _impl->setHandler(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleStops.push_back([weak]() {
    if (auto s = weak.lock()) {
      (void)BLEAudioBroadcastAssistant(s).stopRemoteScan();
    }
  });
  return BLEAudioBroadcastAssistant(ba);
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED
BLEAudioBroadcastAssistant BLEAudio::createBroadcastAssistant() {
  log_e("Broadcast assistant is not enabled in this build (CONFIG_BT_BAP_BROADCAST_ASSISTANT)");
  return BLEAudioBroadcastAssistant();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
