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

#include "BLEAudioVolume.h"
#include "BLEAudioControlLink.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// Renderer
// --------------------------------------------------------------------------

struct BLEAudioVolumeRenderer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  std::atomic<uint8_t> volume{100};
  std::atomic<bool> muted{false};
  std::atomic<uint8_t> step{1};
  StateCallback onStateChangedCb;

  static BTStatus op(uint8_t o, uint8_t value = 0) {
    return bleAudioStatus(bleAudioVcpRendOp(o, value));
  }

  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_VCP_REND_STATE || e.err || !e.data) {
      return;
    }
    const auto *s = static_cast<const ble_audio_vcp_state_t *>(e.data);
    volume.store(s->volume, std::memory_order_relaxed);
    muted.store(s->mute, std::memory_order_relaxed);
    if (auto cb = bleAudioCopyCallback(*this, &Impl::onStateChangedCb)) {
      cb(s->volume, s->mute);
    }
  }
};

BLEAudioVolumeRenderer::BLEAudioVolumeRenderer() = default;

BLEAudioVolumeRenderer::operator bool() const {
  return _impl != nullptr;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::setInitialVolume(uint8_t volume) {
  BLE_CHECK_IMPL(*this);
  impl.volume = volume;
  return *this;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::setInitialMute(bool muted) {
  BLE_CHECK_IMPL(*this);
  impl.muted = muted;
  return *this;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::setVolumeStep(uint8_t step) {
  BLE_CHECK_IMPL(*this);
  if (step) {
    impl.step = step;
    if (impl.audio->started) {
      (void)Impl::op(BLE_AUDIO_VCP_OP_SET_STEP, step);
    }
  }
  return *this;
}

BTStatus BLEAudioVolumeRenderer::setVolume(uint8_t volume) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_VCP_OP_SET_VOLUME, volume);
}

BTStatus BLEAudioVolumeRenderer::mute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_VCP_OP_MUTE);
}

BTStatus BLEAudioVolumeRenderer::unmute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_VCP_OP_UNMUTE);
}

BTStatus BLEAudioVolumeRenderer::volumeUp() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_VCP_OP_UP);
}

BTStatus BLEAudioVolumeRenderer::volumeDown() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_VCP_OP_DOWN);
}

uint8_t BLEAudioVolumeRenderer::getVolume() const {
  BLE_CHECK_IMPL(0);
  return impl.volume.load(std::memory_order_relaxed);
}

bool BLEAudioVolumeRenderer::isMuted() const {
  BLE_CHECK_IMPL(false);
  return impl.muted.load(std::memory_order_relaxed);
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::onStateChanged(StateCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onStateChangedCb = std::move(cb);
  return *this;
}

void BLEAudioVolumeRenderer::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onStateChangedCb = nullptr;
}

BLEAudioVolumeRenderer BLEAudio::createVolumeRenderer() {
#if BLE_AUDIO_VCP_RENDERER_SUPPORTED
  return BLEAudioImplCommon::createStagedRole<BLEAudioVolumeRenderer>(
    _impl, "createVolumeRenderer", BLE_AUDIO_GRP_VCP_RENDERER, [](BLEAudioVolumeRenderer::Impl &s) {
      return bleAudioStatus(bleAudioVcpRendInit(s.volume.load(), s.muted.load(), s.step.load()));
    }
  );
#else
  log_e("Volume renderer is not enabled in this build (CONFIG_BT_VCP_VOL_REND)");
  return BLEAudioVolumeRenderer();
#endif
}

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioVolumeController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEAudioControlLink link;
  std::atomic<uint8_t> volume{0};
  std::atomic<bool> muted{false};
  DiscoveredCallback onDiscoveredCb;
  StateCallback onStateChangedCb;
  OffsetCallback onOffsetChangedCb;
  InputCallback onInputChangedCb;

  BTStatus op(uint8_t o, uint8_t value = 0) const {
    const uint16_t c = link.activeConn();
    return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioVcpCtlrOp(c, o, value)) : BTStatus::InvalidState;
  }

  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioVcpCtlrDiscover);
      if (err) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(err), 0, 0);
        }
      }
      return;
    }
    if (e.conn_handle != link.getConn()) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_VCP_CTLR_DISCOVERED: {
        const auto *d = static_cast<const ble_audio_vcp_discovered_t *>(e.data);
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(e.err), d ? d->vocs_count : 0, d ? d->aics_count : 0);
        }
        break;
      }
      case BLE_AUDIO_EVT_VCP_CTLR_STATE:
        if (!e.err && e.data) {
          const auto *s = static_cast<const ble_audio_vcp_state_t *>(e.data);
          volume.store(s->volume, std::memory_order_relaxed);
          muted.store(s->mute, std::memory_order_relaxed);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onStateChangedCb)) {
            cb(s->volume, s->mute);
          }
        }
        break;
      case BLE_AUDIO_EVT_VCP_CTLR_OFFSET:
        if (!e.err && e.data) {
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onOffsetChangedCb)) {
            cb(*static_cast<const int16_t *>(e.data));
          }
        }
        break;
      case BLE_AUDIO_EVT_VCP_CTLR_INPUT:
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

