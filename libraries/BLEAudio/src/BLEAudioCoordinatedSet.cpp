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

#include <string.h>
#include <vector>
#include "BLEAudioCoordinatedSet.h"
#include "BLEAudioCap.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioStreamImpl.h"
#include "engine/BLEAudioEngineCap.h"
#include "esp32-hal-log.h"
#include "esp_random.h"

/**
 * @file BLEAudioCoordinatedSet.cpp
 * @brief CSIP Set Member and Set Coordinator roles on the CAP engine unit.
 *
 * The member Impl also backs `BLEAudioCapAcceptor`: the acceptor is a CSIS
 * instance included by CAS, registered through the CAP acceptor API. Its
 * events arrive in the CAP_ACCEPTOR group instead of CSIP_MEMBER, so the
 * factory for both lives here.
 */

// --------------------------------------------------------------------------
// Member
// --------------------------------------------------------------------------

struct BLEAudioCoordinatedSetMember::Impl {
  bool cas = false;         ///< CAS-included instance (CAP acceptor).
  bool registered = false;  ///< Registered by audio.begin().
  /** Random per creation: a fixed demo SIRK would let unrelated units that kept
   * the default resolve and join each other. Pair devices with setSirk() using
   * one shared value (persist it for stable pairing across boots). */
  uint8_t sirk[BLE_AUDIO_SIRK_SIZE];
  bool sirkCustom = false;  ///< setSirk() called; silences the random-SIRK warning.

  Impl() {
    for (uint8_t i = 0; i < BLE_AUDIO_SIRK_SIZE; i += 4) {
      const uint32_t r = esp_random();
      memcpy(&sirk[i], &r, 4);
    }
  }
  uint8_t setSize = 2;
  uint8_t rank = 1;
  bool lockable = true;
  String name;         ///< Optional CSIS name (empty = none).
  BLESharedMutex mtx;  ///< Protects this role's callbacks and shared state.
  LockCallback lockCb;
  SirkReadCallback sirkCb;

  /** Register the CSIS instance with the staged values; runs inside audio.begin(). */
  BTStatus apply() {
    if (!sirkCustom) {
      log_w("CoordinatedSet: advertising with a random SIRK; pair devices with setSirk() sharing one value");
    }
    ble_audio_csis_cfg_t cfg = {};
    memcpy(cfg.sirk, sirk, sizeof(cfg.sirk));
    cfg.set_size = setSize;
    cfg.rank = rank;
    cfg.lockable = lockable;
    cfg.name = reinterpret_cast<const uint8_t *>(name.c_str());
    cfg.name_len = (uint8_t)name.length();
    int err = bleAudioCsisRegister(cas, &cfg);
    if (err != 0) {
      log_e("CoordinatedSet: registration failed (size=%u rank=%u err=%d)", setSize, rank, err);
    }
    registered = (err == 0);
    return bleAudioStatus(err);
  }

  /** @brief Log a failed live update; @return the stack status. */
  BTStatus checkLive(int err, const char *what) {
    if (err != 0) {
      log_w("CoordinatedSet: %s update failed (err=%d)", what, err);
    }
    return bleAudioStatus(err);
  }

  /**
   * CSIS event in the CSIP_MEMBER or CAP_ACCEPTOR group. The two groups use
   * the same low-byte event codes, so the switch ignores the group byte.
   * The SIRK request is answered synchronously through its reply pointer.
   */
  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE || !e.data) {
      return;
    }
    switch (e.type & 0xFF) {
      case BLE_AUDIO_CSIS_EVT_LOCK:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::lockCb)) {
          cb(static_cast<const ble_audio_csis_lock_t *>(e.data)->locked, e.conn_handle);
        }
        break;
      case BLE_AUDIO_CSIS_EVT_SIRK_REQ:
      {
        auto cb = bleAudioCopyCallback(*this, &Impl::sirkCb);
        if (cb) {
          *static_cast<const ble_audio_csis_sirk_req_t *>(e.data)->reply = static_cast<uint8_t>(cb(e.conn_handle));
        }
        break;
      }
      default: break;
    }
  }
};

