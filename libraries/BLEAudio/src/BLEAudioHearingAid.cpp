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
#include "BLEAudioHearingAid.h"
#include "BLEAudioControlLink.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// Device
// --------------------------------------------------------------------------

struct BLEAudioHearingAidDevice::Impl {
  struct Preset {
    uint8_t index;
    bool available;
    bool writable;
    String name;
  };

  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEHearingAidType type = BLEHearingAidType::Monaural;
  bool presetSync = false;
  std::vector<Preset> presets;
  SelectCallback onPresetSelectedCb;
  RenameCallback onPresetRenamedCb;
  bool inited = false;

  static int add(const Preset &p) {
    return bleAudioHasSrvAddPreset(p.index, p.name.c_str(), p.available, p.writable);
  }

  BTStatus apply() {
    int err = 0;
    if (!inited) {
      err = bleAudioHasSrvInit(static_cast<uint8_t>(type), presetSync);
      if (err != 0) {
        return bleAudioStatus(err);
      }
      inited = true;
    }
    while (!presets.empty()) {
      err = add(presets.front());
      if (err != 0) {
        log_e("HearingAid: registering preset %u (\"%s\") failed (err=%d)", presets.front().index, presets.front().name.c_str(), err);
        return bleAudioStatus(err);
      }
      presets.erase(presets.begin());
    }
    return BTStatus::OK;
  }

  void handle(const ble_audio_evt_t &e) {
    if (!e.data) {
      return;
    }
    if (e.type == BLE_AUDIO_EVT_HAS_SRV_SELECT) {
      const auto *d = static_cast<const ble_audio_has_select_t *>(e.data);
      if (auto cb = bleAudioCopyCallback(*this, &Impl::onPresetSelectedCb)) {
        cb(d->index, d->sync);
      }
    } else if (e.type == BLE_AUDIO_EVT_HAS_SRV_NAME) {
      const auto *d = static_cast<const ble_audio_has_preset_t *>(e.data);
      if (auto cb = bleAudioCopyCallback(*this, &Impl::onPresetRenamedCb)) {
        cb(d->index, String(d->name ? d->name : ""));
      }
    }
  }
};

BLEAudioHearingAidDevice::BLEAudioHearingAidDevice() = default;

BLEAudioHearingAidDevice::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioHearingAidDevice::setType(BLEHearingAidType type) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (static_cast<uint8_t>(type) > static_cast<uint8_t>(BLEHearingAidType::Banded)) {
    log_e("HearingAidDevice::setType(): invalid type %u", static_cast<unsigned>(type));
    return BTStatus::InvalidParam;
  }
  if (impl.audio->started) {
    log_w("HearingAidDevice::setType() ignored after audio.begin() (the service is registered at start)");
    return BTStatus::InvalidState;
  }
  impl.type = type;
  return BTStatus::OK;
}

BTStatus BLEAudioHearingAidDevice::setPresetSync(bool enabled) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio->started) {
    log_w("HearingAidDevice::setPresetSync() ignored after audio.begin() (the service is registered at start)");
    return BTStatus::InvalidState;
  }
  impl.presetSync = enabled;
  return BTStatus::OK;
}

BTStatus BLEAudioHearingAidDevice::addPreset(uint8_t index, const String &name, bool available, bool writable) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (index == 0) {
    log_e("HearingAid: preset index 0 is invalid (indexes start at 1)");
    return BTStatus::InvalidParam;
  }
  if (name.length() == 0) {
    log_e("HearingAid: preset name must not be empty");
    return BTStatus::InvalidParam;
  }
  for (const Impl::Preset &q : impl.presets) {
    if (q.index == index) {
      log_e("HearingAid: preset index %u already staged", index);
      return BTStatus::InvalidParam;
    }
  }
  if (impl.presets.size() >= (size_t)CONFIG_BT_HAS_PRESET_COUNT) {
    log_e("HearingAid: preset table full (%d entries)", CONFIG_BT_HAS_PRESET_COUNT);
    return BTStatus::NoMemory;
  }
  Impl::Preset p{index, available, writable, name};
  if (!impl.audio->started) {
    impl.presets.push_back(std::move(p));
    return BTStatus::OK;
  }
  if (Impl::add(p) != 0) {
    log_e("HearingAid: registering preset %u (\"%s\") after start failed", index, p.name.c_str());
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAudioHearingAidDevice::removePreset(uint8_t index) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioHasSrvRemovePreset(index));
}

BTStatus BLEAudioHearingAidDevice::setPresetAvailable(uint8_t index, bool available) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioHasSrvSetAvailable(index, available));
}

BTStatus BLEAudioHearingAidDevice::renamePreset(uint8_t index, const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioHasSrvRename(index, name.c_str()));
}

BTStatus BLEAudioHearingAidDevice::setActivePreset(uint8_t index) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioHasSrvSetActive(index));
}

