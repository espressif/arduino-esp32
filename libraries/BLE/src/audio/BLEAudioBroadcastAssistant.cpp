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
#include "audio/BLEAudioBroadcastAssistant.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "audio/BLEAudioEngineAssistant.h"
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
  std::weak_ptr<BLEAudio::Impl> audio;
  uint16_t conn = BLE_AUDIO_CONN_NONE;  ///< The Scan Delegator being driven.

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

  /** @return the remembered entry for @p srcId, or nullptr. */
  Known *find(uint8_t srcId) {
    for (auto &k : known) {
      if (k.used && k.srcId == srcId) {
        return &k;
      }
    }
    return nullptr;
  }

  /** Record or update a source's subgroup count (dropped silently when all slots are used). */
  void remember(uint8_t srcId, uint8_t numSubgroups) {
    Known *k = find(srcId);
    for (auto &slot : known) {
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
      if (e.conn_handle == conn) {
        conn = BLE_AUDIO_CONN_NONE;
        memset(known, 0, sizeof(known));
      }
      return;
    }
    if (BLE_AUDIO_EVT_GROUP(e.type) != BLE_AUDIO_GRP_BROADCAST_ASSISTANT || (e.conn_handle != BLE_AUDIO_CONN_NONE && e.conn_handle != conn)) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_BA_DISCOVERED:
        if (discoverCb) {
          auto *d = static_cast<const ble_audio_ba_discovered_t *>(e.data);
          discoverCb(bleAudioStatus(e.err), d ? d->recv_states : 0);
        }
        break;
      case BLE_AUDIO_EVT_BA_SOURCE_FOUND:
        if (sourceFoundCb && e.data) {
          auto *s = static_cast<const ble_audio_ba_source_t *>(e.data);
          BLEAudioBroadcastSourceInfo info;
          info.address = BTAddress(s->addr, static_cast<BTAddress::Type>(s->addr_type & 0x03));
          info.sid = s->sid;
          info.broadcastId = s->broadcast_id;
          info.name = s->name;
          info.rssi = s->rssi;
          info.paInterval = s->pa_interval;
          sourceFoundCb(info);
        }
        break;
      case BLE_AUDIO_EVT_BA_RECV_STATE:
        if (e.data) {
          auto *r = static_cast<const ble_audio_ba_recv_state_t *>(e.data);
          remember(r->src_id, r->num_subgroups);
          if (receiveStateCb) {
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
            receiveStateCb(st);
          }
        }
        break;
      case BLE_AUDIO_EVT_BA_RECV_STATE_REMOVED:
        if (e.data) {
          auto *r = static_cast<const ble_audio_ba_result_t *>(e.data);
          if (Known *k = find(r->src_id)) {
            k->used = false;
          }
          if (receiveStateCb) {
            BLEAudioReceiveState st;
            st.sourceId = r->src_id;
            st.removed = true;
            receiveStateCb(st);
          }
        }
        break;
      case BLE_AUDIO_EVT_BA_RESULT:
        if (resultCb && e.data) {
          auto *r = static_cast<const ble_audio_ba_result_t *>(e.data);
          resultCb(static_cast<Op>(r->op), bleAudioStatus(e.err));
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
  /* Source_IDs are per remote: forget them when switching peers. */
  if (connHandle != _impl->conn) {
    memset(_impl->known, 0, sizeof(_impl->known));
  }
  _impl->conn = connHandle;
  return bleAudioStatus(bleAudioBaDiscover(connHandle));
}

uint16_t BLEAudioBroadcastAssistant::getConnHandle() const {
  return _impl ? _impl->conn : BLE_AUDIO_CONN_NONE;
}

BTStatus BLEAudioBroadcastAssistant::startRemoteScan() {
  if (!_impl || _impl->conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  /* Tell the remote first (engine starts reporting), then run our own scanner for it. */
  BTStatus st = bleAudioStatus(bleAudioBaScanStart(_impl->conn, false));
  if (!st) {
    log_e("BroadcastAssistant: remote scan start on conn %u failed (%s)", _impl->conn, st.toString());
    return st;
  }
  BLEScan scan = BLE.getScan();
  if (scan.isScanning()) {
    scan.stop();
  }
  // Passive and unfiltered: sources are reported once per remote scan by the engine.
  scan.setActiveScan(false);
  scan.setFilterDuplicates(false);
  st = scan.startExtended(0);
  if (!st) {
    log_e("BroadcastAssistant: local extended scan failed to start (%s); remote scan cancelled", st.toString());
    bleAudioBaScanStop(_impl->conn);
  }
  return st;
}

BTStatus BLEAudioBroadcastAssistant::stopRemoteScan() {
  if (!_impl || _impl->conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  BLEScan scan = BLE.getScan();
  if (scan.isScanning()) {
    scan.stopExtended();
  }
  return bleAudioStatus(bleAudioBaScanStop(_impl->conn));
}

BTStatus BLEAudioBroadcastAssistant::addSource(const BLEAudioBroadcastSourceInfo &source, bool syncPa, uint32_t bisMask) {
  if (!_impl || _impl->conn == BLE_AUDIO_CONN_NONE) {
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
  return bleAudioStatus(bleAudioBaAddSource(_impl->conn, &p));
}

BTStatus BLEAudioBroadcastAssistant::modifySource(uint8_t sourceId, bool syncPa, uint32_t bisMask) {
  if (!_impl || _impl->conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  const Impl::Known *k = _impl->find(sourceId);
  ble_audio_ba_mod_src_t p = {};
  p.src_id = sourceId;
  p.pa_sync = syncPa;
  p.pa_interval = BLE_AUDIO_BA_PA_INTERVAL_UNKNOWN;
  p.num_subgroups = k && k->numSubgroups ? k->numSubgroups : 1;
  // A specific BIS mask names BISes of the first subgroup; "none" / "no preference" apply to all.
  const bool forAll = bisMask == 0 || bisMask == BLE_AUDIO_BA_BIS_NO_PREF;
  for (uint8_t i = 0; i < p.num_subgroups; i++) {
    p.bis_sync[i] = (i == 0 || forAll) ? bisMask : 0;
  }
  return bleAudioStatus(bleAudioBaModifySource(_impl->conn, &p));
}

BTStatus BLEAudioBroadcastAssistant::removeSource(uint8_t sourceId) {
  if (!_impl || _impl->conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  return bleAudioStatus(bleAudioBaRemoveSource(_impl->conn, sourceId));
}

BTStatus BLEAudioBroadcastAssistant::setBroadcastCode(uint8_t sourceId, const String &code) {
  if (!_impl || _impl->conn == BLE_AUDIO_CONN_NONE) {
    return BTStatus::NotConnected;
  }
  if (code.length() > BLE_AUDIO_BA_CODE_SIZE) {
    log_e("BroadcastAssistant: Broadcast Code is %u characters, the maximum is %u", (unsigned)code.length(), BLE_AUDIO_BA_CODE_SIZE);
    return BTStatus::InvalidParam;
  }
  /* Shorter codes are zero-padded to 16 octets, as BAP requires. */
  uint8_t buf[BLE_AUDIO_BA_CODE_SIZE] = {};
  memcpy(buf, code.c_str(), code.length());
  return bleAudioStatus(bleAudioBaSetBroadcastCode(_impl->conn, sourceId, buf));
}

BLEAudioBroadcastAssistant &BLEAudioBroadcastAssistant::onDiscovered(DiscoverCallback cb) {
  if (_impl) {
    _impl->discoverCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastAssistant &BLEAudioBroadcastAssistant::onSourceFound(SourceFoundCallback cb) {
  if (_impl) {
    _impl->sourceFoundCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastAssistant &BLEAudioBroadcastAssistant::onReceiveState(ReceiveStateCallback cb) {
  if (_impl) {
    _impl->receiveStateCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastAssistant &BLEAudioBroadcastAssistant::onResult(ResultCallback cb) {
  if (_impl) {
    _impl->resultCb = std::move(cb);
  }
  return *this;
}

void BLEAudioBroadcastAssistant::resetCallbacks() {
  if (_impl) {
    _impl->discoverCb = nullptr;
    _impl->sourceFoundCb = nullptr;
    _impl->receiveStateCb = nullptr;
    _impl->resultCb = nullptr;
  }
}

BLEAudioBroadcastAssistant BLEAudio::createBroadcastAssistant() {
#if BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED
  if (!_impl || !_impl->active) {
    log_e("createBroadcastAssistant(): call after audio.begin()");
    return BLEAudioBroadcastAssistant();
  }
  auto ba = std::make_shared<BLEAudioBroadcastAssistant::Impl>();
  ba->audio = _impl;
  _impl->roles.push_back(ba);
  /* Created after start too: the engine unit initialises lazily on first use. */
  _impl->roleApplies.push_back([]() -> BTStatus { return bleAudioStatus(bleAudioBaInit()); });
  /* `roles` owns the role until end(); a strong capture here would form a cycle. */
  std::weak_ptr<BLEAudioBroadcastAssistant::Impl> weak = ba;
  _impl->setHandler(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  return BLEAudioBroadcastAssistant(ba);
#else
  log_e("Broadcast assistant is not enabled in this build (CONFIG_BT_BAP_BROADCAST_ASSISTANT)");
  return BLEAudioBroadcastAssistant();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
