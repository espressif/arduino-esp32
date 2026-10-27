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

#include "audio/BLEAudioProfiles.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "audio/BLEAudioEngineProfiles.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioProfiles.cpp
 * @brief TMAP / GMAP role handles and Public Broadcast Profile helpers.
 *
 * TMAS and GMAS are GATT services, so the local roles are registered by a
 * deferred init inside BLEAudio::start() (part of the single GATT commit).
 * A handle created after start() can still discover peers, but its local
 * roles can no longer be registered; setRoles()/setFeatures() then warn.
 *
 * API contract is documented on the declarations in `BLEAudioProfiles.h`; the
 * definitions below carry implementation notes only.
 */

namespace {

/** Log and return true when @p what is called after start (the GATT database is committed). */
bool startedWarn(const std::weak_ptr<BLEAudio::Impl> &audio, const char *what) {
  auto a = audio.lock();
  if (a && a->started) {
    log_w("%s: ignored after audio.start() (the service is registered at start)", what);
    return true;
  }
  return false;
}

}  // namespace

// --------------------------------------------------------------------------
// TMAP
// --------------------------------------------------------------------------

struct BLEAudioTmap::Impl {
  std::weak_ptr<BLEAudio::Impl> audio;
  BLEAudioTmapRole roles = BLEAudioTmapRole::None;  ///< Local roles; None registers no TMAS.
  DiscoverCallback discoverCb;

  /** Engine event for this group: a peer's TMAP roles were read. */
  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_TMAP_DISCOVERED || !discoverCb) {
      return;
    }
    auto *d = static_cast<const ble_audio_tmap_discovered_t *>(e.data);
    discoverCb(e.conn_handle, bleAudioStatus(e.err), static_cast<BLEAudioTmapRole>(d ? d->roles : 0));
  }
};

BLEAudioTmap::BLEAudioTmap() = default;

BLEAudioTmap::operator bool() const {
  return _impl != nullptr;
}

BLEAudioTmap &BLEAudioTmap::setRoles(BLEAudioTmapRole roles) {
  if (_impl && !startedWarn(_impl->audio, "BLEAudioTmap::setRoles")) {
    _impl->roles = roles;
  }
  return *this;
}

BLEAudioTmapRole BLEAudioTmap::getRoles() const {
  return _impl ? _impl->roles : BLEAudioTmapRole::None;
}

BTStatus BLEAudioTmap::discover(uint16_t connHandle) {
  return _impl ? bleAudioStatus(bleAudioTmapDiscover(connHandle)) : BTStatus::InvalidState;
}

BLEAudioTmap &BLEAudioTmap::onDiscovered(DiscoverCallback cb) {
  if (_impl) {
    _impl->discoverCb = std::move(cb);
  }
  return *this;
}

void BLEAudioTmap::resetCallbacks() {
  if (_impl) {
    _impl->discoverCb = nullptr;
  }
}

BLEAudioTmap BLEAudio::createTmap() {
#if BLE_AUDIO_TMAP_SUPPORTED
  if (!_impl || !_impl->active) {
    log_e("createTmap(): call after audio.begin()");
    return BLEAudioTmap();
  }
  auto tmap = std::make_shared<BLEAudioTmap::Impl>();
  tmap->audio = _impl;
  _impl->roles.push_back(tmap);
  /* `roles` owns the handle until end(); captures stay weak. */
  std::weak_ptr<BLEAudioTmap::Impl> weak = tmap;
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto t = weak.lock();
    if (!t || t->roles == BLEAudioTmapRole::None) {
      return BTStatus::OK;
    }
    return bleAudioStatus(bleAudioTmapRegister(static_cast<uint16_t>(t->roles)));
  });
  _impl->setHandler(BLE_AUDIO_GRP_TMAP, [weak](const ble_audio_evt_t &e) {
    if (auto t = weak.lock()) {
      t->handle(e);
    }
  });
  return BLEAudioTmap(tmap);
#else
  log_e("TMAP is not enabled in this build (CONFIG_BT_TMAP)");
  return BLEAudioTmap();
#endif
}

// --------------------------------------------------------------------------
// GMAP
// --------------------------------------------------------------------------

struct BLEAudioGmap::Impl {
  std::weak_ptr<BLEAudio::Impl> audio;
  BLEAudioGmapRole roles = BLEAudioGmapRole::None;  ///< Local roles; None registers no GMAS.
  BLEAudioGmapFeatures features;                    ///< Local features of those roles.
  DiscoverCallback discoverCb;

