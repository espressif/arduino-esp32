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
#include "audio/BLEAudioCoordinatedSet.h"
#include "audio/BLEAudioCap.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "audio/BLEAudioEngineCap.h"
#include "esp32-hal-log.h"

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
  bool registered = false;  ///< Registered by audio.start().
  /** Fixed default so a two-member demo pairs out of the box; products must set their own. */
  uint8_t sirk[BLE_AUDIO_SIRK_SIZE] = {0xB8, 0x03, 0xEA, 0xC6, 0xAF, 0xBB, 0x65, 0xA2,
                                       0x5A, 0x41, 0xF1, 0x53, 0x05, 0x68, 0x8E, 0x83};
  uint8_t setSize = 2;
  uint8_t rank = 1;
  bool lockable = true;
  String name;  ///< Optional CSIS name (empty = none).
  LockCallback lockCb;
  SirkReadCallback sirkCb;

  /** Register the CSIS instance with the staged values; runs inside audio.start(). */
  BTStatus apply() {
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

  /** @brief Log a failed live update; the new value is still used at the next start. */
  void checkLive(int err, const char *what) {
    if (err != 0) {
      log_w("CoordinatedSet: %s update failed (err=%d)", what, err);
    }
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
        if (lockCb) {
          lockCb(static_cast<const ble_audio_csis_lock_t *>(e.data)->locked, e.conn_handle);
        }
        break;
      case BLE_AUDIO_CSIS_EVT_SIRK_REQ:
        if (sirkCb) {
          *static_cast<const ble_audio_csis_sirk_req_t *>(e.data)->reply = static_cast<uint8_t>(sirkCb(e.conn_handle));
        }
        break;
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

BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]) {
  if (_impl && sirk) {
    memcpy(_impl->sirk, sirk, BLE_AUDIO_SIRK_SIZE);
    if (_impl->registered) {
      _impl->checkLive(bleAudioCsisSetSirk(_impl->cas, sirk), "SIRK");
    }
  }
  return *this;
}

// The stack takes size and rank together and rejects the update unless the size changes.
BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::setSetSize(uint8_t size) {
  if (_impl && size != _impl->setSize) {
    _impl->setSize = size;
    if (_impl->registered) {
      _impl->checkLive(bleAudioCsisSetSizeRank(_impl->cas, size, _impl->rank), "size");
    }
  }
  return *this;
}

BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::setRank(uint8_t rank) {
  if (_impl && rank != _impl->rank) {
    _impl->rank = rank;
    if (_impl->registered) {
      log_w("CoordinatedSet: rank %u staged; a running set only takes it with a new set size (setSetSize()) or at the next start", rank);
    }
  }
  return *this;
}

BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::setLockable(bool lockable) {
  if (_impl) {
    _impl->lockable = lockable;
  }
  return *this;
}

BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::setName(const String &name) {
  if (_impl && name != _impl->name) {
    _impl->name = name;
    if (_impl->registered) {
      _impl->checkLive(bleAudioCsisSetName(_impl->cas, reinterpret_cast<const uint8_t *>(name.c_str()), (uint8_t)name.length()), "name");
    }
  }
  return *this;
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

BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::onLockChanged(LockCallback cb) {
  if (_impl) {
    _impl->lockCb = std::move(cb);
  }
  return *this;
}

BLEAudioCoordinatedSetMember &BLEAudioCoordinatedSetMember::onSirkRead(SirkReadCallback cb) {
  if (_impl) {
    _impl->sirkCb = std::move(cb);
  }
  return *this;
}

void BLEAudioCoordinatedSetMember::resetCallbacks() {
  if (_impl) {
    _impl->lockCb = nullptr;
    _impl->sirkCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Coordinator
// --------------------------------------------------------------------------

struct BLEAudioCoordinatedSetCoordinator::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  std::vector<uint16_t> pending;  ///< discover() called before the link's GATT discovery finished.
  BLEAudioCoordinatedSetInfo set; ///< First discovered set (setCount 0 until then).
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
        for (auto it = pending.begin(); it != pending.end(); ++it) {
          if (*it == e.conn_handle) {
            pending.erase(it);
            if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED) {
              (void)start(e.conn_handle);
            }
            break;
          }
        }
        break;
      case BLE_AUDIO_EVT_CSIP_DISCOVERED: {
        BLEAudioCoordinatedSetInfo info;
        info.connHandle = e.conn_handle;
        if (e.err == 0 && e.data) {
          const auto *s = static_cast<const ble_audio_csip_set_t *>(e.data);
          info.setCount = s->set_count;
          info.setSize = s->set_size;
          info.rank = s->rank;
          info.lockable = s->lockable;
          memcpy(info.sirk, s->sirk, sizeof(info.sirk));
          if (set.setCount == 0 && info.setCount) {
            set = info;
          }
        }
        if (discoveredCb) {
          discoveredCb(bleAudioStatus(e.err), info);
        }
        break;
      }
      case BLE_AUDIO_EVT_CSIP_LOCKED:
      case BLE_AUDIO_EVT_CSIP_RELEASED:
        if (lockCb) {
          lockCb(bleAudioStatus(e.err), e.type == BLE_AUDIO_EVT_CSIP_LOCKED);
        }
        break;
      case BLE_AUDIO_EVT_CSIP_LOCK_CHANGED:
        if (lockChangedCb && e.data) {
          lockChangedCb(static_cast<const ble_audio_csis_lock_t *>(e.data)->locked, e.conn_handle);
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
  if (!_impl || !_impl->audio->started) {
    log_e("CoordinatedSet: discover() before audio.start()");
    return BTStatus::InvalidState;
  }
  if (_impl->audio->isGattReady(connHandle)) {
    return _impl->start(connHandle);
  }
  for (uint16_t c : _impl->pending) {
    if (c == connHandle) {
      return BTStatus::OK;
    }
  }
  _impl->pending.push_back(connHandle);
  return BTStatus::OK;
}

bool BLEAudioCoordinatedSetCoordinator::isSetMember(const uint8_t *advPayload, size_t len) const {
  return _impl && _impl->set.setCount && advPayload && bleAudioCsipIsSetMember(_impl->set.sirk, advPayload, (uint16_t)len);
}

BTStatus BLEAudioCoordinatedSetCoordinator::lock() {
  return (_impl && _impl->set.setCount) ? bleAudioStatus(bleAudioCsipCoordLock(_impl->set.sirk, true)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCoordinatedSetCoordinator::unlock() {
  return (_impl && _impl->set.setCount) ? bleAudioStatus(bleAudioCsipCoordLock(_impl->set.sirk, false)) : BTStatus::InvalidState;
}

BLEAudioCoordinatedSetInfo BLEAudioCoordinatedSetCoordinator::setInfo() const {
  return _impl ? _impl->set : BLEAudioCoordinatedSetInfo();
}

BLEAudioCoordinatedSetCoordinator &BLEAudioCoordinatedSetCoordinator::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioCoordinatedSetCoordinator &BLEAudioCoordinatedSetCoordinator::onLock(LockCallback cb) {
  if (_impl) {
    _impl->lockCb = std::move(cb);
  }
  return *this;
}

BLEAudioCoordinatedSetCoordinator &BLEAudioCoordinatedSetCoordinator::onLockChanged(LockChangedCallback cb) {
  if (_impl) {
    _impl->lockChangedCb = std::move(cb);
  }
  return *this;
}

void BLEAudioCoordinatedSetCoordinator::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->lockCb = nullptr;
    _impl->lockChangedCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Factories
// --------------------------------------------------------------------------

BLEAudioCoordinatedSetMember BLEAudio::createCoordinatedSetMember() {
#if BLE_AUDIO_CSIP_MEMBER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("CoordinatedSet: create the member between audio.begin() and audio.start()");
    return BLEAudioCoordinatedSetMember();
  }
  return BLEAudioCoordinatedSetMember(makeMember(_impl, false));
#else
  log_e("CoordinatedSet: member not enabled in this build (CONFIG_BT_CSIP_SET_MEMBER)");
  return BLEAudioCoordinatedSetMember();
#endif
}

BLEAudioCapAcceptor BLEAudio::createCapAcceptor() {
#if BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("CapAcceptor: create between audio.begin() and audio.start()");
    return BLEAudioCapAcceptor();
  }
  return BLEAudioCapAcceptor(makeMember(_impl, true));
#else
  log_e("CapAcceptor: not enabled in this build (CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER)");
  return BLEAudioCapAcceptor();
#endif
}

BLEAudioCoordinatedSetCoordinator BLEAudio::createCoordinatedSetCoordinator() {
#if BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("CoordinatedSet: create the coordinator between audio.begin() and audio.start()");
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
#else
  log_e("CoordinatedSet: coordinator not enabled in this build (CONFIG_BT_CSIP_SET_COORDINATOR)");
  return BLEAudioCoordinatedSetCoordinator();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
