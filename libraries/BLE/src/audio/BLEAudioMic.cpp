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

#include "audio/BLEAudioMic.h"
#include "audio/BLEAudioControlLink.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioMic.cpp
 * @brief MICP microphone device and microphone controller role handles.
 *
 * Both roles are thin wrappers over the control engine unit
 * (BLEAudioEngineControl): the factory registers an event handler for the
 * role's group and a deferred init that runs inside BLEAudio::start(). The
 * handles cache the last reported mute state.
 *
 * API contract is documented on the declarations in `BLEAudioMic.h`; the
 * definitions below carry implementation notes only.
 */

// --------------------------------------------------------------------------
// Device
// --------------------------------------------------------------------------

struct BLEAudioMicDevice::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  bool muted = false;  ///< Initial state before start, then the last reported state.
  MuteCallback muteCb;

  /** Engine event for this group: the mute state changed (by a client or locally). */
  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_MICP_DEV_MUTE || !e.data) {
      return;
    }
    muted = *static_cast<const uint8_t *>(e.data) == BLE_AUDIO_MICP_MUTED;
    if (muteCb) {
      muteCb(muted);
    }
  }
};

BLEAudioMicDevice::BLEAudioMicDevice() = default;

BLEAudioMicDevice::operator bool() const {
  return _impl != nullptr;
}

BLEAudioMicDevice &BLEAudioMicDevice::setInitialMute(bool muted) {
  if (_impl) {
    _impl->muted = muted;
  }
  return *this;
}

BTStatus BLEAudioMicDevice::mute() {
  return _impl ? bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_MUTED)) : BTStatus::InvalidState;
}

BTStatus BLEAudioMicDevice::unmute() {
  return _impl ? bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_UNMUTED)) : BTStatus::InvalidState;
}

BTStatus BLEAudioMicDevice::disableMute() {
  return _impl ? bleAudioStatus(bleAudioMicpDevSetMute(BLE_AUDIO_MICP_DISABLED)) : BTStatus::InvalidState;
}

bool BLEAudioMicDevice::isMuted() const {
  return _impl && _impl->muted;
}

BLEAudioMicDevice &BLEAudioMicDevice::onMuteChanged(MuteCallback cb) {
  if (_impl) {
    _impl->muteCb = std::move(cb);
  }
  return *this;
}

void BLEAudioMicDevice::resetCallbacks() {
  if (_impl) {
    _impl->muteCb = nullptr;
  }
}

BLEAudioMicDevice BLEAudio::createMicDevice() {
#if BLE_AUDIO_MICP_DEVICE_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createMicDevice(): call between audio.begin() and audio.start()");
    return BLEAudioMicDevice();
  }
  auto d = std::make_shared<BLEAudioMicDevice::Impl>();
  d->audio = _impl;
  _impl->roles.push_back(d);
  /* `roles` owns the role until end(); strong captures here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioMicDevice::Impl> weak = d;
  _impl->setHandler(BLE_AUDIO_GRP_MICP_DEVICE, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? bleAudioStatus(bleAudioMicpDevInit(s->muted)) : BTStatus::OK;
  });
  return BLEAudioMicDevice(d);
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
  BLEAudioControlLink link;  ///< The microphone device being controlled.
  bool muted = false;        ///< Last state reported by the device.
  DiscoveredCallback discoveredCb;
  MuteCallback muteCb;
  InputCallback inputCb;

  /**
   * Engine event: core events drive the link (deferred discovery, disconnect);
   * role events are delivered only for the bound peer.
   */
  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioMicpCtlrDiscover);
      if (err && discoveredCb) {
        discoveredCb(bleAudioStatus(err), 0);
      }
      return;
    }
    if (e.conn_handle != link.conn) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_MICP_CTLR_DISCOVERED:
        if (discoveredCb) {
          discoveredCb(bleAudioStatus(e.err), e.data ? *static_cast<const uint8_t *>(e.data) : 0);
        }
        break;
      case BLE_AUDIO_EVT_MICP_CTLR_MUTE:
        if (!e.err && e.data) {
          muted = *static_cast<const uint8_t *>(e.data) == BLE_AUDIO_MICP_MUTED;
          if (muteCb) {
            muteCb(muted);
          }
        }
        break;
      case BLE_AUDIO_EVT_MICP_CTLR_INPUT:
        if (!e.err && e.data && inputCb) {
          const auto *a = static_cast<const ble_audio_aics_state_t *>(e.data);
          inputCb(a->gain, a->mute);
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
  return _impl ? _impl->link.discover(*_impl->audio, connHandle, bleAudioMicpCtlrDiscover) : BTStatus::InvalidState;
}

uint16_t BLEAudioMicController::getConnHandle() const {
  return _impl ? _impl->link.conn : BLE_AUDIO_CONN_NONE;
}

BTStatus BLEAudioMicController::mute() {
  return _impl ? bleAudioStatus(bleAudioMicpCtlrSetMute(_impl->link.conn, true)) : BTStatus::InvalidState;
}

BTStatus BLEAudioMicController::unmute() {
  return _impl ? bleAudioStatus(bleAudioMicpCtlrSetMute(_impl->link.conn, false)) : BTStatus::InvalidState;
}

BTStatus BLEAudioMicController::readMute() {
  return _impl ? bleAudioStatus(bleAudioMicpCtlrReadMute(_impl->link.conn)) : BTStatus::InvalidState;
}

bool BLEAudioMicController::isMuted() const {
  return _impl && _impl->muted;
}

BTStatus BLEAudioMicController::setInputGain(int8_t gain) {
  return _impl ? bleAudioStatus(bleAudioMicpCtlrSetGain(_impl->link.conn, gain)) : BTStatus::InvalidState;
}

BLEAudioMicController &BLEAudioMicController::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioMicController &BLEAudioMicController::onMuteChanged(MuteCallback cb) {
  if (_impl) {
    _impl->muteCb = std::move(cb);
  }
  return *this;
}

BLEAudioMicController &BLEAudioMicController::onInputChanged(InputCallback cb) {
  if (_impl) {
    _impl->inputCb = std::move(cb);
  }
  return *this;
}

void BLEAudioMicController::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->muteCb = nullptr;
    _impl->inputCb = nullptr;
  }
}

BLEAudioMicController BLEAudio::createMicController() {
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createMicController(): call between audio.begin() and audio.start()");
    return BLEAudioMicController();
  }
  auto c = std::make_shared<BLEAudioMicController::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  /* `roles` owns the role until end(); a strong capture here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioMicController::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_MICP_CONTROLLER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioMicpCtlrInit());
  });
  return BLEAudioMicController(c);
#else
  log_e("Microphone controller is not enabled in this build (CONFIG_BT_MICP_MIC_CTLR)");
  return BLEAudioMicController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
