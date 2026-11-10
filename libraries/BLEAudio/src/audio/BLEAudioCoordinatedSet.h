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
 * @brief Coordinated Set Identification Profile (CSIP) roles.
 *
 * A coordinated set is a group of devices that act as one (the two earbuds
 * of a pair). Every member shares a secret SIRK and advertises an RSI
 * derived from it, so a client that knows one member can find the others.
 *
 *  - `BLEAudioCoordinatedSetMember` (CSIS server): publishes the SIRK, set
 *    size and rank. A CAP acceptor owns one of these for its CAS-included
 *    instance (`BLEAudioCapAcceptor::coordinatedSet()`); create a standalone
 *    member only for devices that are not CAP acceptors.
 *  - `BLEAudioCoordinatedSetCoordinator` (CSIP client): reads the set
 *    information of connected members, recognizes other members from their
 *    advertising and locks the whole set for exclusive use.
 *
 * @code
 * // Member: advertise the RSI next to the other advertising data.
 * uint8_t rsi[BLE_AUDIO_RSI_SIZE];
 * if (set.generateRsi(rsi)) {
 *   uint8_t ad[2 + BLE_AUDIO_RSI_SIZE] = {1 + BLE_AUDIO_RSI_SIZE, 0x2E};
 *   memcpy(ad + 2, rsi, sizeof(rsi));
 *   advData.addRaw(ad, sizeof(ad));
 * }
 * // Coordinator: in the scan callback, connect to the other members.
 * if (coordinator.isSetMember(dev.getPayload(), dev.getPayloadLength())) { ... }
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"

class BLEAudio;
class BLEAudioCapAcceptor;

static constexpr size_t BLE_AUDIO_SIRK_SIZE = 16;  ///< Octets in a SIRK.
static constexpr size_t BLE_AUDIO_RSI_SIZE = 6;    ///< Octets in an RSI.

/**
 * @brief CSIP Set Member (CSIS server).
 *
 * Created by BLEAudio::createCoordinatedSetMember() between audio.begin()
 * and audio.start(), which registers it.
 */
class BLEAudioCoordinatedSetMember {
public:
  /** @brief Answer to a client reading the SIRK (see onSirkRead()). */
  enum class SirkAccess : uint8_t {
    Accept = 0,       ///< Send the SIRK in plain text.
    AcceptEncrypted,  ///< Send the SIRK encrypted with the link key.
    Reject,           ///< Refuse the read.
    OobOnly,          ///< The SIRK is only available out of band.
  };

  /** @brief The set lock changed; @p connHandle is the client that (un)locked it. */
  using LockCallback = std::function<void(bool locked, uint16_t connHandle)>;
  /** @brief A client reads the SIRK; return how to answer. */
  using SirkReadCallback = std::function<SirkAccess(uint16_t connHandle)>;

  BLEAudioCoordinatedSetMember() = default;
  ~BLEAudioCoordinatedSetMember() = default;
  BLEAudioCoordinatedSetMember(const BLEAudioCoordinatedSetMember &) = default;
  BLEAudioCoordinatedSetMember &operator=(const BLEAudioCoordinatedSetMember &) = default;

