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
  std::atomic<bool> muted{false};
  MuteCallback onMuteChangedCb;

  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_MICP_DEV_MUTE || !e.data) {
      return;
    }
    const bool m = *static_cast<const uint8_t *>(e.data) == BLE_AUDIO_MICP_MUTED;
    muted.store(m, std::memory_order_relaxed);
    if (auto cb = bleAudioCopyCallback(*this, &Impl::onMuteChangedCb)) {
      cb(m);
    }
  }
};

BLEAudioMicDevice::BLEAudioMicDevice() = default;

BLEAudioMicDevice::operator bool() const {
  return _impl != nullptr;
}

BLEAudioMicDevice &BLEAudioMicDevice::setInitialMute(bool muted) {
  BLE_CHECK_IMPL(*this);
  impl.muted = muted;
  return *this;
}

BTStatus BLEAudioMicDevice::mute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_MUTED));
}

BTStatus BLEAudioMicDevice::unmute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_UNMUTED));
}

BTStatus BLEAudioMicDevice::disableMute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_DISABLED));
}

bool BLEAudioMicDevice::isMuted() const {
  BLE_CHECK_IMPL(false);
  return impl.muted.load(std::memory_order_relaxed);
}

BLEAudioMicDevice &BLEAudioMicDevice::onMuteChanged(MuteCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onMuteChangedCb = std::move(cb);
  return *this;
}

void BLEAudioMicDevice::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onMuteChangedCb = nullptr;
}

BLEAudioMicDevice BLEAudio::createMicDevice() {
#if BLE_AUDIO_MICP_DEVICE_SUPPORTED
  return BLEAudioImplCommon::createStagedRole<BLEAudioMicDevice>(
    _impl, "createMicDevice", BLE_AUDIO_GRP_MICP_DEVICE, [](BLEAudioMicDevice::Impl &s) {
      return bleAudioStatus(bleAudioMicpDevInit(s.muted.load()));
    }
  );
#else
  log_e("Microphone device is not enabled in this build (CONFIG_BT_MICP_MIC_DEV)");
  return BLEAudioMicDevice();
#endif
}

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioMicController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEAudioControlLink link;
  std::atomic<bool> muted{false};
  DiscoveredCallback onDiscoveredCb;
  MuteCallback onMuteChangedCb;
  InputCallback onInputChangedCb;

  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
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
        if (!e.err && e.data) {
          const bool m = *static_cast<const uint8_t *>(e.data) == BLE_AUDIO_MICP_MUTED;
          muted.store(m, std::memory_order_relaxed);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onMuteChangedCb)) {
            cb(m);
          }
        }
        break;
      case BLE_AUDIO_EVT_MICP_CTLR_INPUT:
        if (!e.err && e.data) {
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
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioMicpCtlrSetMute(c, true)) : BTStatus::InvalidState;
}

BTStatus BLEAudioMicController::unmute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioMicpCtlrSetMute(c, false)) : BTStatus::InvalidState;
}

BTStatus BLEAudioMicController::readMute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioMicpCtlrReadMute(c)) : BTStatus::InvalidState;
}

bool BLEAudioMicController::isMuted() const {
  BLE_CHECK_IMPL(false);
  return impl.muted.load(std::memory_order_relaxed);
}

BTStatus BLEAudioMicController::setInputGain(int8_t gain) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioMicpCtlrSetGain(c, gain)) : BTStatus::InvalidState;
}

BLEAudioMicController &BLEAudioMicController::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
  return *this;
}

BLEAudioMicController &BLEAudioMicController::onMuteChanged(MuteCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onMuteChangedCb = std::move(cb);
  return *this;
}

BLEAudioMicController &BLEAudioMicController::onInputChanged(InputCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onInputChangedCb = std::move(cb);
  return *this;
}

void BLEAudioMicController::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
  impl.onMuteChangedCb = nullptr;
  impl.onInputChangedCb = nullptr;
}

BLEAudioMicController BLEAudio::createMicController() {
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
  return BLEAudioImplCommon::createStagedRole<BLEAudioMicController>(
    _impl, "createMicController", BLE_AUDIO_GRP_MICP_CONTROLLER, [](BLEAudioMicController::Impl &) {
      return bleAudioStatus(bleAudioMicpCtlrInit());
    }
  );
#else
  log_e("Microphone controller is not enabled in this build (CONFIG_BT_MICP_MIC_CTLR)");
  return BLEAudioMicController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
