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
#include "BLEAudioBroadcastSource.h"
#include "BLEAudioAuracast.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioControlLink.h"
#include "BLEAudioStreamImpl.h"
#include "BLEAudioProfiles.h"
#include "BLE.h"
#include "advertising/BLEAdvertising.h"
#include "advertising/BLEAdvertisementData.h"
#include "esp_random.h"
#include "esp32-hal-log.h"
#include "core/BLESync.h"
#include "core/BLETask.h"

/**
 * @file BLEAudioBroadcastSource.cpp
 * @brief BAP Broadcast Source role on top of the BAP engine unit.
 *
 * The engine creates the source (BIG parameters + BASE) and attaches the BIG
 * to an advertising set; everything the receivers scan for (extended and
 * periodic advertising payloads) is built here with the library's own
 * `BLEAdvertising`, so it works the same on NimBLE and Bluedroid.
 *
 * The source is created lazily by start() and re-created only when a setting
 * that is baked into it (codec, QoS, channels, code, context) changed: the
 * `dirty` flag tracks that.
 *
 * API contract is documented on the declarations in `BLEAudioBroadcastSource.h`;
 * the definitions below carry implementation notes only.
 */

namespace {

constexpr uint8_t kAdTypeServiceData16 = BLEAudioAuracast::kAdTypeServiceData16;

}  // namespace

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioBroadcastSource::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (defaults, role list).
  BLESharedMutex mtx;                     ///< Protects this role's callbacks and shared state.
  BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_16_2_1;
  BLEAudioQos qos;  ///< Used instead of the preset QoS when customQos.
  bool customQos = false;
  uint8_t channels = 1;  ///< BIS count (1 or 2).
  uint32_t broadcastId = esp_random() & 0xFFFFFF;
  String code;  ///< Broadcast Code; empty = unencrypted.
  String name;  ///< Broadcast Name; empty = device name.
  BLEAudioContext context = BLEAudioContext::Media;
  bool publicBroadcast = true;
  bool created = false;      ///< Engine source exists.
  bool dirty = true;         ///< A baked-in setting changed; re-create at start().
  bool starting = false;     ///< start() called, BSRC_STARTED not received yet.
  bool streaming = false;    ///< BIG up (BSRC_STARTED .. BSRC_STOPPED).
  int16_t advInstance = -1;  ///< Advertising instance in use, -1 when stopped.
  BLESync stopSync{"bsrc-stop"};
  std::vector<BLEAudioStream> streams;  ///< One Tx stream per BIS.
  Callback startedCb;
  StoppedCallback stoppedCb;

  BTStatus waitStopped(uint32_t timeoutMs = 5000) {
    return bleAudioWaitUntilIdle(
      mtx, stopSync, "BroadcastSource",
      [this]() {
        return streaming || starting;
      },
      timeoutMs
    );
  }

  /** @brief Match the owned streams to the channel count (stops at the first pool failure). */
  void resizeStreams() {
    while (streams.size() > channels) {
      streams.pop_back();
    }
    while (streams.size() < channels) {
      BLEAudioStream s = BLEAudioStreamAccess::create(BLE_AUDIO_STREAM_BROADCAST_SOURCE);
      if (!s) {
        break;
      }
      streams.push_back(s);
    }
    dirty = true;
  }

  /**
   * @brief (Re)create the engine source from the current settings.
   *
   * Stereo gives each BIS its own location (FL, FR); mono leaves the
   * per-BIS allocation out, which the BASE reads as Mono. The Broadcast Code
   * is zero-padded to 16 octets as the Core spec requires.
   */
  int create() {
    BLELockGuard lock(mtx.get());
    if (created) {
      bleAudioBsrcDelete();
      created = false;
    }
    ble_audio_codec_t c;
    ble_audio_qos_t q;
    if (!bleAudioPresetGet(static_cast<uint8_t>(preset), true, &c, &q)) {
      log_e("BroadcastSource: invalid preset %u", (unsigned)preset);
      return ESP_ERR_INVALID_ARG;
    }
    if (customQos) {
      q = bleAudioQosToEngine(qos);
    } else {
      q.pd_us = audio->presentationDelayUs;
    }
    const uint8_t n = (uint8_t)streams.size();
    ble_audio_slot_t *slots[2] = {};
    uint32_t locations[2] = {};
    for (uint8_t i = 0; i < n && i < 2; i++) {
      slots[i] = BLEAudioStreamAccess::slot(streams[i]);
      locations[i] = n > 1 ? static_cast<uint32_t>(i == 0 ? BLEAudioLocation::FrontLeft : BLEAudioLocation::FrontRight) : 0;
    }
    uint8_t bcode[BLE_AUDIO_BCODE_SIZE] = {};
    int err = bleAudioBsrcCreate(slots, n > 1 ? locations : nullptr, n, &c, &q, static_cast<uint16_t>(context), bleAudioPackBroadcastCode(code, bcode));
    if (err != 0) {
      log_e("BroadcastSource: create failed (%u BIS, %lu Hz, err=%d)", n, (unsigned long)c.sample_rate_hz, err);
    }
    created = (err == 0);
    dirty = !created;
    return err;
  }

  /** @brief PBP features: High Quality for 48 kHz presets, Standard otherwise, plus Encryption. */
  uint8_t pbaFeatures() const {
    ble_audio_codec_t c = {};
    uint8_t f = BLEAudioPublicBroadcast::StandardQuality;
    if (bleAudioPresetGet(static_cast<uint8_t>(preset), true, &c, nullptr) && c.sample_rate_hz == 48000) {
      f = BLEAudioPublicBroadcast::HighQuality;
    }
    if (code.length()) {
      f |= BLEAudioPublicBroadcast::Encryption;
    }
    return f;
  }

  /**
   * @brief Configure and start extended + periodic advertising on @p inst.
   *
   * Extended advertising: Flags, Appearance, Broadcast Audio Announcement
   * (0x1852 + Broadcast ID), Public Broadcast Announcement (0x1856, PBP
   * builds), Broadcast Name and Complete Local Name.
   * Periodic advertising: the BASE as one Service Data AD structure (the
   * engine's BASE already starts with its 0x1851 UUID).
   */
  BTStatus advertise(uint8_t inst, const String &advName) {
    uint8_t base[128];
    uint16_t baseLen = 0;
    int err = bleAudioBsrcGetBase(base, sizeof(base), &baseLen);
    if (err != 0) {
      log_e("BroadcastSource: BASE encoding failed (err=%d)", err);
      return bleAudioStatus(err);
    }

    BLEAdvertising adv = BLE.getAdvertising();
    adv.setExtType(inst, BLEAdvType::NonConnectable);
    // Primary and secondary PHY on 1M: phone Auracast scanners often fail to
    // join when the AUX/periodic train is on 2M (they list the source as
    // "Unknown" and ignore taps).
    adv.setExtPhy(inst, BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    adv.setExtSID(inst, inst & 0x0F);

    BLEAdvertisementData ext(BLEAdvertisementData::EXTENDED_MAX_PAYLOAD);
    {
      BLELockGuard lock(mtx.get());
#if BLE_AUDIO_PBP_SUPPORTED
      BLEAudioAuracast::buildExt(ext, broadcastId, advName, publicBroadcast, static_cast<uint16_t>(context), pbaFeatures());
#else
      BLEAudioAuracast::buildExt(ext, broadcastId, advName, false, 0, 0);
#endif
    }
    BTStatus st = adv.setExtAdvertisementData(inst, ext);
    if (!st) {
      log_e("BroadcastSource: extended advertising data rejected on instance %u (%s)", inst, st.toString());
      return st;
    }

    adv.setPeriodicAdvInterval(inst, 0x20, 0x40);  // 40-80 ms.
    BLEAdvertisementData per(BLEAdvertisementData::EXTENDED_MAX_PAYLOAD);
    uint8_t field[2 + sizeof(base)];
    field[0] = (uint8_t)(1 + baseLen);
    field[1] = kAdTypeServiceData16;
    memcpy(field + 2, base, baseLen);
    per.addRaw(field, 2 + baseLen);
    if (!(st = adv.setPeriodicAdvData(inst, per)) || !(st = adv.startExtended(inst, 0, 0)) || !(st = adv.startPeriodicAdv(inst))) {
      log_e("BroadcastSource: advertising start failed on instance %u (%s)", inst, st.toString());
      (void)adv.stopPeriodicAdv(inst);
      (void)adv.stopExtended(inst);
      return st;
    }
    return BTStatus::OK;
  }

  /** @brief Event handler for the BROADCAST_SOURCE group (host task). */
  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_BSRC_STARTED) {
      Callback cb;
      {
        BLELockGuard lock(mtx.get());
        // A late STARTED after stop() already tore the source down must not
        // mark us streaming: that wedges the next start() (InvalidState).
        if (!starting) {
          stopSync.give(BTStatus::OK);
          return;
        }
        starting = false;
        streaming = true;
        cb = startedCb;
      }
      log_i("BroadcastSource: BIG up, %u BIS streaming (id 0x%06lX)", (unsigned)streams.size(), (unsigned long)broadcastId);
      // A stop() issued while start() was in flight waits on this, so it can
      // issue the real terminate now that the source exists.
      stopSync.give(BTStatus::OK);
      if (cb) {
        cb();
      }
    } else if (e.type == BLE_AUDIO_EVT_BSRC_STOPPED) {
      StoppedCallback cb;
      {
        BLELockGuard lock(mtx.get());
        starting = false;
        streaming = false;
        cb = stoppedCb;
      }
      log_i("BroadcastSource: BIG terminated (reason 0x%02x)", (unsigned)(uint8_t)e.err);
      stopSync.give(BTStatus::OK);
      if (cb) {
        cb((uint8_t)e.err);
      }
    }
  }
};