#if BLE_AUDIO_CSIP_MEMBER_SUPPORTED || BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
namespace {

/** @brief Create a member Impl, route its event group and stage its registration. */
std::shared_ptr<BLEAudioCoordinatedSetMember::Impl> makeMember(const std::shared_ptr<BLEAudio::Impl> &audio, bool cas) {
  auto m = std::make_shared<BLEAudioCoordinatedSetMember::Impl>();
  m->cas = cas;
  audio->roles.push_back(m);
  /* `roles` owns the member until end(); captures stay weak. */
  std::weak_ptr<BLEAudioCoordinatedSetMember::Impl> weak = m;
  audio->setHandler(cas ? BLE_AUDIO_GRP_CAP_ACCEPTOR : BLE_AUDIO_GRP_CSIP_MEMBER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  audio->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? s->apply() : BTStatus::OK;
  });
  return m;
}

}  // namespace
#endif

BLEAudioCoordinatedSetMember::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioCoordinatedSetMember::setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!sirk) {
    log_e("CoordinatedSet: setSirk() needs a 16-octet SIRK");
    return BTStatus::InvalidParam;
  }
  memcpy(impl.sirk, sirk, BLE_AUDIO_SIRK_SIZE);
  impl.sirkCustom = true;
  if (impl.registered) {
    return impl.checkLive(bleAudioCsisSetSirk(impl.cas, impl.sirk), "SIRK");
  }
  return BTStatus::OK;
}

// Size and rank are registered together at audio.begin().
BTStatus BLEAudioCoordinatedSetMember::setSetSize(uint8_t size) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (size == 0) {
    log_e("CoordinatedSet: invalid set size 0");
    return BTStatus::InvalidParam;
  }
  impl.setSize = size;
  if (impl.registered) {
    return impl.checkLive(bleAudioCsisSetSizeRank(impl.cas, impl.setSize, impl.rank), "size");
  }
  return BTStatus::OK;
}

BTStatus BLEAudioCoordinatedSetMember::setRank(uint8_t rank) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (rank == 0) {
    log_e("CoordinatedSet: invalid rank 0");
    return BTStatus::InvalidParam;
  }
  impl.rank = rank;
  if (impl.registered) {
    return impl.checkLive(bleAudioCsisSetSizeRank(impl.cas, impl.setSize, impl.rank), "rank");
  }
  return BTStatus::OK;
}

BTStatus BLEAudioCoordinatedSetMember::setLockable(bool lockable) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.registered) {
    log_e("CoordinatedSet: setLockable() before audio.begin()");
    return BTStatus::InvalidState;
  }
  impl.lockable = lockable;
  return BTStatus::OK;
}

BTStatus BLEAudioCoordinatedSetMember::setName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (name == impl.name) {
    return BTStatus::OK;
  }
  impl.name = name;
  if (impl.registered) {
    return impl.checkLive(bleAudioCsisSetName(impl.cas, reinterpret_cast<const uint8_t *>(name.c_str()), (uint8_t)name.length()), "name");
  }
  return BTStatus::OK;
}

BTStatus BLEAudioCoordinatedSetMember::generateRsi(uint8_t rsi[BLE_AUDIO_RSI_SIZE]) const {
  if (!_impl || !_impl->registered || !rsi) {
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioCsisGenerateRsi(_impl->cas, rsi));
}

