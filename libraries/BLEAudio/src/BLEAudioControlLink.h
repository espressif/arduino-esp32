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
 * @brief Internal: the peer a control-profile client role drives.
 *
 * Client roles (volume/microphone/media/call/hearing-aid controllers) may be
 * asked to discover a peer before the engine finished GATT discovery on it.
 * The link remembers the peer and defers the service discovery until
 * BLE_AUDIO_EVT_GATT_DISCOVERED arrives; the role forwards every engine core
 * event to onCore() so the deferred step runs and a dropped link is forgotten.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "BLEAudioImpl.h"
#include "engine/BLEAudioStreamInternal.h"
#include "engine/BLEAudioEngineControl.h"

struct BLEAudioControlLink {
  /** Engine discovery entry point of the role, e.g. bleAudioVcpCtlrDiscover. */
  using DiscoverFn = int (*)(uint16_t);

  mutable BLESharedMutex mtx;           ///< Protects conn/pending (never held across engine calls or callbacks).
  uint16_t conn = BLE_AUDIO_CONN_NONE;  ///< Bound peer, BLE_AUDIO_CONN_NONE when unbound.
  bool pending = false;                 ///< Discovery deferred until the engine's GATT discovery completes.
  uint32_t generation = 0;              ///< Bumped on every bind/disconnect to detect concurrent rebind.

  /** @return Bound peer handle (including while pending GATT discovery), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConn() const {
    BLELockGuard lock(mtx.get());
    return conn;
  }

  /** @return Active peer handle once GATT discovery finished, or BLE_AUDIO_CONN_NONE. */
  uint16_t activeConn() const {
    BLELockGuard lock(mtx.get());
    return pending ? BLE_AUDIO_CONN_NONE : conn;
  }

  /** @return true while a peer is bound (discovered or pending). */
  bool bound() const {
    return getConn() != BLE_AUDIO_CONN_NONE;
  }

  /** @brief Bind @p c and discover now, or once the engine finished GATT discovery on it. */
  BTStatus discover(const BLEAudioImplCommon &audio, uint16_t c, DiscoverFn fn) {
    if (!audio.started.load() || c == BLE_AUDIO_CONN_NONE) {
      return BTStatus::InvalidState;
    }
    uint32_t gen;
    {
      BLELockGuard lock(mtx.get());
      conn = c;
      gen = ++generation;
      pending = !audio.isGattReady(c);
      if (pending) {
        return BTStatus::OK;
      }
    }
    // Call the engine unlocked: avoids AB-BA lock inversion with bt_le_host_lock()
    // and keeps Bluedroid inline discovery callbacks out of mtx.
    BTStatus st = bleAudioStatus(fn(c));
    if (!st) {
      BLELockGuard lock(mtx.get());
      if (generation == gen && conn == c) {
        conn = BLE_AUDIO_CONN_NONE;
      }
    }
    return st;
  }

  /**
   * @brief Resume a deferred discovery; forget the link when it drops.
   * @return the error of a deferred discovery that could not run (report it as
   *         a failed discovery), 0 otherwise.
   */
  int onCore(const ble_audio_evt_t &e, DiscoverFn fn) {
    uint32_t gen;
    {
      BLELockGuard lock(mtx.get());
      if (e.conn_handle != conn) {
        return 0;
      }
      if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED) {
        conn = BLE_AUDIO_CONN_NONE;
        pending = false;
        ++generation;
        return 0;
      }
      if (e.type != BLE_AUDIO_EVT_GATT_DISCOVERED || !pending) {
        return 0;
      }
      pending = false;
      if (e.err) {
        conn = BLE_AUDIO_CONN_NONE;
        ++generation;
        return e.err;
      }
      gen = generation;
    }
    int err = fn(e.conn_handle);
    if (err) {
      BLELockGuard lock(mtx.get());
      if (generation == gen && conn == e.conn_handle) {
        conn = BLE_AUDIO_CONN_NONE;
      }
    }
    return err;
  }
};

#endif /* BLE_AUDIO_SUPPORTED */
