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
 * @file BLEAudioStreamImpl.h
 * @brief Stream PIMPL internals (`BLEAudioStream::Impl`, `BLEAudioStreamAccess`,
 *        public <-> engine value conversions, and `bleAudioStatus()`).
 *
 * NOT part of the public API. Lives in `src/` alongside `BLEAudioStream.{h,cpp}`
 * so `src/engine/` remains 100% pure C.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <atomic>
#include <errno.h>
#include <memory>
#include "esp_err.h"
#include "core/BLEMutex.h"
#include "BLEAudioStream.h"
#include "engine/BLEAudioEngineBap.h"

// ==========================================================================
// Data-path tap & BLEAudioStream::Impl
// ==========================================================================

/**
 * @brief Data-path tap of one stream (used by `BLEAudioPlayer` / `BLEAudioRecorder`).
 *
 * Plain function pointers keep the per-SDU hot path free of `std::function`
 * overhead, and sit in a separate slot from user callbacks so attaching a
 * player/recorder never overrides `onReceive()` / `onSent()`.
 */
struct BLEAudioStreamTap {
  void (*recv)(void *ctx, const ble_audio_recv_info_t *info, const uint8_t *data, uint16_t len) = nullptr;  ///< One SDU received.
  void (*sent)(void *ctx) = nullptr;                                                                        ///< One SDU released by the controller.
  void (*started)(void *ctx) = nullptr;                                                                     ///< Stream entered Streaming.
  void (*stopped)(void *ctx) = nullptr;  ///< Stream left Streaming, or its endpoint was released.
  void *ctx = nullptr;                   ///< Owner of the tap (the data path).
};

/**
 * @brief State behind a `BLEAudioStream` handle.
 *
 * Owns one engine slot for its whole lifetime: allocated in
 * `BLEAudioStreamAccess::create()` and freed in `~Impl()`, so the engine never
 * reports an event for a destroyed `Impl`.
 */
struct BLEAudioStream::Impl : std::enable_shared_from_this<BLEAudioStream::Impl> {
  ble_audio_slot_t *slot = nullptr;  ///< Engine stream slot (never null for a live handle).
  BLESharedMutex mtx;                ///< Protects the application callbacks below.
  Callback onConfiguredCb;           ///< Application onConfigured().
  Callback onStartedCb;              ///< Application onStarted().
  StoppedCallback onStoppedCb;       ///< Application onStopped().
  ReceiveCallback onReceiveCb;       ///< Application onReceive().
  Callback onSentCb;                 ///< Application onSent().
  /// Internal data path (Player/Recorder); null when none. Mutated only via setTap()/clearTap().
  std::atomic<const BLEAudioStreamTap *> tap{nullptr};
  /// Tap calls in flight on the host task; setTap()/clearTap() wait for zero.
  std::atomic<uint8_t> tapBusy{0};

  ~Impl() {
    bleAudioStreamFree(slot);
  }
};

// ==========================================================================
// Internal access bridge (sole friend of BLEAudioStream)
// ==========================================================================

struct BLEAudioStreamAccess {
  /** @brief Allocate a stream of @p kind from the engine pool (empty handle when exhausted). */
  static BLEAudioStream create(ble_audio_stream_kind_t kind);

  /** @brief Build a public handle around an existing `Impl`. */
  static BLEAudioStream wrap(std::shared_ptr<BLEAudioStream::Impl> impl) {
    return BLEAudioStream(std::move(impl));
  }

  /** @brief `Impl` behind @p s (`nullptr` for an empty handle). */
  static BLEAudioStream::Impl *impl(const BLEAudioStream &s) {
    return s._impl.get();
  }

  /** @brief Engine slot behind @p s (`nullptr` for an empty handle). */
  static ble_audio_slot_t *slot(const BLEAudioStream &s) {
    return s._impl ? s._impl->slot : nullptr;
  }

  /**
   * @brief Install @p tap on @p s, replacing any previous one (`nullptr` removes it).
   *
   * Blocks until no host-task call is still inside the previous tap. Refused
   * (with a log) from inside a tap hook, where draining would wedge the host
   * task; detach outside the data path instead.
   */
  static void setTap(const BLEAudioStream &s, const BLEAudioStreamTap *tap);

  /** @brief Remove @p tap from @p s if still installed; waits for in-flight tap calls. Same refusal as setTap. */
  static void clearTap(const BLEAudioStream &s, const BLEAudioStreamTap *tap);

  /** @brief Install the engine stream callback table; called once by `BLEAudio::begin()`. */
  static void installEngineCallbacks();
};

// ==========================================================================
// Public <-> engine value conversions
// ==========================================================================

/** @brief Engine codec -> public codec config (0 frames_per_sdu reads as 1). */
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

// ==========================================================================
// Engine error -> BTStatus mapping
// ==========================================================================

/**
 * @brief Map a host-glue entry point result onto `BTStatus`.
 *
 * `bleAudioEngineDisconnect`, `bleAudioEngineReceivePeriodicSync` and
 * `bleAudioEngineStopPeriodicSyncReceive` return numeric `BTStatus::Code`
 * values directly (0 on success) instead of `esp_err_t`/errno, so callers
 * must not run them through `bleAudioStatus()`. A native leak would
 * reinterpret (and truncate, `Code` is `int8_t`) into garbage, so
 * out-of-range values fail closed. Callers log the failure with context.
 */
inline BTStatus bleAudioEngineStatus(int err) {
  if (err == 0) {
    return BTStatus::OK;
  }
  if (err < static_cast<int>(BTStatus::NotSupported) || err > static_cast<int>(BTStatus::Fail)) {
    return BTStatus::Fail;
  }
  return BTStatus(static_cast<BTStatus::Code>(err));
}

/**
 * @brief Map an engine result (`esp_err_t` or negative errno) onto `BTStatus`.
 *
 * `-EALREADY` is handled by specific call sites when it represents an
 * idempotent no-op; the fallback here is `InvalidState`.
 */
inline BTStatus bleAudioStatus(int err) {
  switch (err) {
    case 0:                     return BTStatus::OK;
    case ESP_ERR_INVALID_ARG:
    case ESP_ERR_INVALID_SIZE:
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
    case -EACCES:               return BTStatus::PermissionDenied;
    default:                    return BTStatus::Fail;
  }
}

#endif /* BLE_AUDIO_SUPPORTED */