BLEAudioVolumeController::BLEAudioVolumeController() = default;

BLEAudioVolumeController::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioVolumeController::discover(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.link.discover(*impl.audio, connHandle, bleAudioVcpCtlrDiscover);
}

uint16_t BLEAudioVolumeController::getConnHandle() const {
  BLE_CHECK_IMPL(BLE_AUDIO_CONN_NONE);
  return impl.link.getConn();
}

BTStatus BLEAudioVolumeController::setVolume(uint8_t volume) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_VCP_OP_SET_VOLUME, volume);
}

BTStatus BLEAudioVolumeController::mute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_VCP_OP_MUTE);
}

BTStatus BLEAudioVolumeController::unmute() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_VCP_OP_UNMUTE);
}

BTStatus BLEAudioVolumeController::volumeUp() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_VCP_OP_UP);
}

BTStatus BLEAudioVolumeController::volumeDown() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_VCP_OP_DOWN);
}

BTStatus BLEAudioVolumeController::readState() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_VCP_OP_READ);
}

uint8_t BLEAudioVolumeController::getVolume() const {
  BLE_CHECK_IMPL(0);
  return impl.volume.load(std::memory_order_relaxed);
}

bool BLEAudioVolumeController::isMuted() const {
  BLE_CHECK_IMPL(false);
  return impl.muted.load(std::memory_order_relaxed);
}

BTStatus BLEAudioVolumeController::setVolumeOffset(int16_t offset) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioVcpCtlrSetOffset(c, offset)) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeController::setInputGain(int8_t gain) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioVcpCtlrSetGain(c, gain)) : BTStatus::InvalidState;
}

BLEAudioVolumeController &BLEAudioVolumeController::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
  return *this;
}

BLEAudioVolumeController &BLEAudioVolumeController::onStateChanged(StateCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onStateChangedCb = std::move(cb);
  return *this;
}

BLEAudioVolumeController &BLEAudioVolumeController::onOffsetChanged(OffsetCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onOffsetChangedCb = std::move(cb);
  return *this;
}

BLEAudioVolumeController &BLEAudioVolumeController::onInputChanged(InputCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onInputChangedCb = std::move(cb);
  return *this;
}

void BLEAudioVolumeController::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
  impl.onStateChangedCb = nullptr;
  impl.onOffsetChangedCb = nullptr;
  impl.onInputChangedCb = nullptr;
}

BLEAudioVolumeController BLEAudio::createVolumeController() {
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
  return BLEAudioImplCommon::createStagedRole<BLEAudioVolumeController>(
    _impl, "createVolumeController", BLE_AUDIO_GRP_VCP_CONTROLLER, [](BLEAudioVolumeController::Impl &) {
      return bleAudioStatus(bleAudioVcpCtlrInit());
    }
  );
#else
  log_e("Volume controller is not enabled in this build (CONFIG_BT_VCP_VOL_CTLR)");
  return BLEAudioVolumeController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
