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
#include "audio/BLEAudioHearingAid.h"
#include "audio/BLEAudioControlLink.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioHearingAid.cpp
 * @brief HAS hearing aid device and hearing aid controller role handles.
 *
 * Both roles are thin wrappers over the control engine unit
 * (BLEAudioEngineControl): the factory registers an event handler for the
 * role's group and a deferred init that runs inside BLEAudio::start().
 * Presets added before start are staged and registered by that init.
 *
 * API contract is documented on the declarations in `BLEAudioHearingAid.h`;
 * the definitions below carry implementation notes only.
 */

// --------------------------------------------------------------------------
// Device
// --------------------------------------------------------------------------

struct BLEAudioHearingAidDevice::Impl {
  /** A preset staged before start. */
  struct Preset {
    uint8_t index;
    bool available;
    bool writable;
    String name;
  };

  std::shared_ptr<BLEAudio::Impl> audio;
  BLEHearingAidType type = BLEHearingAidType::Monaural;
  bool presetSync = false;
  std::vector<Preset> presets;  ///< Staged until start; released once registered.
  SelectCallback selectCb;
  RenameCallback renameCb;

  /** Register one preset with the stack. */
  static int add(const Preset &p) {
    return bleAudioHasSrvAddPreset(p.index, p.name.c_str(), p.available, p.writable);
  }

  /** Deferred init run by BLEAudio::start(): register HAS, then every staged preset (stops at the first failure). */
  BTStatus apply() {
    int err = bleAudioHasSrvInit(static_cast<uint8_t>(type), presetSync);
    for (const Preset &p : presets) {
      if (err != 0) {
        break;
      }
      err = add(p);
      if (err != 0) {
        log_e("HearingAid: registering preset %u (\"%s\") failed (err=%d)", p.index, p.name.c_str(), err);
      }
    }
    std::vector<Preset>().swap(presets);
    return bleAudioStatus(err);
  }

  /** Engine event for this group: a client selected or renamed a preset. */
  void handle(const ble_audio_evt_t &e) {
    if (!e.data) {
      return;
    }
    if (e.type == BLE_AUDIO_EVT_HAS_SRV_SELECT) {
      const auto *d = static_cast<const ble_audio_has_select_t *>(e.data);
      if (selectCb) {
        selectCb(d->index, d->sync);
      }
    } else if (e.type == BLE_AUDIO_EVT_HAS_SRV_NAME) {
      const auto *d = static_cast<const ble_audio_has_preset_t *>(e.data);
      if (renameCb) {
        renameCb(d->index, String(d->name ? d->name : ""));
      }
    }
  }
};

BLEAudioHearingAidDevice::BLEAudioHearingAidDevice() = default;

BLEAudioHearingAidDevice::operator bool() const {
  return _impl != nullptr;
}

BLEAudioHearingAidDevice &BLEAudioHearingAidDevice::setType(BLEHearingAidType type) {
  if (_impl) {
    _impl->type = type;
  }
  return *this;
}

BLEAudioHearingAidDevice &BLEAudioHearingAidDevice::setPresetSync(bool enabled) {
  if (_impl) {
    _impl->presetSync = enabled;
  }
  return *this;
}

BLEAudioHearingAidDevice &BLEAudioHearingAidDevice::addPreset(uint8_t index, const String &name, bool available, bool writable) {
  if (!_impl) {
    return *this;
  }
  Impl::Preset p{index, available, writable, name};
  if (!_impl->audio->started) {
    _impl->presets.push_back(std::move(p));
  } else if (Impl::add(p) != 0) {
    log_e("HearingAid: registering preset %u (\"%s\") after start failed", index, p.name.c_str());
  }
  return *this;
}

