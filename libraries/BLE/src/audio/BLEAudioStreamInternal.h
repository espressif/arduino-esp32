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
 * @brief Stream internals shared by the role handles and the LC3 data path.
 *
 * NOT part of the public API. This header is the single place where the
 * public stream handle meets the engine:
 *  - `BLEAudioStream::Impl`: the handle's state (engine slot + callbacks).
 *  - `BLEAudioStreamAccess`: the friend through which role handles allocate
 *    streams and reach their slot, and the data path installs its tap
 *    (synchronized with the host task, see setTap()).
 *  - Converters between the public value types (`BLEAudioTypes.h`) and the
 *    engine's plain C structs (`BLEAudioEngine.h`).
 *  - `bleAudioStatus()`: the one mapping of engine errors onto `BTStatus`.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <atomic>
#include <memory>
#include <errno.h>
#include "esp_err.h"
#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioEngineBap.h"

// --------------------------------------------------------------------------
// Stream state
// --------------------------------------------------------------------------

/**
 * @brief Data-path tap of one stream (used by `BLEAudioPlayer`/`BLEAudioRecorder`).
 *
 * Plain function pointers so the per-SDU path costs no std::function call,
 * and a separate slot from the application callbacks so attaching a player
 * never overrides the user's onReceive()/onSent(). Every hook receives `ctx`.
 *
 * The struct is owned by the data path and installed by pointer with
 * BLEAudioStreamAccess::setTap(), so the host task always sees one complete
 * set of hooks and never a half-written one.
 */
struct BLEAudioStreamTap {
  void (*recv)(void *ctx, const ble_audio_recv_info_t *info, const uint8_t *data, uint16_t len) = nullptr;  ///< One SDU received.
  void (*sent)(void *ctx) = nullptr;     ///< One SDU released by the controller.
  void (*started)(void *ctx) = nullptr;  ///< Stream entered Streaming.
  void (*stopped)(void *ctx) = nullptr;  ///< Stream left Streaming, or its endpoint was released.
  void *ctx = nullptr;                   ///< Owner of the tap (the data path).
};

/**
 * @brief State behind a `BLEAudioStream` handle.
 *
 * Owns exactly one engine slot for its whole lifetime: the slot is allocated
 * by BLEAudioStreamAccess::create() and returned to the pool by the
 * destructor, so the engine never reports an event for a destroyed Impl.
 * The raw `this` pointer is the slot's owner (see BLEAudioStream.cpp).
 */
struct BLEAudioStream::Impl : std::enable_shared_from_this<BLEAudioStream::Impl> {
  ble_audio_slot_t *slot = nullptr;  ///< Engine stream slot (never null for a live handle).
  Callback configuredCb;             ///< Application onConfigured().
  Callback startedCb;                ///< Application onStarted().
  StoppedCallback stoppedCb;         ///< Application onStopped().
  ReceiveCallback receiveCb;         ///< Application onReceive().
  Callback sentCb;                   ///< Application onSent().
  /** Internal data path (Player/Recorder); null when none. Written only through setTap()/clearTap(). */
  std::atomic<const BLEAudioStreamTap *> tap{nullptr};
  /** Tap calls in flight on the host task; setTap()/clearTap() wait for it to reach zero. */
  std::atomic<uint8_t> tapBusy{0};

  ~Impl() {
    bleAudioStreamFree(slot);
  }
};

/**
 * @brief Internal access to `BLEAudioStream` (the only friend of the handle).
 *
 * Role handles create their streams here and pass the slot to the engine; the
 * data path reaches the Impl to install its tap.
 */
struct BLEAudioStreamAccess {
  /**
   * @brief Allocate a stream of @p kind from the engine pool.
   * @return A handle owning the new slot, or a null handle when the pool
   *         (sized from Kconfig) is exhausted.
   */
  static BLEAudioStream create(ble_audio_stream_kind_t kind);

  /** @brief Build a public handle around an existing Impl. */
  static BLEAudioStream wrap(std::shared_ptr<BLEAudioStream::Impl> impl) {
    return BLEAudioStream(std::move(impl));
  }

  /** @brief Impl behind @p s (null for a null handle). */
  static BLEAudioStream::Impl *impl(const BLEAudioStream &s) {
    return s._impl.get();
  }

  /** @brief Engine slot behind @p s (null for a null handle). */
  static ble_audio_slot_t *slot(const BLEAudioStream &s) {
    return s._impl ? s._impl->slot : nullptr;
  }