// --------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------

BLEAudioBroadcastSource::BLEAudioBroadcastSource() = default;

BLEAudioBroadcastSource::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioBroadcastSource::setPreset(BLEAudioCodecPreset preset) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.starting || impl.streaming) {
    log_e("BroadcastSource: setPreset() while starting or streaming");
    return BTStatus::InvalidState;
  }
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  if (!bleAudioPresetGet(static_cast<uint8_t>(preset), true, &c, &q)) {
    log_e("BroadcastSource: invalid preset %u", (unsigned)preset);
    return BTStatus::InvalidParam;
  }
  impl.preset = preset;
  impl.dirty = true;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::setQos(const BLEAudioQos &qos) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.starting || impl.streaming) {
    log_e("BroadcastSource: setQos() while starting or streaming");
    return BTStatus::InvalidState;
  }
  impl.qos = qos;
  impl.customQos = true;
  impl.dirty = true;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::setChannels(uint8_t channels) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.streaming || impl.starting) {
    log_e("BroadcastSource: setChannels() while starting or streaming");
    return BTStatus::InvalidState;
  }
  if (channels < 1 || channels > 2) {
    log_e("BroadcastSource: invalid channels %u (1 = mono, 2 = stereo)", channels);
    return BTStatus::InvalidParam;
  }
  if (impl.created) {
    // A stopped source still owns its stream slots; delete it before the
    // vector is resized so the engine never points at freed handles.
    bleAudioBsrcDelete();
    impl.created = false;
    impl.dirty = true;
  }
  impl.channels = channels;
  impl.resizeStreams();
  return BTStatus::OK;
}

