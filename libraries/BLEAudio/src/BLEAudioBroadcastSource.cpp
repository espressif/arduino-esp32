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
#include "BLEAudioImpl.h"
#include "engine/BLEAudioStreamInternal.h"
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
  BLESharedMutex mtx;  ///< Protects this role's callbacks and shared state.
  BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_16_2_1;
  BLEAudioQos qos;                        ///< Used instead of the preset QoS when customQos.
  bool customQos = false;
  uint8_t channels = 1;                   ///< BIS count (1 or 2).
  uint32_t broadcastId = esp_random() & 0xFFFFFF;
  String code;                            ///< Broadcast Code; empty = unencrypted.
  String name;                            ///< Broadcast Name; empty = device name.
  BLEAudioContext context = BLEAudioContext::Media;
  bool publicBroadcast = true;
  bool created = false;                   ///< Engine source exists.
  bool dirty = true;                      ///< A baked-in setting changed; re-create at start().
  bool starting = false;                  ///< start() called, BSRC_STARTED not received yet.
  bool streaming = false;                 ///< BIG up (BSRC_STARTED .. BSRC_STOPPED).
  int16_t advInstance = -1;               ///< Advertising instance in use, -1 when stopped.
  BLESync stopSync{"bsrc-stop"};
  std::vector<BLEAudioStream> streams;    ///< One Tx stream per BIS.
  Callback startedCb;
  StoppedCallback stoppedCb;

  BTStatus waitStopped() {
    {
      BLELockGuard lock(mtx.get());
      if (!streaming && !starting) {
        return BTStatus::OK;
      }
    }
    if (bleInStackCallbackTask()) {
      log_e("BroadcastSource: cannot wait for BIG terminate from a stack callback");
      return BTStatus::InvalidState;
    }
    stopSync.take();
    {
      BLELockGuard lock(mtx.get());
      if (!streaming && !starting) {
        stopSync.give(BTStatus::OK);
        return BTStatus::OK;
      }
    }
    BTStatus st = stopSync.wait(5000);
    BLELockGuard lock(mtx.get());
    if (st != BTStatus::OK || streaming || starting) {
      log_e("BroadcastSource: timed out waiting for BIG to terminate");
      return st == BTStatus::OK ? BTStatus::Fail : st;
    }
    return BTStatus::OK;
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
    const size_t codeLen = code.length() < sizeof(bcode) ? code.length() : sizeof(bcode);
    memcpy(bcode, code.c_str(), codeLen);
    int err = bleAudioBsrcCreate(slots, n > 1 ? locations : nullptr, n, &c, &q, static_cast<uint16_t>(context), codeLen ? bcode : nullptr);
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
#if BLE_AUDIO_PBP_SUPPORTED
    BTStatus st = BLEAudioAuracast::buildExt(ext, broadcastId, advName, publicBroadcast, static_cast<uint16_t>(context), pbaFeatures());
#else
    BTStatus st = BLEAudioAuracast::buildExt(ext, broadcastId, advName, false, 0, 0);
#endif
    st = adv.setExtAdvertisementData(inst, ext);
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

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setPreset(BLEAudioCodecPreset preset) {
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  if (_impl && bleAudioPresetGet(static_cast<uint8_t>(preset), true, &c, &q)) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->preset = preset;
    _impl->dirty = true;
  } else if (_impl) {
    log_e("BroadcastSource: invalid preset %u", (unsigned)preset);
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setQos(const BLEAudioQos &qos) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->qos = qos;
    _impl->customQos = true;
    _impl->dirty = true;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setChannels(uint8_t channels) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->streaming && !_impl->starting) {
      if (_impl->created) {
        // A stopped source still owns its stream slots; delete it before the
        // vector is resized so the engine never points at freed handles.
        bleAudioBsrcDelete();
        _impl->created = false;
        _impl->dirty = true;
      }
      _impl->channels = channels < 1 ? 1 : (channels > 2 ? 2 : channels);
      _impl->resizeStreams();
    } else {
      log_w("BroadcastSource: setChannels() ignored while streaming or starting");
    }
  }
  return *this;
}

