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
 * Set the local roles before `audio.begin()`; discover a connected peer after
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
  None = 0x0000,                    ///< No TMAP role.
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
 * packaged libraries; otherwise `audio.begin()` fails with InvalidParam.
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
  enum : uint8_t {
    UggMultiplex = 0x01,     ///< UGG supports multiplex.
    Ugg96kbpsSource = 0x02,  ///< UGG supports 96 kbps source.
    UggMultisink = 0x04,     ///< UGG supports multisink.
  };
  enum : uint8_t {
    UgtSource = 0x01,        ///< UGT supports source.
    Ugt80kbpsSource = 0x02,  ///< UGT supports 80 kbps source.
    UgtSink = 0x04,          ///< UGT supports sink.
    Ugt64kbpsSink = 0x08,    ///< UGT supports 64 kbps sink.
    UgtMultiplex = 0x10,     ///< UGT supports multiplex.
    UgtMultisink = 0x20,     ///< UGT supports multisink.
    UgtMultisource = 0x40,   ///< UGT supports multisource.
  };
  enum : uint8_t {
    Bgs96kbps = 0x01,  ///< BGS supports 96 kbps.
  };
  enum : uint8_t {
    BgrMultisink = 0x01,  ///< BGR supports multisink.
    BgrMultiplex = 0x02,  ///< BGR supports multiplex.
  };

  uint8_t unicastGateway = 0;     //!< Ugg* bits
  uint8_t unicastTerminal = 0;    //!< Ugt* bits; UGT needs UgtSource and/or UgtSink
  uint8_t broadcastSender = 0;    //!< Bgs* bits
  uint8_t broadcastReceiver = 0;  //!< Bgr* bits
};

/** @brief TMAP identity (TMAS server and TMAP client). */
class BLEAudioTmap {
public:
  /**
   * @brief Roles read from the peer.
   * @param connHandle ACL connection handle the roles were read from.
   * @param status OK when the read finished; error status when it failed.
   * @param peerRoles Roles published by the peer (None when @p status is an error).
   */
  using DiscoverCallback = std::function<void(uint16_t connHandle, BTStatus status, BLEAudioTmapRole peerRoles)>;

  BLEAudioTmap();
  ~BLEAudioTmap() = default;
  BLEAudioTmap(const BLEAudioTmap &) = default;
  BLEAudioTmap &operator=(const BLEAudioTmap &) = default;
  BLEAudioTmap(BLEAudioTmap &&) = default;
  BLEAudioTmap &operator=(BLEAudioTmap &&) = default;

  /** @return true when the handle was created by BLEAudio::createTmap(). */
  explicit operator bool() const;

  /** @brief Roles published by TMAS (before `audio.begin()`).
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or after audio.begin();
   *         InvalidParam for unknown role bits. */
  BTStatus setRoles(BLEAudioTmapRole roles);
  /** @return The local roles set with setRoles(). */
  BLEAudioTmapRole getRoles() const;

  /** @brief Read the TMAP roles of a connected peer (result in onDiscovered()).
   * @param connHandle ACL connection handle of the peer.
   * @return BTStatus::InvalidState when the handle is empty, before audio.begin(), or for BLE_AUDIO_CONN_NONE;
   *         OK when discovery started or was deferred until GATT discovery; another status when discovery failed to start.
   */
  BTStatus discover(uint16_t connHandle);

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Result of each discover(). */
  void onDiscovered(DiscoverCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioTmap(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

/** @brief GMAP identity (GMAS server and GMAP client). */
class BLEAudioGmap {
public:
  /**
   * @brief Roles and features read from the peer.
   * @param connHandle ACL connection handle the roles were read from.
   * @param status OK when the read finished; error status when it failed.
   * @param peerRoles Roles published by the peer.
   * @param peerFeatures Features published by the peer (zero when @p status is an error).
   */
  using DiscoverCallback = std::function<void(uint16_t connHandle, BTStatus status, BLEAudioGmapRole peerRoles, const BLEAudioGmapFeatures &peerFeatures)>;

  BLEAudioGmap();
  ~BLEAudioGmap() = default;
  BLEAudioGmap(const BLEAudioGmap &) = default;
  BLEAudioGmap &operator=(const BLEAudioGmap &) = default;
  BLEAudioGmap(BLEAudioGmap &&) = default;
  BLEAudioGmap &operator=(BLEAudioGmap &&) = default;

  /** @return true when the handle was created by BLEAudio::createGmap(). */
  explicit operator bool() const;

  /** @brief Roles published by GMAS (before `audio.begin()`).
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or after audio.begin();
   *         InvalidParam for unknown role bits. */
  BTStatus setRoles(BLEAudioGmapRole roles);
  /** @brief Features of the published roles (before `audio.begin()`).
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or after audio.begin();
   *         InvalidParam for unknown feature bits. */
  BTStatus setFeatures(const BLEAudioGmapFeatures &features);
  /** @brief Same as setFeatures(const BLEAudioGmapFeatures &), one bitfield per role.
   * @return As setFeatures(const BLEAudioGmapFeatures &). */
  BTStatus setFeatures(uint8_t unicastGateway, uint8_t unicastTerminal, uint8_t broadcastSender, uint8_t broadcastReceiver);
  /** @return The local roles set with setRoles(). */
  BLEAudioGmapRole getRoles() const;

  /** @brief Read the GMAP roles and features of a connected peer (result in onDiscovered()).
   * @param connHandle ACL connection handle of the peer.
   * @return BTStatus::InvalidState when the handle is empty, before audio.begin(), or for BLE_AUDIO_CONN_NONE;
   *         OK when discovery started or was deferred until GATT discovery; another status when discovery failed to start.
   */
  BTStatus discover(uint16_t connHandle);

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Result of each discover(). */
  void onDiscovered(DiscoverCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioGmap(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
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
   * @param features Feature bits (Feature values combined with `|`).
   * @param metadata LTV metadata to embed (may be null when @p metadataLen is 0).
   * @param metadataLen Octets in @p metadata.
   * @param out Destination buffer for the announcement.
   * @param outCap Capacity of @p out in octets.
   * @return Bytes written (4 + @p metadataLen), or 0 on error.
   */
  static size_t buildAnnouncement(uint8_t features, const uint8_t *metadata, size_t metadataLen, uint8_t *out, size_t outCap);

  /**
   * @brief Parse a Service Data 16-bit AD value (starting with its UUID).
   * @param data Announcement bytes starting with the 16-bit UUID.
   * @param dataLen Octets in @p data.
   * @param featuresOut Receives the feature bits.
   * @param metadataOut Points into @p data (null when there is no metadata).
   * @param metadataLenOut Receives the octets in @p metadataOut.
   * @return false if @p data is not a Public Broadcast Announcement.
   */
  static bool parseAnnouncement(const uint8_t *data, size_t dataLen, uint8_t &featuresOut, const uint8_t *&metadataOut, size_t &metadataLenOut);
};

#endif /* BLE_AUDIO_SUPPORTED */
