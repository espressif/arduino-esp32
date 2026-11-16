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
 * @brief Per-stream handle for one unidirectional LE Audio stream.
 *
 * A `BLEAudioStream` is a shared handle (like the rest of the library's value
 * types) to one audio stream: a unicast ASE carried on a CIS, or one BIS of a
 * broadcast group. Role handles (`BLEAudioUnicastServer`,
 * `BLEAudioUnicastClient`, `BLEAudioBroadcastSource`, `BLEAudioBroadcastSink`)
 * create and own their streams and expose them through `stream()`; the
 * application uses a stream to observe its state, receive SDUs and, on a
 * transmit stream, send them.
 *
 * The direction is seen from the local device: a `Tx` stream carries SDUs this
 * device sends, an `Rx` stream SDUs it receives. Broadcast and unicast client
 * streams know their direction when they are configured; a unicast server
 * stream gets it when the peer configures an ASE onto it (see onConfigured()).
 *
 * The payload is passed through unchanged (no LC3 encoding). `BLEAudioPlayer`
 * and `BLEAudioRecorder` layer the LC3 codec and I2S on top of a stream.
 *
 * Backend-agnostic: this header names no `esp_ble_audio_*` type. The stream
 * itself lives in the engine's stream pool (`BLEAudioEngineBap.h`); this
 * handle carries the pool slot and the application callbacks.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include <cstdint>
#include "BTStatus.h"
#include "BLEAudioTypes.h"

class BLEAudioStream {
public:
  /** @brief Data direction as seen from the local device. */
  enum class Direction : uint8_t {
    Unknown,  ///< Not configured yet (unicast server stream before the peer configures it).
    Tx,       ///< This device sends SDUs (unicast source ASE, or a broadcast source BIS).
    Rx,       ///< This device receives SDUs (unicast sink ASE, or a broadcast sink BIS).
  };

  /** @brief State change of @p stream (configured, started, SDU sent). */
  using Callback = std::function<void(BLEAudioStream &stream)>;
  /** @brief @p stream left the Streaming state; @p reason is the HCI or ASCS reason code. */
  using StoppedCallback = std::function<void(BLEAudioStream &stream, uint8_t reason)>;
  /**
   * @brief One SDU arrived on @p stream.
   * @param info Sequence number, timestamp and packet status.
   * @param sdu  Payload; only valid during the call. Empty when `info.status` is Lost.
   * @param len  Payload length in octets.
   */
  using ReceiveCallback = std::function<void(BLEAudioStream &stream, const BLEAudioSduInfo &info, const uint8_t *sdu, uint16_t len)>;

  /**
   * @brief Create an empty stream handle.
   *
   * Get a live one from a role's stream(); the empty handle reports Unknown /
   * defaults for state, refuses write() with InvalidParam and ignores every
   * callback setter.
   */
  BLEAudioStream();
  ~BLEAudioStream() = default;
  /** @brief Share the referenced stream (copying never duplicates the engine slot). */
  BLEAudioStream(const BLEAudioStream &) = default;
  /** @brief Share the referenced stream (copying never duplicates the engine slot). */
  BLEAudioStream &operator=(const BLEAudioStream &) = default;
  /** @brief Move the stream reference. */
  BLEAudioStream(BLEAudioStream &&) = default;
  /** @brief Move the stream reference. */
  BLEAudioStream &operator=(BLEAudioStream &&) = default;

  /** @brief Whether this handle references a stream (false for the handles returned on error). */
  explicit operator bool() const;
  /**
   * @brief Two handles are equal when they reference the same stream.
   * @param other Handle to compare with.
   * @return True when both reference the same engine slot (two empty handles compare equal).
   */
  bool operator==(const BLEAudioStream &other) const {
    return _impl == other._impl;
  }
  /**
   * @brief Two handles differ when they reference different streams.
   * @param other Handle to compare with.
   * @return True when the handles reference different engine slots.
   */
  bool operator!=(const BLEAudioStream &other) const {
    return _impl != other._impl;
  }

  // --- State ---

  /** @brief Direction of the stream.
   *
   * Unicast streams report Unknown until the endpoint is bound; broadcast
   * source streams are always Tx and broadcast sink streams always Rx, even
   * before the codec is configured.
   */
  Direction direction() const;
  /**
   * @brief Whether the stream is in the Streaming state (SDUs may flow).
   * @return True while streaming; false on an empty handle.
   */
  bool isStreaming() const;
  /**
   * @brief ACL connection of a unicast stream; 0xFFFF for broadcast or unbound streams.
   * @return The connection handle, or BLE_AUDIO_CONN_NONE (0xFFFF) for broadcast,
   *         unbound or empty handles.
   */
  uint16_t connHandle() const;
  /**
   * @brief Codec configuration in use; defaults until the stream is configured.
   * @return The negotiated codec, or a default BLEAudioCodecConfig on an empty handle.
   */
  BLEAudioCodecConfig codecConfig() const;
  /**
   * @brief QoS in use; defaults until QoS is configured.
   * @return The negotiated QoS, or a default BLEAudioQos on an empty handle.
   */
  BLEAudioQos qos() const;

  // --- Data ---

  /**
   * @brief Send one SDU on a streaming Tx stream.
   *
   * Call once per SDU interval (`qos().sduIntervalUs`) with
   * `codecConfig().sduOctets()` bytes. The sequence number is managed
   * internally and advances only when the controller accepted the SDU.
   *
   * @param sdu SDU payload (copied before the call returns).
   * @param len Payload length in octets.
   * @return BTStatus::OK when queued; InvalidParam for an empty payload or a
   *         null handle; InvalidState on an Rx stream or one that is not
   *         streaming; another error when the controller refused the SDU.
   */
  BTStatus write(const uint8_t *sdu, uint16_t len);

  // --- Callbacks (run on the Bluetooth host task; keep them short) ---
  //
  // Every setter below ignores a null handle; a later call replaces the callback.

  /**
   * @brief Codec configured: by the peer (unicast), at create (broadcast source) or at sync (broadcast sink).
   * @param cb Callback receiving the configured @p stream.
   */
  void onConfigured(Callback cb);
  /**
   * @brief The stream entered the Streaming state.
   * @param cb Callback receiving the started @p stream.
   */
  void onStarted(Callback cb);
  /**
   * @brief The stream left the Streaming state (disable, release, CIS/BIG loss).
   * @param cb Callback receiving the stopped @p stream and the HCI/ASCS @p reason.
   */
  void onStopped(StoppedCallback cb);
  /**
   * @brief An SDU arrived on an Rx stream.
   * @param cb Callback receiving the @p stream, @p info, @p sdu and @p len.
   */
  void onReceive(ReceiveCallback cb);
  /**
   * @brief The controller released a queued SDU of a Tx stream (one call per write()).
   * @param cb Callback receiving the @p stream whose SDU was released.
   */
  void onSent(Callback cb);
  /**
   * @brief Clear every callback of this stream.
   *
   * Leaves the data-path tap (BLEAudioPlayer/BLEAudioRecorder) untouched.
   */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioStream(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend struct BLEAudioStreamAccess;  // Role handles and the data path (BLEAudioStreamImpl.h).
};

#endif /* BLE_AUDIO_SUPPORTED */