BTStatus BLEAudioHearingAidDevice::removePreset(uint8_t index) {
  return _impl ? bleAudioStatus(bleAudioHasSrvRemovePreset(index)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidDevice::setPresetAvailable(uint8_t index, bool available) {
  return _impl ? bleAudioStatus(bleAudioHasSrvSetAvailable(index, available)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidDevice::renamePreset(uint8_t index, const String &name) {
  return _impl ? bleAudioStatus(bleAudioHasSrvRename(index, name.c_str())) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidDevice::setActivePreset(uint8_t index) {
  return _impl ? bleAudioStatus(bleAudioHasSrvSetActive(index)) : BTStatus::InvalidState;
}

uint8_t BLEAudioHearingAidDevice::getActivePreset() const {
  return _impl ? bleAudioHasSrvGetActive() : 0;
}

BLEAudioHearingAidDevice &BLEAudioHearingAidDevice::onPresetSelected(SelectCallback cb) {
  if (_impl) {
    _impl->selectCb = std::move(cb);
  }
  return *this;
}

BLEAudioHearingAidDevice &BLEAudioHearingAidDevice::onPresetRenamed(RenameCallback cb) {
  if (_impl) {
    _impl->renameCb = std::move(cb);
  }
  return *this;
}

void BLEAudioHearingAidDevice::resetCallbacks() {
  if (_impl) {
    _impl->selectCb = nullptr;
    _impl->renameCb = nullptr;
  }
}

BLEAudioHearingAidDevice BLEAudio::createHearingAidDevice() {
#if BLE_AUDIO_HAS_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createHearingAidDevice(): call between audio.begin() and audio.start()");
    return BLEAudioHearingAidDevice();
  }
  auto d = std::make_shared<BLEAudioHearingAidDevice::Impl>();
  d->audio = _impl;
  _impl->roles.push_back(d);
  /* `roles` owns the role until end(); strong captures here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioHearingAidDevice::Impl> weak = d;
  _impl->setHandler(BLE_AUDIO_GRP_HAS_SERVER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? s->apply() : BTStatus::OK;
  });
  return BLEAudioHearingAidDevice(d);
#else
  log_e("Hearing aid device is not enabled in this build (CONFIG_BT_HAS)");
  return BLEAudioHearingAidDevice();
#endif
}

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioHearingAidController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLEAudioControlLink link;  ///< The hearing aid being controlled.
  uint8_t active = 0;        ///< Peer's active preset, 0 when unknown (reset on discover and disconnect).
  DiscoveredCallback discoveredCb;
  PresetCallback presetCb;
  SwitchCallback switchCb;

  /**
   * Engine event: core events drive the link (deferred discovery, disconnect);
   * role events are delivered only for the bound peer.
   */
  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioHasCliDiscover);
      if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED && !link.bound()) {
        active = 0;
      }
      if (err && discoveredCb) {
        discoveredCb(bleAudioStatus(err), BLEHearingAidType::Monaural);
      }
      return;
    }
    if (e.conn_handle != link.conn || !e.data) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_HAS_CLI_DISCOVERED:
        if (discoveredCb) {
          discoveredCb(bleAudioStatus(e.err), static_cast<BLEHearingAidType>(*static_cast<const uint8_t *>(e.data)));
        }
        break;
      case BLE_AUDIO_EVT_HAS_CLI_PRESET:
        if (!e.err && presetCb) {
          const auto *p = static_cast<const ble_audio_has_preset_t *>(e.data);
          presetCb(p->index, p->available, String(p->name ? p->name : ""), p->is_last);
        }
        break;
      case BLE_AUDIO_EVT_HAS_CLI_ACTIVE: {
        const uint8_t index = *static_cast<const uint8_t *>(e.data);
        if (!e.err) {
          active = index;
        }
        if (switchCb) {
          switchCb(bleAudioStatus(e.err), index);
        }
        break;
      }
      default: break;
    }
  }
};

BLEAudioHearingAidController::BLEAudioHearingAidController() = default;

BLEAudioHearingAidController::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioHearingAidController::discover(uint16_t connHandle) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  _impl->active = 0;
  return _impl->link.discover(*_impl->audio, connHandle, bleAudioHasCliDiscover);
}

uint16_t BLEAudioHearingAidController::getConnHandle() const {
  return _impl ? _impl->link.conn : BLE_AUDIO_CONN_NONE;
}

BTStatus BLEAudioHearingAidController::readPresets(uint8_t startIndex, uint8_t maxCount) {
  return _impl ? bleAudioStatus(bleAudioHasCliReadPresets(_impl->link.conn, startIndex, maxCount)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::setActivePreset(uint8_t index, bool sync) {
  return _impl ? bleAudioStatus(bleAudioHasCliSetActive(_impl->link.conn, index, sync)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::nextPreset(bool sync) {
  return _impl ? bleAudioStatus(bleAudioHasCliStep(_impl->link.conn, true, sync)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::previousPreset(bool sync) {
  return _impl ? bleAudioStatus(bleAudioHasCliStep(_impl->link.conn, false, sync)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::renamePreset(uint8_t index, const String &name) {
  return _impl ? bleAudioStatus(bleAudioHasCliRename(_impl->link.conn, index, name.c_str())) : BTStatus::InvalidState;
}

uint8_t BLEAudioHearingAidController::getActivePreset() const {
  return _impl ? _impl->active : 0;
}

BLEAudioHearingAidController &BLEAudioHearingAidController::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioHearingAidController &BLEAudioHearingAidController::onPreset(PresetCallback cb) {
  if (_impl) {
    _impl->presetCb = std::move(cb);
  }
  return *this;
}

BLEAudioHearingAidController &BLEAudioHearingAidController::onPresetSwitch(SwitchCallback cb) {
  if (_impl) {
    _impl->switchCb = std::move(cb);
  }
  return *this;
}

void BLEAudioHearingAidController::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->presetCb = nullptr;
    _impl->switchCb = nullptr;
  }
}

BLEAudioHearingAidController BLEAudio::createHearingAidController() {
#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createHearingAidController(): call between audio.begin() and audio.start()");
    return BLEAudioHearingAidController();
  }
  auto c = std::make_shared<BLEAudioHearingAidController::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  /* `roles` owns the role until end(); a strong capture here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioHearingAidController::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_HAS_CLIENT, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioHasCliInit());
  });
  return BLEAudioHearingAidController(c);
#else
  log_e("Hearing aid controller is not enabled in this build (CONFIG_BT_HAS_CLIENT)");
  return BLEAudioHearingAidController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
