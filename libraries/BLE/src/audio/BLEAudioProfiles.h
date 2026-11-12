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
 * @brief Top-level LE Audio profiles: TMAP, GMAP and the PBP announcement.
 *
 * TMAP and GMAP move no audio themselves (CAP, BAP and the control profiles
 * do): a device publishes which top-level roles it implements in a small
 * identity service (TMAS / GMAS), and a client reads the roles of a peer.
 * Set the local roles before `audio.start()`; discover a connected peer after
 * its GATT discovery. A pure client sets no roles and publishes nothing.
 *
 * `BLEAudioPublicBroadcast` builds and parses the Public Broadcast
 * Announcement that Auracast sources advertise next to the Broadcast Audio
 * Announcement.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include "BTStatus.h"

class BLEAudio;

/** @brief TMAP roles (TMAP Role characteristic bits). */
enum class BLEAudioTmapRole : uint16_t {
  None = 0x0000,
  CallGateway = 0x0001,             //!< CG: places / controls calls
  CallTerminal = 0x0002,            //!< CT: the headset end of a call
  UnicastMediaSender = 0x0004,      //!< UMS
  UnicastMediaReceiver = 0x0008,    //!< UMR
  BroadcastMediaSender = 0x0010,    //!< BMS: Auracast transmitter
  BroadcastMediaReceiver = 0x0020,  //!< BMR: Auracast receiver
};

