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

#include "BLE.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEAudioHooks.h"
#include "core/BLEImplHelpers.h"
#include "core/BLESync.h"
#include "core/BLETask.h"
#include "engine/BLEAudioEngineBap.h"
#include "engine/BLEAudioEngineCap.h"
#include "BLEAudioStreamImpl.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudio.cpp
 * @brief Backend-agnostic bodies of the LE Audio controller handle, core weak hook, and value types.
 */

// --------------------------------------------------------------------------
// BLEClass factory & pre-end weak hook (shared across NimBLE and Bluedroid)
// --------------------------------------------------------------------------

namespace {

BLEAudio &audioControllerSlot() {
  static BLEAudio slot;
  return slot;
}

constexpr uint32_t kIdleTimeoutMs = 5000;
BLESync sBapIdleSync{"bap-idle"};

bool waitBapIdle() {
  if (!bleAudioBapIsoBusy()) {
    return true;
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("Audio: cannot wait for ISO idle from a stack callback");
    return false;
  }
  sBapIdleSync.take();
  if (!bleAudioBapIsoBusy()) {
    sBapIdleSync.give(BTStatus::OK);
    return true;
  }
  BTStatus st = sBapIdleSync.wait(kIdleTimeoutMs);
  if (st != BTStatus::OK || bleAudioBapIsoBusy()) {
    log_e("Audio: timed out waiting for ISO streams to disconnect");
    return false;
  }
  return true;
}

void engineSink(const ble_audio_evt_t *evt, void *ctx) {
  static_cast<BLEAudio::Impl *>(ctx)->dispatch(*evt);
  bleAudioNotifyBapIdle();
}

}  // namespace

extern "C" bool bleAudioPreEndHook() {
  auto &audio = audioControllerSlot();
  if (!audio) {
    return true;
  }
  return static_cast<bool>(audio.end());
}

BLEAudio BLEClass::getAudioController() {
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || !_impl || _impl->ending) {
    log_e("getAudioController: BLE not initialized");
    return BLEAudio();
  }
  auto &slot = audioControllerSlot();
  if (!slot) {
    slot = BLEAudioImplCommon::makeHandle(std::make_shared<BLEAudio::Impl>());
    BLEAudio::Impl::backendOnControllerCreated(*_impl);
  }
  return slot;
}

// --------------------------------------------------------------------------
// BLEAudioImplCommon shared event routing
// --------------------------------------------------------------------------

void BLEAudioImplCommon::trackLink(const ble_audio_evt_t &e) {
  BLELockGuard lock(mtx.get());
  if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED && e.err == 0 && !isGattReady(e.conn_handle)) {
    for (uint16_t &c : gattReady) {
      if (c == BLE_AUDIO_CONN_NONE) {
        c = e.conn_handle;
        break;
      }
    }
  } else if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED) {
    for (uint16_t &c : gattReady) {
      if (c == e.conn_handle) {
        c = BLE_AUDIO_CONN_NONE;
      }
    }
    static_cast<BLEAudio::Impl *>(this)->backendOnAclDisconnected(e.conn_handle);
  }
}

void BLEAudioImplCommon::dispatch(const ble_audio_evt_t &e) {
  const uint8_t group = BLE_AUDIO_EVT_GROUP(e.type);
  EventHandler coreHandlers[BLE_AUDIO_GRP_COUNT];
  EventHandler groupHandler;
  LinkCallback linkReady;
  LinkCallback disconnected;
  if (group == BLE_AUDIO_GRP_CORE) {
    trackLink(e);
    BLELockGuard lock(mtx.get());
    for (size_t i = 0; i < BLE_AUDIO_GRP_COUNT; i++) {
      coreHandlers[i] = handlers[i];
    }
    linkReady = onLinkReadyCb;
    disconnected = onDisconnectedCb;
  } else if (group < BLE_AUDIO_GRP_COUNT) {
    BLELockGuard lock(mtx.get());
    groupHandler = handlers[group];
  }
  if (group == BLE_AUDIO_GRP_CORE) {
    for (auto &h : coreHandlers) {
      if (h) {
        h(e);
      }
    }
    if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED && e.err == 0 && linkReady) {
      linkReady(e.conn_handle);
    } else if ((e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED || (e.type == BLE_AUDIO_EVT_ACL_CONNECTED && e.err != 0)) && disconnected) {
      disconnected(e.conn_handle);
    }
  } else if (groupHandler) {
    groupHandler(e);
  }
}

// --------------------------------------------------------------------------
// Value types (BAP LC3 presets)
// --------------------------------------------------------------------------