/** Only advertised, not baked into the source, so no re-create is needed. */
BLEAudioBroadcastSource &BLEAudioBroadcastSource::setBroadcastId(uint32_t broadcastId) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->broadcastId = broadcastId & 0xFFFFFF;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setBroadcastCode(const String &code) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    if (code.length() > BLE_AUDIO_BCODE_SIZE) {
      log_w("BroadcastSource: Broadcast Code truncated to %u characters", BLE_AUDIO_BCODE_SIZE);
    }
    _impl->code = code;
    _impl->dirty = true;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setName(const String &name) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->name = name;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setContext(BLEAudioContext context) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->context = context;
    _impl->dirty = true;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setPublicBroadcast(bool enable) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->publicBroadcast = enable;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::configureAuracast(const String &name, uint8_t channels, BLEAudioCodecPreset preset) {
  return setPreset(preset)
    .setChannels(channels)
    .setName(name)
    .setContext(BLEAudioContext::Media)
    .setPublicBroadcast(true);
}

// --------------------------------------------------------------------------
// Control
// --------------------------------------------------------------------------

/** Order: source (BASE) first, then advertising (needs the BASE), then the BIG (needs the train). */
BTStatus BLEAudioBroadcastSource::start(uint8_t advInstance) {
  if (!_impl || !_impl->audio->started) {
    log_e("BroadcastSource: start() before audio.start()");
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
  const String advName = _impl->name.length() ? _impl->name : String(BLE.getDeviceName().c_str());
  BTStatus st = _impl->advertise(advInstance, advName);
  if (!st) {
    BLELockGuard lock(_impl->mtx.get());
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
    BLELockGuard lock(_impl->mtx.get());
    _impl->starting = false;
    _impl->advInstance = -1;
    return bleAudioStatus(err);
  }
  log_i("BroadcastSource: \"%s\" starting on adv instance %u (id 0x%06lX)", advName.c_str(), advInstance, (unsigned long)_impl->broadcastId);
  return BTStatus::OK;
}

/** Advertising is released only after the BIG has terminated. */
BTStatus BLEAudioBroadcastSource::stop() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
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
    if (bleInStackCallbackTask()) {
      log_e("BroadcastSource: cannot wait for start to resolve from a stack callback");
      return BTStatus::InvalidState;
    }
    _impl->stopSync.take();
    {
      BLELockGuard lock(_impl->mtx.get());
      if (!_impl->starting) {
        _impl->stopSync.give(BTStatus::OK);
      }
    }
    (void)_impl->stopSync.wait(5000);
  }
  int err = bleAudioBsrcStop();
  if (err != 0 && err != ESP_ERR_INVALID_STATE && err != -EALREADY) {
    log_w("BroadcastSource: BIG terminate failed (err=%d)", err);
  } else {
    (void)_impl->waitStopped();
  }
  // Always drop advertising and the engine source so a later start() is not
  // wedged by leftover starting/streaming flags. NimBLE BAP may refuse
  // source_stop() at QoS Configured and omit STOPPED after a successful stop;
  // delete + recreate is the recoverable teardown.
  if (bleAudioBsrcHandle() != NULL) {
    bleAudioBsrcDelete();
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

BLEAudioBroadcastSource &BLEAudioBroadcastSource::onStarted(Callback cb) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->startedCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::onStopped(StoppedCallback cb) {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->stoppedCb = std::move(cb);
  }
  return *this;
}

void BLEAudioBroadcastSource::resetCallbacks() {
  if (_impl) {
    BLELockGuard lock(_impl->mtx.get());
    _impl->startedCb = nullptr;
    _impl->stoppedCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Factory
// --------------------------------------------------------------------------

/**
 * A broadcast source registers no GATT service, so it may also be created
 * after audio.start(); it only needs the engine (begin()).
 */
BLEAudioBroadcastSource BLEAudio::createBroadcastSource() {
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
  if (!_impl || !_impl->active) {
    log_e("BroadcastSource: create after audio.begin()");
    return BLEAudioBroadcastSource();
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
#else
  log_e("BroadcastSource: not enabled in this build (CONFIG_BT_BAP_BROADCAST_SOURCE)");
  return BLEAudioBroadcastSource();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
