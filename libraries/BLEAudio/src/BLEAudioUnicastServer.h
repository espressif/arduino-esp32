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
 * @brief BAP Unicast Server role: publishes PACS + ASCS and accepts streams.
 *
 * The unicast server is the acceptor side of a unicast audio link (a headset,
 * earbud or speaker). It publishes what it can render and capture (PACS) and
 * a set of Audio Stream Endpoints (ASCS); a remote unicast client then
 * configures, enables and starts streams on those endpoints. The server never
 * initiates a stream itself.
 *
 * The server owns `sinkStreams + sourceStreams` streams. When the client
 * configures an ASE, the engine binds it to a free stream, which fixes that
 * stream's direction: a sink ASE (the client sends) becomes an `Rx` stream
 * here, a source ASE (the client receives) a `Tx` stream. Register
 * `BLEAudioStream::onConfigured()` on every stream, or look them up with
 * stream(dir, index) once configured.
 *
 * @code
 * BLEAudioUnicastServer server = audio.createUnicastServer();
 * server.setSinkStreams(1);
 * server.setSourceStreams(1);
 * server.setSupportedPresets(BLEAudioPresetBit(BLEAudioCodecPreset::LC3_16_2_1));
 * for (size_t i = 0; i < server.streamCount(); i++) {
 *   server.stream(i).onReceive(onAudio);
 * }
 * audio.begin();
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include "BTStatus.h"
#include "BLEAudioTypes.h"
#include "BLEAudioStream.h"

class BLEAudio;

class BLEAudioUnicastServer {
public:
  /**
   * @brief Create an empty server handle.
   *
   * Get a live one from BLEAudio::createUnicastServer() while the engine is active and
 * before audio.begin(); the empty handle reports InvalidState from every setter and
   * empty stream from every stream().
   */
  BLEAudioUnicastServer();
  ~BLEAudioUnicastServer() = default;
  /** @brief Share the referenced server (copying never duplicates the engine registration). */
  BLEAudioUnicastServer(const BLEAudioUnicastServer &) = default;
  /** @brief Share the referenced server (copying never duplicates the engine registration). */
  BLEAudioUnicastServer &operator=(const BLEAudioUnicastServer &) = default;
  /** @brief Move the server reference. */
  BLEAudioUnicastServer(BLEAudioUnicastServer &&) = default;
  /** @brief Move the server reference. */
  BLEAudioUnicastServer &operator=(BLEAudioUnicastServer &&) = default;

  /** @brief Whether this handle references a server (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (before audio.begin(); later calls return InvalidState) ---
  //
  // Every setter below returns BTStatus::OK on success, InvalidState on an
  // empty handle or after audio.begin(), and InvalidParam for an invalid value
  // (the stored value is left unchanged).

  /**
   * @brief Number of sink ASEs to publish (streams the client sends to).
   * @param count 0 disables the sink direction. Default 1; capped by the
   *              packaged Kconfig ASE count (the stream pool logs when full).
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin().
   */
  BTStatus setSinkStreams(uint8_t count);
  /**
   * @brief Number of source ASEs to publish (streams the client receives).
   * @param count 0 disables the source direction. Default 1; same cap as setSinkStreams().
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin().
   */
  BTStatus setSourceStreams(uint8_t count);
  /**
   * @brief LC3 presets advertised in PACS, as a mask of BLEAudioPresetBit().
   *
   * The server publishes one PAC record covering the union of the presets
   * (rates, durations, octet range), so a client may pick any combination
   * inside it. Default: every 10 ms preset at 16, 24, 32 and 48 kHz. A zero
   * mask keeps the current value.
   * @param presetMask OR of BLEAudioPresetBit() values; 0 keeps the current mask.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin(); InvalidParam for a zero mask.
   */
  BTStatus setSupportedPresets(uint32_t presetMask);
  /**
   * @brief Channels one ASE may carry: 1 (default) or 2.
   * @param channels 1 = mono, 2 = mono or stereo per ASE.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin(); InvalidParam for a value other than 1 or 2.
   */
  BTStatus setMaxChannelsPerStream(uint8_t channels);
  /**
   * @brief Contexts the sink supports (PACS "Supported Audio Contexts"); also the initial available set.
   * @param contexts Bitfield of BLEAudioContext values the sink renders.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin().
   */
  BTStatus setSinkContexts(BLEAudioContext contexts);
  /**
   * @brief Contexts the source supports; also the initial available set.
   * @param contexts Bitfield of BLEAudioContext values the source captures.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin().
   */
  BTStatus setSourceContexts(BLEAudioContext contexts);
  /**
   * @brief Sink Audio Locations published in PACS (default Mono).
   * @param location Union of the speaker positions the sink renders.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin().
   */
  BTStatus setSinkLocation(BLEAudioLocation location);
  /**
   * @brief Source Audio Locations published in PACS (default Mono).
   * @param location Union of the positions the source captures.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin().
   */
  BTStatus setSourceLocation(BLEAudioLocation location);
  /**
   * @brief Presentation delay range the server supports (ASCS QoS preferences).
   * @param minUs Minimum delay (default 20000 us).
   * @param maxUs Maximum delay; 0 (default) uses the controller's
   *              BLEAudio::setPresentationDelay() value.
   * @return BTStatus::OK on success; InvalidState on an empty handle or after
   *         audio.begin(); InvalidParam when @p maxUs is nonzero and @p minUs
   *         exceeds it.
   */
  BTStatus setPresentationDelayRange(uint32_t minUs, uint32_t maxUs);