BLEAudioCodecConfig BLEAudioCodecConfig::fromPreset(BLEAudioCodecPreset preset) {
  ble_audio_codec_t c;
  if (!bleAudioPresetGet(static_cast<uint8_t>(preset), false, &c, nullptr)) {
    log_e("Audio: unknown codec preset %u", (unsigned)preset);
    return BLEAudioCodecConfig();
  }
  return bleAudioCodecFromEngine(c);
}

uint8_t BLEAudioCodecConfig::channels() const {
  return bleAudioChannelCount(static_cast<uint32_t>(channelAllocation));
}

BLEAudioQos BLEAudioQos::fromPreset(BLEAudioCodecPreset preset, bool broadcast) {
  ble_audio_qos_t q;
  if (!bleAudioPresetGet(static_cast<uint8_t>(preset), broadcast, nullptr, &q)) {
    log_e("Audio: unknown codec preset %u", (unsigned)preset);
    return BLEAudioQos();
  }
  return bleAudioQosFromEngine(q);
}

extern "C" void bleAudioNotifyBapIdle(void) {
  if (!bleAudioBapIsoBusy()) {
    sBapIdleSync.give(BTStatus::OK);
  }
}

#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
namespace {
BLESync sBsrcStateSync{"bsrc-state"};
}  // namespace

extern "C" void bleAudioNotifyBsrcState(void) {
  sBsrcStateSync.give(BTStatus::OK);
}

extern "C" void bleAudioArmBsrcStateWait(void) {
  sBsrcStateSync.take();
}

extern "C" bool bleAudioWaitBsrcState(uint32_t timeoutMs) {
  if (BLETask::inStackCallbackTask()) {
    return false;
  }
  return sBsrcStateSync.wait(timeoutMs) == BTStatus::OK;
}
#endif

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

BLEAudio::BLEAudio() = default;

BLEAudio::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudio::ensureActive() {
  if (_impl && _impl->active) {
    return BTStatus::OK;
  }
  return initEngine();
}

BTStatus BLEAudio::initEngine() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!BLE.isInitialized() || BLE.isEnding()) {
    log_e("Audio: begin() requires an active BLE session");
    return BTStatus::InvalidState;
  }
  if (impl.active) {
    return BTStatus::OK;
  }
  int err = bleAudioEngineInit(engineSink, &impl);
  if (err != 0) {
    log_e("Audio: engine init failed (err=%d)", err);
    return bleAudioStatus(err);
  }
  BLEAudioStreamAccess::installEngineCallbacks();
  err = bleAudioBapAttach();
  if (err != 0) {
    log_e("Audio: BAP attach failed (err=%d)", err);
    (void)bleAudioEngineDeinit();
    return bleAudioStatus(err);
  }
  impl.active = true;
  log_i("Audio: engine initialized");
  return BTStatus::OK;
}

BTStatus BLEAudio::begin() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus ens = ensureActive();
  if (ens != BTStatus::OK) {
    log_e("Audio: begin() without an initialized engine (%s)", ens.toString());
    return ens;
  }
  if (impl.started) {
    return BTStatus::OK;
  }
  if (impl.beginFailed) {
    log_e("Audio: begin() failed before; run end(), fix the role set, then begin() again");
    return BTStatus::InvalidState;
  }
  // Security is settled here, not at the first create*(): explicit BLESecurity
  // setup and setAutoSecurityPolicy() may come in any order before begin().
  // Afterwards the setters just overwrite these defaults (last writer wins
  // until the first pairing), but the flag itself is no longer consulted.
  BLESecurity sec = BLE.getSecurity();
  if (sec && !sec.isConfigured()) {
    if (impl.autoSecurity.load()) {
      sec.setIOCapability(BLEIOCapability::NoInputNoOutput);
      sec.setAuthenticationMode(true, false, true);
      log_i("Audio: applying default security policy (Just Works, bonding, SC, no MITM); "
            "configure BLESecurity explicitly or call setAutoSecurityPolicy(false) to override");
    } else {
      log_w("Audio: automatic security policy off and BLESecurity not configured; "
            "links stay unencrypted and audio discovery may fail");
    }
  }
  // The commit always runs — even with no staged audio roles, classic
  // services and the broadcast engine still need the single GATT publish.
  // Busy is transient (retryable) and never poisons begin(); anything else
  // is terminal until end().
  while (!impl.roleApplies.empty()) {
    auto &apply = impl.roleApplies.front();
    BTStatus st = apply ? apply() : BTStatus::OK;
    if (st == BTStatus::Busy) {
      return st;
    }
    if (!st) {
      log_e("Audio: role registration failed (%s); run end(), fix the role set, then begin() again", st.toString());
      impl.beginFailed = true;
      return st;
    }
    impl.roleApplies.erase(impl.roleApplies.begin());
  }
  int err = bleAudioPacsCommit();
  if (err != 0) {
    BTStatus pst = bleAudioStatus(err);
    if (pst == BTStatus::Busy) {
      return pst;
    }
    log_e("Audio: PACS registration failed (%s)", pst.toString());
    impl.beginFailed = true;
    return pst;
  }
  err = bleAudioEngineStart();
  if (err != 0) {
    BTStatus est = bleAudioStatus(err);
    if (est == BTStatus::Busy) {
      return est;
    }
    log_e("Audio: engine start / GATT commit failed (%s)", est.toString());
    impl.beginFailed = true;
    return est;
  }
  impl.started = true;
  log_i("Audio: started (%u roles)", (unsigned)impl.roles.size());
  return BTStatus::OK;
}