BTStatus BLEAudioCoordinatedSetMember::lock() {
  return (_impl && _impl->registered) ? bleAudioStatus(bleAudioCsisLock(_impl->cas, true, false)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCoordinatedSetMember::unlock(bool force) {
  return (_impl && _impl->registered) ? bleAudioStatus(bleAudioCsisLock(_impl->cas, false, force)) : BTStatus::InvalidState;
}

bool BLEAudioCoordinatedSetMember::isLocked() const {
  return _impl && _impl->registered && bleAudioCsisIsLocked(_impl->cas);
}

void BLEAudioCoordinatedSetMember::onLockChanged(LockCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.lockCb = std::move(cb);
}

void BLEAudioCoordinatedSetMember::onSirkRead(SirkReadCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.sirkCb = std::move(cb);
}

void BLEAudioCoordinatedSetMember::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.lockCb = nullptr;
  impl.sirkCb = nullptr;
}

// --------------------------------------------------------------------------
// Coordinator
// --------------------------------------------------------------------------

struct BLEAudioCoordinatedSetCoordinator::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;              ///< Protects this role's callbacks and shared state.
  std::vector<uint16_t> pending;   ///< discover() called before the link's GATT discovery finished.
  BLEAudioCoordinatedSetInfo set;  ///< First discovered set (setCount 0 until then).
  DiscoveredCallback discoveredCb;
  LockCallback lockCb;
  LockChangedCallback lockChangedCb;

  /** Start CSIS discovery on @p conn. */
  BTStatus start(uint16_t conn) {
    int err = bleAudioCsipCoordDiscover(conn);
    if (err != 0) {
      log_e("CoordinatedSet: discovery of conn %u failed to start (err=%d)", conn, err);
    }
    return bleAudioStatus(err);
  }

  /**
   * Engine event: link readiness releases pending discoveries (a disconnect
   * drops them); the first discovered set becomes the one lock()/unlock()
   * and isSetMember() work on.
   */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_GATT_DISCOVERED:
      case BLE_AUDIO_EVT_ACL_DISCONNECTED:
      {
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
        if (doStart) {
          BTStatus st = e.err ? bleAudioStatus(e.err) : start(e.conn_handle);
          if (!st) {
            BLEAudioCoordinatedSetInfo info;
            info.connHandle = e.conn_handle;
            if (auto cb = bleAudioCopyCallback(*this, &Impl::discoveredCb)) {
              cb(st, info);
            }
          }
        }
        break;
      }
      case BLE_AUDIO_EVT_CSIP_DISCOVERED:
      {
        BLEAudioCoordinatedSetInfo info;
        info.connHandle = e.conn_handle;
        if (e.err == 0 && e.data) {
          const auto *s = static_cast<const ble_audio_csip_set_t *>(e.data);
          info.setCount = s->set_count;
          info.setSize = s->set_size;
          info.rank = s->rank;
          info.lockable = s->lockable;
          memcpy(info.sirk, s->sirk, sizeof(info.sirk));
          BLELockGuard lock(mtx.get());
          if (set.setCount == 0 && info.setCount) {
            set = info;
          }
        }
        if (auto cb = bleAudioCopyCallback(*this, &Impl::discoveredCb)) {
          cb(bleAudioStatus(e.err), info);
        }
        break;
      }
      case BLE_AUDIO_EVT_CSIP_LOCKED:
      case BLE_AUDIO_EVT_CSIP_RELEASED:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::lockCb)) {
          cb(bleAudioStatus(e.err), e.type == BLE_AUDIO_EVT_CSIP_LOCKED);
        }
        break;
      case BLE_AUDIO_EVT_CSIP_LOCK_CHANGED:
        if (e.data) {
          if (auto cb = bleAudioCopyCallback(*this, &Impl::lockChangedCb)) {
            cb(static_cast<const ble_audio_csis_lock_t *>(e.data)->locked, e.conn_handle);
          }
        }
        break;
      default: break;
    }
  }
};

BLEAudioCoordinatedSetCoordinator::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioCoordinatedSetCoordinator::discover(uint16_t connHandle) {
  if (!_impl || !_impl->audio->started.load() || connHandle == BLE_AUDIO_CONN_NONE) {
    log_e("CoordinatedSet: discover() before audio.begin()");
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->audio->isGattReady(connHandle)) {
      for (uint16_t c : _impl->pending) {
        if (c == connHandle) {
          return BTStatus::OK;
        }
      }
      _impl->pending.push_back(connHandle);
      return BTStatus::OK;
    }
  }
  return _impl->start(connHandle);
}

