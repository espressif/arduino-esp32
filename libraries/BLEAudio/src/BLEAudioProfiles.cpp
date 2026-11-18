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

/**
 * @file BLEAudioProfiles.cpp
 * @brief TMAP, GMAP, and Public Broadcast Profile (`BLEAudioPublicBroadcast`)
 *        public handle implementations and feature-disabled factory stubs.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <vector>
#include "BLEAudioProfiles.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioControlLink.h"
#include "BLEAudioStreamImpl.h"
#include "engine/BLEAudioEngineProfiles.h"
#include "esp32-hal-log.h"

namespace {

bool startedWarn(const std::shared_ptr<BLEAudio::Impl> &audio, const char *what) {
  if (audio && audio->started) {
    log_w("%s: ignored after audio.begin() (the service registers at commit)", what);
    return true;
  }
  return false;
}

struct DeferredProfileDiscovery {
  // Multi-peer variant of BLEAudioControlLink's defer-until-GATT-ready pattern
  // (ControlLink is single-link and cannot serve TMAP/GMAP's pending vector).
  // The engine call runs unlocked, like ControlLink::discover, so Bluedroid's
  // inline discovery callbacks cannot deadlock against this mutex.
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  std::vector<uint16_t> pending;

  BTStatus queueOrRun(uint16_t connHandle, const char *name, int (*discoverFn)(uint16_t)) {
    if (!audio->started.load() || connHandle == BLE_AUDIO_CONN_NONE) {
      log_e("%s: discover() before audio.begin()", name);
      return BTStatus::InvalidState;
    }
    {
      BLELockGuard lock(mtx.get());
      if (!audio->isGattReady(connHandle)) {
        for (uint16_t c : pending) {
          if (c == connHandle) {
            return BTStatus::OK;
          }
        }
        pending.push_back(connHandle);
        return BTStatus::OK;
      }
    }
    return bleAudioStatus(discoverFn(connHandle));
  }

  int handleCore(const ble_audio_evt_t &e, int (*discoverFn)(uint16_t)) {
    bool doStart = false;
    {
      BLELockGuard lock(mtx.get());
      for (auto it = pending.begin(); it != pending.end(); ++it) {
        if (*it == e.conn_handle) {
          pending.erase(it);
          doStart = (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED);
          break;
        }
      }
    }
    if (!doStart) {
      return 0;
    }
    return e.err ? e.err : discoverFn(e.conn_handle);
  }
};

}  // namespace

// ==========================================================================
// 1. Telephony and Media Audio Profile (TMAP)
// ==========================================================================

struct BLEAudioTmap::Impl : DeferredProfileDiscovery {
  BLEAudioTmapRole roles = BLEAudioTmapRole::None;
  DiscoverCallback onDiscoveredCb;

  BTStatus apply() {
    BLEAudioTmapRole localRoles;
    {
      BLELockGuard lock(mtx.get());
      localRoles = roles;
    }
    return localRoles == BLEAudioTmapRole::None ? BTStatus::OK : bleAudioStatus(bleAudioTmapRegister(static_cast<uint16_t>(localRoles)));
  }

  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED || e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED) {
      if (int err = handleCore(e, bleAudioTmapDiscover)) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(e.conn_handle, bleAudioStatus(err), BLEAudioTmapRole::None);
        }
      }
      return;
    }
    if (e.type != BLE_AUDIO_EVT_TMAP_DISCOVERED) {
      return;
    }
    if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
      const auto *d = static_cast<const ble_audio_tmap_discovered_t *>(e.data);
      cb(e.conn_handle, bleAudioStatus(e.err), static_cast<BLEAudioTmapRole>(d ? d->roles : 0));
    }
  }
};

BLEAudioTmap::BLEAudioTmap() = default;

BLEAudioTmap::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioTmap::setRoles(BLEAudioTmapRole roles) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (startedWarn(impl.audio, "BLEAudioTmap::setRoles")) {
    return BTStatus::InvalidState;
  }
  if (static_cast<uint16_t>(roles) & ~0x003Fu) {
    log_e("BLEAudioTmap::setRoles(): unknown role bits 0x%04x (valid: 0x0001-0x0020)", static_cast<unsigned>(roles) & ~0x3Fu);
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.roles = roles;
  return BTStatus::OK;
}

BLEAudioTmapRole BLEAudioTmap::getRoles() const {
  BLE_CHECK_IMPL(BLEAudioTmapRole::None);
  BLELockGuard lock(impl.mtx.get());
  return impl.roles;
}

BTStatus BLEAudioTmap::discover(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.queueOrRun(connHandle, "TMAP", bleAudioTmapDiscover);
}

void BLEAudioTmap::onDiscovered(DiscoverCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
}

void BLEAudioTmap::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
}

#if BLE_AUDIO_TMAP_SUPPORTED
BLEAudioTmap BLEAudio::createTmap() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioTmap();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioTmap>(_impl, "createTmap", BLE_AUDIO_GRP_TMAP, [](BLEAudioTmap::Impl &t) {
    return t.apply();
  });
}
#endif

// ==========================================================================
// 2. Gaming Audio Profile (GMAP)
// ==========================================================================

struct BLEAudioGmap::Impl : DeferredProfileDiscovery {
  BLEAudioGmapRole roles = BLEAudioGmapRole::None;
  BLEAudioGmapFeatures features;
  DiscoverCallback onDiscoveredCb;

  BTStatus apply() {
    BLEAudioGmapRole localRoles;
    BLEAudioGmapFeatures localFeatures;
    {
      BLELockGuard lock(mtx.get());
      localRoles = roles;
      localFeatures = features;
    }
    if (localRoles == BLEAudioGmapRole::None) {
      return BTStatus::OK;
    }
    if ((localRoles & BLEAudioGmapRole::UnicastGameTerminal)
        && !(localFeatures.unicastTerminal & (BLEAudioGmapFeatures::UgtSource | BLEAudioGmapFeatures::UgtSink))) {
      log_e("GMAP: UGT role needs UgtSource and/or UgtSink in unicastTerminal");
      return BTStatus::InvalidParam;
    }
    const ble_audio_gmap_features_t f = {
      localFeatures.unicastGateway,
      localFeatures.unicastTerminal,
      localFeatures.broadcastSender,
      localFeatures.broadcastReceiver,
    };
    return bleAudioStatus(bleAudioGmapRegister(static_cast<uint8_t>(localRoles), &f));
  }

  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED || e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED) {
      if (int err = handleCore(e, bleAudioGmapDiscover)) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(e.conn_handle, bleAudioStatus(err), BLEAudioGmapRole::None, BLEAudioGmapFeatures{});
        }
      }
      return;
    }
    if (e.type != BLE_AUDIO_EVT_GMAP_DISCOVERED) {
      return;
    }
    if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
      const auto *d = static_cast<const ble_audio_gmap_discovered_t *>(e.data);
      BLEAudioGmapFeatures f;
      uint8_t r = 0;
      if (d) {
        r = d->roles;
        f.unicastGateway = d->features.ugg;
        f.unicastTerminal = d->features.ugt;
        f.broadcastSender = d->features.bgs;
        f.broadcastReceiver = d->features.bgr;
      }
      cb(e.conn_handle, bleAudioStatus(e.err), static_cast<BLEAudioGmapRole>(r), f);
    }
  }
};

BLEAudioGmap::BLEAudioGmap() = default;

BLEAudioGmap::operator bool() const {
  return _impl != nullptr;
}

namespace {

// Roles and features are set in either order (typically chained as
// setRoles(...).setFeatures(...)), so each setter only validates its own
// domain here; the role↔feature combination is checked in apply(), where a
// mismatch fails audio.begin() loudly instead of publishing a broken GMAS.
bool gmapRolesOk(BLEAudioGmapRole roles, const char *what) {
  if (static_cast<uint8_t>(roles) & ~0x0Fu) {
    log_e("%s: unknown role bits 0x%02x (valid: UGG/UGT/BGS/BGR)", what, static_cast<unsigned>(roles) & ~0x0Fu);
    return false;
  }
  return true;
}

bool gmapFeaturesOk(const BLEAudioGmapFeatures &f, const char *what) {
  if ((f.unicastGateway & ~0x07u) || (f.unicastTerminal & ~0x7Fu) || (f.broadcastSender & ~0x01u) || (f.broadcastReceiver & ~0x03u)) {
    log_e("%s: feature bits outside the known masks (UGG<=0x07 UGT<=0x7F BGS<=0x01 BGR<=0x03)", what);
    return false;
  }
  return true;
}

}  // namespace

BTStatus BLEAudioGmap::setRoles(BLEAudioGmapRole roles) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (startedWarn(impl.audio, "BLEAudioGmap::setRoles")) {
    return BTStatus::InvalidState;
  }
  if (!gmapRolesOk(roles, "BLEAudioGmap::setRoles")) {
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.roles = roles;
  return BTStatus::OK;
}

BTStatus BLEAudioGmap::setFeatures(const BLEAudioGmapFeatures &features) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (startedWarn(impl.audio, "BLEAudioGmap::setFeatures")) {
    return BTStatus::InvalidState;
  }
  if (!gmapFeaturesOk(features, "BLEAudioGmap::setFeatures")) {
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.features = features;
  return BTStatus::OK;
}

BTStatus BLEAudioGmap::setFeatures(uint8_t unicastGateway, uint8_t unicastTerminal, uint8_t broadcastSender, uint8_t broadcastReceiver) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAudioGmapFeatures f;
  f.unicastGateway = unicastGateway;
  f.unicastTerminal = unicastTerminal;
  f.broadcastSender = broadcastSender;
  f.broadcastReceiver = broadcastReceiver;
  return setFeatures(f);
}

BLEAudioGmapRole BLEAudioGmap::getRoles() const {
  BLE_CHECK_IMPL(BLEAudioGmapRole::None);
  BLELockGuard lock(impl.mtx.get());
  return impl.roles;
}

BTStatus BLEAudioGmap::discover(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.queueOrRun(connHandle, "GMAP", bleAudioGmapDiscover);
}

void BLEAudioGmap::onDiscovered(DiscoverCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
}

void BLEAudioGmap::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
}

#if BLE_AUDIO_GMAP_SUPPORTED
BLEAudioGmap BLEAudio::createGmap() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioGmap();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioGmap>(_impl, "createGmap", BLE_AUDIO_GRP_GMAP, [](BLEAudioGmap::Impl &g) {
    return g.apply();
  });
}
#endif

// ==========================================================================
// 3. Public Broadcast Profile (PBP)
// ==========================================================================

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

// ==========================================================================
// 4. Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_TMAP_SUPPORTED
BLEAudioTmap BLEAudio::createTmap() {
  log_e("TMAP is not enabled in this build (CONFIG_BT_TMAP)");
  return BLEAudioTmap();
}
#endif

#if !BLE_AUDIO_GMAP_SUPPORTED
BLEAudioGmap BLEAudio::createGmap() {
  log_e("GMAP is not enabled in this build (CONFIG_BT_GMAP)");
  return BLEAudioGmap();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
