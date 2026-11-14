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
#include "BLEAudioProfiles.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioControlLink.h"
#include "engine/BLEAudioStreamInternal.h"
#include "engine/BLEAudioEngineProfiles.h"
#include "esp32-hal-log.h"

namespace {

bool startedWarn(const std::shared_ptr<BLEAudio::Impl> &audio, const char *what) {
  if (audio && audio->started) {
    log_w("%s: ignored after audio.start() (the service is registered at start)", what);
    return true;
  }
  return false;
}

struct DeferredProfileDiscovery {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  std::vector<uint16_t> pending;

  BTStatus queueOrRun(uint16_t connHandle, const char *name, int (*discoverFn)(uint16_t)) {
    if (!audio->started.load() || connHandle == BLE_AUDIO_CONN_NONE) {
      log_e("%s: discover() before audio.start()", name);
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

// --------------------------------------------------------------------------
// TMAP
// --------------------------------------------------------------------------

struct BLEAudioTmap::Impl : DeferredProfileDiscovery {
  BLEAudioTmapRole roles = BLEAudioTmapRole::None;
  DiscoverCallback onDiscoveredCb;

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

BLEAudioTmap &BLEAudioTmap::setRoles(BLEAudioTmapRole roles) {
  BLE_CHECK_IMPL(*this);
  if (!startedWarn(impl.audio, "BLEAudioTmap::setRoles")) {
    BLELockGuard lock(impl.mtx.get());
    impl.roles = roles;
  }
  return *this;
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

BLEAudioTmap &BLEAudioTmap::onDiscovered(DiscoverCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
  return *this;
}

void BLEAudioTmap::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
}

BLEAudioTmap BLEAudio::createTmap() {
#if BLE_AUDIO_TMAP_SUPPORTED
  return BLEAudioImplCommon::createStagedRole<BLEAudioTmap>(
    _impl, "createTmap", BLE_AUDIO_GRP_TMAP, [](BLEAudioTmap::Impl &t) {
      BLEAudioTmapRole localRoles;
      {
        BLELockGuard lock(t.mtx.get());
        localRoles = t.roles;
      }
      return localRoles == BLEAudioTmapRole::None ? BTStatus::OK : bleAudioStatus(bleAudioTmapRegister(static_cast<uint16_t>(localRoles)));
    }
  );
#else
  log_e("TMAP is not enabled in this build (CONFIG_BT_TMAP)");
  return BLEAudioTmap();
#endif
}

// --------------------------------------------------------------------------
// GMAP
// --------------------------------------------------------------------------

struct BLEAudioGmap::Impl : DeferredProfileDiscovery {
  BLEAudioGmapRole roles = BLEAudioGmapRole::None;
  BLEAudioGmapFeatures features;
  DiscoverCallback onDiscoveredCb;

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

BLEAudioGmap &BLEAudioGmap::setRoles(BLEAudioGmapRole roles) {
  BLE_CHECK_IMPL(*this);
  if (!startedWarn(impl.audio, "BLEAudioGmap::setRoles")) {
    BLELockGuard lock(impl.mtx.get());
    impl.roles = roles;
  }
  return *this;
}

BLEAudioGmap &BLEAudioGmap::setFeatures(const BLEAudioGmapFeatures &features) {
  BLE_CHECK_IMPL(*this);
  if (!startedWarn(impl.audio, "BLEAudioGmap::setFeatures")) {
    BLELockGuard lock(impl.mtx.get());
    impl.features = features;
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
  BLE_CHECK_IMPL(BLEAudioGmapRole::None);
  BLELockGuard lock(impl.mtx.get());
  return impl.roles;
}

BTStatus BLEAudioGmap::discover(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.queueOrRun(connHandle, "GMAP", bleAudioGmapDiscover);
}

BLEAudioGmap &BLEAudioGmap::onDiscovered(DiscoverCallback cb) {
  BLE_CHECK_IMPL(*this);
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
  return *this;
}

void BLEAudioGmap::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
}

BLEAudioGmap BLEAudio::createGmap() {
#if BLE_AUDIO_GMAP_SUPPORTED
  return BLEAudioImplCommon::createStagedRole<BLEAudioGmap>(
    _impl, "createGmap", BLE_AUDIO_GRP_GMAP, [](BLEAudioGmap::Impl &g) -> BTStatus {
      BLEAudioGmapRole localRoles;
      BLEAudioGmapFeatures localFeatures;
      {
        BLELockGuard lock(g.mtx.get());
        localRoles = g.roles;
        localFeatures = g.features;
      }
      if (localRoles == BLEAudioGmapRole::None) {
        return BTStatus::OK;
      }
      const ble_audio_gmap_features_t f = {
        localFeatures.unicastGateway, localFeatures.unicastTerminal, localFeatures.broadcastSender, localFeatures.broadcastReceiver,
      };
      return bleAudioStatus(bleAudioGmapRegister(static_cast<uint8_t>(localRoles), &f));
    }
  );
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
