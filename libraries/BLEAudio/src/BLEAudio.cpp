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

#include "BLEAudioImpl.h"
#include <type_traits>

static_assert(std::is_base_of<BLEAudioImplCommon, BLEAudio::Impl>::value, "BLEAudio::Impl must inherit BLEAudioImplCommon");
#include "engine/BLEAudioEngineBap.h"
#include "engine/BLEAudioEngineCap.h"
#include "engine/BLEAudioStreamInternal.h"
#include "BLE.h"
#include "client/BLEClient.h"
#include "core/BLESync.h"
#include "core/BLETask.h"
#include "esp32-hal-log.h"
#include <algorithm>

/**
 * @file BLEAudio.cpp
 * @brief Backend-agnostic bodies of the LE Audio controller handle and value types.
 *
 * The controller drives the engine lifecycle (`BLEAudioEngine.h`):
 * begin() initializes the engine with `BLEAudio::Impl::dispatch` as its only
 * event sink, start() runs the roles' staged registrations and commits GATT,
 * end() tears everything down. Role factories live next to their roles.
 *
 * API contract is documented on the declarations in `BLEAudio.h` and
 * `BLEAudioTypes.h`; the definitions below carry implementation notes only.
 */

// --------------------------------------------------------------------------
// Value types (BAP LC3 presets)
// --------------------------------------------------------------------------

/** The preset table lives in the engine (`BLEAudioEngine.c`) and is shared with the roles. */
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

// --------------------------------------------------------------------------
// Engine event sink
// --------------------------------------------------------------------------

