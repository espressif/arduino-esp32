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
#include "BLEAudioUnicastServer.h"
#include "core/BLEMutex.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioStreamImpl.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioUnicastServer.cpp
 * @brief BAP Unicast Server role on top of the BAP engine unit.
 *
 * Configuration is staged in the Impl and applied once by BLEAudio::begin():
 * apply() merges the server's PAC records into the shared PACS staging
 * (`bleAudioPacsAdd`) and registers ASCS (`bleAudioUsInit`). From then on the
 * engine binds client-configured ASEs to the server's pool slots and reports
 * everything through the streams, so this role has no event handler.
 *
 * API contract is documented on the declarations in `BLEAudioUnicastServer.h`;
 * the definitions below carry implementation notes only.
 */

// --------------------------------------------------------------------------
// Defaults
// --------------------------------------------------------------------------

/** Every 10 ms preset at 16/24/32/48 kHz: the rates most clients propose first. */
static constexpr uint32_t kDefaultPresets = BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_24_2_1)
                                            | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_32_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1)
                                            | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_4_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_6_1);

static constexpr BLEAudioContext kDefaultContexts = BLEAudioContext::Unspecified | BLEAudioContext::Conversational | BLEAudioContext::Media;

/** Presets every interoperable phone offers first: 16/24/48 kHz at 10 ms. */
static constexpr uint32_t kMediaPresets =
  BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_24_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1);

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioUnicastServer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (defaults, role list).
  BLESharedMutex mtx;                     ///< Guards the staged configuration below.
  uint8_t sinkStreams = 1;                ///< Sink ASEs to register.
  uint8_t sourceStreams = 1;              ///< Source ASEs to register.
  uint8_t maxChannels = 1;                ///< Channels per ASE advertised in the PAC.
  uint32_t presets = kDefaultPresets;     ///< BLEAudioPresetBit() mask for the PAC.
  BLEAudioContext sinkContexts = kDefaultContexts;
  BLEAudioContext sourceContexts = kDefaultContexts;
  BLEAudioLocation sinkLocation = BLEAudioLocation::Mono;
  BLEAudioLocation sourceLocation = BLEAudioLocation::Mono;
  uint32_t pdMinUs = 20000;             ///< Supported presentation delay range.
  uint32_t pdMaxUs = 0;                 ///< 0 = controller default.
  bool applied = false;                 ///< ASCS registered; the ASE counts are frozen.
  std::vector<BLEAudioStream> streams;  ///< One pool slot per ASE.

  /**
   * @brief Match the owned streams to the configured ASE count.
   *
   * Streams are allocated eagerly so the sketch can attach callbacks before
   * start(). Stops at the first allocation failure (the pool logs it); the
   * engine then rejects client configs beyond the available slots.
   */
  void resizeStreams() {
    const size_t want = (size_t)sinkStreams + sourceStreams;
    while (streams.size() > want) {
      streams.pop_back();
    }
    while (streams.size() < want) {
      BLEAudioStream s = BLEAudioStreamAccess::create(BLE_AUDIO_STREAM_UNICAST_SERVER);
      if (!s) {
        break;
      }
      streams.push_back(s);
    }
  }

  /**
   * @brief Staged registration, run by BLEAudio::begin().
   *
   * Both directions publish the same capability record (the presets are
   * symmetric); a direction with no ASE publishes nothing. The ASCS QoS
   * preferences use the BAP low-latency defaults (RTN 2, 10 ms, 2M PHY).
   */
  BTStatus apply() {
    BLELockGuard lock(mtx.get());
    ble_audio_pac_t pac;
    bleAudioPacFromPresets(presets, maxChannels, &pac);
    if (sinkStreams) {
      bleAudioPacsAdd(BLE_AUDIO_DIR_SINK, &pac, static_cast<uint32_t>(sinkLocation), static_cast<uint16_t>(sinkContexts));
    }
    if (sourceStreams) {
      bleAudioPacsAdd(BLE_AUDIO_DIR_SOURCE, &pac, static_cast<uint32_t>(sourceLocation), static_cast<uint16_t>(sourceContexts));
    }
    const uint32_t pdMax = pdMaxUs ? pdMaxUs : audio->presentationDelayUs.load();
    const uint32_t pdMin = pdMinUs < pdMax ? pdMinUs : pdMax;
    const ble_audio_qos_pref_t pref = {
      .rtn = 2,
      .latency_ms = 10,
      .pd_min_us = pdMin,
      .pd_max_us = pdMax,
      .pref_pd_min_us = pdMin,
      .pref_pd_max_us = pdMax,
      .phy = BLE_AUDIO_PHY_2M,
      .unframed = true,
    };
    int err = bleAudioUsInit(sinkStreams, sourceStreams, &pref);
    if (err != 0) {
      log_e("UnicastServer: ASCS registration failed (sink=%u source=%u err=%d)", sinkStreams, sourceStreams, err);
      return bleAudioStatus(err);
    }
    applied = true;
    log_d("UnicastServer: registered %u sink + %u source ASEs (pd %lu-%lu us)", sinkStreams, sourceStreams, (unsigned long)pdMin, (unsigned long)pdMax);
    return BTStatus::OK;
  }
};

// --------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------

BLEAudioUnicastServer::BLEAudioUnicastServer() = default;

BLEAudioUnicastServer::operator bool() const {
  return _impl != nullptr;
}

