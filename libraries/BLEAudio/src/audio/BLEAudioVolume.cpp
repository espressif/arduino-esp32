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

#include "audio/BLEAudioVolume.h"
#include "audio/BLEAudioControlLink.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioVolume.cpp
 * @brief VCP volume renderer and volume controller role handles.
 *
 * Both roles are thin wrappers over the control engine unit
 * (BLEAudioEngineControl): the factory registers an event handler for the
 * role's group and a deferred init that runs inside BLEAudio::start(). The
 * handles cache the last reported state so getters never touch the stack.
 *
 * API contract is documented on the declarations in `BLEAudioVolume.h`; the
 * definitions below carry implementation notes only.
 */

// --------------------------------------------------------------------------
// Renderer
// --------------------------------------------------------------------------

struct BLEAudioVolumeRenderer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  uint8_t volume = 100;  ///< Initial volume before start, then the last reported state.
  bool muted = false;    ///< Initial mute before start, then the last reported state.
  uint8_t step = 1;      ///< Volume up/down step (never 0).
  StateCallback stateCb;

  /** Run a BLE_AUDIO_VCP_OP_* on the local renderer. */
  static BTStatus op(uint8_t o, uint8_t value = 0) {
    return bleAudioStatus(bleAudioVcpRendOp(o, value));
  }

  /** Engine event for this group: the volume state changed (by a client or locally). */
  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_VCP_REND_STATE || e.err || !e.data) {
      return;
    }
    const auto *s = static_cast<const ble_audio_vcp_state_t *>(e.data);
    volume = s->volume;
    muted = s->mute;
    if (stateCb) {
      stateCb(volume, muted);
    }
  }
};

BLEAudioVolumeRenderer::BLEAudioVolumeRenderer() = default;