uint8_t BLEAudioHearingAidDevice::getActivePreset() const {
  BLE_CHECK_IMPL(0);
  return bleAudioHasSrvGetActive();
}

void BLEAudioHearingAidDevice::onPresetSelected(SelectCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onPresetSelectedCb = std::move(cb);
}

void BLEAudioHearingAidDevice::onPresetRenamed(RenameCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onPresetRenamedCb = std::move(cb);
}

void BLEAudioHearingAidDevice::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onPresetSelectedCb = nullptr;
  impl.onPresetRenamedCb = nullptr;
}

#if BLE_AUDIO_HAS_SUPPORTED
BLEAudioHearingAidDevice BLEAudio::createHearingAidDevice() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioHearingAidDevice();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioHearingAidDevice>(
    _impl, "createHearingAidDevice", BLE_AUDIO_GRP_HAS_SERVER,
    [](BLEAudioHearingAidDevice::Impl &s) {
      return s.apply();
    }
  );
}
#endif

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioHearingAidController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEAudioControlLink link;
  std::atomic<uint8_t> active{0};
  DiscoveredCallback onDiscoveredCb;
  PresetCallback onPresetCb;
  SwitchCallback onPresetSwitchCb;

  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioHasCliDiscover);
      if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED && !link.bound()) {
        active.store(0, std::memory_order_relaxed);
      }
      if (err) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(err), BLEHearingAidType::Monaural);
        }
      }
      return;
    }
    if (e.conn_handle != link.getConn() || !e.data) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_HAS_CLI_DISCOVERED:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(e.err), static_cast<BLEHearingAidType>(*static_cast<const uint8_t *>(e.data)));
        }
        break;
      case BLE_AUDIO_EVT_HAS_CLI_PRESET:
        if (!e.err) {
          const auto *p = static_cast<const ble_audio_has_preset_t *>(e.data);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onPresetCb)) {
            cb(p->index, p->available, String(p->name ? p->name : ""), p->is_last);
          }
        }
        break;
      case BLE_AUDIO_EVT_HAS_CLI_ACTIVE:
      {
        const uint8_t index = *static_cast<const uint8_t *>(e.data);
        if (!e.err) {
          active.store(index, std::memory_order_relaxed);
        }
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onPresetSwitchCb)) {
          cb(bleAudioStatus(e.err), index);
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
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.active.store(0, std::memory_order_relaxed);
  return impl.link.discover(*impl.audio, connHandle, bleAudioHasCliDiscover);
}

uint16_t BLEAudioHearingAidController::getConnHandle() const {
  BLE_CHECK_IMPL(BLE_AUDIO_CONN_NONE);
  return impl.link.getConn();
}

BTStatus BLEAudioHearingAidController::readPresets(uint8_t startIndex, uint8_t maxCount) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioHasCliReadPresets(c, startIndex, maxCount)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::setActivePreset(uint8_t index, bool sync) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioHasCliSetActive(c, index, sync)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::nextPreset(bool sync) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioHasCliStep(c, true, sync)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::previousPreset(bool sync) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioHasCliStep(c, false, sync)) : BTStatus::InvalidState;
}

BTStatus BLEAudioHearingAidController::renamePreset(uint8_t index, const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioHasCliRename(c, index, name.c_str())) : BTStatus::InvalidState;
}

uint8_t BLEAudioHearingAidController::getActivePreset() const {
  BLE_CHECK_IMPL(0);
  return impl.active.load(std::memory_order_relaxed);
}

void BLEAudioHearingAidController::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
}

void BLEAudioHearingAidController::onPreset(PresetCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onPresetCb = std::move(cb);
}

void BLEAudioHearingAidController::onPresetSwitch(SwitchCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onPresetSwitchCb = std::move(cb);
}

void BLEAudioHearingAidController::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
  impl.onPresetCb = nullptr;
  impl.onPresetSwitchCb = nullptr;
}

#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
BLEAudioHearingAidController BLEAudio::createHearingAidController() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioHearingAidController();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioHearingAidController>(
    _impl, "createHearingAidController", BLE_AUDIO_GRP_HAS_CLIENT,
    [](BLEAudioHearingAidController::Impl &) {
      return bleAudioStatus(bleAudioHasCliInit());
    }
  );
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_HAS_SUPPORTED
BLEAudioHearingAidDevice BLEAudio::createHearingAidDevice() {
  log_e("Hearing aid device is not enabled in this build (CONFIG_BT_HAS)");
  return BLEAudioHearingAidDevice();
}
#endif

#if !BLE_AUDIO_HAS_CLIENT_SUPPORTED
BLEAudioHearingAidController BLEAudio::createHearingAidController() {
  log_e("Hearing aid controller is not enabled in this build (CONFIG_BT_HAS_CLIENT)");
  return BLEAudioHearingAidController();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