/** Only advertised, not baked into the source, so no re-create is needed. */
BTStatus BLEAudioBroadcastSource::setBroadcastId(uint32_t broadcastId) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  impl.broadcastId = broadcastId & 0xFFFFFF;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::setBroadcastCode(const String &code) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (code.length() > BLE_AUDIO_BCODE_SIZE) {
    log_e("BroadcastSource: Broadcast Code is %u characters, the maximum is %u", (unsigned)code.length(), BLE_AUDIO_BCODE_SIZE);
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.code = code;
  impl.dirty = true;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::setName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (name.length() > BLEAudioAuracast::kMaxNameLen) {
    log_e("BroadcastSource: Broadcast Name is %u characters, the maximum is %u", (unsigned)name.length(), BLEAudioAuracast::kMaxNameLen);
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.name = name;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::setContext(BLEAudioContext context) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  impl.context = context;
  impl.dirty = true;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::setPublicBroadcast(bool enable) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  impl.publicBroadcast = enable;
  return BTStatus::OK;
}

BTStatus BLEAudioBroadcastSource::configureAuracast(const String &name, uint8_t channels, BLEAudioCodecPreset preset) {
  if (!name.length()) {
    log_e("BroadcastSource: configureAuracast() needs a Broadcast Name");
    return BTStatus::InvalidParam;
  }
  BTStatus st = setPreset(preset);
  if (!st) {
    return st;
  }
  st = setChannels(channels);
  if (!st) {
    return st;
  }
  st = setName(name);
  if (!st) {
    return st;
  }
  st = setContext(BLEAudioContext::Media);
  if (!st) {
    return st;
  }
  return setPublicBroadcast(true);
}

// --------------------------------------------------------------------------
// Control
// --------------------------------------------------------------------------

/** Order: source (BASE) first, then advertising (needs the BASE), then the BIG (needs the train). */
BTStatus BLEAudioBroadcastSource::start(uint8_t advInstance) {
  if (!_impl || !_impl->audio->started) {
    log_e("BroadcastSource: start() before audio.begin()");
    return BTStatus::InvalidState;
  }
  bool dirty;
  {
    BLELockGuard lock(_impl->mtx.get());
    if (_impl->streaming || _impl->starting) {
      log_w("BroadcastSource: start() while already starting/streaming");
      return BTStatus::InvalidState;
    }
    if (_impl->streams.empty()) {
      log_e("BroadcastSource: start() without streams (stream pool exhausted?)");
      return BTStatus::InvalidState;
    }
    dirty = _impl->dirty;
    // Mark starting before create(): source_create can emit BSRC_STARTED, and a
    // late STARTED after start() returns must not be dropped as spurious.
    _impl->starting = true;
    _impl->advInstance = advInstance;
  }
  if (dirty) {
    int err = _impl->create();
    if (err != 0) {
      BLELockGuard lock(_impl->mtx.get());
      _impl->starting = false;
      _impl->advInstance = -1;
      return bleAudioStatus(err);
    }
  }
  String cfgName;
  {
    BLELockGuard lock(_impl->mtx.get());
    cfgName = _impl->name;
  }
  const String advName = cfgName.length() ? cfgName : String(BLE.getDeviceName().c_str());
  BTStatus st = _impl->advertise(advInstance, advName);
  if (!st) {
    bleAudioBsrcDelete();
    BLELockGuard lock(_impl->mtx.get());
    _impl->created = false;
    _impl->dirty = true;
    _impl->starting = false;
    _impl->advInstance = -1;
    return st;
  }
  int err = bleAudioBsrcStart(advInstance);
  if (err != 0) {
    log_e("BroadcastSource: BIG creation failed on instance %u (err=%d)", advInstance, err);
    BLEAdvertising adv = BLE.getAdvertising();
    (void)adv.stopPeriodicAdv(advInstance);
    (void)adv.stopExtended(advInstance);
    bleAudioBsrcDelete();
    BLELockGuard lock(_impl->mtx.get());
    _impl->created = false;
    _impl->dirty = true;
    _impl->starting = false;
    _impl->advInstance = -1;
    return bleAudioStatus(err);
  }
  log_i("BroadcastSource: \"%s\" starting on adv instance %u (id 0x%06lX)", advName.c_str(), advInstance, (unsigned long)getBroadcastId());
  return BTStatus::OK;
}

/** Advertising is released only after the BIG has terminated. */
BTStatus BLEAudioBroadcastSource::stop() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("BroadcastSource: stop() cannot run from a stack callback; defer it to loop()");
    return BTStatus::Busy;
  }
  BLEAudioStopBudget budget;
  budget.start();
  int16_t instance;
  bool live;
  bool starting;
  {
    BLELockGuard lock(_impl->mtx.get());
    instance = _impl->advInstance;
    live = _impl->streaming;
    starting = _impl->starting;
  }
  if (instance < 0) {
    // Already stopped is a successful stop; a live BIG without an instance is
    // not recoverable here.
    return live ? BTStatus::InvalidState : BTStatus::OK;
  }
  // A start() still in flight: wait for it to resolve before terminating, so
  // bleAudioBsrcStop() targets a fully-created source. Otherwise the stop is a
  // no-op and the late BSRC_STARTED strands a live BIG with no advertising
  // instance (unrecoverable).
  if (starting) {
    _impl->stopSync.take();
    {
      BLELockGuard lock(_impl->mtx.get());
      if (!_impl->starting) {
        _impl->stopSync.give(BTStatus::OK);
      }
    }
    if (_impl->stopSync.wait(budget.remainingMs()) != BTStatus::OK) {
      return budget.stall("BroadcastSource", "start-resolve");
    }
  }
  // No extra gate here: with an exhausted budget the calls below simply do
  // not wait (timeout 0 fails fast); the exists-check at the end still
  // reports a live transport as Fail and clears a torn-down one as OK.
  int err = bleAudioBsrcStopTimeout(budget.remainingMs());
  if (err != 0 && err != ESP_ERR_INVALID_STATE && err != -EALREADY) {
    log_w("BroadcastSource: BIG terminate failed (err=%d)", err);
  } else if (!budget.expired()) {
    (void)_impl->waitStopped(budget.remainingMs());
  }
  // Drop the engine source only when it is really gone: delete is refused
  // while the BIG streams, and clearing the flags anyway wedges the next
  // start() behind a live source. NimBLE BAP may refuse source_stop() at QoS
  // Configured and omit STOPPED after a successful stop; delete + recreate is
  // the recoverable teardown.
  if (bleAudioBsrcHandle() != NULL) {
    bleAudioBsrcDelete();
  }
  if (bleAudioBsrcHandle() != NULL) {
    return budget.stall("BroadcastSource", "source-delete");
  }
  StoppedCallback cb;
  {
    BLELockGuard lock(_impl->mtx.get());
    const bool fire = _impl->starting || _impl->streaming;
    _impl->starting = false;
    _impl->streaming = false;
    _impl->created = false;
    _impl->dirty = true;
    _impl->advInstance = -1;
    if (fire) {
      cb = _impl->stoppedCb;
    }
  }
  BLEAdvertising adv = BLE.getAdvertising();
  (void)adv.stopPeriodicAdv((uint8_t)instance);
  (void)adv.stopExtended((uint8_t)instance);
  if (cb) {
    cb(0);
  }
  return BTStatus::OK;
}