BLEAudioVolumeRenderer::operator bool() const {
  return _impl != nullptr;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::setInitialVolume(uint8_t volume) {
  if (_impl) {
    _impl->volume = volume;
  }
  return *this;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::setInitialMute(bool muted) {
  if (_impl) {
    _impl->muted = muted;
  }
  return *this;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::setVolumeStep(uint8_t step) {
  if (_impl && step) {
    _impl->step = step;
    /* Before start the step is applied by the role init. */
    if (_impl->audio->started) {
      (void)Impl::op(BLE_AUDIO_VCP_OP_SET_STEP, step);
    }
  }
  return *this;
}

BTStatus BLEAudioVolumeRenderer::setVolume(uint8_t volume) {
  return _impl ? Impl::op(BLE_AUDIO_VCP_OP_SET_VOLUME, volume) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeRenderer::mute() {
  return _impl ? Impl::op(BLE_AUDIO_VCP_OP_MUTE) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeRenderer::unmute() {
  return _impl ? Impl::op(BLE_AUDIO_VCP_OP_UNMUTE) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeRenderer::volumeUp() {
  return _impl ? Impl::op(BLE_AUDIO_VCP_OP_UP) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeRenderer::volumeDown() {
  return _impl ? Impl::op(BLE_AUDIO_VCP_OP_DOWN) : BTStatus::InvalidState;
}

uint8_t BLEAudioVolumeRenderer::getVolume() const {
  return _impl ? _impl->volume : 0;
}

bool BLEAudioVolumeRenderer::isMuted() const {
  return _impl && _impl->muted;
}

BLEAudioVolumeRenderer &BLEAudioVolumeRenderer::onStateChanged(StateCallback cb) {
  if (_impl) {
    _impl->stateCb = std::move(cb);
  }
  return *this;
}

void BLEAudioVolumeRenderer::resetCallbacks() {
  if (_impl) {
    _impl->stateCb = nullptr;
  }
}

BLEAudioVolumeRenderer BLEAudio::createVolumeRenderer() {
#if BLE_AUDIO_VCP_RENDERER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createVolumeRenderer(): call between audio.begin() and audio.start()");
    return BLEAudioVolumeRenderer();
  }
  auto r = std::make_shared<BLEAudioVolumeRenderer::Impl>();
  r->audio = _impl;
  _impl->roles.push_back(r);
  /* `roles` owns the role until end(); strong captures here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioVolumeRenderer::Impl> weak = r;
  _impl->setHandler(BLE_AUDIO_GRP_VCP_RENDERER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? bleAudioStatus(bleAudioVcpRendInit(s->volume, s->muted, s->step)) : BTStatus::OK;
  });
  return BLEAudioVolumeRenderer(r);
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
  BLEAudioControlLink link;  ///< The renderer being controlled.
  uint8_t volume = 0;        ///< Last state reported by the renderer.
  bool muted = false;
  DiscoveredCallback discoveredCb;
  StateCallback stateCb;
  OffsetCallback offsetCb;
  InputCallback inputCb;

  /** Run a BLE_AUDIO_VCP_OP_* on the bound renderer. */
  BTStatus op(uint8_t o, uint8_t value = 0) const {
    return link.bound() ? bleAudioStatus(bleAudioVcpCtlrOp(link.conn, o, value)) : BTStatus::InvalidState;
  }

  /**
   * Engine event: core events drive the link (deferred discovery, disconnect);
   * role events are delivered only for the bound peer.
   */
  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioVcpCtlrDiscover);
      if (err && discoveredCb) {
        discoveredCb(bleAudioStatus(err), 0, 0);
      }
      return;
    }
    if (e.conn_handle != link.conn) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_VCP_CTLR_DISCOVERED:
        if (discoveredCb) {
          const auto *d = static_cast<const ble_audio_vcp_discovered_t *>(e.data);
          discoveredCb(bleAudioStatus(e.err), d ? d->vocs_count : 0, d ? d->aics_count : 0);
        }
        break;
      case BLE_AUDIO_EVT_VCP_CTLR_STATE:
        if (!e.err && e.data) {
          const auto *s = static_cast<const ble_audio_vcp_state_t *>(e.data);
          volume = s->volume;
          muted = s->mute;
          if (stateCb) {
            stateCb(volume, muted);
          }
        }
        break;
      case BLE_AUDIO_EVT_VCP_CTLR_OFFSET:
        if (!e.err && e.data && offsetCb) {
          offsetCb(*static_cast<const int16_t *>(e.data));
        }
        break;
      case BLE_AUDIO_EVT_VCP_CTLR_INPUT:
        if (!e.err && e.data && inputCb) {
          const auto *a = static_cast<const ble_audio_aics_state_t *>(e.data);
          inputCb(a->gain, a->mute);
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
  return _impl ? _impl->link.discover(*_impl->audio, connHandle, bleAudioVcpCtlrDiscover) : BTStatus::InvalidState;
}

uint16_t BLEAudioVolumeController::getConnHandle() const {
  return _impl ? _impl->link.conn : BLE_AUDIO_CONN_NONE;
}

BTStatus BLEAudioVolumeController::setVolume(uint8_t volume) {
  return _impl ? _impl->op(BLE_AUDIO_VCP_OP_SET_VOLUME, volume) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeController::mute() {
  return _impl ? _impl->op(BLE_AUDIO_VCP_OP_MUTE) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeController::unmute() {
  return _impl ? _impl->op(BLE_AUDIO_VCP_OP_UNMUTE) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeController::volumeUp() {
  return _impl ? _impl->op(BLE_AUDIO_VCP_OP_UP) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeController::volumeDown() {
  return _impl ? _impl->op(BLE_AUDIO_VCP_OP_DOWN) : BTStatus::InvalidState;
}

BTStatus BLEAudioVolumeController::readState() {
  return _impl ? _impl->op(BLE_AUDIO_VCP_OP_READ) : BTStatus::InvalidState;
}

uint8_t BLEAudioVolumeController::getVolume() const {
  return _impl ? _impl->volume : 0;
}

bool BLEAudioVolumeController::isMuted() const {
  return _impl && _impl->muted;
}

BTStatus BLEAudioVolumeController::setVolumeOffset(int16_t offset) {
  if (!_impl || !_impl->link.bound()) {
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioVcpCtlrSetOffset(_impl->link.conn, offset));
}

BTStatus BLEAudioVolumeController::setInputGain(int8_t gain) {
  if (!_impl || !_impl->link.bound()) {
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioVcpCtlrSetGain(_impl->link.conn, gain));
}

BLEAudioVolumeController &BLEAudioVolumeController::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioVolumeController &BLEAudioVolumeController::onStateChanged(StateCallback cb) {
  if (_impl) {
    _impl->stateCb = std::move(cb);
  }
  return *this;
}

BLEAudioVolumeController &BLEAudioVolumeController::onOffsetChanged(OffsetCallback cb) {
  if (_impl) {
    _impl->offsetCb = std::move(cb);
  }
  return *this;
}

BLEAudioVolumeController &BLEAudioVolumeController::onInputChanged(InputCallback cb) {
  if (_impl) {
    _impl->inputCb = std::move(cb);
  }
  return *this;
}

void BLEAudioVolumeController::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->stateCb = nullptr;
    _impl->offsetCb = nullptr;
    _impl->inputCb = nullptr;
  }
}

BLEAudioVolumeController BLEAudio::createVolumeController() {
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createVolumeController(): call between audio.begin() and audio.start()");
    return BLEAudioVolumeController();
  }
  auto c = std::make_shared<BLEAudioVolumeController::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  /* `roles` owns the role until end(); a strong capture here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioVolumeController::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_VCP_CONTROLLER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioVcpCtlrInit());
  });
  return BLEAudioVolumeController(c);
#else
  log_e("Volume controller is not enabled in this build (CONFIG_BT_VCP_VOL_CTLR)");
  return BLEAudioVolumeController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