/** @brief Combine TMAP roles. */
inline BLEAudioTmapRole operator|(BLEAudioTmapRole a, BLEAudioTmapRole b) {
  return static_cast<BLEAudioTmapRole>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline BLEAudioTmapRole &operator|=(BLEAudioTmapRole &a, BLEAudioTmapRole b) {
  return a = a | b;
}
/** @return true when @p a and @p b share a role (e.g. `roles & CallTerminal`). */
inline bool operator&(BLEAudioTmapRole a, BLEAudioTmapRole b) {
  return (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0;
}

/**
 * @brief GMAP roles (GMAP Role characteristic bits).
 *
 * Each role also needs its CONFIG_BT_GMAP_<role>_SUPPORTED option in the
 * packaged libraries; otherwise `audio.start()` fails with InvalidParam.
 */
enum class BLEAudioGmapRole : uint8_t {
  None = 0x00,
  UnicastGameGateway = 0x01,     //!< UGG: the console / PC end
  UnicastGameTerminal = 0x02,    //!< UGT: the headset end
  BroadcastGameSender = 0x04,    //!< BGS
  BroadcastGameReceiver = 0x08,  //!< BGR
};

/** @brief Combine GMAP roles. */
inline BLEAudioGmapRole operator|(BLEAudioGmapRole a, BLEAudioGmapRole b) {
  return static_cast<BLEAudioGmapRole>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline BLEAudioGmapRole &operator|=(BLEAudioGmapRole &a, BLEAudioGmapRole b) {
  return a = a | b;
}
/** @return true when @p a and @p b share a role. */
inline bool operator&(BLEAudioGmapRole a, BLEAudioGmapRole b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/** @brief GMAP feature bitfields, one per role; a role's field is ignored unless the role is set. */
struct BLEAudioGmapFeatures {
  enum : uint8_t { UggMultiplex = 0x01, Ugg96kbpsSource = 0x02, UggMultisink = 0x04 };
  enum : uint8_t {
    UgtSource = 0x01,
    Ugt80kbpsSource = 0x02,
    UgtSink = 0x04,
    Ugt64kbpsSink = 0x08,
    UgtMultiplex = 0x10,
    UgtMultisink = 0x20,
    UgtMultisource = 0x40,
  };
  enum : uint8_t { Bgs96kbps = 0x01 };
  enum : uint8_t { BgrMultisink = 0x01, BgrMultiplex = 0x02 };

  uint8_t unicastGateway = 0;     //!< Ugg* bits
  uint8_t unicastTerminal = 0;    //!< Ugt* bits; UGT needs UgtSource and/or UgtSink
  uint8_t broadcastSender = 0;    //!< Bgs* bits
  uint8_t broadcastReceiver = 0;  //!< Bgr* bits
};

/** @brief TMAP identity (TMAS server and TMAP client). */
class BLEAudioTmap {
public:
  /** @brief Roles read from the peer on @p connHandle (None when @p status is an error). */
  using DiscoverCallback = std::function<void(uint16_t connHandle, BTStatus status, BLEAudioTmapRole peerRoles)>;

  BLEAudioTmap();
  ~BLEAudioTmap() = default;
  BLEAudioTmap(const BLEAudioTmap &) = default;
  BLEAudioTmap &operator=(const BLEAudioTmap &) = default;

  /** @return true when the handle was created by BLEAudio::createTmap(). */
  explicit operator bool() const;

  /** @brief Roles published by TMAS (before `audio.start()`). */
  BLEAudioTmap &setRoles(BLEAudioTmapRole roles);
  /** @return The local roles set with setRoles(). */
  BLEAudioTmapRole getRoles() const;

  /** @brief Read the TMAP roles of a connected peer (result in onDiscovered()). */
  BTStatus discover(uint16_t connHandle);

  /** @brief Result of each discover(). */
  BLEAudioTmap &onDiscovered(DiscoverCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioTmap(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/** @brief GMAP identity (GMAS server and GMAP client). */
class BLEAudioGmap {
public:
  /** @brief Roles and features read from the peer on @p connHandle (zero when @p status is an error). */
  using DiscoverCallback = std::function<void(uint16_t connHandle, BTStatus status, BLEAudioGmapRole peerRoles, const BLEAudioGmapFeatures &peerFeatures)>;

  BLEAudioGmap();
  ~BLEAudioGmap() = default;
  BLEAudioGmap(const BLEAudioGmap &) = default;
  BLEAudioGmap &operator=(const BLEAudioGmap &) = default;

  /** @return true when the handle was created by BLEAudio::createGmap(). */
  explicit operator bool() const;

  /** @brief Roles published by GMAS (before `audio.start()`). */
  BLEAudioGmap &setRoles(BLEAudioGmapRole roles);
  /** @brief Features of the published roles (before `audio.start()`). */
  BLEAudioGmap &setFeatures(const BLEAudioGmapFeatures &features);
  /** @brief Same as setFeatures(const BLEAudioGmapFeatures &), one bitfield per role. */
  BLEAudioGmap &setFeatures(uint8_t unicastGateway, uint8_t unicastTerminal, uint8_t broadcastSender, uint8_t broadcastReceiver);
  /** @return The local roles set with setRoles(). */
  BLEAudioGmapRole getRoles() const;

  /** @brief Read the GMAP roles and features of a connected peer (result in onDiscovered()). */
  BTStatus discover(uint16_t connHandle);

  /** @brief Result of each discover(). */
  BLEAudioGmap &onDiscovered(DiscoverCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioGmap(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/**
 * @brief Public Broadcast Announcement (PBP) builder / parser.
 *
 * Needs PBP in the packaged libraries (`BLE_AUDIO_PBP_SUPPORTED`); without it
 * both helpers fail.
 */
class BLEAudioPublicBroadcast {
public:
  /** Announcement feature bits (combine with `|`). */
  enum Feature : uint8_t {
    Encryption = 0x01,       //!< The broadcast is encrypted
    StandardQuality = 0x02,  //!< Carries a Standard Quality (16/24 kHz) stream
    HighQuality = 0x04,      //!< Carries a High Quality (48 kHz) stream
  };

  /**
   * @brief Build the value of a Service Data 16-bit AD structure.
   *
   * @p out receives the 0x1856 UUID (2 bytes, little-endian), the features,
   * the metadata length and the metadata.
   * @param metadata LTV metadata to embed (may be null when @p metadataLen is 0).
   * @return Bytes written (4 + @p metadataLen), or 0 on error.
   */
  static size_t buildAnnouncement(uint8_t features, const uint8_t *metadata, size_t metadataLen, uint8_t *out, size_t outCap);

  /**
   * @brief Parse a Service Data 16-bit AD value (starting with its UUID).
   * @param metadataOut Points into @p data (null when there is no metadata).
   * @return false if @p data is not a Public Broadcast Announcement.
   */
  static bool parseAnnouncement(const uint8_t *data, size_t dataLen, uint8_t &featuresOut, const uint8_t *&metadataOut, size_t &metadataLenOut);
};

#endif /* BLE_AUDIO_SUPPORTED */
