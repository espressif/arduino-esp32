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

#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "BLE.h"
#include "client/BLEClient.h"
#include "esp32-hal-log.h"

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

/**
 * @brief Engine event sink registered by begin() (Bluetooth host task).
 *
 * @p ctx is the controller Impl, which outlives the engine: end() deinits
 * the engine before the Impl can be released.
 */
void engineSink(const ble_audio_evt_t *evt, void *ctx) {
  static_cast<BLEAudio::Impl *>(ctx)->dispatch(*evt);
}

}  // namespace

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
  if (!_impl) {
    log_e("Audio: begin() on a null handle; use BLE.getAudioController() after BLE.begin()");
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
  if (!_impl || !_impl->active) {
    log_e("Audio: start() before begin()");
    return BTStatus::InvalidState;
  }
  if (_impl->started) {
    return BTStatus::OK;
  }
  for (auto &apply : _impl->roleApplies) {
    BTStatus st = apply ? apply() : BTStatus::OK;
    if (!st) {
      log_e("Audio: role registration failed (%s)", st.toString());
      return st;
    }
  }
  _impl->roleApplies.clear();
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
 * The engine refuses to deinit while an ISO stream is up; in that case
 * nothing is released and the controller stays usable. On success the role
 * handlers are dropped before the roles themselves, so no event can reach a
 * role being destroyed.
 */
BTStatus BLEAudio::end() {
  if (!_impl || !_impl->active) {
    return BTStatus::OK;
  }
  int err = bleAudioEngineDeinit();
  if (err != 0) {
    log_w("Audio: end() refused while streams are active; stop them first (err=%d)", err);
    return bleAudioStatus(err);
  }
  for (auto &h : _impl->handlers) {
    h = nullptr;
  }
  _impl->roleApplies.clear();
  _impl->roles.clear();
  for (uint16_t &c : _impl->gattReady) {
    c = BLE_AUDIO_CONN_NONE;
  }
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
  if (!_impl || !_impl->started) {
    log_e("Audio: connect() before start()");
    return BTStatus::InvalidState;
  }
  log_d("Audio: connecting to %s", address.toString().c_str());
  uint8_t bda[6];
  address.toEspBdAddr(bda);
  int err = bleAudioEngineConnect(static_cast<uint8_t>(address.type()), bda);
  if (err != ESP_ERR_NOT_SUPPORTED) {
    if (err != 0) {
      log_e("Audio: connect to %s failed (err=%d)", address.toString().c_str(), err);
    }
    return bleAudioStatus(err);
  }
  auto client = std::make_shared<BLEClient>(BLE.createClient());
  BTStatus st = client->connectAsync(address);
  if (st) {
    _impl->roles.push_back(client);
  } else {
    log_e("Audio: connect to %s failed (%s)", address.toString().c_str(), st.toString());
    bleAudioEngineCancelConnect();
  }
  return st;
}

BLEAudio &BLEAudio::onLinkReady(LinkCallback cb) {
  if (_impl) {
    _impl->linkReadyCb = std::move(cb);
  }
  return *this;
}

BLEAudio &BLEAudio::onDisconnected(LinkCallback cb) {
  if (_impl) {
    _impl->disconnectedCb = std::move(cb);
  }
  return *this;
}

void BLEAudio::resetCallbacks() {
  if (_impl) {
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
  return _impl ? _impl->presentationDelayUs : 40000u;
}

#endif /* BLE_AUDIO_SUPPORTED */
