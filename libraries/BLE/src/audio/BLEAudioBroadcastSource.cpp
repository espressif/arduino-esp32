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
#include "audio/BLEAudioBroadcastSource.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "audio/BLEAudioProfiles.h"
#include "BLE.h"
#include "advertising/BLEAdvertising.h"
#include "advertising/BLEAdvertisementData.h"
#include "esp_random.h"
#include "esp32-hal-log.h"

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

constexpr uint16_t kBroadcastAudioAnnouncementUuid = 0x1852;   ///< Carries the 24-bit Broadcast ID.
constexpr uint16_t kPublicBroadcastAnnouncementUuid = 0x1856;  ///< PBP: features + metadata.
constexpr uint8_t kAdTypeServiceData16 = 0x16;
constexpr uint16_t kAppearanceBroadcastingDevice = 0x0885;
constexpr uint8_t kMetaStreamingContext = 0x02;  ///< LTV type: Streaming Audio Contexts.
constexpr uint8_t kMetaProgramInfo = 0x03;       ///< LTV type: Program Info (UTF-8).
constexpr uint8_t kMaxNameLen = 32;              ///< Broadcast Name limit (Assigned Numbers, 4..32 octets).

}  // namespace

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioBroadcastSource::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (defaults, role list).
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
  bool streaming = false;                 ///< BIG up (BSRC_STARTED .. BSRC_STOPPED).
  int16_t advInstance = -1;               ///< Advertising instance in use, -1 when stopped.
  std::vector<BLEAudioStream> streams;    ///< One Tx stream per BIS.
  Callback startedCb;
  StoppedCallback stoppedCb;

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
    if (created) {
      bleAudioBsrcDelete();
      created = false;
    }
    ble_audio_codec_t c;
    ble_audio_qos_t q;
    bleAudioPresetGet(static_cast<uint8_t>(preset), true, &c, &q);
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
    ble_audio_codec_t c;
    bleAudioPresetGet(static_cast<uint8_t>(preset), true, &c, nullptr);
    uint8_t f = (c.sample_rate_hz == 48000) ? BLEAudioPublicBroadcast::HighQuality : BLEAudioPublicBroadcast::StandardQuality;
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
    ext.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
    ext.setAppearance(kAppearanceBroadcastingDevice);
    const uint8_t id[3] = {(uint8_t)broadcastId, (uint8_t)(broadcastId >> 8), (uint8_t)(broadcastId >> 16)};
    ext.setServiceData(BLEUUID(kBroadcastAudioAnnouncementUuid), id, sizeof(id));
#if BLE_AUDIO_PBP_SUPPORTED
    if (publicBroadcast) {
      // PBA metadata: Streaming Audio Contexts + Program Info (the name).
      const uint16_t ctx = static_cast<uint16_t>(context);
      const size_t n = advName.length() < kMaxNameLen ? advName.length() : kMaxNameLen;
      uint8_t meta[4 + 2 + kMaxNameLen];
      size_t m = 0;
      meta[m++] = 3;
      meta[m++] = kMetaStreamingContext;
      meta[m++] = (uint8_t)ctx;
      meta[m++] = (uint8_t)(ctx >> 8);
      meta[m++] = (uint8_t)(1 + n);
      meta[m++] = kMetaProgramInfo;
      memcpy(meta + m, advName.c_str(), n);
      m += n;
      uint8_t pba[2 + 2 + sizeof(meta)];
      const size_t pbaLen = BLEAudioPublicBroadcast::buildAnnouncement(pbaFeatures(), meta, m, pba, sizeof(pba));
      if (pbaLen > 2) {
        // buildAnnouncement() starts with the UUID, which setServiceData() adds itself.
        ext.setServiceData(BLEUUID(kPublicBroadcastAnnouncementUuid), pba + 2, pbaLen - 2);
      }
    }