bool BLEAudioBroadcastSource::isStreaming() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streaming;
}

/** Updates the metadata of the existing source in place (no re-create, no BIG restart). */
BTStatus BLEAudioBroadcastSource::updateContext(BLEAudioContext context) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->created) {
      log_e("BroadcastSource: updateContext() before start()");
      return BTStatus::InvalidState;
    }
  }
  int err = bleAudioBsrcUpdateContext(static_cast<uint16_t>(context));
  if (err != 0) {
    log_e("BroadcastSource: metadata update failed (err=%d)", err);
    return bleAudioStatus(err);
  }
  BLELockGuard lock(_impl->mtx.get());
  _impl->context = context;
  return BTStatus::OK;
}

uint32_t BLEAudioBroadcastSource::getBroadcastId() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->broadcastId;
}

// --------------------------------------------------------------------------
// Streams and callbacks
// --------------------------------------------------------------------------

size_t BLEAudioBroadcastSource::streamCount() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streams.size();
}

BLEAudioStream BLEAudioBroadcastSource::stream(size_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  BLELockGuard lock(_impl->mtx.get());
  return index < _impl->streams.size() ? _impl->streams[index] : BLEAudioStream();
}

void BLEAudioBroadcastSource::onStarted(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.startedCb = std::move(cb);
}