  /**
   * @brief Configure the usual speaker shape: one stereo-capable sink ASE.
   *
   * Shorthand for:
   * @code
   * setSinkStreams(1);
   * setSourceStreams(0);
   * setMaxChannelsPerStream(2);
   * setSupportedPresets(LC3_16_2_1 | LC3_24_2_1 | LC3_48_2_1);
   * setSinkContexts(Media | Conversational);
   * setSinkLocation(FrontLeft | FrontRight);
   * @endcode
   *
   * The single ASE accepts mono or stereo, so a phone can send one
   * interleaved stream instead of one stream per speaker. Call it after
   * createUnicastServer() and before audio.begin(); individual setters can
   * still override any field afterwards.
   * @return BTStatus::OK on success; the first failing setter's status
   *         otherwise (InvalidState on an empty handle or after audio.begin()).
   */
  BTStatus configureMediaSink();

  /**
   * @brief Configure the usual headset shape: one sink ASE and one source ASE.
   *
   * Shorthand for:
   * @code
   * setSinkStreams(1);
   * setSourceStreams(1);
   * setMaxChannelsPerStream(2);
   * setSupportedPresets(LC3_16_2_1 | LC3_24_2_1 | LC3_48_2_1);
   * setSinkContexts(Media | Conversational);
   * setSourceContexts(Conversational | Media);
   * setSinkLocation(FrontLeft | FrontRight);
   * setSourceLocation(Mono);
   * @endcode
   *
   * The sink ASE renders phone media and call audio; the source ASE carries
   * the headset microphone back to the phone. Call it after
   * createUnicastServer() and before audio.begin(); individual setters can
   * still override any field afterwards.
   * @return BTStatus::OK on success; the first failing setter's status
   *         otherwise (InvalidState on an empty handle or after audio.begin()).
   */
  BTStatus configureHeadset();

  // --- Runtime (after audio.begin()) ---

  /**
   * @brief Change the contexts currently available (PACS "Available Audio Contexts").
   *
   * A client only enables streams for an available context; publish
   * `Prohibited` while busy (e.g. in a call on another link).
   * @param sink Contexts currently available on the sink ASEs.
   * @param source Contexts currently available on the source ASEs.
   * @return BTStatus::OK on success; InvalidState before audio.begin().
   */
  BTStatus setAvailableContexts(BLEAudioContext sink, BLEAudioContext source);

  // --- Streams ---

  /** @brief Streams owned by the server (sink + source count, fewer if the pool ran out). */
  size_t streamCount() const;
  /**
   * @brief Stream @p index in [0, streamCount()); empty handle when out of range.
   * @param index Position in allocation order (sink streams first, then source).
   * @return The stream, or an empty handle when @p index is out of range.
   */
  BLEAudioStream stream(size_t index) const;
  /**
   * @brief The @p index-th stream a client configured in direction @p dir.
   * @param dir Tx = stream the client receives from here (source ASE); Rx = stream
   *            the client sends to here (sink ASE).
   * @param index Position among the streams configured in @p dir.
   * @return An empty handle when fewer streams are configured in @p dir.
   */
  BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

  struct Impl;

private:
  explicit BLEAudioUnicastServer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
