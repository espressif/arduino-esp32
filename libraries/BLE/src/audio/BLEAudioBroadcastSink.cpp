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

#include <vector>
#include "audio/BLEAudioBroadcastSink.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "BLE.h"
#include "scan/BLEScan.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioBroadcastSink.cpp
 * @brief BAP Broadcast Sink role on top of the BAP engine unit.
 *
 * Split of work:
 *  - The engine parses the scan reports it receives through the GAP forwarding
 *    (announcements -> BSINK_FOUND), creates the BAP sink when a PA sync to
 *    the armed Broadcast ID appears, reads the BASE and syncs the BIG.
 *  - This role drives the radio through the library's `BLEScan` (extended
 *    scan, PA sync create/cancel/terminate, PAST receive), so the same code
 *    runs on NimBLE and Bluedroid.
 *
 * API contract is documented on the declarations in `BLEAudioBroadcastSink.h`;
 * the definitions below carry implementation notes only.
 */

namespace {

/** PA sync timeout: generous so a source with a long periodic interval is not dropped. */
constexpr uint16_t kPaSyncTimeoutMs = 10000;

/** 16/24 kHz for voice-grade sources plus every 48 kHz preset (what Auracast sources use). */
constexpr uint32_t kDefaultPresets =
  BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_24_2_1)
  | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_1_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1)
  | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_3_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_4_1)
  | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_5_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_6_1);

}  // namespace

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioBroadcastSink::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (role list).
  uint8_t streamCount = 1;                ///< BISes to join (1 or 2).
  uint32_t presets = kDefaultPresets;     ///< BLEAudioPresetBit() mask for the sink PAC.
  BLEAudioLocation location = BLEAudioLocation::Mono;
  BLEAudioContext contexts = BLEAudioContext::Unspecified | BLEAudioContext::Media;
  bool delegator = BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED;
  String code;                            ///< Local Broadcast Code; empty = none.
  String targetName;                      ///< Auto-sync filter; empty = any.
  uint32_t targetId = 0;                  ///< Auto-sync filter; 0 = any.
  bool autoSync = true;
  bool applied = false;                   ///< Sink registered by BLEAudio::start().
  bool running = false;                   ///< Between start() and stop(): keep (re)scanning.
  bool syncPending = false;               ///< PA sync create / PAST receive in flight.
  bool synced = false;                    ///< PA sync established.
  bool streaming = false;                 ///< BIG joined.
  std::vector<BLEAudioStream> streams;    ///< One Rx stream per BIS.

  SourceFoundCallback foundCb;
  Callback syncedCb;
  BaseCallback baseCb;
  Callback codeCb;
  ReasonCallback syncLostCb;
  ErrorCallback syncFailedCb;
  Callback startedCb;
  ReasonCallback stoppedCb;

  /** @brief Match the owned streams to the BIS count (stops at the first pool failure). */
  void resizeStreams() {
    while (streams.size() > streamCount) {
      streams.pop_back();
    }
    while (streams.size() < streamCount) {
      BLEAudioStream s = BLEAudioStreamAccess::create(BLE_AUDIO_STREAM_BROADCAST_SINK);
      if (!s) {
        break;
      }
      streams.push_back(s);
    }
  }

  /** @brief Hand the local code to the engine (zero-padded to 16 octets; empty clears it). */
  void pushCode() {
    uint8_t bcode[BLE_AUDIO_BCODE_SIZE] = {};
    const size_t n = code.length() < sizeof(bcode) ? code.length() : sizeof(bcode);
    memcpy(bcode, code.c_str(), n);
    bleAudioBsinkSetCode(n ? bcode : nullptr);
  }

  /**
   * @brief Staged registration, run by BLEAudio::start().
   *
   * The sink PAC is required: the stack only reports BIS indexes of subgroups
   * whose codec matches a registered sink PAC. It is merged with a unicast
   * server's sink PAC when both roles exist.
   */
  BTStatus apply() {
    ble_audio_pac_t pac;
    bleAudioPacFromPresets(presets, 1, &pac);
    bleAudioPacsAdd(BLE_AUDIO_DIR_SINK, &pac, static_cast<uint32_t>(location), static_cast<uint16_t>(contexts));
    int err = bleAudioBsinkInit(delegator);
    if (err != 0) {
      log_e("BroadcastSink: init failed (delegator=%d err=%d)", delegator, err);
      return bleAudioStatus(err);
    }
    ble_audio_slot_t *slots[2] = {};
    const uint8_t n = streams.size() < 2 ? (uint8_t)streams.size() : 2;
    for (uint8_t i = 0; i < n; i++) {
      slots[i] = BLEAudioStreamAccess::slot(streams[i]);
    }
    err = bleAudioBsinkSetStreams(slots, n);
    if (err != 0) {
      log_e("BroadcastSink: stream setup failed (%u streams, err=%d)", n, err);
      return bleAudioStatus(err);
    }
    applied = true;
    pushCode();
    log_d("BroadcastSink: registered (%u streams, scan delegator %s)", n, delegator ? "on" : "off");
    return BTStatus::OK;
  }

  /**
   * @brief Start or stop the passive extended scan that finds sources.
   *
   * Duplicate filtering MUST be off: phones put the SyncInfo on ADV_EXT_IND
   * and the 0x1852/0x1856 announcements in the AUX packet. With filtering the
   * controller often reports only the empty primary and the sink never sees
   * a Broadcast ID.
   */
  BTStatus scan(bool on) {
    BLEScan sc = BLE.getScan();
    bleAudioBsinkSetScanning(on);
    if (!on) {
      return sc.stopExtended();
    }
    sc.setActiveScan(false);
    sc.setFilterDuplicates(false);
    BTStatus st = sc.startExtended(0);
    if (!st) {
      log_e("BroadcastSink: scan start failed (%s)", st.toString());
    } else {
      log_d("BroadcastSink: scanning for broadcast sources");
    }
    return st;
  }

  /**
   * @brief Arm the engine for @p broadcastId and create the PA sync.
   *
   * The scan keeps running until the sync is established: stopping it first
   * makes the controller miss the periodic train.
   */
  BTStatus sync(const BTAddress &addr, uint8_t sid, uint32_t broadcastId) {
    bleAudioBsinkArm(broadcastId);
    BTStatus st = BLE.getScan().createPeriodicSync(addr, sid, 0, kPaSyncTimeoutMs);
    if (!st) {
      log_w("BroadcastSink: PA sync to 0x%06lX failed (%s)", (unsigned long)broadcastId, st.toString());
    } else {
      log_d("BroadcastSink: PA syncing to 0x%06lX (%s sid=%u)", (unsigned long)broadcastId, addr.toString().c_str(), sid);
    }
    syncPending = (bool)st;
    return st;
  }

  /** @brief Auto-sync filter: every configured criterion must match. */
  bool matches(const BLEAudioBroadcastSourceInfo &s) const {
    return (!targetId || s.broadcastId == targetId) && (!targetName.length() || s.name == targetName);
  }

  /** @brief Event handler for the BROADCAST_SINK group (host task). */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_BSINK_FOUND: {
        const auto *f = static_cast<const ble_audio_bsink_found_t *>(e.data);
        BLEAudioBroadcastSourceInfo info;
        info.address = BTAddress(f->addr, static_cast<BTAddress::Type>(f->addr_type));
        info.sid = f->sid;
        info.broadcastId = f->broadcast_id;
        info.name = f->name;
        info.rssi = f->rssi;
        if (foundCb) {
          foundCb(info);
        }
        if (autoSync && running && !syncPending && !synced && matches(info)) {
          (void)sync(info.address, info.sid, info.broadcastId);
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_PA_SYNCED:
        syncPending = false;
        synced = (e.err == 0);
        if (synced) {
          log_i("BroadcastSink: PA synced, waiting for the BASE");
          (void)scan(false);
          if (syncedCb) {
            syncedCb();
          }
        } else {
          log_w("BroadcastSink: PA sync failed (status 0x%02x)", (unsigned)(uint8_t)e.err);
          if (running) {
            (void)scan(true);
          }
        }
        break;
      case BLE_AUDIO_EVT_BSINK_BASE: {
        const auto *b = static_cast<const ble_audio_bsink_base_t *>(e.data);
        BLEAudioBroadcastBaseInfo base;
        base.bisMask = b->bis_mask;
        base.subgroups = b->subgroups;
        base.presentationDelayUs = b->pd_us;
        base.codec = bleAudioCodecFromEngine(b->codec);
        log_d(
          "BroadcastSink: BASE with BIS mask 0x%08lx, %u subgroup(s), %lu Hz", (unsigned long)base.bisMask, base.subgroups,
          (unsigned long)base.codec.samplingRateHz
        );
        if (baseCb) {
          baseCb(base);
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_CODE:
        log_d("BroadcastSink: Broadcast Code received from an assistant (conn %u)", e.conn_handle);
        if (codeCb) {
          codeCb();
        }
        break;
      case BLE_AUDIO_EVT_BSINK_PA_LOST:
        synced = false;
        log_w("BroadcastSink: PA sync lost (reason 0x%02x)%s", (unsigned)(uint8_t)e.err, running ? ", rescanning" : "");
        if (syncLostCb) {
          syncLostCb((uint8_t)e.err);
        }
        if (running) {
          (void)scan(true);
        }
        break;
      case BLE_AUDIO_EVT_BSINK_SYNC_FAILED:
        // -EACCES: the BIG is encrypted and the code is missing or wrong.
        log_w("BroadcastSink: BIG sync failed (err=%d)%s", e.err, e.err == -EACCES ? ": Broadcast Code missing or wrong" : "");
        if (syncFailedCb) {
          syncFailedCb(e.err == -EACCES ? BTStatus::PermissionDenied : bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_BSINK_STARTED:
        streaming = true;
        log_i("BroadcastSink: BIG joined, %u streams", (unsigned)streams.size());
        if (startedCb) {
          startedCb();
        }
        break;
      case BLE_AUDIO_EVT_BSINK_STOPPED:
        streaming = false;
        log_i("BroadcastSink: BIG left (reason 0x%02x)", (unsigned)(uint8_t)e.err);
        if (stoppedCb) {
          stoppedCb((uint8_t)e.err);
        }
        break;
      case BLE_AUDIO_EVT_BSINK_PA_REQ: {
        // An assistant selected a source: sync to it directly, or wait for it
        // to transfer its own sync over the connection (PAST). The PAST
        // receive occupies the controller's single sync-create slot, so it is
        // only armed on request, never speculatively on connect.
        const auto *r = static_cast<const ble_audio_bsink_pa_req_t *>(e.data);
        BLEScan sc = BLE.getScan();
        BTStatus st = r->past ? sc.receivePeriodicSync(e.conn_handle, 0, kPaSyncTimeoutMs)
                              : sc.createPeriodicSync(BTAddress(r->addr, static_cast<BTAddress::Type>(r->addr_type)), r->sid, 0, kPaSyncTimeoutMs);
        if (!st) {
          log_w("BroadcastSink: assistant sync request for 0x%06lX failed (%s)", (unsigned long)r->broadcast_id, st.toString());
        } else {
          log_d("BroadcastSink: assistant sync to 0x%06lX via %s", (unsigned long)r->broadcast_id, r->past ? "PAST" : "scan");
        }
        syncPending = (bool)st;
        *r->reply = st ? 0 : -EIO;
        break;
      }
      case BLE_AUDIO_EVT_BSINK_PA_TERM_REQ:
        log_d("BroadcastSink: assistant asked to stop the PA sync");
        (void)BLE.getScan().terminatePeriodicSync(bleAudioBsinkSyncHandle());
        break;
      default: break;
    }
  }
};

// --------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------

BLEAudioBroadcastSink::BLEAudioBroadcastSink() = default;

BLEAudioBroadcastSink::operator bool() const {
  return _impl != nullptr;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setStreams(uint8_t count) {
  if (_impl && !_impl->applied) {
    _impl->streamCount = count < 1 ? 1 : (count > 2 ? 2 : count);
    _impl->resizeStreams();
  } else if (_impl) {
    log_w("BroadcastSink: setStreams() ignored after audio.start()");
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setSupportedPresets(uint32_t presetMask) {
  if (_impl && presetMask) {
    _impl->presets = presetMask;
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setLocation(BLEAudioLocation location) {
  if (_impl) {
    _impl->location = location;
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setContexts(BLEAudioContext contexts) {
  if (_impl) {
    _impl->contexts = contexts;
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setScanDelegator(bool enable) {
  if (_impl) {
    _impl->delegator = enable;
  }
  return *this;
}

// --------------------------------------------------------------------------
// Source selection
// --------------------------------------------------------------------------

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setBroadcastCode(const String &code) {
  if (_impl) {
    if (code.length() > BLE_AUDIO_BCODE_SIZE) {
      log_w("BroadcastSink: Broadcast Code truncated to %u characters", BLE_AUDIO_BCODE_SIZE);
    }
    _impl->code = code;
    if (_impl->applied) {
      _impl->pushCode();
    }
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setTargetName(const String &name) {
  if (_impl) {
    _impl->targetName = name;
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setTargetBroadcastId(uint32_t broadcastId) {
  if (_impl) {
    _impl->targetId = broadcastId & 0xFFFFFF;
  }
  return *this;
}

/** Stored in the engine, which applies it at the next BIG sync. */
BLEAudioBroadcastSink &BLEAudioBroadcastSink::setBisMask(uint32_t mask) {
  if (_impl) {
    bleAudioBsinkSetBisMask(mask);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::setAutoSync(bool enable) {
  if (_impl) {
    _impl->autoSync = enable;
  }
  return *this;
}

// --------------------------------------------------------------------------
// Control
// --------------------------------------------------------------------------

BTStatus BLEAudioBroadcastSink::start() {
  if (!_impl || !_impl->applied) {
    log_e("BroadcastSink: start() before audio.start()");
    return BTStatus::InvalidState;
  }
  _impl->running = true;
  return _impl->synced ? BTStatus::OK : _impl->scan(true);
}

BTStatus BLEAudioBroadcastSink::syncTo(const BLEAudioBroadcastSourceInfo &source) {
  if (!_impl || !_impl->applied) {
    log_e("BroadcastSink: syncTo() before audio.start()");
    return BTStatus::InvalidState;
  }
  if (_impl->syncPending || _impl->synced) {
    log_w("BroadcastSink: syncTo() while a sync is pending or established; stop() first");
    return BTStatus::InvalidState;
  }
  return _impl->sync(source.address, source.sid, source.broadcastId);
}

/**
 * Tear down in reverse order: scan, BIG, pending sync create, established PA
 * sync. Every step is best effort so a partial failure still releases the rest.
 */
BTStatus BLEAudioBroadcastSink::stop() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  _impl->running = false;
  (void)_impl->scan(false);
  int err = bleAudioBsinkStop();
  if (err != 0) {
    log_w("BroadcastSink: leaving the BIG failed (err=%d)", err);
  }
  BLEScan sc = BLE.getScan();
  if (_impl->syncPending) {
    (void)sc.cancelPeriodicSync();
    _impl->syncPending = false;
  }
  const uint16_t sh = bleAudioBsinkSyncHandle();
  if (sh != 0xFFFF) {
    (void)sc.terminatePeriodicSync(sh);
  }
  return bleAudioStatus(err);
}

bool BLEAudioBroadcastSink::isStreaming() const {
  return _impl && _impl->streaming;
}

// --------------------------------------------------------------------------
// Streams and callbacks
// --------------------------------------------------------------------------

size_t BLEAudioBroadcastSink::streamCount() const {
  return _impl ? _impl->streams.size() : 0;
}

BLEAudioStream BLEAudioBroadcastSink::stream(size_t index) const {
  return (_impl && index < _impl->streams.size()) ? _impl->streams[index] : BLEAudioStream();
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onSourceFound(SourceFoundCallback cb) {
  if (_impl) {
    _impl->foundCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onSynced(Callback cb) {
  if (_impl) {
    _impl->syncedCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onBaseReceived(BaseCallback cb) {
  if (_impl) {
    _impl->baseCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onBroadcastCodeReceived(Callback cb) {
  if (_impl) {
    _impl->codeCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onSyncLost(ReasonCallback cb) {
  if (_impl) {
    _impl->syncLostCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onSyncFailed(ErrorCallback cb) {
  if (_impl) {
    _impl->syncFailedCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onStarted(Callback cb) {
  if (_impl) {
    _impl->startedCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSink &BLEAudioBroadcastSink::onStopped(ReasonCallback cb) {
  if (_impl) {
    _impl->stoppedCb = std::move(cb);
  }
  return *this;
}

void BLEAudioBroadcastSink::resetCallbacks() {
  if (_impl) {
    _impl->foundCb = nullptr;
    _impl->syncedCb = nullptr;
    _impl->baseCb = nullptr;
    _impl->codeCb = nullptr;
    _impl->syncLostCb = nullptr;
    _impl->syncFailedCb = nullptr;
    _impl->startedCb = nullptr;
    _impl->stoppedCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Factory
// --------------------------------------------------------------------------

/** The sink registers PACS (and BASS), so it must exist before the GATT commit in start(). */
BLEAudioBroadcastSink BLEAudio::createBroadcastSink() {
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("BroadcastSink: create between audio.begin() and audio.start()");
    return BLEAudioBroadcastSink();
  }
  auto sink = std::make_shared<BLEAudioBroadcastSink::Impl>();
  sink->audio = _impl;
  sink->resizeStreams();
  _impl->roles.push_back(sink);
  std::weak_ptr<BLEAudioBroadcastSink::Impl> weak = sink;
  _impl->setHandler(BLE_AUDIO_GRP_BROADCAST_SINK, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? s->apply() : BTStatus::OK;
  });
  return BLEAudioBroadcastSink(sink);
#else
  log_e("BroadcastSink: not enabled in this build (CONFIG_BT_BAP_BROADCAST_SINK)");
  return BLEAudioBroadcastSink();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