/** The ASE counts are registered with ASCS, so they can only change before start(). */
BTStatus BLEAudioUnicastServer::setSinkStreams(uint8_t count) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("UnicastServer: setSinkStreams() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.sinkStreams = count;
  impl.resizeStreams();
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setSourceStreams(uint8_t count) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("UnicastServer: setSourceStreams() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.sourceStreams = count;
  impl.resizeStreams();
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setSupportedPresets(uint32_t presetMask) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (presetMask == 0) {
    log_e("UnicastServer: setSupportedPresets() with zero mask; keeping current mask");
    return BTStatus::InvalidParam;
  }
  if (impl.applied) {
    log_e("UnicastServer: setSupportedPresets() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.presets = presetMask;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setMaxChannelsPerStream(uint8_t channels) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (channels < 1 || channels > 2) {
    log_e("UnicastServer: invalid channels %u (want 1 or 2)", channels);
    return BTStatus::InvalidParam;
  }
  if (impl.applied) {
    log_e("UnicastServer: setMaxChannelsPerStream() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.maxChannels = channels;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setSinkContexts(BLEAudioContext contexts) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("UnicastServer: setSinkContexts() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.sinkContexts = contexts;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setSourceContexts(BLEAudioContext contexts) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("UnicastServer: setSourceContexts() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.sourceContexts = contexts;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setSinkLocation(BLEAudioLocation location) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("UnicastServer: setSinkLocation() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.sinkLocation = location;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setSourceLocation(BLEAudioLocation location) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.applied) {
    log_e("UnicastServer: setSourceLocation() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.sourceLocation = location;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::setPresentationDelayRange(uint32_t minUs, uint32_t maxUs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (maxUs != 0 && minUs > maxUs) {
    log_e("UnicastServer: invalid presentation delay range %lu-%lu us", (unsigned long)minUs, (unsigned long)maxUs);
    return BTStatus::InvalidParam;
  }
  if (impl.applied) {
    log_e("UnicastServer: setPresentationDelayRange() after audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.pdMinUs = minUs;
  impl.pdMaxUs = maxUs;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::configureMediaSink() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = setSinkStreams(1);
  if (!st) {
    return st;
  }
  st = setSourceStreams(0);
  if (!st) {
    return st;
  }
  st = setMaxChannelsPerStream(2);
  if (!st) {
    return st;
  }
  st = setSupportedPresets(kMediaPresets);
  if (!st) {
    return st;
  }
  st = setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
  if (!st) {
    return st;
  }
  st = setSinkLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight);
  if (!st) {
    return st;
  }
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastServer::configureHeadset() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = setSinkStreams(1);
  if (!st) {
    return st;
  }
  st = setSourceStreams(1);
  if (!st) {
    return st;
  }
  st = setMaxChannelsPerStream(2);
  if (!st) {
    return st;
  }
  st = setSupportedPresets(kMediaPresets);
  if (!st) {
    return st;
  }
  st = setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
  if (!st) {
    return st;
  }
  st = setSourceContexts(BLEAudioContext::Conversational | BLEAudioContext::Media);
  if (!st) {
    return st;
  }
  st = setSinkLocation(BLEAudioLocation::FrontLeft | BLEAudioLocation::FrontRight);
  if (!st) {
    return st;
  }
  st = setSourceLocation(BLEAudioLocation::Mono);
  if (!st) {
    return st;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Runtime
// --------------------------------------------------------------------------

/** Only the directions that have ASEs are updated; PACS has no record for the others. */
BTStatus BLEAudioUnicastServer::setAvailableContexts(BLEAudioContext sink, BLEAudioContext source) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (!impl.applied) {
    log_e("UnicastServer: setAvailableContexts() before audio.begin()");
    return BTStatus::InvalidState;
  }
  int err = 0;
  if (_impl->sinkStreams) {
    err = bleAudioPacsSetAvailable(BLE_AUDIO_DIR_SINK, static_cast<uint16_t>(sink));
  }
  if (err == 0 && _impl->sourceStreams) {
    err = bleAudioPacsSetAvailable(BLE_AUDIO_DIR_SOURCE, static_cast<uint16_t>(source));
  }
  if (err != 0) {
    log_e("UnicastServer: update of available contexts failed (err=%d)", err);
  }
  return bleAudioStatus(err);
}

// --------------------------------------------------------------------------
// Streams
// --------------------------------------------------------------------------

size_t BLEAudioUnicastServer::streamCount() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streams.size();
}

BLEAudioStream BLEAudioUnicastServer::stream(size_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  BLELockGuard lock(_impl->mtx.get());
  return (index < _impl->streams.size()) ? _impl->streams[index] : BLEAudioStream();
}

/** Streams have no direction until configured, so unconfigured ones never match. */
BLEAudioStream BLEAudioUnicastServer::stream(BLEAudioStream::Direction dir, uint8_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  BLELockGuard lock(_impl->mtx.get());
  for (const auto &s : _impl->streams) {
    if (s.direction() == dir && index-- == 0) {
      return s;
    }
  }
  return BLEAudioStream();
}

// --------------------------------------------------------------------------
// Factory
// --------------------------------------------------------------------------

/**
 * The role is kept alive by the controller (`roles`) and only referenced
 * weakly by its staged apply, so the controller owns its lifetime.
 */
#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED
BLEAudioUnicastServer BLEAudio::createUnicastServer() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioUnicastServer();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("UnicastServer: create before audio.begin() commits");
    return BLEAudioUnicastServer();
  }
  auto srv = std::make_shared<BLEAudioUnicastServer::Impl>();
  srv->audio = _impl;
  srv->resizeStreams();
  _impl->roles.push_back(srv);
  std::weak_ptr<BLEAudioUnicastServer::Impl> weak = srv;
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? s->apply() : BTStatus::OK;
  });
  return BLEAudioUnicastServer(srv);
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_UNICAST_SERVER_SUPPORTED
BLEAudioUnicastServer BLEAudio::createUnicastServer() {
  log_e("UnicastServer: not enabled in this build (CONFIG_BT_BAP_UNICAST_SERVER)");
  return BLEAudioUnicastServer();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