BTStatus BLEAudio::end() {
  BLE_CHECK_IMPL(BTStatus::OK);
  if (!impl.active) {
    return BTStatus::OK;
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("Audio: end() cannot run from a stack callback; defer it to loop()");
    return BTStatus::Busy;
  }
  for (auto &stop : impl.roleStops) {
    if (stop) {
      stop();
    }
  }
  impl.backendPreEndTeardown();
  if (bleAudioCapBroadcastExists()) {
    (void)bleAudioCapBroadcastStop();
  }
  if (bleAudioCapUnicastGroupExists()) {
    (void)bleAudioCapUnicastStop();
  }
  bleAudioBapStopLeftover();
  if (!waitBapIdle()) {
    return BTStatus::Fail;
  }
  int err = bleAudioEngineDeinit();
  if (err != 0) {
    log_e("Audio: end() refused while streams are active; stop them first (err=%d)", err);
    return bleAudioStatus(err);
  }
  {
    BLELockGuard lock(impl.mtx.get());
    for (auto &h : impl.handlers) {
      h = nullptr;
    }
    for (uint16_t &c : impl.gattReady) {
      c = BLE_AUDIO_CONN_NONE;
    }
    impl.backendResetLinks();
  }
  impl.roleStops.clear();
  impl.roleApplies.clear();
  impl.roles.clear();
  impl.active = false;
  impl.started = false;
  impl.beginFailed = false;
  log_i("Audio: engine released");
  return BTStatus::OK;
}

bool BLEAudio::isActive() const {
  BLE_CHECK_IMPL(false);
  return impl.active;
}

bool BLEAudio::isStarted() const {
  BLE_CHECK_IMPL(false);
  return impl.started;
}

// --------------------------------------------------------------------------
// Links
// --------------------------------------------------------------------------

BTStatus BLEAudio::connect(const BTAddress &address) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.started || !BLE.isInitialized() || BLE.isEnding()) {
    log_e("Audio: connect() before the commit (audio.begin())");
    return BTStatus::InvalidState;
  }
  log_d("Audio: connecting to %s", address.toString().c_str());
  int err = bleAudioEngineConnect(static_cast<uint8_t>(address.type()), address.data());
  if (err != ESP_ERR_NOT_SUPPORTED) {
    if (err != 0) {
      log_e("Audio: connect to %s failed (err=%d)", address.toString().c_str(), err);
    }
    return bleAudioStatus(err);
  }
  BTStatus st = impl.backendConnect(address);
  if (!st) {
    log_e("Audio: connect to %s failed (%s)", address.toString().c_str(), st.toString());
    bleAudioEngineCancelConnect();
  }
  return st;
}

BTStatus BLEAudio::disconnect(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.active || !impl.started || !BLE.isInitialized() || BLE.isEnding()) {
    return BTStatus::InvalidState;
  }
  if (connHandle == BLE_AUDIO_CONN_NONE) {
    return BTStatus::InvalidParam;
  }
  const BTStatus st = bleAudioEngineStatus(bleAudioEngineDisconnect(connHandle));
  if (st == BTStatus::Fail) {
    log_e("Audio: disconnect failed");
  }
  return st;
}

void BLEAudio::onLinkReady(LinkCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onLinkReadyCb = std::move(cb);
}

void BLEAudio::onDisconnected(LinkCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDisconnectedCb = std::move(cb);
}

void BLEAudio::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onLinkReadyCb = nullptr;
  impl.onDisconnectedCb = nullptr;
}

// --------------------------------------------------------------------------
// Defaults
// --------------------------------------------------------------------------

BTStatus BLEAudio::setPresentationDelay(uint32_t delayUs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.presentationDelayUs = delayUs;
  return BTStatus::OK;
}

uint32_t BLEAudio::getPresentationDelay() const {
  BLE_CHECK_IMPL(40000u);
  return impl.presentationDelayUs.load();
}

BTStatus BLEAudio::setAutoSecurityPolicy(bool enable) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.autoSecurity.store(enable);
  return BTStatus::OK;
}

bool BLEAudio::autoSecurityPolicy() const {
  BLE_CHECK_IMPL(true);
  return impl.autoSecurity.load();
}

#endif /* BLE_AUDIO_SUPPORTED */
