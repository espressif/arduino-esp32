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
 * @brief Public, backend-agnostic value types for the LE Audio component.
 *
 * These mirror the LE Audio / Generic Audio Framework (GAF) values from the
 * Bluetooth Assigned Numbers so they map 1:1 onto the engine, but they NEVER
 * name an engine (`esp_ble_audio_*`) type: the translation happens only in the
 * audio implementation files (see `BLEAudioStreamInternal.h`). Audio-only value
 * types live here, not in `types/`, which is reserved for types shared across
 * components.
 *
 * Errors are reported with the library-wide `BTStatus`; the audio component
 * maps engine error codes onto it in one place (`bleAudioStatus()`).
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <cstdint>
#include <cstddef>

// --------------------------------------------------------------------------
// Contexts and locations
// --------------------------------------------------------------------------

/**
 * @brief Audio context types (Assigned Numbers, "Context Type" bitfield).
 *
 * Describes what a stream carries and which contexts a device supports or
 * currently makes available. PACS publishes them as "Supported" and
 * "Available" Audio Contexts; a unicast client sends one with Enable, and
 * a broadcast source puts one in its BASE metadata. Combine with `operator|`.
 */
enum class BLEAudioContext : uint16_t {
  Prohibited = 0x0000,       ///< No context; also "not available right now".
  Unspecified = 0x0001,      ///< Any audio not covered by another type.
  Conversational = 0x0002,   ///< Phone or video call.
  Media = 0x0004,            ///< Music, radio, podcast, video soundtrack.
  Game = 0x0008,             ///< Game audio.
  Instructional = 0x0010,    ///< Navigation, announcements, user guidance.
  VoiceAssistants = 0x0020,  ///< Man-machine voice interaction.
  Live = 0x0040,             ///< Live audio (e.g. a venue broadcast).
  SoundEffects = 0x0080,     ///< Keyboard and touch feedback, UI sounds.
  Notifications = 0x0100,    ///< Incoming message alerts.
  Ringtone = 0x0200,         ///< Incoming call alert.
  Alerts = 0x0400,           ///< Alarms and timers.
  EmergencyAlarm = 0x0800,   ///< Emergency sounds (e.g. fire alarm).
};

