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
 * @brief BAP Broadcast Sink role (Auracast receiver) with optional Scan Delegator.
 *
 * Joining a broadcast takes three steps, each reported by a callback:
 *  1. Scan: start() scans for Broadcast Audio Announcements (onSourceFound()).
 *  2. PA sync: sync to the source's periodic advertising (onSynced()), which
 *     carries the BASE describing the BISes (onBaseReceived()).
 *  3. BIG sync: join the BISes that match the sink's PAC (onStarted()); each
 *     joined BIS feeds one Rx stream.
 *
 * With auto-sync (default) the first source matching the target name and/or
 * Broadcast ID is synced automatically; without it, pick one from
 * onSourceFound() and call syncTo(). With the Scan Delegator (BASS) enabled,
 * a Broadcast Assistant such as a phone can also select the source and hand
 * over the Broadcast Code, including over PAST on a connection.
 *
 * @code
 * BLEAudioBroadcastSink sink = audio.createBroadcastSink();
 * sink.setStreams(2).setTargetName("Living room");
 * sink.stream(0).onReceive(onLeft);
 * sink.stream(1).onReceive(onRight);
 * audio.start();
 * sink.start();
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"
#include "BLEAudioTypes.h"
#include "BLEAudioStream.h"
#include "BLEAudioBroadcastAssistant.h"

class BLEAudio;

/** @brief Summary of the BASE (Broadcast Audio Source Endpoint) of the synced source. */
struct BLEAudioBroadcastBaseInfo {
  uint32_t bisMask = 0;              ///< BIT(index - 1) for every BIS in the BASE.
  uint8_t subgroups = 0;             ///< Subgroups in the BASE.
  uint32_t presentationDelayUs = 0;  ///< Presentation delay the source requests.
  BLEAudioCodecConfig codec;         ///< Codec of the first subgroup.
};

class BLEAudioBroadcastSink {
public:
  /** @brief A broadcast source was seen while scanning. */
  using SourceFoundCallback = std::function<void(const BLEAudioBroadcastSourceInfo &source)>;
  /** @brief The BASE of the synced source was received. */
  using BaseCallback = std::function<void(const BLEAudioBroadcastBaseInfo &base)>;
  /**
   * @brief A state change with no payload (synced, BIG joined).
   *
   * Runs on the Bluetooth host task; keep it short.
   */
  using Callback = std::function<void()>;
  /**
   * @brief Sync lost or BIG left; @p reason is the HCI reason code.
   *
   * Runs on the Bluetooth host task; keep it short.
   */
  using ReasonCallback = std::function<void(uint8_t reason)>;
  /**
   * @brief Joining the BIG failed with @p status.
   *
   * NotFound: no BIS matches the sink PAC; PermissionDenied: the broadcast is
   * encrypted and no (or a wrong) Broadcast Code is set. Runs on the Bluetooth
   * host task; keep it short.
   */
  using ErrorCallback = std::function<void(BTStatus status)>;

  BLEAudioBroadcastSink();
  ~BLEAudioBroadcastSink() = default;
  BLEAudioBroadcastSink(const BLEAudioBroadcastSink &) = default;
  BLEAudioBroadcastSink &operator=(const BLEAudioBroadcastSink &) = default;
  BLEAudioBroadcastSink(BLEAudioBroadcastSink &&) = default;
  BLEAudioBroadcastSink &operator=(BLEAudioBroadcastSink &&) = default;

