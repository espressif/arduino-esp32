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

#pragma once

/**
 * @file BLEAudioImpl.h
 * @brief Shared implementation base (`BLEAudioImplCommon`) of the LE Audio controller handle.
 *
 * Concrete `BLEAudio::Impl` is defined per backend in `BLEAudio.nimble.h` /
 * `BLEAudio.bluedroid.h` (matching the core BLE two-layer PIMPL model) so
 * NimBLE-only fallback clients stay out of the shared controller state.
 */

#include "BLEAudio.h"
#if BLE_AUDIO_SUPPORTED

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include "BTStatus.h"
#include "core/BLEMutex.h"
#include "engine/BLEAudioEngine.h"
#include "esp32-hal-log.h"

/**
 * @brief Copy a role callback under the role mutex so it can be invoked unlocked.
 */
template<typename T, typename Fn> Fn bleAudioCopyCallback(const T &impl, Fn T::*member) {
  BLELockGuard lock(impl.mtx.get());
  return impl.*member;
}

struct BLEAudioImplCommon {
  using EventHandler = std::function<void(const ble_audio_evt_t &)>;
  using LinkCallback = std::function<void(uint16_t connHandle)>;

  BLESharedMutex mtx;                                ///< Protects handlers and link callbacks.
  std::atomic<uint32_t> presentationDelayUs{40000};  ///< Default for new roles (setPresentationDelay()).
  std::atomic<bool> autoSecurity{true};              ///< begin() installs the default audio policy unless disabled.
  LinkCallback onLinkReadyCb;                        ///< Application onLinkReady().
  LinkCallback onDisconnectedCb;                     ///< Application onDisconnected().
  std::atomic<bool> active{false};                   ///< begin() succeeded, end() not called.
  std::atomic<bool> started{false};                  ///< begin() committed the roles and GATT.
  std::atomic<bool> beginFailed{false};              ///< begin() failed midway; end() + begin() required.

  std::vector<std::function<BTStatus()>> roleApplies;
  std::vector<std::shared_ptr<void>> roles;
  std::vector<std::function<void()>> roleStops;

  EventHandler handlers[BLE_AUDIO_GRP_COUNT];

  static constexpr size_t kMaxLinks = BLE_MAX_CONN;
  std::array<uint16_t, kMaxLinks> gattReady;

  BLEAudioImplCommon() {
    gattReady.fill(BLE_AUDIO_CONN_NONE);
  }

  static BLEAudio makeHandle(std::shared_ptr<BLEAudio::Impl> impl) {
    return BLEAudio(std::move(impl));
  }

  /**
   * @brief Shared factory helper for staging a role handle on `BLEAudio::Impl`.
   *
   * Enforces the "between begin() and start()" precondition, stores the strong
   * role reference in `roles`, and binds a weak group dispatcher + apply hook.
   * Used by the control/identity roles (VCP, MICP, MCP, CCP, TMAP, GMAP, HAS)
   * whose events map 1:1 onto one event group. The BAP data-plane and
   * CAP/CSIP factories (unicast, broadcast, CAP, coordinated set) stage by
   * hand: peer tables, roles with no handler, multi-group dispatch, and
   * post-start creation do not fit the shared shape.
   */
  template<typename Handle, typename AudioImpl, typename ApplyFn>
  static Handle createStagedRole(const std::shared_ptr<AudioImpl> &audio, const char *factoryName, ble_audio_evt_group_t group, ApplyFn &&apply) {
    if (!audio || !audio->active || audio->started) {
      log_e("%s(): create before audio.begin() commits", factoryName);
      return Handle();
    }
    auto role = std::make_shared<typename Handle::Impl>();
    role->audio = audio;
    audio->roles.push_back(role);
    std::weak_ptr<typename Handle::Impl> weak = role;
    audio->setHandler(group, [weak](const ble_audio_evt_t &e) {
      if (auto s = weak.lock()) {
        s->handle(e);
      }
    });
    audio->roleApplies.push_back([weak, applyFn = std::forward<ApplyFn>(apply)]() mutable -> BTStatus {
      auto s = weak.lock();
      return s ? applyFn(*s) : BTStatus::OK;
    });
    return Handle(role);
  }

  void setHandler(ble_audio_evt_group_t group, EventHandler h) {
    switch (group) {
      case BLE_AUDIO_GRP_UNICAST_CLIENT:
      case BLE_AUDIO_GRP_BROADCAST_ASSISTANT:
      case BLE_AUDIO_GRP_CAP_INITIATOR:
      case BLE_AUDIO_GRP_CAP_COMMANDER:
      case BLE_AUDIO_GRP_CAP_HANDOVER:
      case BLE_AUDIO_GRP_CSIP_COORDINATOR:
      case BLE_AUDIO_GRP_VCP_CONTROLLER:
      case BLE_AUDIO_GRP_MICP_CONTROLLER:
      case BLE_AUDIO_GRP_MCP_CLIENT:
      case BLE_AUDIO_GRP_CCP_CLIENT:
      case BLE_AUDIO_GRP_HAS_CLIENT:
      case BLE_AUDIO_GRP_TMAP:
      case BLE_AUDIO_GRP_GMAP:                bleAudioEngineEnablePeerDiscovery(); break;
      default:                                break;
    }
    BLELockGuard lock(mtx.get());
    handlers[group] = std::move(h);
  }

  bool isGattReady(uint16_t conn) const {
    BLELockGuard lock(mtx.get());
    for (uint16_t c : gattReady) {
      if (c == conn) {
        return true;
      }
    }
    return false;
  }

  void trackLink(const ble_audio_evt_t &e);
  void dispatch(const ble_audio_evt_t &e);
};

#endif /* BLE_AUDIO_SUPPORTED */
