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
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "core/BLESync.h"
#include "core/BLETask.h"
#include "freertos/task.h"
#include "esp32-hal-log.h"
#include "BLEAudioStreamImpl.h"
#include "engine/BLEAudioEngineControl.h"

/**
 * @brief Shared 5 s `BLESync` wait helper for broadcast roles (`BroadcastSource`, `BroadcastSink`, `CapInitiator`).
 */
template<typename BusyFn>
inline BTStatus bleAudioWaitUntilIdle(BLESharedMutex &mtx, BLESync &sync, const char *roleName, BusyFn &&isBusy, uint32_t timeoutMs = 5000) {
  {
    BLELockGuard lock(mtx.get());
    if (!isBusy()) {
      return BTStatus::OK;
    }
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("%s: cannot wait for BIG teardown from a stack callback", roleName);
    return BTStatus::InvalidState;
  }
  sync.take();
  {
    BLELockGuard lock(mtx.get());
    if (!isBusy()) {
      sync.give(BTStatus::OK);
      return BTStatus::OK;
    }
  }
  BTStatus st = sync.wait(timeoutMs);
  BLELockGuard lock(mtx.get());
  if (st != BTStatus::OK || isBusy()) {
    log_e("%s: timed out waiting for BIG teardown", roleName);
    return st == BTStatus::OK ? BTStatus::Fail : st;
  }
  return BTStatus::OK;
}

/**
 * @brief One deadline shared by every wait of a broadcast stop.
 *
 * A stop used to stack three independent 5 s waits (start-resolve, engine
 * stop, stopped-event) for a ~15 s worst case. Every phase now consumes from
 * the same budget, so the documented worst case is one budget. When a phase
 * finds the budget exhausted with the transport still live, the stop reports
 * where it stalled (Fail) with the state retained for a retry.
 *
 * Tick differences are used throughout so a 32-bit tick wrap cannot fake
 * expiry (only differences shorter than half the wrap period occur here).
 */
class BLEAudioStopBudget {
public:
  void start(uint32_t totalMs = 5000) {
    _start = xTaskGetTickCount();
    _total = pdMS_TO_TICKS(totalMs);
  }

  /** @brief Milliseconds left, 0 when exhausted. */
  uint32_t remainingMs() const {
    const TickType_t elapsed = xTaskGetTickCount() - _start;
    if (elapsed >= _total) {
      return 0;
    }
    return (uint32_t)((_total - elapsed) * (TickType_t)portTICK_PERIOD_MS);
  }

  bool expired() const {
    return remainingMs() == 0;
  }

  /** @brief Report a stall at @p phase with the transport still live. */
  BTStatus stall(const char *role, const char *phase) const {
    log_e("%s: stop stalled at %s, transport still live; retry stop()", role, phase);
    return BTStatus::Fail;
  }

private:
  TickType_t _start = 0;
  TickType_t _total = 0;
};

/** @return The @p n-th set bit of an Audio Location mask, or 0 (Mono) when there are fewer bits. */
inline uint32_t bleAudioNthLocation(uint32_t mask, uint8_t n) {
  for (uint32_t bit = 1; bit; bit <<= 1) {
    if ((mask & bit) && n-- == 0) {
      return bit;
    }
  }
  return 0;
}

/** @brief Pack a Broadcast Code string into a zero-padded 16-octet buffer, returning nullptr when empty. */
inline const uint8_t *bleAudioPackBroadcastCode(const String &code, uint8_t out[16]) {
  memset(out, 0, 16);
  const size_t n = code.length() < 16 ? code.length() : 16;
  memcpy(out, code.c_str(), n);
  return n ? out : nullptr;
}

/**
 * @brief BTAddress::Type as the numeric host address type.
 *
 * Passed through unmasked: identity address types use both bits, so masking
 * with 1 corrupts PublicID (2 -> 0) and RandomID (3 -> 1) on paths that feed
 * the stack's esp_ble_addr_type_t.
 */
inline uint8_t bleAudioAddrType(BTAddress::Type type) {
  return static_cast<uint8_t>(type);
}

struct BLEAudioControlLink {
  /** Engine discovery entry point of the role, e.g. bleAudioVcpCtlrDiscover. */
  using DiscoverFn = int (*)(uint16_t);

  // Single-link only: multi-peer roles (unicast client, CAP initiator /
  // commander, TMAP/GMAP discovery) mirror this shape with their own tables
  // instead of reusing it. Rule for all of them: mtx is released before any
  // engine discovery call, so Bluedroid's cache-backed inline discovery
  // callbacks can invoke controller operations without AB-BA lock inversion
  // against bt_le_host_lock().
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