  /** Engine event for this group: a peer's GMAP roles and features were read. */
  void handle(const ble_audio_evt_t &e) {
    if (e.type != BLE_AUDIO_EVT_GMAP_DISCOVERED || !discoverCb) {
      return;
    }
    auto *d = static_cast<const ble_audio_gmap_discovered_t *>(e.data);
    BLEAudioGmapFeatures f;
    uint8_t r = 0;
    if (d) {
      r = d->roles;
      f.unicastGateway = d->features.ugg;
      f.unicastTerminal = d->features.ugt;
      f.broadcastSender = d->features.bgs;
      f.broadcastReceiver = d->features.bgr;
    }
    discoverCb(e.conn_handle, bleAudioStatus(e.err), static_cast<BLEAudioGmapRole>(r), f);
  }
};

BLEAudioGmap::BLEAudioGmap() = default;

BLEAudioGmap::operator bool() const {
  return _impl != nullptr;
}

BLEAudioGmap &BLEAudioGmap::setRoles(BLEAudioGmapRole roles) {
  if (_impl && !startedWarn(_impl->audio, "BLEAudioGmap::setRoles")) {
    _impl->roles = roles;
  }
  return *this;
}

BLEAudioGmap &BLEAudioGmap::setFeatures(const BLEAudioGmapFeatures &features) {
  if (_impl && !startedWarn(_impl->audio, "BLEAudioGmap::setFeatures")) {
    _impl->features = features;
  }
  return *this;
}

BLEAudioGmap &BLEAudioGmap::setFeatures(uint8_t unicastGateway, uint8_t unicastTerminal, uint8_t broadcastSender, uint8_t broadcastReceiver) {
  BLEAudioGmapFeatures f;
  f.unicastGateway = unicastGateway;
  f.unicastTerminal = unicastTerminal;
  f.broadcastSender = broadcastSender;
  f.broadcastReceiver = broadcastReceiver;
  return setFeatures(f);
}

BLEAudioGmapRole BLEAudioGmap::getRoles() const {
  return _impl ? _impl->roles : BLEAudioGmapRole::None;
}

BTStatus BLEAudioGmap::discover(uint16_t connHandle) {
  return _impl ? bleAudioStatus(bleAudioGmapDiscover(connHandle)) : BTStatus::InvalidState;
}

BLEAudioGmap &BLEAudioGmap::onDiscovered(DiscoverCallback cb) {
  if (_impl) {
    _impl->discoverCb = std::move(cb);
  }
  return *this;
}

void BLEAudioGmap::resetCallbacks() {
  if (_impl) {
    _impl->discoverCb = nullptr;
  }
}

BLEAudioGmap BLEAudio::createGmap() {
#if BLE_AUDIO_GMAP_SUPPORTED
  if (!_impl || !_impl->active) {
    log_e("createGmap(): call after audio.begin()");
    return BLEAudioGmap();
  }
  auto gmap = std::make_shared<BLEAudioGmap::Impl>();
  gmap->audio = _impl;
  _impl->roles.push_back(gmap);
  /* `roles` owns the handle until end(); captures stay weak. */
  std::weak_ptr<BLEAudioGmap::Impl> weak = gmap;
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto g = weak.lock();
    if (!g || g->roles == BLEAudioGmapRole::None) {
      return BTStatus::OK;
    }
    const ble_audio_gmap_features_t f = {
      g->features.unicastGateway, g->features.unicastTerminal, g->features.broadcastSender, g->features.broadcastReceiver,
    };
    return bleAudioStatus(bleAudioGmapRegister(static_cast<uint8_t>(g->roles), &f));
  });
  _impl->setHandler(BLE_AUDIO_GRP_GMAP, [weak](const ble_audio_evt_t &e) {
    if (auto g = weak.lock()) {
      g->handle(e);
    }
  });
  return BLEAudioGmap(gmap);
#else
  log_e("GMAP is not enabled in this build (CONFIG_BT_GMAP)");
  return BLEAudioGmap();
#endif
}

// --------------------------------------------------------------------------
// Public Broadcast Profile
// --------------------------------------------------------------------------

size_t BLEAudioPublicBroadcast::buildAnnouncement(uint8_t features, const uint8_t *metadata, size_t metadataLen, uint8_t *out, size_t outCap) {
  size_t len = 0;
  int err = bleAudioPbpBuild(features, metadata, metadataLen, out, outCap, &len);
  if (err != 0) {
    log_e("PublicBroadcast: building the announcement (%u metadata octets, %u-octet buffer) failed (err=%d)", (unsigned)metadataLen, (unsigned)outCap, err);
    return 0;
  }
  return len;
}

bool BLEAudioPublicBroadcast::parseAnnouncement(
  const uint8_t *data, size_t dataLen, uint8_t &featuresOut, const uint8_t *&metadataOut, size_t &metadataLenOut
) {
  return bleAudioPbpParse(data, dataLen, &featuresOut, &metadataOut, &metadataLenOut) == 0;
}

#endif /* BLE_AUDIO_SUPPORTED */