#endif
    // Scanners disagree on the title: Samsung Listen uses the Broadcast Name,
    // others the Complete Local Name, so publish both.
    ext.setBroadcastName(advName);
    ext.setName(advName);
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
      return st;
    }
    return BTStatus::OK;
  }

  /** @brief Event handler for the BROADCAST_SOURCE group (host task). */
  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_BSRC_STARTED) {
      streaming = true;
      log_i("BroadcastSource: BIG up, %u BIS streaming (id 0x%06lX)", (unsigned)streams.size(), (unsigned long)broadcastId);
      if (startedCb) {
        startedCb();
      }
    } else if (e.type == BLE_AUDIO_EVT_BSRC_STOPPED) {
      streaming = false;
      log_i("BroadcastSource: BIG terminated (reason 0x%02x)", (unsigned)(uint8_t)e.err);
      if (stoppedCb) {
        stoppedCb((uint8_t)e.err);
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
  if (_impl) {
    _impl->preset = preset;
    _impl->dirty = true;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setQos(const BLEAudioQos &qos) {
  if (_impl) {
    _impl->qos = qos;
    _impl->customQos = true;
    _impl->dirty = true;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setChannels(uint8_t channels) {
  if (_impl && !_impl->streaming) {
    _impl->channels = channels < 1 ? 1 : (channels > 2 ? 2 : channels);
    _impl->resizeStreams();
  } else if (_impl) {
    log_w("BroadcastSource: setChannels() ignored while streaming");
  }
  return *this;
}

/** Only advertised, not baked into the source, so no re-create is needed. */
BLEAudioBroadcastSource &BLEAudioBroadcastSource::setBroadcastId(uint32_t broadcastId) {
  if (_impl) {
    _impl->broadcastId = broadcastId & 0xFFFFFF;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setBroadcastCode(const String &code) {
  if (_impl) {
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
    _impl->name = name;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setContext(BLEAudioContext context) {
  if (_impl) {
    _impl->context = context;
    _impl->dirty = true;
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::setPublicBroadcast(bool enable) {
  if (_impl) {
    _impl->publicBroadcast = enable;
  }
  return *this;
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
  if (_impl->streaming) {
    log_w("BroadcastSource: start() while already streaming");
    return BTStatus::InvalidState;
  }
  if (_impl->streams.empty()) {
    log_e("BroadcastSource: start() without streams (stream pool exhausted?)");
    return BTStatus::InvalidState;
  }
  if (_impl->dirty) {
    int err = _impl->create();
    if (err != 0) {
      return bleAudioStatus(err);
    }
  }
  const String advName = _impl->name.length() ? _impl->name : String(BLE.getDeviceName().c_str());
  BTStatus st = _impl->advertise(advInstance, advName);
  if (!st) {
    return st;
  }
  _impl->advInstance = advInstance;
  int err = bleAudioBsrcStart(advInstance);
  if (err != 0) {
    log_e("BroadcastSource: BIG creation failed on instance %u (err=%d)", advInstance, err);
    return bleAudioStatus(err);
  }
  log_i("BroadcastSource: \"%s\" starting on adv instance %u (id 0x%06lX)", advName.c_str(), advInstance, (unsigned long)_impl->broadcastId);
  return BTStatus::OK;
}

/** The advertising is stopped even when the BIG terminate fails, so the instance is released. */
BTStatus BLEAudioBroadcastSource::stop() {
  if (!_impl || _impl->advInstance < 0) {
    return BTStatus::InvalidState;
  }
  int err = bleAudioBsrcStop();
  if (err != 0) {
    log_w("BroadcastSource: BIG terminate failed (err=%d)", err);
  }
  BLEAdvertising adv = BLE.getAdvertising();
  (void)adv.stopPeriodicAdv((uint8_t)_impl->advInstance);
  (void)adv.stopExtended((uint8_t)_impl->advInstance);
  _impl->advInstance = -1;
  return bleAudioStatus(err);
}

bool BLEAudioBroadcastSource::isStreaming() const {
  return _impl && _impl->streaming;
}

/** Updates the metadata of the existing source in place (no re-create, no BIG restart). */
BTStatus BLEAudioBroadcastSource::updateContext(BLEAudioContext context) {
  if (!_impl || !_impl->created) {
    log_e("BroadcastSource: updateContext() before start()");
    return BTStatus::InvalidState;
  }
  _impl->context = context;
  int err = bleAudioBsrcUpdateContext(static_cast<uint16_t>(context));
  if (err != 0) {
    log_e("BroadcastSource: metadata update failed (err=%d)", err);
  }
  return bleAudioStatus(err);
}

uint32_t BLEAudioBroadcastSource::getBroadcastId() const {
  return _impl ? _impl->broadcastId : 0;
}

// --------------------------------------------------------------------------
// Streams and callbacks
// --------------------------------------------------------------------------

size_t BLEAudioBroadcastSource::streamCount() const {
  return _impl ? _impl->streams.size() : 0;
}

BLEAudioStream BLEAudioBroadcastSource::stream(size_t index) const {
  return (_impl && index < _impl->streams.size()) ? _impl->streams[index] : BLEAudioStream();
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::onStarted(Callback cb) {
  if (_impl) {
    _impl->startedCb = std::move(cb);
  }
  return *this;
}

BLEAudioBroadcastSource &BLEAudioBroadcastSource::onStopped(StoppedCallback cb) {
  if (_impl) {
    _impl->stoppedCb = std::move(cb);
  }
  return *this;
}

void BLEAudioBroadcastSource::resetCallbacks() {
  if (_impl) {
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
  return BLEAudioBroadcastSource(src);
#else
  log_e("BroadcastSource: not enabled in this build (CONFIG_BT_BAP_BROADCAST_SOURCE)");
  return BLEAudioBroadcastSource();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
