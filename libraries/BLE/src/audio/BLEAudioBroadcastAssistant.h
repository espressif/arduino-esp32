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
 * @brief BAP Broadcast Assistant: finds Auracast sources for a remote sink.
 *
 * The assistant (a phone or remote control) connects to a Broadcast Sink that
 * exposes BASS, scans on its behalf and tells it which broadcast to receive:
 * @code
 * assistant.discover(connHandle);   // after audio.onLinkReady()
 * assistant.startRemoteScan();      // onSourceFound() reports broadcasts
 * assistant.addSource(source);      // the sink syncs; onReceiveState() tracks it
 * @endcode
 * When the sink asks for SyncInfo and this device is synced to the source's
 * periodic advertising, the sync is transferred with PAST automatically.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <cstdint>
#include <functional>
#include <memory>
#include "BTAddress.h"
#include "BTStatus.h"
#include "WString.h"

class BLEAudio;

/** @brief A broadcast source seen while scanning. */
struct BLEAudioBroadcastSourceInfo {
  BTAddress address;        //!< Advertiser address (with its type)
  uint8_t sid = 0;          //!< Advertising SID of the periodic train
  uint32_t broadcastId = 0; //!< 24-bit Broadcast_ID
  String name;              //!< Broadcast Name, else the device name
  int8_t rssi = 0;          //!< dBm, from the report that found it
  uint16_t paInterval = 0xFFFF;  //!< Periodic advertising interval (1.25 ms units), 0xFFFF unknown
};

/** @brief A Broadcast Receive State of the remote sink. */
struct BLEAudioReceiveState {
  /** PA_Sync_State values defined by BASS. */
  enum class PaState : uint8_t {
    NotSynced,          //!< Not synchronized to the periodic advertising
    SyncInfoRequested,  //!< The sink asks the assistant for SyncInfo (PAST)
    Synced,             //!< Synchronized
    Failed,             //!< Synchronization failed
    NoPast,             //!< No PAST available; the sink must scan itself
  };
  /** BIG_Encryption values defined by BASS. */
  enum class Encryption : uint8_t {
    None,          //!< Not encrypted
    CodeRequired,  //!< Waiting for setBroadcastCode()
    Decrypting,    //!< Code accepted, decrypting
    BadCode,       //!< The code was wrong (see badCode)
  };
  static constexpr uint8_t kMaxSubgroups = 4;  //!< Size of bisSync

  uint8_t sourceId = 0;  //!< Source_ID assigned by the sink; used by modify/remove/code calls
  bool removed = false;  //!< The source was removed; only sourceId is meaningful
  BTAddress address;     //!< Source advertiser address
  uint8_t sid = 0;       //!< Source advertising SID
  uint32_t broadcastId = 0;
  PaState paState = PaState::NotSynced;
  Encryption encryption = Encryption::None;
  uint8_t badCode[16] = {};  //!< Code the sink rejected (Encryption::BadCode)
  uint8_t numSubgroups = 0;  //!< As reported; only the first kMaxSubgroups have a bisSync entry
  uint32_t bisSync[kMaxSubgroups] = {};  //!< BIS indexes synced, per subgroup (bit n = BIS n+1)
};

/**
 * @brief BAP Broadcast Assistant (BASS client), bound to one sink at a time.
 *
 * Created by BLEAudio::createBroadcastAssistant() after audio.begin().
 * Control point operations return once the write is queued; their outcome
 * arrives through onResult and the sink's state through onReceiveState.
 * Calls before discover() return BTStatus::NotConnected.
 */
class BLEAudioBroadcastAssistant {
public:
  /** Operation reported by onResult. */
  enum class Op : uint8_t {
    ScanStart,      //!< Remote Scan Started write
    ScanStop,       //!< Remote Scan Stopped write
    AddSource,      //!< Add Source write
    ModifySource,   //!< Modify Source write
    RemoveSource,   //!< Remove Source write
    BroadcastCode,  //!< Set Broadcast_Code write
    Past,           //!< SyncInfo transfer requested by the sink
  };

  /** @brief Discovery result, with the number of Broadcast Receive State characteristics. */
  using DiscoverCallback = std::function<void(BTStatus status, uint8_t receiveStates)>;
  /** @brief A broadcast found by the remote scan. */
  using SourceFoundCallback = std::function<void(const BLEAudioBroadcastSourceInfo &source)>;
  /** @brief A Broadcast Receive State was read or notified. */
  using ReceiveStateCallback = std::function<void(const BLEAudioReceiveState &state)>;
  /** @brief Outcome of @p op. */
  using ResultCallback = std::function<void(Op op, BTStatus status)>;

  BLEAudioBroadcastAssistant();
  ~BLEAudioBroadcastAssistant() = default;
  BLEAudioBroadcastAssistant(const BLEAudioBroadcastAssistant &) = default;
  BLEAudioBroadcastAssistant &operator=(const BLEAudioBroadcastAssistant &) = default;

  /** @return true when the handle refers to a created assistant. */
  explicit operator bool() const;

  /** @brief Discover the sink's BASS on @p connHandle; later calls act on this link. */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /**
   * @brief Tell the sink we scan for it and start a passive extended scan.
   *
   * Uses `BLE.getScan()` (stopping any scan in progress); each broadcast is
   * reported once per remote scan through onSourceFound().
   */
  BTStatus startRemoteScan();
  /** @brief Stop the local scan and tell the sink the remote scan ended. */
  BTStatus stopRemoteScan();

  /**
   * @brief Ask the sink to receive @p source.
   * @param syncPa Sync to the source's periodic advertising.
   * @param bisMask BIS indexes to sync (bit n = BIS n+1); 0xFFFFFFFF lets the sink choose.
   */
  BTStatus addSource(const BLEAudioBroadcastSourceInfo &source, bool syncPa = true, uint32_t bisMask = 0xFFFFFFFF);
  /** @brief Change PA / BIS sync of a source (e.g. syncPa = false, bisMask = 0 to pause). */
  BTStatus modifySource(uint8_t sourceId, bool syncPa, uint32_t bisMask);
  /**
   * @brief Ask the sink to forget @p sourceId.
   *
   * BASS lets the sink refuse while it is still synced; unsync first with
   * modifySource(sourceId, false, 0).
   */
  BTStatus removeSource(uint8_t sourceId);
  /** @brief Send the Broadcast Code (up to 16 characters) of an encrypted source. */
  BTStatus setBroadcastCode(uint8_t sourceId, const String &code);

  /** @brief BASS discovery finished. */
  BLEAudioBroadcastAssistant &onDiscovered(DiscoverCallback cb);
  /** @brief A broadcast was found during the remote scan. */
  BLEAudioBroadcastAssistant &onSourceFound(SourceFoundCallback cb);
  /** @brief The sink's receive state for a source changed (or was read at discovery). */
  BLEAudioBroadcastAssistant &onReceiveState(ReceiveStateCallback cb);
  /** @brief Outcome of each control point operation. */
  BLEAudioBroadcastAssistant &onResult(ResultCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioBroadcastAssistant(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