namespace {

constexpr uint32_t kIdleTimeoutMs = 5000;
BLESync sBapIdleSync{"bap-idle"};

bool waitBapIdle() {
  if (!bleAudioBapIsoBusy()) {
    return true;
  }
  if (bleInStackCallbackTask()) {
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

/**
 * @brief Engine event sink registered by begin() (Bluetooth host task).
 *
 * @p ctx is the controller Impl, which outlives the engine: end() deinits
 * the engine before the Impl can be released.
 */
void engineSink(const ble_audio_evt_t *evt, void *ctx) {
  static_cast<BLEAudio::Impl *>(ctx)->dispatch(*evt);
  bleAudioNotifyBapIdle();
}

}  // namespace

extern "C" void bleAudioNotifyBapIdle(void) {
  if (!bleAudioBapIsoBusy()) {
    sBapIdleSync.give(BTStatus::OK);
  }
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

BLEAudio::BLEAudio() = default;

BLEAudio::operator bool() const {
  return _impl != nullptr;
}

/**
 * The stream callback table is installed before the BAP unit attaches so no
 * stream event can reach a null table. A failed attach rolls the engine back,
 * leaving the controller inactive and begin() retryable.
 */
BTStatus BLEAudio::begin() {
  if (!_impl || !BLE.isInitialized() || BLE.isEnding()) {
    log_e("Audio: begin() requires an active BLE session");
    return BTStatus::InvalidState;
  }
  if (_impl->active) {
    return BTStatus::OK;
  }
  int err = bleAudioEngineInit(engineSink, _impl.get());
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
  // Every LE Audio link is encrypted before any profile is discovered, and on
  // Bluedroid PACS/ASCS carry encrypted-only permissions (a plain discovery on
  // an unencrypted link is rejected with Insufficient Authentication). Apply the
  // Just Works association model unless the sketch already chose one, so
  // audio.begin() alone yields a usable link. Reconfigure BLE.getSecurity()
  // afterwards for passkey/numeric-comparison setups.
  BLESecurity sec = BLE.getSecurity();
  if (sec && !sec.isConfigured()) {
    sec.setIOCapability(BLEIOCapability::NoInputNoOutput);
    sec.setAuthenticationMode(true, false, true);
  }
  _impl->active = true;
  log_i("Audio: engine initialized");
  return BTStatus::OK;
}

/**
 * Order matters: every role stages its PAC records while applying, so the
 * PACS commit must follow the role applies, and the engine start (which owns
 * the single GATT commit) must follow PACS. A failure leaves `started` false;
 * the roles already applied stay registered until end().
 */
BTStatus BLEAudio::start() {
  if (!_impl || !_impl->active || !BLE.isInitialized() || BLE.isEnding()) {
    log_e("Audio: start() before begin()");
    return BTStatus::InvalidState;
  }
  if (_impl->started) {
    return BTStatus::OK;
  }
  // Consume successful applies from the front. If role N fails, roles
  // 0..N-1 stay registered and a later start() resumes at N instead of
  // re-running non-idempotent registrations.
  while (!_impl->roleApplies.empty()) {
    auto &apply = _impl->roleApplies.front();
    BTStatus st = apply ? apply() : BTStatus::OK;
    if (!st) {
      log_e("Audio: role registration failed (%s)", st.toString());
      return st;
    }
    _impl->roleApplies.erase(_impl->roleApplies.begin());
  }
  int err = bleAudioPacsCommit();
  if (err != 0) {
    log_e("Audio: PACS registration failed (err=%d)", err);
    return bleAudioStatus(err);
  }
  err = bleAudioEngineStart();
  if (err != 0) {
    log_e("Audio: engine start / GATT commit failed (err=%d)", err);
    return bleAudioStatus(err);
  }
  _impl->started = true;
  log_i("Audio: started (%u roles)", (unsigned)_impl->roles.size());
  return BTStatus::OK;
}

/**
 * Stop leftover BAP transports and wait for every pool ISO channel to go
 * DISCONNECTED. Native deinit is refused while a stream is still up; in that
 * case nothing is released and the controller stays usable. On success the
 * role handlers are dropped before the roles themselves, so no event can
 * reach a role being destroyed.
 */
BTStatus BLEAudio::end() {
  if (!_impl || !_impl->active) {
    return BTStatus::OK;
  }
  if (bleInStackCallbackTask()) {
    log_e("Audio: end() cannot run from a stack callback; defer it to loop()");
    return BTStatus::Busy;
  }
  // Release role-owned advertising/scans while the host is still up. The
  // engine deinit below only releases profiles and ISO. Keep roleStops until
  // deinit succeeds so a refused end() can retry them later.
  for (auto &stop : _impl->roleStops) {
    if (stop) {
      stop();
    }
  }
  // NimBLE fallback clients are owned by audio; cancel a pending connect and
  // drop a live ACL before the host is deinitialized. Move them out under the
  // lock, then call into the core without holding it.
  std::vector<std::shared_ptr<BLEClient>> clients;
  {
    BLELockGuard lock(_impl->mtx.get());
    clients.swap(_impl->clients);
  }
  for (auto &client : clients) {
    if (client) {
      (void)client->cancelConnect();
      if (client->isConnected()) {
        (void)client->disconnect();
      }
    }
  }
  // CAP initiator streams live in their own engine group, not in the BAP
  // leftovers. Stop them before waiting so a handover session cannot keep the
  // CIG/BIG allocated into the next BLE session.
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
    BLELockGuard lock(_impl->mtx.get());
    for (auto &h : _impl->handlers) {
      h = nullptr;
    }
    for (uint16_t &c : _impl->gattReady) {
      c = BLE_AUDIO_CONN_NONE;
    }
    _impl->clients.clear();
  }
  _impl->roleStops.clear();
  _impl->roleApplies.clear();
  _impl->roles.clear();
  _impl->active = false;
  _impl->started = false;
  log_i("Audio: engine released");
  return BTStatus::OK;
}

bool BLEAudio::isActive() const {
  return _impl && _impl->active;
}

bool BLEAudio::isStarted() const {
  return _impl && _impl->started;
}

// --------------------------------------------------------------------------
// Links
// --------------------------------------------------------------------------

/**
 * Bluedroid: the engine opens the link on its own GATT client interface, so
 * the peer's audio services are discovered by the engine.
 *
 * NimBLE: the engine reports ESP_ERR_NOT_SUPPORTED after arming its link
 * bring-up, and the ACL is opened with a plain `BLEClient`. The engine then
 * drives encryption, MTU exchange and discovery from the GAP events, exactly
 * as on Bluedroid. The client is kept in `roles` so it lives until end().
 */
BTStatus BLEAudio::connect(const BTAddress &address) {
  if (!_impl || !_impl->started || !BLE.isInitialized() || BLE.isEnding()) {
    log_e("Audio: connect() before start()");
    return BTStatus::InvalidState;
  }
  log_d("Audio: connecting to %s", address.toString().c_str());
  int err = bleAudioEngineConnect(static_cast<uint8_t>(address.type()), address.data());
#if BLE_BLUEDROID
  if (err == ESP_ERR_NOT_SUPPORTED) {
    log_e("Audio: Bluedroid connect to %s reported NotSupported", address.toString().c_str());
    return BTStatus::NotSupported;
  }
#endif
  if (err != ESP_ERR_NOT_SUPPORTED) {
    if (err != 0) {
      log_e("Audio: connect to %s failed (err=%d)", address.toString().c_str(), err);
    }
    return bleAudioStatus(err);
  }
  auto client = std::make_shared<BLEClient>(BLE.createClient());
  BTStatus st = client->connectAsync(address);
  if (st) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->clients.erase(std::remove_if(_impl->clients.begin(), _impl->clients.end(), [](const std::shared_ptr<BLEClient> &client) {
                           return !client || !client->isConnected();
                         }),
                         _impl->clients.end());
    _impl->clients.push_back(client);
  } else {
    log_e("Audio: connect to %s failed (%s)", address.toString().c_str(), st.toString());
    bleAudioEngineCancelConnect();
  }
  return st;
}

BTStatus BLEAudio::disconnect(uint16_t connHandle) {
  if (!_impl || !_impl->active || !_impl->started || !BLE.isInitialized() || BLE.isEnding()) {
    return BTStatus::InvalidState;
  }
  if (connHandle == BLE_AUDIO_CONN_NONE) {
    return BTStatus::InvalidParam;
  }
  int err = bleAudioEngineDisconnect(connHandle);
  if (err == 0) {
    return BTStatus::OK;
  }
  return BTStatus(static_cast<BTStatus::Code>(err));
}

BLEAudio &BLEAudio::onLinkReady(LinkCallback cb) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->linkReadyCb = std::move(cb);
  }
  return *this;
}

BLEAudio &BLEAudio::onDisconnected(LinkCallback cb) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->disconnectedCb = std::move(cb);
  }
  return *this;
}

void BLEAudio::resetCallbacks() {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->linkReadyCb = nullptr;
    _impl->disconnectedCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Defaults
// --------------------------------------------------------------------------

/** Read by the role factories and applies, so it must be set before creating roles. */
BLEAudio &BLEAudio::setPresentationDelay(uint32_t delayUs) {
  if (_impl) {
    _impl->presentationDelayUs = delayUs;
  }
  return *this;
}

uint32_t BLEAudio::getPresentationDelay() const {
  return _impl ? _impl->presentationDelayUs.load() : 40000u;
}

#endif /* BLE_AUDIO_SUPPORTED */