void BLEAudioBroadcastSource::onStopped(StoppedCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.stoppedCb = std::move(cb);
}

void BLEAudioBroadcastSource::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.startedCb = nullptr;
  impl.stoppedCb = nullptr;
}

// --------------------------------------------------------------------------
// Factory
// --------------------------------------------------------------------------

/**
 * A broadcast source registers no GATT service, so it may also be created
 * after audio.begin(); it only needs the engine (begin()).
 */
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
BLEAudioBroadcastSource BLEAudio::createBroadcastSource() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioBroadcastSource();  // begin() already logged the cause
  }
  auto src = std::make_shared<BLEAudioBroadcastSource::Impl>();
  src->audio = _impl;
  src->resizeStreams();
  _impl->roles.push_back(src);
  std::weak_ptr<BLEAudioBroadcastSource::Impl> weak = src;
  _impl->setHandler(BLE_AUDIO_GRP_BROADCAST_SOURCE, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleStops.push_back([weak]() {
    if (auto s = weak.lock()) {
      (void)BLEAudioBroadcastSource(s).stop();
    }
  });
  return BLEAudioBroadcastSource(src);
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
BLEAudioBroadcastSource BLEAudio::createBroadcastSource() {
  log_e("BroadcastSource: not enabled in this build (CONFIG_BT_BAP_BROADCAST_SOURCE)");
  return BLEAudioBroadcastSource();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
