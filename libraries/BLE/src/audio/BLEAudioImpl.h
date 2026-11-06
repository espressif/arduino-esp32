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
 * @file
 * @brief Shared implementation state of the LE Audio controller handle.
 *
 * The engine API is host-agnostic, so `BLEAudio::Impl` is defined once and
 * compiled into every build (NimBLE and Bluedroid alike).
 *
 * It is the one place engine events enter C++. The engine posts a tagged
 * `ble_audio_evt_t` envelope to dispatch() on the Bluetooth host task:
 *  - Core events (`BLE_AUDIO_GRP_CORE`: ACL, security, GATT discovery) update
 *    the link table, reach EVERY registered role handler (each role may care
 *    about a link), then the application's link callbacks.
 *  - Every other group reaches the single handler its role registered.
 * Stream events do not come through here; they go straight to the stream
 * (see `BLEAudioStream.cpp`).
 */

#include "audio/BLEAudio.h"
#if BLE_AUDIO_SUPPORTED

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "BTStatus.h"
#include "audio/BLEAudioEngine.h"

struct BLEAudioImplCommon {
  /** @brief Role handler for one event group (host task). */
  using EventHandler = std::function<void(const ble_audio_evt_t &)>;
  using LinkCallback = std::function<void(uint16_t connHandle)>;

  // --------------------------------------------------------------------------
  // State
  // --------------------------------------------------------------------------

  uint32_t presentationDelayUs = 40000;  ///< Default for new roles (setPresentationDelay()).
  LinkCallback linkReadyCb;              ///< Application onLinkReady().
  LinkCallback disconnectedCb;           ///< Application onDisconnected().
  bool active = false;                   ///< begin() succeeded, end() not called.
  bool started = false;                  ///< start() succeeded.

  /**
   * Staged role registrations, run in creation order by BLEAudio::start()
   * between engine init and the single GATT commit, then cleared.
   */
  std::vector<std::function<BTStatus()>> roleApplies;

  /**
   * Keeps every role (and helper objects such as the NimBLE connect client)
   * alive until end(): registered services point into their roles' streams,
   * so a role must not die when the sketch drops its handle.
   */
  std::vector<std::shared_ptr<void>> roles;

  /** One handler per event group; roles register theirs at creation, before start(). */
  EventHandler handlers[BLE_AUDIO_GRP_COUNT];

  /** Links whose GATT discovery finished; client roles may start profile discovery on them. */
  static constexpr size_t kMaxLinks = 4;
  uint16_t gattReady[kMaxLinks] = {BLE_AUDIO_CONN_NONE, BLE_AUDIO_CONN_NONE, BLE_AUDIO_CONN_NONE, BLE_AUDIO_CONN_NONE};

  // --------------------------------------------------------------------------
  // Helpers
  // --------------------------------------------------------------------------

  /** @brief Register (or replace) the handler of @p group; one role per group. */
  void setHandler(ble_audio_evt_group_t group, EventHandler h) {
    handlers[group] = std::move(h);
  }

  /** @brief Whether the GATT discovery of @p conn finished and the link is still up. */
  bool isGattReady(uint16_t conn) const {
    for (uint16_t c : gattReady) {
      if (c == conn) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Maintain the link table from core events.
   *
   * Adds a link on a successful GATT discovery (ignored when the table is
   * full: the link still works, isGattReady() just reports false for it) and
   * removes it on disconnect.
   */
  void trackLink(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED && e.err == 0 && !isGattReady(e.conn_handle)) {
      for (uint16_t &c : gattReady) {
        if (c == BLE_AUDIO_CONN_NONE) {
          c = e.conn_handle;
          break;
        }
      }
    } else if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED) {
      for (uint16_t &c : gattReady) {
        if (c == e.conn_handle) {
          c = BLE_AUDIO_CONN_NONE;
        }
      }
    }
  }

  /**
   * @brief Route one engine event (host task).
   *
   * Core events: link table first, so a role handler already sees the
   * updated isGattReady(); then every role; the application callbacks last,
   * so roles are ready when the sketch reacts to a link.
   */
  void dispatch(const ble_audio_evt_t &e) {
    const uint8_t group = BLE_AUDIO_EVT_GROUP(e.type);
    if (group == BLE_AUDIO_GRP_CORE) {
      trackLink(e);
      for (auto &h : handlers) {
        if (h) {
          h(e);
        }
      }
      if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED && e.err == 0 && linkReadyCb) {
        linkReadyCb(e.conn_handle);
      } else if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED && disconnectedCb) {
        disconnectedCb(e.conn_handle);
      }
    } else if (group < BLE_AUDIO_GRP_COUNT && handlers[group]) {
      handlers[group](e);
    }
  }
};

/** Single definition for both hosts (see the file comment). */
struct BLEAudio::Impl : BLEAudioImplCommon {};

#endif /* BLE_AUDIO_SUPPORTED */