  /** @brief Whether this handle references a sink (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (before audio.start(); every setter returns this handle) ---
  //
  // Setters are no-op chain links on an empty handle. Only setStreams() is
  // gated on start() (ignored with a warning afterwards); the other staged
  // values have no effect once registered — use the live source-selection
  // setters below for changes after start().

  /**
   * @brief Number of BISes to join: 1 = mono (default), 2 = stereo.
   *
   * Clamped to 1..2 and capped by the packaged Kconfig BIS count. Streams
   * are (re)allocated immediately, so attach stream callbacks after calling it.
   * @param count 1 = mono (one BIS), 2 = stereo (two BISes).
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setStreams(uint8_t count);
  /**
   * @brief LC3 presets advertised in the sink PAC (BLEAudioPresetBit() mask).
   *
   * Only BISes whose codec fits this PAC can be joined. Default: 16 and
   * 24 kHz (10 ms) plus every 48 kHz preset. A zero mask is ignored.
   * @param presetMask OR of BLEAudioPresetBit() values; 0 keeps the current mask.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setSupportedPresets(uint32_t presetMask);
  /**
   * @brief Channels one BIS may carry in the sink PAC: 1 or 2 (default 2); clamped to that range.
   * @param channels 1 = mono, 2 = mono or stereo per BIS.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setMaxChannelsPerStream(uint8_t channels);
  /**
   * @brief Sink Audio Locations published in PACS (default Mono).
   * @param location Union of the speaker positions the sink renders.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setLocation(BLEAudioLocation location);
  /**
   * @brief Sink contexts published in PACS (default Unspecified | Media).
   * @param contexts Bitfield of BLEAudioContext values the sink renders.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setContexts(BLEAudioContext contexts);
  /**
   * @brief Publish BASS so Broadcast Assistants can control the sink. Default on when compiled in.
   * @param enable True to publish the Scan Delegator (BASS) role.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setScanDelegator(bool enable);

  // --- Source selection (any time) ---

  /**
   * @brief Broadcast Code (up to 16 characters) for encrypted broadcasts.
   *
   * Empty (default): only unencrypted broadcasts can be joined, unless an
   * assistant provides the code. Takes effect immediately after audio.start().
   * @param code Longer codes are truncated with a warning and zero-padded per spec.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setBroadcastCode(const String &code);
  /**
   * @brief Only auto-sync to sources with this Broadcast Name (empty = any).
   * @param name Name filter; empty disables the name filter.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setTargetName(const String &name);
  /**
   * @brief Only auto-sync to this 24-bit Broadcast ID (0 = any).
   * @param broadcastId Only the low 24 bits are kept.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setTargetBroadcastId(uint32_t broadcastId);
  /**
   * @brief Preferred BIS indexes as BIT(index - 1).
   * @param mask 0 (default) joins the lowest indexes in the BASE.
   *
   * Takes effect at the next BIG sync; safe to call any time (including on an
   * empty handle, where it is a no-op).
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setBisMask(uint32_t mask);
  /**
   * @brief Sync to the first matching source automatically. Default on.
   * @param enable False to pick sources manually with syncTo().
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &setAutoSync(bool enable);

  /**
   * @brief Configure a ready-to-start Auracast receiver.
   *
   * Shorthand for:
   * @code
   * setStreams(streams)
   *   .setLocation(streams == 2 ? FrontLeft | FrontRight : Mono)
   *   .setContexts(Media)
   *   .setTargetName(targetName)
   *   .setAutoSync(true);
   * @endcode
   *
   * @param targetName Only sync to this Broadcast Name; empty (default) syncs
   *                   to the first source found. With a name set, the sink
   *                   ignores other broadcasts in range.
   * @param streams    1 = mono (one BIS), 2 = stereo (two BISes). Default 2.
   *
   * Call it after createBroadcastSink() and before audio.start(), then
   * start() to begin scanning. The Scan Delegator stays enabled when
   * compiled in, so a phone or Broadcast Assistant can also select a source.
   * Encryption stays opt-in: call setBroadcastCode() separately.
   */
  BLEAudioBroadcastSink &configureAuracast(const String &targetName = "", uint8_t streams = 2);

  // --- Control (after audio.start()) ---

