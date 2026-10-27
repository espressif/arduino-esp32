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
#include "audio/BLEAudioUnicastServer.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioUnicastServer.cpp
 * @brief BAP Unicast Server role on top of the BAP engine unit.
 *
 * Configuration is staged in the Impl and applied once by BLEAudio::start():
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
static constexpr uint32_t kDefaultPresets =
  BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_24_2_1)
  | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_32_2_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_2_1)
  | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_4_1) | BLEAudioPresetBit(BLEAudioCodecPreset::LC3_48_6_1);

static constexpr BLEAudioContext kDefaultContexts = BLEAudioContext::Unspecified | BLEAudioContext::Conversational | BLEAudioContext::Media;

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioUnicastServer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (defaults, role list).
  uint8_t sinkStreams = 1;                ///< Sink ASEs to register.
  uint8_t sourceStreams = 1;              ///< Source ASEs to register.
  uint8_t maxChannels = 1;                ///< Channels per ASE advertised in the PAC.
  uint32_t presets = kDefaultPresets;     ///< BLEAudioPresetBit() mask for the PAC.
  BLEAudioContext sinkContexts = kDefaultContexts;
  BLEAudioContext sourceContexts = kDefaultContexts;
  BLEAudioLocation sinkLocation = BLEAudioLocation::Mono;
  BLEAudioLocation sourceLocation = BLEAudioLocation::Mono;
  uint32_t pdMinUs = 20000;               ///< Supported presentation delay range.
  uint32_t pdMaxUs = 0;                   ///< 0 = controller default.
  bool applied = false;                   ///< ASCS registered; the ASE counts are frozen.
  std::vector<BLEAudioStream> streams;    ///< One pool slot per ASE.

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
   * @brief Staged registration, run by BLEAudio::start().
   *
   * Both directions publish the same capability record (the presets are
   * symmetric); a direction with no ASE publishes nothing. The ASCS QoS
   * preferences use the BAP low-latency defaults (RTN 2, 10 ms, 2M PHY).
   */
  BTStatus apply() {
    ble_audio_pac_t pac;
    bleAudioPacFromPresets(presets, maxChannels, &pac);
    if (sinkStreams) {
      bleAudioPacsAdd(BLE_AUDIO_DIR_SINK, &pac, static_cast<uint32_t>(sinkLocation), static_cast<uint16_t>(sinkContexts));
    }
    if (sourceStreams) {
      bleAudioPacsAdd(BLE_AUDIO_DIR_SOURCE, &pac, static_cast<uint32_t>(sourceLocation), static_cast<uint16_t>(sourceContexts));
    }
    const uint32_t pdMax = pdMaxUs ? pdMaxUs : audio->presentationDelayUs;
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
    log_d("UnicastServer: registered %u sink + %u source ASEs (pd %lu-%lu us)", sinkStreams, sourceStreams, (unsigned long)pdMin,
          (unsigned long)pdMax);
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

/** The ASE count is registered with ASCS, so it can only change before start(). */
BLEAudioUnicastServer &BLEAudioUnicastServer::setSinkStreams(uint8_t count) {
  if (_impl && !_impl->applied) {
    _impl->sinkStreams = count;
    _impl->resizeStreams();
  } else if (_impl) {
    log_w("UnicastServer: setSinkStreams() ignored after audio.start()");
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setSourceStreams(uint8_t count) {
  if (_impl && !_impl->applied) {
    _impl->sourceStreams = count;
    _impl->resizeStreams();
  } else if (_impl) {
    log_w("UnicastServer: setSourceStreams() ignored after audio.start()");
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setSupportedPresets(uint32_t presetMask) {
  if (_impl && presetMask) {
    _impl->presets = presetMask;
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setMaxChannelsPerStream(uint8_t channels) {
  if (_impl) {
    _impl->maxChannels = channels < 1 ? 1 : (channels > 2 ? 2 : channels);
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setSinkContexts(BLEAudioContext contexts) {
  if (_impl) {
    _impl->sinkContexts = contexts;
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setSourceContexts(BLEAudioContext contexts) {
  if (_impl) {
    _impl->sourceContexts = contexts;
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setSinkLocation(BLEAudioLocation location) {
  if (_impl) {
    _impl->sinkLocation = location;
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setSourceLocation(BLEAudioLocation location) {
  if (_impl) {
    _impl->sourceLocation = location;
  }
  return *this;
}

BLEAudioUnicastServer &BLEAudioUnicastServer::setPresentationDelayRange(uint32_t minUs, uint32_t maxUs) {
  if (_impl) {
    _impl->pdMinUs = minUs;
    _impl->pdMaxUs = maxUs;
  }
  return *this;
}

// --------------------------------------------------------------------------
// Runtime
// --------------------------------------------------------------------------

/** Only the directions that have ASEs are updated; PACS has no record for the others. */
BTStatus BLEAudioUnicastServer::setAvailableContexts(BLEAudioContext sink, BLEAudioContext source) {
  if (!_impl || !_impl->applied) {
    log_e("UnicastServer: setAvailableContexts() before audio.start()");
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
  return _impl ? _impl->streams.size() : 0;
}

BLEAudioStream BLEAudioUnicastServer::stream(size_t index) const {
  return (_impl && index < _impl->streams.size()) ? _impl->streams[index] : BLEAudioStream();
}

/** Streams have no direction until configured, so unconfigured ones never match. */
BLEAudioStream BLEAudioUnicastServer::stream(BLEAudioStream::Direction dir, uint8_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
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
BLEAudioUnicastServer BLEAudio::createUnicastServer() {
#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("UnicastServer: create between audio.begin() and audio.start()");
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
#else
  log_e("UnicastServer: not enabled in this build (CONFIG_BT_BAP_UNICAST_SERVER)");
  return BLEAudioUnicastServer();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