  /**
   * @brief Install @p tap on @p s, replacing any previous one (nullptr removes it).
   *
   * @p tap must stay valid until it is removed. Returns only once no host-task
   * call is still running inside the previous tap, so its owner may be freed
   * right after. Must not be called from inside a tap hook (it would wait on
   * itself).
   */
  static void setTap(const BLEAudioStream &s, const BLEAudioStreamTap *tap);

  /** @brief Remove @p tap from @p s only if it is still the installed one; waits like setTap(). */
  static void clearTap(const BLEAudioStream &s, const BLEAudioStreamTap *tap);

  /** @brief Install the engine stream callback table; called once by `BLEAudio::begin()`. */
  static void installEngineCallbacks();
};

// --------------------------------------------------------------------------
// Public <-> engine value conversions
// --------------------------------------------------------------------------

/** @brief Engine codec -> public codec config (a zero frame count means one block). */
inline BLEAudioCodecConfig bleAudioCodecFromEngine(const ble_audio_codec_t &c) {
  BLEAudioCodecConfig out;
  out.samplingRateHz = c.sample_rate_hz;
  out.frameDurationUs = c.frame_dur_us;
  out.octetsPerFrame = c.octets_per_frame;
  out.framesPerSdu = c.frames_per_sdu ? c.frames_per_sdu : 1;
  out.channelAllocation = static_cast<BLEAudioLocation>(c.chan_alloc);
  return out;
}

/** @brief Public codec config -> engine codec. */
inline ble_audio_codec_t bleAudioCodecToEngine(const BLEAudioCodecConfig &c) {
  ble_audio_codec_t out = {};
  out.sample_rate_hz = c.samplingRateHz;
  out.frame_dur_us = c.frameDurationUs;
  out.octets_per_frame = c.octetsPerFrame;
  out.frames_per_sdu = c.framesPerSdu;
  out.chan_alloc = static_cast<uint32_t>(c.channelAllocation);
  return out;
}

/** @brief Engine QoS -> public QoS. */
inline BLEAudioQos bleAudioQosFromEngine(const ble_audio_qos_t &q) {
  BLEAudioQos out;
  out.sduIntervalUs = q.sdu_interval_us;
  out.maxSdu = q.max_sdu;
  out.retransmissions = q.rtn;
  out.maxTransportLatencyMs = q.latency_ms;
  out.presentationDelayUs = q.pd_us;
  out.phy = q.phy;
  out.framed = q.framed;
  return out;
}

/** @brief Public QoS -> engine QoS. */
inline ble_audio_qos_t bleAudioQosToEngine(const BLEAudioQos &q) {
  ble_audio_qos_t out = {};
  out.sdu_interval_us = q.sduIntervalUs;
  out.max_sdu = q.maxSdu;
  out.rtn = q.retransmissions;
  out.latency_ms = q.maxTransportLatencyMs;
  out.pd_us = q.presentationDelayUs;
  out.phy = q.phy;
  out.framed = q.framed;
  return out;
}

// --------------------------------------------------------------------------
// Error mapping
// --------------------------------------------------------------------------

/**
 * @brief Map an engine result onto `BTStatus`.
 *
 * The engine returns either an `esp_err_t` (esp_ble_audio_* API) or a
 * negative errno (Zephyr-derived host code); both families are accepted.
 * Anything unrecognised maps to `BTStatus::Fail`.
 *
 * `-EALREADY` means a no-op for some calls (stop, set-same-value) and a
 * rejection for others (start with different parameters), so callers that can
 * tell which handle it before mapping; the fallback here is `InvalidState`.
 */
inline BTStatus bleAudioStatus(int err) {
  switch (err) {
    case 0:                     return BTStatus::OK;
    case ESP_ERR_INVALID_ARG:
    case -EINVAL:               return BTStatus::InvalidParam;
    case ESP_ERR_INVALID_STATE:
    case -EALREADY:             return BTStatus::InvalidState;
    case -EBUSY:                return BTStatus::Busy;
    case ESP_ERR_NO_MEM:
    case -ENOMEM:               return BTStatus::NoMemory;
    case ESP_ERR_NOT_SUPPORTED:
    case -ENOTSUP:              return BTStatus::NotSupported;
    case ESP_ERR_TIMEOUT:       return BTStatus::Timeout;
    case -ENOTCONN:             return BTStatus::NotConnected;
    case -ENOENT:               return BTStatus::NotFound;
    default:                    return BTStatus::Fail;
  }
}

#endif /* BLE_AUDIO_SUPPORTED */