  /**
   * @brief Start scanning for sources (no-op when already synced).
   *
   * Scanning continues until a PA sync is established and restarts on its
   * own when the sync is lost, until stop().
   *
   * @return BTStatus::OK when scanning; InvalidState before audio.start();
   *         another error when the scan could not start.
   */
  BTStatus start();
  /**
   * @brief Sync to @p source (from onSourceFound()); use with setAutoSync(false).
   * @param source Source report to sync to (address, SID and Broadcast ID are used).
   * @return BTStatus::OK when the PA sync is being created; InvalidState
   *         before audio.start() or while a sync is pending or established.
   */
  BTStatus syncTo(const BLEAudioBroadcastSourceInfo &source);
  /**
   * @brief Leave the BIG, drop the PA sync and stop scanning.
   *
   * Blocks until the BIG is down so a following `audio.end()` / `BLE.end()`
   * does not tear the host down over a live ISO channel. Called from a
   * Bluetooth stack callback, stop() is refused with Busy because it must wait
   * for the asynchronous BIG leave; defer it to loop().
   * @return BTStatus::OK on success; InvalidState on an empty handle; Busy from
   *         a stack callback; another error (engine status, Timeout) when the
   *         teardown failed or stalled.
   */
  BTStatus stop();
  /**
   * @brief Whether the BIG is joined (between onStarted() and onStopped()).
   * @return True while the BIG streams; false on an empty handle.
   */
  bool isStreaming() const;

  // --- Streams (one Rx stream per joined BIS) ---

  /** @brief Number of streams (= setStreams() count, fewer if the pool ran out). */
  size_t streamCount() const;
  /**
   * @brief Stream @p index in [0, streamCount()); empty handle when out of range.
   * @param index BIS position in join order.
   * @return The stream, or an empty handle when @p index is out of range.
   */
  BLEAudioStream stream(size_t index) const;

  // --- Callbacks (Bluetooth host task; keep them short) ---
  //
  // Every setter below takes the callback in @p cb and returns this handle for
  // chaining; a no-op chain link on an empty handle.

  /** @brief A broadcast source was seen while scanning (repeats per advertising report).
   *  @param cb Callback receiving each matching source @p report.
   *  @return This handle, for chaining. */
  BLEAudioBroadcastSink &onSourceFound(SourceFoundCallback cb);
  /** @brief Periodic advertising sync established; the BASE and BIG sync follow.
   *  @param cb Callback fired once the PA sync is established.
   *  @return This handle, for chaining. */
  BLEAudioBroadcastSink &onSynced(Callback cb);
  /**
   * @brief The BASE of the synced source was received (once per PA sync).
   *
   * Fires before the BIG sync, so setBisMask() may still pick the BISes to join.
   * @param cb Callback receiving the BASE @p summary.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &onBaseReceived(BaseCallback cb);
  /**
   * @brief A Broadcast Assistant provided the Broadcast Code (Scan Delegator).
   *
   * The code replaces the one from setBroadcastCode() from the next BIG sync attempt.
   * @param cb Callback fired once the code arrives.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &onBroadcastCodeReceived(Callback cb);
  /**
   * @brief Periodic advertising sync lost; scanning resumes while started.
   * @param cb Callback receiving the HCI loss @p reason.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &onSyncLost(ReasonCallback cb);
  /**
   * @brief Joining the BIG failed.
   *
   * NotFound: no BIS matches the sink PAC; PermissionDenied: the broadcast is
   * encrypted and no (or a wrong) Broadcast Code is set.
   * @param cb Callback receiving the failure @p status.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &onSyncFailed(ErrorCallback cb);
  /**
   * @brief The BIG is joined: every stream is streaming.
   * @param cb Callback fired once the BIG streams.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &onStarted(Callback cb);
  /**
   * @brief The BIG was left (stop(), source stopped, or sync lost).
   * @param cb Callback receiving the HCI leave @p reason.
   * @return This handle, for chaining.
   */
  BLEAudioBroadcastSink &onStopped(ReasonCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioBroadcastSink(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
