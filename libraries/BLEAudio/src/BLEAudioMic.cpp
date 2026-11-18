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

#include "BLEAudioMic.h"
#include "BLEAudioControlLink.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// Device
// --------------------------------------------------------------------------

struct BLEAudioMicDevice::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  std::atomic<uint8_t> muteState{BLE_AUDIO_MICP_UNMUTED};  ///< Raw MICS state (UNMUTED/MUTED/DISABLED).
  std::atomic<uint8_t> lastOp{(uint8_t)BLEAudioMicDevice::Operation::Unknown};
  MuteCallback onMuteChangedCb;
  ResultCallback onResultCb;

  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_MICP_DEV_MUTE || !e.data) {
      return;
    }
    const uint8_t st = *static_cast<const uint8_t *>(e.data);
    if (e.err) {
      const auto op = static_cast<BLEAudioMicDevice::Operation>(lastOp.exchange((uint8_t)BLEAudioMicDevice::Operation::Unknown));
      if (auto cb = bleAudioCopyCallback(*this, &Impl::onResultCb)) {
        cb(op, bleAudioStatus(e.err));
      }
      return;
    }
    muteState.store(st, std::memory_order_relaxed);
    lastOp.store((uint8_t)BLEAudioMicDevice::Operation::Unknown, std::memory_order_relaxed);
    if (auto cb = bleAudioCopyCallback(*this, &Impl::onMuteChangedCb)) {
      cb(st == BLE_AUDIO_MICP_MUTED);
    }
  }
};

BLEAudioMicDevice::BLEAudioMicDevice() = default;

BLEAudioMicDevice::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioMicDevice::setInitialMute(bool muted) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio->started) {
    return BTStatus::InvalidState;
  }
  impl.muteState.store(muted ? BLE_AUDIO_MICP_MUTED : BLE_AUDIO_MICP_UNMUTED, std::memory_order_relaxed);
  return BTStatus::OK;
}

BTStatus BLEAudioMicDevice::mute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.lastOp.store((uint8_t)Operation::Mute, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_MUTED));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

BTStatus BLEAudioMicDevice::unmute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.lastOp.store((uint8_t)Operation::Unmute, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_UNMUTED));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

BTStatus BLEAudioMicDevice::disableMute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.lastOp.store((uint8_t)Operation::DisableMute, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_DISABLED));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

bool BLEAudioMicDevice::isMuted() const {
  BLE_CHECK_IMPL(false);
  return impl.muteState.load(std::memory_order_relaxed) == BLE_AUDIO_MICP_MUTED;
}

bool BLEAudioMicDevice::isMuteDisabled() const {
  BLE_CHECK_IMPL(false);
  return impl.muteState.load(std::memory_order_relaxed) == BLE_AUDIO_MICP_DISABLED;
}

void BLEAudioMicDevice::onMuteChanged(MuteCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onMuteChangedCb = std::move(cb);
}

void BLEAudioMicDevice::onResult(ResultCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onResultCb = std::move(cb);
}

void BLEAudioMicDevice::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onMuteChangedCb = nullptr;
  impl.onResultCb = nullptr;
}

#if BLE_AUDIO_MICP_DEVICE_SUPPORTED
BLEAudioMicDevice BLEAudio::createMicDevice() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioMicDevice();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioMicDevice>(_impl, "createMicDevice", BLE_AUDIO_GRP_MICP_DEVICE, [](BLEAudioMicDevice::Impl &s) {
    BTStatus st = bleAudioStatus(bleAudioMicpDevInit(s.muteState.load() == BLE_AUDIO_MICP_MUTED));
    if (st && s.muteState.load() == BLE_AUDIO_MICP_DISABLED) {
      st = bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_DISABLED));
    }
    return st;
  });
}
#endif

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioMicController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEAudioControlLink link;
  std::atomic<uint8_t> muteState{BLE_AUDIO_MICP_UNMUTED};  ///< Raw MICS state (UNMUTED/MUTED/DISABLED).
  std::atomic<uint8_t> lastOp{(uint8_t)BLEAudioMicController::Operation::Unknown};
  DiscoveredCallback onDiscoveredCb;
  MuteCallback onMuteChangedCb;
  InputCallback onInputChangedCb;
  ResultCallback onResultCb;

  void reportError(int err) {
    const auto op = static_cast<BLEAudioMicController::Operation>(lastOp.exchange((uint8_t)BLEAudioMicController::Operation::Unknown));
    if (auto cb = bleAudioCopyCallback(*this, &Impl::onResultCb)) {
      cb(op, bleAudioStatus(err));
    }
  }

  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED && e.conn_handle == link.getConn()) {
        // A queued write that never reported must still terminate: the peer
        // is gone, so report it here (NotConnected) instead of leaving the
        // application waiting for a notification that will never arrive. A
        // late stack error for the same op is dropped below (unknown link),
        // keeping exactly one terminal event per operation.
        const auto pending = static_cast<BLEAudioMicController::Operation>(lastOp.exchange((uint8_t)BLEAudioMicController::Operation::Unknown));
        muteState.store(BLE_AUDIO_MICP_UNMUTED, std::memory_order_relaxed);
        if (pending != BLEAudioMicController::Operation::Unknown) {
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onResultCb)) {
            cb(pending, BTStatus::NotConnected);
          }
        }
      }
      const int err = link.onCore(e, bleAudioMicpCtlrDiscover);
      if (err) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(err), 0);
        }
      }
      return;
    }
    if (e.conn_handle != link.getConn()) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_MICP_CTLR_DISCOVERED:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(e.err), e.data ? *static_cast<const uint8_t *>(e.data) : 0);
        }
        break;
      case BLE_AUDIO_EVT_MICP_CTLR_MUTE:
        if (e.err || !e.data) {
          if (e.err) {
            reportError(e.err);
          }
          break;
        }
        muteState.store(*static_cast<const uint8_t *>(e.data), std::memory_order_relaxed);
        lastOp.store((uint8_t)BLEAudioMicController::Operation::Unknown, std::memory_order_relaxed);
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onMuteChangedCb)) {
          cb(muteState.load(std::memory_order_relaxed) == BLE_AUDIO_MICP_MUTED);
        }
        break;
      case BLE_AUDIO_EVT_MICP_CTLR_INPUT:
        if (e.err || !e.data) {
          if (e.err) {
            reportError(e.err);
          }
          break;
        }
        lastOp.store((uint8_t)BLEAudioMicController::Operation::Unknown, std::memory_order_relaxed);
        {
          const auto *a = static_cast<const ble_audio_aics_state_t *>(e.data);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onInputChangedCb)) {
            cb(a->gain, a->mute);
          }
        }
        break;
      default: break;
    }
  }
};

