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
#include "BLEAudioBroadcastSink.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioControlLink.h"
#include "BLEAudioStreamImpl.h"
#include "BLE.h"
#include "scan/BLEScan.h"
#include "esp32-hal-log.h"
#include "core/BLESync.h"
#include "core/BLETask.h"

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
constexpr uint32_t kDefaultPresets = BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_24_2_1)
                                     | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_1_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1)
                                     | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_3_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_4_1)
                                     | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_5_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_6_1);

}  // namespace

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioBroadcastSink::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (role list).
  BLESharedMutex mtx;                     ///< Protects this role's callbacks and shared state.
  uint8_t streamCount = 1;                ///< BISes to join (1 or 2).
  uint8_t maxChannels = 2;                ///< Max channels per BIS advertised in the sink PAC (1 or 2).
  uint32_t presets = kDefaultPresets;     ///< BLEAudioPresetBit() mask for the sink PAC.
  BLEAudioLocation location = BLEAudioLocation::Mono;
  BLEAudioContext contexts = BLEAudioContext::Unspecified | BLEAudioContext::Media;
  bool delegator = BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED;
  String code;            ///< Local Broadcast Code; empty = none.
  String targetName;      ///< Auto-sync filter; empty = any.
  uint32_t targetId = 0;  ///< Auto-sync filter; 0 = any.
  uint32_t bisMask = 0;   ///< Preferred BIS indexes; 0 = lowest ones in the BASE.
  bool autoSync = true;
  bool applied = false;      ///< Sink registered by BLEAudio::begin().
  bool running = false;      ///< Between start() and stop(): keep (re)scanning.
  bool scanStarted = false;  ///< This sink started the shared extended scan.
  bool syncPending = false;  ///< PA sync create / PAST receive in flight.
  bool synced = false;       ///< PA sync established.
  bool streaming = false;    ///< BIG joined.
  BLESync stopSync{"bsink-stop"};
  std::vector<BLEAudioStream> streams;  ///< One Rx stream per BIS.

  BTStatus waitStopped(uint32_t timeoutMs = 5000) {
    return bleAudioWaitUntilIdle(
      mtx, stopSync, "BroadcastSink",
      [this]() {
        return streaming;
      },
      timeoutMs
    );
  }

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
    String local;
    {
      BLELockGuard lock(mtx.get());
      local = code;
    }
    uint8_t bcode[BLE_AUDIO_BCODE_SIZE] = {};
    bleAudioBsinkSetCode(bleAudioPackBroadcastCode(local, bcode));
  }

  /**
   * @brief Staged registration, run by BLEAudio::begin().
   *
   * The sink PAC is required: the stack only reports BIS indexes of subgroups
   * whose codec matches a registered sink PAC. It is merged with a unicast
   * server's sink PAC when both roles exist.
   */
  BTStatus apply() {
    if (streams.empty()) {
      log_e("BroadcastSink: no streams available (stream pool exhausted?)");
      return BTStatus::NoMemory;
    }
    ble_audio_pac_t pac;
    bleAudioPacFromPresets(presets, maxChannels, &pac);
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
    if (!on) {
      bool wasStarted;
      {
        BLELockGuard lock(mtx.get());
        wasStarted = scanStarted;
        scanStarted = false;
      }
      bleAudioBsinkSetScanning(false);
      if (!wasStarted) {
        return BTStatus::OK;
      }
      return sc.stopExtended();
    }
    {
      BLELockGuard lock(mtx.get());
      if (scanStarted) {
        return BTStatus::OK;
      }
    }
    bleAudioBsinkSetScanning(true);
    sc.setActiveScan(false);
    sc.setFilterDuplicates(false);
    BTStatus st = sc.startExtended(0);
    {
      BLELockGuard lock(mtx.get());
      scanStarted = (bool)st;
    }
    if (!st) {
      log_e("BroadcastSink: scan start failed (%s)", st.toString());
      bleAudioBsinkSetScanning(false);
      return st;
    }
    log_d("BroadcastSink: scanning for broadcast sources");
    return BTStatus::OK;
  }

  /**
   * @brief Arm the engine for @p broadcastId and create the PA sync.
   *
   * The scan keeps running until the sync is established: stopping it first
   * makes the controller miss the periodic train.
   */
  BTStatus sync(const BTAddress &addr, uint8_t sid, uint32_t broadcastId) {
    bleAudioBsinkArm(broadcastId);
    {
      BLELockGuard lock(mtx.get());
      // Mark pending before the call: the PA-synced event may arrive before
      // createPeriodicSync() returns.
      syncPending = true;
    }
    BTStatus st = BLE.getScan().createPeriodicSync(addr, sid, 0, kPaSyncTimeoutMs);
    if (!st) {
      bleAudioBsinkDisarm();
      BLELockGuard lock(mtx.get());
      syncPending = false;
      log_w("BroadcastSink: PA sync to 0x%06lX failed (%s)", (unsigned long)broadcastId, st.toString());
    } else {
      log_d("BroadcastSink: PA syncing to 0x%06lX (%s sid=%u)", (unsigned long)broadcastId, addr.toString().c_str(), sid);
    }
    return st;
  }

  /** @brief Auto-sync filter: every configured criterion must match. The caller holds mtx. */
  bool matches(const BLEAudioBroadcastSourceInfo &s) const {
    return (!targetId || s.broadcastId == targetId) && (!targetName.length() || s.name == targetName);
  }

  /** @brief Event handler for the BROADCAST_SINK group (host task). */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_BSINK_FOUND:
      {
        const auto *f = static_cast<const ble_audio_bsink_found_t *>(e.data);
        if (!f) {
          break;
        }
        BLEAudioBroadcastSourceInfo info;
        info.address = BTAddress(f->addr, static_cast<BTAddress::Type>(f->addr_type));
        info.sid = f->sid;
        info.broadcastId = f->broadcast_id;
        info.name = f->name;
        info.rssi = f->rssi;
        SourceFoundCallback cb;
        bool trySync;
        {
          BLELockGuard lock(mtx.get());
          cb = foundCb;
          trySync = autoSync && running && !syncPending && !synced && matches(info);
        }
        if (cb) {
          cb(info);
        }
        if (trySync) {
          (void)sync(info.address, info.sid, info.broadcastId);
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_PA_SYNCED:
      {
        Callback cb;
        bool ok;
        {
          BLELockGuard lock(mtx.get());
          syncPending = false;
          synced = (e.err == 0);
          ok = synced;
          cb = syncedCb;
        }
        if (ok) {
          log_i("BroadcastSink: PA synced, waiting for the BASE");
          (void)scan(false);
          if (cb) {
            cb();
          }
        } else {
          log_w("BroadcastSink: PA sync failed (status 0x%02x)", (unsigned)(uint8_t)e.err);
          bool runningNow;
          {
            BLELockGuard lock(mtx.get());
            runningNow = running;
          }
          if (runningNow) {
            (void)scan(true);
          }
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_BASE:
      {
        const auto *b = static_cast<const ble_audio_bsink_base_t *>(e.data);
        if (!b) {
          break;
        }
        BLEAudioBroadcastBaseInfo base;
        base.bisMask = b->bis_mask;
        base.subgroups = b->subgroups;
        base.presentationDelayUs = b->pd_us;
        base.codec = bleAudioCodecFromEngine(b->codec);
        log_d(
          "BroadcastSink: BASE with BIS mask 0x%08lx, %u subgroup(s), %lu Hz", (unsigned long)base.bisMask, base.subgroups,
          (unsigned long)base.codec.samplingRateHz
        );
        if (auto cb = bleAudioCopyCallback(*this, &Impl::baseCb)) {
          cb(base);
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_CODE:
        log_d("BroadcastSink: Broadcast Code received from an assistant (conn %u)", e.conn_handle);
        if (auto cb = bleAudioCopyCallback(*this, &Impl::codeCb)) {
          cb();
        }
        break;
      case BLE_AUDIO_EVT_BSINK_PA_LOST:
      {
        ReasonCallback cb;
        bool runningNow;
        {
          BLELockGuard lock(mtx.get());
          synced = false;
          cb = syncLostCb;
          runningNow = running;
        }
        log_w("BroadcastSink: PA sync lost (reason 0x%02x)%s", (unsigned)(uint8_t)e.err, runningNow ? ", rescanning" : "");
        if (cb) {
          cb((uint8_t)e.err);
        }
        if (runningNow) {
          (void)scan(true);
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_SYNC_FAILED:
        // -EACCES: the BIG is encrypted and the code is missing or wrong.
        log_w("BroadcastSink: BIG sync failed (err=%d)%s", e.err, e.err == -EACCES ? ": Broadcast Code missing or wrong" : "");
        if (auto cb = bleAudioCopyCallback(*this, &Impl::syncFailedCb)) {
          cb(e.err == -EACCES ? BTStatus::PermissionDenied : bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_BSINK_STARTED:
      {
        Callback cb;
        {
          BLELockGuard lock(mtx.get());
          streaming = true;
          cb = startedCb;
        }
        log_i("BroadcastSink: BIG joined, %u streams", (unsigned)streams.size());
        if (cb) {
          cb();
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_STOPPED:
      {
        ReasonCallback cb;
        {
          BLELockGuard lock(mtx.get());
          streaming = false;
          cb = stoppedCb;
        }
        log_i("BroadcastSink: BIG left (reason 0x%02x)", (unsigned)(uint8_t)e.err);
        stopSync.give(BTStatus::OK);
        if (cb) {
          cb((uint8_t)e.err);
        }
        break;
      }
      case BLE_AUDIO_EVT_BSINK_PA_REQ:
      {
        // An assistant selected a source: sync to it directly, or wait for it
        // to transfer its own sync over the connection (PAST). The PAST
        // receive occupies the controller's single sync-create slot, so it is
        // only armed on request, never speculatively on connect.
        const auto *r = static_cast<const ble_audio_bsink_pa_req_t *>(e.data);
        if (!r || !r->reply) {
          break;
        }
        log_d("evt PA_REQ past=%d bid=0x%06lx", r->past, (unsigned long)r->broadcast_id);
        {
          BLELockGuard lock(mtx.get());
          // The PA-synced event may arrive before the create call returns.
          syncPending = true;
        }
        BTStatus st;
        if (r->past) {
          st = bleAudioEngineStatus(bleAudioEngineReceivePeriodicSync(e.conn_handle, r->peer_addr_type, r->peer_addr, 0, kPaSyncTimeoutMs));
        } else {
          BLEScan sc = BLE.getScan();
          st = sc.createPeriodicSync(BTAddress(r->addr, static_cast<BTAddress::Type>(r->addr_type)), r->sid, 0, kPaSyncTimeoutMs);
        }
        if (!st) {
          BLELockGuard lock(mtx.get());
          syncPending = false;
        }
        if (!st) {
          log_w("BroadcastSink: assistant sync request for 0x%06lX failed (%s)", (unsigned long)r->broadcast_id, st.toString());
        } else {
          log_d("BroadcastSink: assistant sync to 0x%06lX via %s", (unsigned long)r->broadcast_id, r->past ? "PAST" : "scan");
        }
        *r->reply = st ? 0 : -EIO;
        break;
      }
      case BLE_AUDIO_EVT_BSINK_PA_TERM_REQ:
      {
        log_d("BroadcastSink: assistant asked to stop the PA sync");
        bool wasPending;
        {
          BLELockGuard lock(mtx.get());
          wasPending = syncPending;
          syncPending = false;
        }
        // Drop the armed Broadcast ID too: without disarming, the next local
        // sync() would join the stale ID instead of the newly requested one.
        bleAudioBsinkDisarm();
        BLEScan sc = BLE.getScan();
        const uint16_t sh = bleAudioBsinkSyncHandle();
        if (sh != 0xFFFF) {
          (void)sc.terminatePeriodicSync(sh);
        } else if (wasPending) {
          if (e.conn_handle != BLE_AUDIO_CONN_NONE) {
            (void)bleAudioEngineStopPeriodicSyncReceive(e.conn_handle);
          }
          (void)sc.cancelPeriodicSyncCreate();
        }
        break;
      }
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

BTStatus BLEAudioBroadcastSink::setStreams(uint8_t count) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("BroadcastSink: setStreams() after audio.begin()");
    return BTStatus::InvalidState;
  }
  if (count < 1 || count > 2) {
    log_e("BroadcastSink: invalid stream count %u (1 = mono, 2 = stereo)", count);
    return BTStatus::InvalidParam;
  }
  impl.streamCount = count;
  impl.resizeStreams();
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setSupportedPresets(uint32_t presetMask) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("BroadcastSink: setSupportedPresets() after audio.begin()");
    return BTStatus::InvalidState;
  }
  if (!presetMask) {
    log_e("BroadcastSink: invalid preset mask 0");
    return BTStatus::InvalidParam;
  }
  impl.presets = presetMask;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setMaxChannelsPerStream(uint8_t channels) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("BroadcastSink: setMaxChannelsPerStream() after audio.begin()");
    return BTStatus::InvalidState;
  }
  if (channels < 1 || channels > 2) {
    log_e("BroadcastSink: invalid channels %u (1 = mono, 2 = stereo)", channels);
    return BTStatus::InvalidParam;
  }
  impl.maxChannels = channels;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setLocation(BLEAudioLocation location) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("BroadcastSink: setLocation() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.location = location;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setContexts(BLEAudioContext contexts) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("BroadcastSink: setContexts() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.contexts = contexts;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setScanDelegator(bool enable) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("BroadcastSink: setScanDelegator() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.delegator = enable;
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Source selection
// --------------------------------------------------------------------------

BTStatus BLEAudioBroadcastSink::setBroadcastCode(const String &code) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (code.length() > BLE_AUDIO_BCODE_SIZE) {
    log_e("BroadcastSink: Broadcast Code is %u characters, the maximum is %u", (unsigned)code.length(), BLE_AUDIO_BCODE_SIZE);
    return BTStatus::InvalidParam;
  }
  bool applied;
  {
    BLELockGuard lock(impl.mtx.get());
    impl.code = code;
    applied = impl.applied;
  }
  if (applied) {
    impl.pushCode();
  }
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setTargetName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  impl.targetName = name;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setTargetBroadcastId(uint32_t broadcastId) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  impl.targetId = broadcastId & 0xFFFFFF;
  return BTStatus::OK;
}

/** Stored in the engine, which applies it at the next BIG sync. */
BTStatus BLEAudioBroadcastSink::setBisMask(uint32_t mask) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  {
    BLELockGuard lock(impl.mtx.get());
    impl.bisMask = mask;
  }
  bleAudioBsinkSetBisMask(mask);
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::setAutoSync(bool enable) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  impl.autoSync = enable;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSink::configureAuracast(const String &targetName, uint8_t streams) {
  BTStatus st = setStreams(streams);
  if (!st) {
    return st;
  }
  st = setLocation(streams == 2 ? BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight : BLEAudioLocation::Mono);
  if (!st) {
    return st;
  }
  st = setContexts(BLEAudioContext::Media);
  if (!st) {
    return st;
  }
  st = setTargetName(targetName);
  if (!st) {
    return st;
  }
  return setAutoSync(true);
}

// --------------------------------------------------------------------------
// Control
// --------------------------------------------------------------------------

BTStatus BLEAudioBroadcastSink::start() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  bool already;
  bool synced;
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->applied) {
      log_e("BroadcastSink: start() before audio.begin()");
      return BTStatus::InvalidState;
    }
    already = _impl->running && (_impl->synced || _impl->scanStarted);
    synced = _impl->synced;
    if (!already) {
      _impl->running = true;
    }
  }
  if (already) {
    return BTStatus::OK;
  }
  BTStatus st = synced ? BTStatus::OK : _impl->scan(true);
  if (!st) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->running = false;
  }
  return st;
}

BTStatus BLEAudioBroadcastSink::syncTo(const BLEAudioBroadcastSourceInfo &source) {
  if (!_impl || !_impl->applied) {
    log_e("BroadcastSink: syncTo() before audio.begin()");
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (_impl->syncPending || _impl->synced) {
      log_w("BroadcastSink: syncTo() while a sync is pending or established; stop() first");
      return BTStatus::InvalidState;
    }
  }
  return _impl->sync(source.address, source.sid, source.broadcastId);
}

/**
 * Tear down in reverse order: scan, BIG, pending sync create, established PA
 * sync. PA/scan resources are released only after the BIG has left.
 */
BTStatus BLEAudioBroadcastSink::stop() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("BroadcastSink: stop() cannot run from a stack callback; defer it to loop()");
    return BTStatus::Busy;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    _impl->running = false;
  }
  (void)_impl->scan(false);
  int err = bleAudioBsinkStop();
  if (err != 0) {
    log_w("BroadcastSink: leaving the BIG failed (err=%d)", err);
    return bleAudioStatus(err);
  }
  BTStatus st = _impl->waitStopped();
  if (!st) {
    return st;
  }
  BLEScan sc = BLE.getScan();
  bool pending;
  {
    BLELockGuard lock(_impl->mtx.get());
    pending = _impl->syncPending;
  }
  if (pending) {
    (void)sc.cancelPeriodicSyncCreate();
  }
  const uint16_t sh = bleAudioBsinkSyncHandle();
  if (sh != 0xFFFF) {
    (void)sc.terminatePeriodicSync(sh);
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    _impl->synced = false;
    _impl->syncPending = false;
  }
  return BTStatus::OK;
}

bool BLEAudioBroadcastSink::isStreaming() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streaming;
}

// --------------------------------------------------------------------------
// Streams and callbacks
// --------------------------------------------------------------------------

size_t BLEAudioBroadcastSink::streamCount() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streams.size();
}

BLEAudioStream BLEAudioBroadcastSink::stream(size_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  BLELockGuard lock(_impl->mtx.get());
  return index < _impl->streams.size() ? _impl->streams[index] : BLEAudioStream();
}

void BLEAudioBroadcastSink::onSourceFound(SourceFoundCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.foundCb = std::move(cb);
}

void BLEAudioBroadcastSink::onSynced(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.syncedCb = std::move(cb);
}

void BLEAudioBroadcastSink::onBaseReceived(BaseCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.baseCb = std::move(cb);
}

void BLEAudioBroadcastSink::onBroadcastCodeReceived(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.codeCb = std::move(cb);
}

void BLEAudioBroadcastSink::onSyncLost(ReasonCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.syncLostCb = std::move(cb);
}

void BLEAudioBroadcastSink::onSyncFailed(ErrorCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.syncFailedCb = std::move(cb);
}

void BLEAudioBroadcastSink::onStarted(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.startedCb = std::move(cb);
}

void BLEAudioBroadcastSink::onStopped(ReasonCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.stoppedCb = std::move(cb);
}

void BLEAudioBroadcastSink::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.foundCb = nullptr;
  impl.syncedCb = nullptr;
  impl.baseCb = nullptr;
  impl.codeCb = nullptr;
  impl.syncLostCb = nullptr;
  impl.syncFailedCb = nullptr;
  impl.startedCb = nullptr;
  impl.stoppedCb = nullptr;
}

// --------------------------------------------------------------------------
// Factory
// --------------------------------------------------------------------------

/** The sink registers PACS (and BASS), so it must exist before the GATT commit in start(). */
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
BLEAudioBroadcastSink BLEAudio::createBroadcastSink() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioBroadcastSink();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("BroadcastSink: create before audio.begin() commits");
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
  _impl->roleStops.push_back([weak]() {
    if (auto s = weak.lock()) {
      (void)BLEAudioBroadcastSink(s).stop();
    }
  });
  return BLEAudioBroadcastSink(sink);
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_BROADCAST_SINK_SUPPORTED
BLEAudioBroadcastSink BLEAudio::createBroadcastSink() {
  log_e("BroadcastSink: not enabled in this build (CONFIG_BT_BAP_BROADCAST_SINK)");
  return BLEAudioBroadcastSink();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
