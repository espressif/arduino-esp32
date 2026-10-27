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
 * @brief BAP Unicast Client role: sets up streams on one or two unicast servers.
 *
 * The unicast client is the initiator side of a unicast audio link (a phone,
 * PC or dongle). It discovers what a connected server supports (PACS + ASCS)
 * and then runs the ASCS procedure on the server's endpoints: Config Codec,
 * Config QoS, Enable, CIS connect and Receiver Start Ready. Each stream
 * reports its own progress through `BLEAudioStream` callbacks; the client
 * reports the aggregate (discovered, all streaming, stopped, error).
 *
 * Single peer: `connect(connHandle)` discovers the peer once its GATT
 * database is known and starts the streams right away.
 *
 * Two peers (e.g. left + right earbuds, TMAP 2P_2CIS): call addPeer() for
 * each, wait for both onDiscovered() callbacks, then start(). Each peer gets
 * `sinkStreams + sourceStreams` streams, one per endpoint, and each stream
 * takes the next Audio Location the peer published (left, then right...).
 * A sink and a source stream to the same peer share one bidirectional CIS.
 *
 * @code
 * audio.connect(serverAddress);
 * audio.onLinkReady([](uint16_t conn) { client.connect(conn); });
 * client.onStarted([] { Serial.println("streaming"); });
 * // loop(): client.stream(BLEAudioStream::Direction::Tx).write(sdu, len);
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "BTStatus.h"
#include "audio/BLEAudioTypes.h"
#include "audio/BLEAudioStream.h"

class BLEAudio;

/** @brief What a unicast server exposes, as discovered by the client (PACS + ASCS). */
struct BLEAudioUnicastPeerInfo {
  uint16_t connHandle = 0xFFFF;  ///< ACL connection of the server.
  uint8_t sinkEndpoints = 0;     ///< Remote sink ASEs: streams this device can send.
  uint8_t sourceEndpoints = 0;   ///< Remote source ASEs: streams this device can receive.
  BLEAudioLocation sinkLocation = BLEAudioLocation::Mono;    ///< Locations the server renders.
  BLEAudioLocation sourceLocation = BLEAudioLocation::Mono;  ///< Locations the server captures.
  BLEAudioContext sinkContexts = BLEAudioContext::Prohibited;    ///< Contexts available for sink streams now.
  BLEAudioContext sourceContexts = BLEAudioContext::Prohibited;  ///< Contexts available for source streams now.
  /** Presets the server's sink PAC records accept (BLEAudioPresetBit() mask): what this device can send. */
  uint32_t sinkPresets = 0;
  /** Presets the server's source PAC records accept (BLEAudioPresetBit() mask): what this device can receive. */
  uint32_t sourcePresets = 0;
};

class BLEAudioUnicastClient {
public:
  /** @brief Stream setup step that failed (see onError()). */
  enum class Step : uint8_t {
    Config = 1,  ///< ASCS Config Codec.
    Qos,         ///< ASCS Config QoS.
    Enable,      ///< ASCS Enable.
    Start,       ///< ASCS Receiver Start Ready.
    Connect,     ///< CIS creation.
    Group,       ///< CIG creation (controller rejected the QoS / stream set).
  };

  /** @brief A peer finished discovery; @p peer lists what it supports. */
  using DiscoveredCallback = std::function<void(const BLEAudioUnicastPeerInfo &peer)>;
  using Callback = std::function<void()>;
  /**
   * @brief Stream setup failed at @p step; the client then releases every stream.
   * @param responseCode ASCS response code from the server, or 0 when the
   *                     local request failed (CIG/CIS creation included).
   * @param reason       ASCS reason from the server, or 0 for local failures.
   */
  using ErrorCallback = std::function<void(Step step, uint8_t responseCode, uint8_t reason)>;

  BLEAudioUnicastClient();
  ~BLEAudioUnicastClient() = default;
  BLEAudioUnicastClient(const BLEAudioUnicastClient &) = default;
  BLEAudioUnicastClient &operator=(const BLEAudioUnicastClient &) = default;

  /** @brief Whether this handle references a client (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (read at start(); change it while stopped) ---

  /** @brief Codec and QoS of every stream, from a BAP preset. Default LC3_16_2_1. */
  BLEAudioUnicastClient &setPreset(BLEAudioCodecPreset preset);
  /**
   * @brief Replace the preset's QoS with a custom one.
   *
   * Without it, the preset QoS is used with the controller's presentation
   * delay. `maxSdu` is always recomputed per stream from its codec.
   */
  BLEAudioUnicastClient &setQos(const BLEAudioQos &qos);
  /** @brief Streaming context sent with Enable. Default Media. */
  BLEAudioUnicastClient &setContext(BLEAudioContext context);
  /** @brief Streams this device sends, per peer (remote sink ASEs). Default 1. */
  BLEAudioUnicastClient &setSinkStreams(uint8_t perPeer);
  /** @brief Streams this device receives, per peer (remote source ASEs). Default 0. */
  BLEAudioUnicastClient &setSourceStreams(uint8_t perPeer);

  // --- Procedures (after audio.start()) ---

  /**
   * @brief Add a connected server and discover its PACS and ASCS.
   *
   * When the peer's GATT database is not known yet, discovery starts on its
   * own once `BLEAudio::onLinkReady()` fires for @p connHandle. The result is
   * reported by onDiscovered(). Adding a known peer again is a no-op.
   *
   * @return BTStatus::OK when discovery started or is pending; InvalidState
   *         before audio.start(); another error when discovery failed to start.
   */
  BTStatus addPeer(uint16_t connHandle);
  /**
   * @brief Configure and start streams on every discovered peer.
   *
   * Per peer, requests at most as many streams as the peer has endpoints.
   * Non-blocking: onStarted() fires once every stream streams, onError() if
   * a step fails.
   *
   * @return BTStatus::OK when the setup started; InvalidState when already
   *         running or when no discovered peer has a usable endpoint;
   *         NoMemory when the stream pool is exhausted.
   */
  BTStatus start();
  /** @brief addPeer(), then start() as soon as the peer is discovered. */
  BTStatus connect(uint16_t connHandle);
  /**
   * @brief Release every stream and remove the CIG (onStopped() follows).
   * @return BTStatus::OK when the release started; InvalidState when not running.
   */
  BTStatus stop();
  /** @brief Whether every requested stream is streaming (between onStarted() and onStopped()). */
  bool isStreaming() const;

  // --- Streams (valid once start() returned OK) ---

  /** @brief Streams of the current setup, in request order (per peer: sink streams, then source). */
  size_t streamCount() const;
  /** @brief Stream @p index in [0, streamCount()); empty handle when out of range. */
  BLEAudioStream stream(size_t index) const;
  /**
   * @brief The @p index-th stream in direction @p dir (Tx = to a remote sink).
   * @return An empty handle when there are fewer streams in @p dir.
   */
  BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief A peer finished PACS/ASCS discovery. */
  BLEAudioUnicastClient &onDiscovered(DiscoveredCallback cb);
  /** @brief Every stream of the setup is streaming. */
  BLEAudioUnicastClient &onStarted(Callback cb);
  /** @brief Every stream was released (after stop() or a failure). */
  BLEAudioUnicastClient &onStopped(Callback cb);
  /** @brief A setup step failed; onStopped() follows once the streams are released. */
  BLEAudioUnicastClient &onError(ErrorCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioUnicastClient(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