bool BLEAudioCoordinatedSetCoordinator::isSetMember(const uint8_t *advPayload, size_t len) const {
  if (!_impl || !advPayload) {
    return false;
  }
  BLEAudioCoordinatedSetInfo s;
  {
    BLELockGuard lock(_impl->mtx.get());
    s = _impl->set;
  }
  return s.setCount && bleAudioCsipIsSetMember(s.sirk, advPayload, (uint16_t)len);
}

BTStatus BLEAudioCoordinatedSetCoordinator::lock() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  BLEAudioCoordinatedSetInfo s;
  {
    BLELockGuard lock(_impl->mtx.get());
    s = _impl->set;
  }
  return s.setCount ? bleAudioStatus(bleAudioCsipCoordLock(s.sirk, true)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCoordinatedSetCoordinator::unlock() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  BLEAudioCoordinatedSetInfo s;
  {
    BLELockGuard lock(_impl->mtx.get());
    s = _impl->set;
  }
  return s.setCount ? bleAudioStatus(bleAudioCsipCoordLock(s.sirk, false)) : BTStatus::InvalidState;
}

BLEAudioCoordinatedSetInfo BLEAudioCoordinatedSetCoordinator::setInfo() const {
  if (!_impl) {
    return BLEAudioCoordinatedSetInfo();
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->set;
}

void BLEAudioCoordinatedSetCoordinator::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = std::move(cb);
}

void BLEAudioCoordinatedSetCoordinator::onLock(LockCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.lockCb = std::move(cb);
}

void BLEAudioCoordinatedSetCoordinator::onLockChanged(LockChangedCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.lockChangedCb = std::move(cb);
}

void BLEAudioCoordinatedSetCoordinator::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = nullptr;
  impl.lockCb = nullptr;
  impl.lockChangedCb = nullptr;
}

// --------------------------------------------------------------------------
// Factories
// --------------------------------------------------------------------------

#if BLE_AUDIO_CSIP_MEMBER_SUPPORTED
BLEAudioCoordinatedSetMember BLEAudio::createCoordinatedSetMember() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCoordinatedSetMember();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("CoordinatedSet: create before audio.begin() commits");
    return BLEAudioCoordinatedSetMember();
  }
  return BLEAudioCoordinatedSetMember(makeMember(_impl, false));
}
#endif

#if BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
BLEAudioCapAcceptor BLEAudio::createCapAcceptor() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCapAcceptor();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("CapAcceptor: create before audio.begin() commits");
    return BLEAudioCapAcceptor();
  }
  return BLEAudioCapAcceptor(makeMember(_impl, true));
}
#endif

#if BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED
BLEAudioCoordinatedSetCoordinator BLEAudio::createCoordinatedSetCoordinator() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCoordinatedSetCoordinator();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("CoordinatedSet: create before audio.begin() commits");
    return BLEAudioCoordinatedSetCoordinator();
  }
  auto c = std::make_shared<BLEAudioCoordinatedSetCoordinator::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  std::weak_ptr<BLEAudioCoordinatedSetCoordinator::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_CSIP_COORDINATOR, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  /* The coordinator has no GATT service; its callbacks are registered with the other roles at start. */
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioCsipCoordInit());
  });
  return BLEAudioCoordinatedSetCoordinator(c);
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_CSIP_MEMBER_SUPPORTED
BLEAudioCoordinatedSetMember BLEAudio::createCoordinatedSetMember() {
  log_e("CoordinatedSet: member not enabled in this build (CONFIG_BT_CSIP_SET_MEMBER)");
  return BLEAudioCoordinatedSetMember();
}
#endif

#if !BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
BLEAudioCapAcceptor BLEAudio::createCapAcceptor() {
  log_e("CapAcceptor: not enabled in this build (CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER)");
  return BLEAudioCapAcceptor();
}
#endif

#if !BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED
BLEAudioCoordinatedSetCoordinator BLEAudio::createCoordinatedSetCoordinator() {
  log_e("CoordinatedSet: coordinator not enabled in this build (CONFIG_BT_CSIP_SET_COORDINATOR)");
  return BLEAudioCoordinatedSetCoordinator();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