BLEAudioMicController::BLEAudioMicController() = default;

BLEAudioMicController::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioMicController::discover(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.link.discover(*impl.audio, connHandle, bleAudioMicpCtlrDiscover);
}

uint16_t BLEAudioMicController::getConnHandle() const {
  BLE_CHECK_IMPL(BLE_AUDIO_CONN_NONE);
  return impl.link.getConn();
}

BTStatus BLEAudioMicController::mute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  if (c == BLE_AUDIO_CONN_NONE) {
    return BTStatus::InvalidState;
  }
  impl.lastOp.store((uint8_t)Operation::Mute, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpCtlrSetMute(c, true));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

BTStatus BLEAudioMicController::unmute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  if (c == BLE_AUDIO_CONN_NONE) {
    return BTStatus::InvalidState;
  }
  if (impl.muteState.load(std::memory_order_relaxed) == BLE_AUDIO_MICP_DISABLED) {
    return BTStatus::InvalidState; /* Fail fast: the peer rejects unmute while disabled. */
  }
  impl.lastOp.store((uint8_t)Operation::Unmute, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpCtlrSetMute(c, false));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

BTStatus BLEAudioMicController::readMute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  if (c == BLE_AUDIO_CONN_NONE) {
    return BTStatus::InvalidState;
  }
  impl.lastOp.store((uint8_t)Operation::ReadMute, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpCtlrReadMute(c));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

bool BLEAudioMicController::isMuted() const {
  BLE_CHECK_IMPL(false);
  return impl.muteState.load(std::memory_order_relaxed) == BLE_AUDIO_MICP_MUTED;
}

bool BLEAudioMicController::isMuteDisabled() const {
  BLE_CHECK_IMPL(false);
  return impl.muteState.load(std::memory_order_relaxed) == BLE_AUDIO_MICP_DISABLED;
}

BTStatus BLEAudioMicController::setInputGain(int8_t gain) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  if (c == BLE_AUDIO_CONN_NONE) {
    return BTStatus::InvalidState;
  }
  impl.lastOp.store((uint8_t)Operation::SetInputGain, std::memory_order_relaxed);
  const BTStatus st = bleAudioStatus(bleAudioMicpCtlrSetGain(c, gain));
  if (!st) {
    impl.lastOp.store((uint8_t)Operation::Unknown, std::memory_order_relaxed);
  }
  return st;
}

void BLEAudioMicController::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
}

void BLEAudioMicController::onMuteChanged(MuteCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onMuteChangedCb = std::move(cb);
}

void BLEAudioMicController::onInputChanged(InputCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onInputChangedCb = std::move(cb);
}

void BLEAudioMicController::onResult(ResultCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onResultCb = std::move(cb);
}

void BLEAudioMicController::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
  impl.onMuteChangedCb = nullptr;
  impl.onInputChangedCb = nullptr;
  impl.onResultCb = nullptr;
}

#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
BLEAudioMicController BLEAudio::createMicController() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioMicController();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioMicController>(
    _impl, "createMicController", BLE_AUDIO_GRP_MICP_CONTROLLER,
    [](BLEAudioMicController::Impl &) {
      return bleAudioStatus(bleAudioMicpCtlrInit());
    }
  );
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_MICP_DEVICE_SUPPORTED
BLEAudioMicDevice BLEAudio::createMicDevice() {
  log_e("Microphone device is not enabled in this build (CONFIG_BT_MICP_MIC_DEV)");
  return BLEAudioMicDevice();
}
#endif

#if !BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
BLEAudioMicController BLEAudio::createMicController() {
  log_e("Microphone controller is not enabled in this build (CONFIG_BT_MICP_MIC_CTLR)");
  return BLEAudioMicController();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