/** @brief Combine two context sets. */
inline constexpr BLEAudioContext operator|(BLEAudioContext a, BLEAudioContext b) {
  return static_cast<BLEAudioContext>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
/** @brief Add @p b to the context set @p a. */
inline BLEAudioContext &operator|=(BLEAudioContext &a, BLEAudioContext b) {
  a = a | b;
  return a;
}

/**
 * @brief Audio channel allocation (Assigned Numbers, "Audio Location" bitfield).
 *
 * A 32-bit bitmap of speaker positions. `Mono` is the special all-zero value
 * (one channel with no position). A stream carries one channel per set bit;
 * a device publishes the union of the locations it can render or capture.
 * Only the common positions are named; any other Assigned Numbers bit can be
 * passed with a cast.
 */
enum class BLEAudioLocation : uint32_t {
  Mono = 0x00000000,
  FrontLeft = 0x00000001,
  FrontRight = 0x00000002,
  FrontCenter = 0x00000004,
  LowFreqEffects1 = 0x00000008,
  BackLeft = 0x00000010,
  BackRight = 0x00000020,
  SideLeft = 0x00000400,
  SideRight = 0x00000800,
};

/** @brief Combine two location sets. */
inline constexpr BLEAudioLocation operator|(BLEAudioLocation a, BLEAudioLocation b) {
  return static_cast<BLEAudioLocation>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
/** @brief Add @p b to the location set @p a. */
inline BLEAudioLocation &operator|=(BLEAudioLocation &a, BLEAudioLocation b) {
  a = a | b;
  return a;
}

// --------------------------------------------------------------------------
// Codec presets and configuration
// --------------------------------------------------------------------------

/**
 * @brief Standard BAP LC3 presets ("LC3_<rate>_<variant>_1", BAP Tables 5.2 / 6.4).
 *
 * Each preset fixes the sampling rate, frame duration and octets per frame,
 * and brings the matching low-latency QoS (SDU interval, retransmissions,
 * max transport latency and a 40 ms presentation delay). The 44.1 kHz presets
 * use framed ISO because their frames do not divide the SDU interval evenly.
 *
 * The enumerator order is part of the engine contract: it indexes the preset
 * table in `BLEAudioEngine.c`, so never reorder or insert values.
 */
enum class BLEAudioCodecPreset : uint8_t {
  LC3_8_1_1,    ///<  8 kHz,   7.5 ms,  26 octets
  LC3_8_2_1,    ///<  8 kHz,  10 ms,    30 octets
  LC3_16_1_1,   ///< 16 kHz,   7.5 ms,  30 octets
  LC3_16_2_1,   ///< 16 kHz,  10 ms,    40 octets (mandatory for most roles)
  LC3_24_1_1,   ///< 24 kHz,   7.5 ms,  45 octets
  LC3_24_2_1,   ///< 24 kHz,  10 ms,    60 octets
  LC3_32_1_1,   ///< 32 kHz,   7.5 ms,  60 octets
  LC3_32_2_1,   ///< 32 kHz,  10 ms,    80 octets
  LC3_441_1_1,  ///< 44.1 kHz, 7.5 ms,  97 octets (framed)
  LC3_441_2_1,  ///< 44.1 kHz, 10 ms,  130 octets (framed)
  LC3_48_1_1,   ///< 48 kHz,   7.5 ms,  75 octets
  LC3_48_2_1,   ///< 48 kHz,  10 ms,   100 octets
  LC3_48_3_1,   ///< 48 kHz,   7.5 ms,  90 octets
  LC3_48_4_1,   ///< 48 kHz,  10 ms,   120 octets
  LC3_48_5_1,   ///< 48 kHz,   7.5 ms, 117 octets
  LC3_48_6_1,   ///< 48 kHz,  10 ms,   155 octets
};

/**
 * @brief Bit of preset @p p in a preset mask.
 *
 * Used by the roles that publish capabilities, e.g.
 * `server.setSupportedPresets(BLEAudioPresetBit(LC3_16_2_1) | BLEAudioPresetBit(LC3_48_2_1))`.
 */
inline constexpr uint32_t BLEAudioPresetBit(BLEAudioCodecPreset p) {
  return 1UL << static_cast<uint8_t>(p);
}

/**
 * @brief LC3 codec configuration of one stream.
 *
 * Prefer building it with fromPreset(); set the fields directly only for a
 * custom configuration. All fields use the specification's units. A stream
 * negotiated by a peer reports its actual configuration through
 * `BLEAudioStream::codecConfig()`.
 */
struct BLEAudioCodecConfig {
  uint32_t samplingRateHz = 16000;   ///< PCM sampling rate (8000 to 48000).
  uint16_t frameDurationUs = 10000;  ///< LC3 frame duration: 7500 or 10000.
  uint16_t octetsPerFrame = 40;      ///< Encoded octets per channel per frame.
  uint8_t framesPerSdu = 1;          ///< Codec frame blocks packed into one SDU.
  BLEAudioLocation channelAllocation = BLEAudioLocation::Mono;  ///< One channel per set bit.

  /** @brief Build the configuration of a standard BAP preset (mono). */
  static BLEAudioCodecConfig fromPreset(BLEAudioCodecPreset preset);

  /** @brief Channels carried by one stream: set bits of channelAllocation, 1 for Mono. */
  uint8_t channels() const;

  /**
   * @brief PCM samples per channel in one LC3 frame (e.g. 160 at 16 kHz / 10 ms).
   *
   * LC3 runs 44.1 kHz with the 48 kHz frame length (480 / 360 samples), so
   * the actual frame there lasts 10.884 / 8.163 ms instead of the nominal
   * frameDurationUs.
   */
  uint16_t samplesPerFrame() const {
    uint32_t rate = (samplingRateHz == 44100) ? 48000 : samplingRateHz;
    return static_cast<uint16_t>((uint64_t)rate * frameDurationUs / 1000000);
  }

  /** @brief Octets in one SDU: octets per frame x channels x frame blocks. */
  uint16_t sduOctets() const {
    return static_cast<uint16_t>(octetsPerFrame * channels() * framesPerSdu);
  }
};

// --------------------------------------------------------------------------
// Quality of service
// --------------------------------------------------------------------------

/**
 * @brief Isochronous QoS of one stream.
 *
 * fromPreset() returns the BAP low-latency values for a preset; override
 * fields only for a custom link. The presentation delay is the time between
 * the SDU reference anchor and the moment the audio is rendered; every device
 * of a group renders at the same instant thanks to it.
 */
struct BLEAudioQos {
  uint32_t sduIntervalUs = 10000;        ///< Time between SDUs; matches the frame duration.
  uint16_t maxSdu = 40;                  ///< Largest SDU in octets (all channels, all blocks).
  uint8_t retransmissions = 2;           ///< Controller retransmission number (RTN).
  uint16_t maxTransportLatencyMs = 10;   ///< Upper bound on the controller's transport latency.
  uint32_t presentationDelayUs = 40000;  ///< Render delay after the SDU anchor.
  uint8_t phy = 0x02;                    ///< PHY bitmask: 0x01 1M, 0x02 2M, 0x04 Coded.
  bool framed = false;                   ///< Framed ISO (required by the 44.1 kHz presets).

  /**
   * @brief Build the low-latency QoS of a BAP preset.
   * @param preset    Preset whose QoS to return.
   * @param broadcast Use the broadcast table (BAP Table 6.4), whose RTN differs
   *                  from unicast at 44.1 and 48 kHz.
   */
  static BLEAudioQos fromPreset(BLEAudioCodecPreset preset, bool broadcast = false);
};

// --------------------------------------------------------------------------
// Received SDU metadata
// --------------------------------------------------------------------------

/** @brief Per-SDU metadata handed to `BLEAudioStream::onReceive` callbacks. */
struct BLEAudioSduInfo {
  /** @brief Controller packet status flag of the SDU. */
  enum class Status : uint8_t {
    Valid = 0,    ///< Received intact.
    Invalid = 1,  ///< Received, but the controller flagged possible errors.
    Lost = 2,     ///< Nothing received for this interval; the payload is empty.
  };
  uint32_t timestamp = 0;       ///< Controller SDU reference timestamp (us); see timestampValid.
  uint16_t seq = 0;             ///< Packet sequence number, one per SDU interval.
  Status status = Status::Valid;
  bool timestampValid = false;  ///< Whether the controller supplied a timestamp.
};

#endif /* BLE_AUDIO_SUPPORTED */