  /** @brief Whether this handle references a member (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (read at audio.start(); SIRK, size, rank and name also update a running set) ---

  /** @brief Set Identity Resolving Key shared by every member. Default: a fixed demo SIRK; products must set their own. */
  BLEAudioCoordinatedSetMember &setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]);
  /** @brief Number of devices in the set. Default 2. */
  BLEAudioCoordinatedSetMember &setSetSize(uint8_t size);
  /**
   * @brief Position of this device in the set, 1..size (lock order). Default 1.
   * @note On a running set the stack only accepts a new rank together with a new
   *       set size: call setRank() first, then setSetSize(). Otherwise the rank
   *       is used at the next audio.start().
   */
  BLEAudioCoordinatedSetMember &setRank(uint8_t rank);
  /** @brief Whether clients may lock the set. Default true. Only read at audio.start(). */
  BLEAudioCoordinatedSetMember &setLockable(bool lockable);
  /** @brief Optional set name exposed to clients. */
  BLEAudioCoordinatedSetMember &setName(const String &name);

  // --- Runtime (after audio.start()) ---

  /** @brief Generate a fresh Resolvable Set Identifier to advertise (AD type 0x2E). */
  BTStatus generateRsi(uint8_t rsi[BLE_AUDIO_RSI_SIZE]) const;
  /** @brief Lock the set locally, as a client would. */
  BTStatus lock();
  /** @brief Release the lock; @p force also releases a lock held by a client. */
  BTStatus unlock(bool force = false);
  /** @return true while the set is locked, locally or by a client. */
  bool isLocked() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Lock changes, local or remote. */
  BLEAudioCoordinatedSetMember &onLockChanged(LockCallback cb);
  /** @brief Without a callback every read is accepted. */
  BLEAudioCoordinatedSetMember &onSirkRead(SirkReadCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCoordinatedSetMember(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend class BLEAudioCapAcceptor;
};

/** @brief Set information read from one member. */
struct BLEAudioCoordinatedSetInfo {
  uint16_t connHandle = 0xFFFF;  ///< Member the information was read from.
  uint8_t setCount = 0;          ///< CSIS instances on the member (0: not in a set).
  uint8_t setSize = 0;           ///< Devices in the set.
  uint8_t rank = 0;              ///< Position of this member in the set.
  bool lockable = false;         ///< The set can be locked.
  uint8_t sirk[BLE_AUDIO_SIRK_SIZE] = {};  ///< SIRK read from the member.
};

/**
 * @brief CSIP Set Coordinator (CSIP client).
 *
 * Created by BLEAudio::createCoordinatedSetCoordinator() between
 * audio.begin() and audio.start().
 */
class BLEAudioCoordinatedSetCoordinator {
public:
  /** @brief Set information read from one member (setCount 0 when @p status is an error). */
  using DiscoveredCallback = std::function<void(BTStatus status, const BLEAudioCoordinatedSetInfo &info)>;
  /** @brief lock() (@p locked true) or unlock() finished on every member. */
  using LockCallback = std::function<void(BTStatus status, bool locked)>;
  /** @brief A member reported a lock change made by another client. */
  using LockChangedCallback = std::function<void(bool locked, uint16_t connHandle)>;

  BLEAudioCoordinatedSetCoordinator() = default;
  ~BLEAudioCoordinatedSetCoordinator() = default;
  BLEAudioCoordinatedSetCoordinator(const BLEAudioCoordinatedSetCoordinator &) = default;
  BLEAudioCoordinatedSetCoordinator &operator=(const BLEAudioCoordinatedSetCoordinator &) = default;

  /** @brief Whether this handle references a coordinator (false when the factory failed). */
  explicit operator bool() const;

  // --- Procedures (after audio.start()) ---

  /**
   * @brief Read the set information of a connected member.
   *
   * Starts once `BLEAudio::onLinkReady()` fired for @p connHandle; the
   * result is reported by onDiscovered(). The first member discovered
   * defines the set used by isSetMember(), lock() and unlock().
   */
  BTStatus discover(uint16_t connHandle);
  /** @brief Whether an advertising payload carries an RSI of the discovered set. */
  bool isSetMember(const uint8_t *advPayload, size_t len) const;
  /** @brief Lock every connected, discovered member of the set, in rank order. */
  BTStatus lock();
  /** @brief Release the lock on every member. */
  BTStatus unlock();
  /** @brief The set of the first discovered member (setCount 0 before). */
  BLEAudioCoordinatedSetInfo setInfo() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Result of each discover(). */
  BLEAudioCoordinatedSetCoordinator &onDiscovered(DiscoveredCallback cb);
  /** @brief lock() / unlock() finished. */
  BLEAudioCoordinatedSetCoordinator &onLock(LockCallback cb);
  /** @brief A member's lock changed outside this coordinator. */
  BLEAudioCoordinatedSetCoordinator &onLockChanged(LockChangedCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCoordinatedSetCoordinator(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
