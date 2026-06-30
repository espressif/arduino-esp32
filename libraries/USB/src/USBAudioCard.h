// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED
#include "sdkconfig.h"
#if CONFIG_TINYUSB_AUDIO_ENABLED
#include "esp_event.h"

/** @file USBAudioCard.h
 *  @brief USB Audio Class (UAC) device API for ESP32 Arduino.
 *
 *  Available when @c SOC_USB_OTG_SUPPORTED and @c CONFIG_TINYUSB_AUDIO_ENABLED
 *  are enabled. Uses TinyUSB UAC1 (full-speed) or UAC2 (high-speed) depending
 *  on the build configuration.
 */

ESP_EVENT_DECLARE_BASE(ARDUINO_USB_AUDIO_CARD_EVENTS);

/** @brief Event identifiers posted on @ref ARDUINO_USB_AUDIO_CARD_EVENTS. */
typedef enum {
  ARDUINO_USB_AUDIO_CARD_ANY_EVENT = ESP_EVENT_ANY_ID, /**< Wildcard: register for all audio card events. */
  ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT,                 /**< Host or device changed a channel volume. */
  ARDUINO_USB_AUDIO_CARD_MUTE_EVENT,                   /**< Host or device changed a channel mute state. */
  ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT,            /**< Sample rate changed. */
  ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT,       /**< Speaker or microphone streaming interface alt setting changed. */
  ARDUINO_USB_AUDIO_CARD_MAX_EVENT,                    /**< Upper bound for valid event IDs (exclusive). */
} arduino_usb_audio_card_event_t;

/** @brief Logical audio channel for volume and mute (USB feature unit indices). */
typedef enum {
  UAC_CHAN_MASTER, /**< Master channel (index 0). */
  UAC_CHAN_LEFT,   /**< Left channel. */
  UAC_CHAN_RIGHT   /**< Right channel. */
} UAC_Channel;

/** @brief Speaker or microphone side of the composite device. */
typedef enum {
  UAC_INTERFACE_SPK, /**< Speaker (playback) interface. */
  UAC_INTERFACE_MIC  /**< Microphone (capture) interface. */
} UAC_Interface;

/** @brief Payload for @ref ARDUINO_USB_AUDIO_CARD_EVENTS; only one branch is valid per event type. */
typedef union {
  struct {
    UAC_Channel channel; /**< Affected channel. */
    int8_t db;           /**< Volume in dB. */
  } volume;              /**< Valid for @ref ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT. */
  struct {
    UAC_Channel channel; /**< Affected channel. */
    bool muted;          /**< New mute state. */
  } mute;                /**< Valid for @ref ARDUINO_USB_AUDIO_CARD_MUTE_EVENT. */
  struct {
    uint32_t rate; /**< Sample rate in Hz. */
  } sample_rate;   /**< Valid for @ref ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT. */
  struct {
    UAC_Interface interface; /**< Speaker or microphone. */
    bool enable;             /**< @c true if the non-zero alternate setting is active (streaming). */
  } interface_enable;        /**< Valid for @ref ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT. */
} arduino_usb_audio_card_event_data_t;

/** @brief Number of speaker (playback) channels exposed in the USB descriptor. */
typedef enum {
  UAC_SPK_NONE,  /**< No speaker path (microphone-only device). */
  UAC_SPK_MONO,  /**< Mono speaker. */
  UAC_SPK_STEREO /**< Stereo speaker. */
} UAC_SPK_Channels;

/** @brief Number of microphone (capture) channels exposed in the USB descriptor. */
typedef enum {
  UAC_MIC_NONE,  /**< No microphone path (speaker-only device). */
  UAC_MIC_MONO,  /**< Mono microphone. */
  UAC_MIC_STEREO /**< Stereo microphone. */
} UAC_MIC_Channels;

/** @brief PCM bit depth used for USB audio streams. */
typedef enum {
  UAC_BPS_16 = 16, /**< 16-bit samples. */
  UAC_BPS_24 = 24, /**< 24-bit samples (stored in 32-bit containers where applicable). */
  UAC_BPS_32 = 32  /**< 32-bit samples. */
} UAC_Bits_Per_Sample;

/** @brief Callback for incoming speaker (host-to-device) PCM data.
 *  @param data Pointer to PCM samples in the format configured for the device.
 *  @param len  Length of @a data in bytes.
 */
typedef void (*arduino_usb_audio_card_data_handler_t)(void *data, uint16_t len);

/** @brief USB composite audio device (UAC): speaker and/or microphone with volume/mute and events.
 *
 *  Construct with the desired sample rate, bit depth, and channel counts, then call
 *  begin() after USB is running. Use onData() to receive decoded playback data, write()
 *  to send microphone data, and onEvent() for host control changes.
 */
class USBAudioCard {
public:
  /** @brief Creates the audio device configuration and registers the USB audio interface.
   *  @param sample_rate   Initial sample rate in Hz (UAC1 full-speed: typically up to 48000).
   *  @param bps             Bits per sample (@ref UAC_Bits_Per_Sample).
   *  @param spk_channels    Speaker channel layout (@ref UAC_SPK_Channels).
   *  @param mic_channels    Microphone channel layout (@ref UAC_MIC_Channels).
   */
  USBAudioCard(uint32_t sample_rate, UAC_Bits_Per_Sample bps, UAC_SPK_Channels spk_channels = UAC_SPK_STEREO, UAC_MIC_Channels mic_channels = UAC_MIC_STEREO);
  ~USBAudioCard();

  /** @brief Allocates buffers and starts the speaker receive task if a speaker path exists.
   *  @return @c true on success, @c false if allocation or task creation failed.
   */
  bool begin();
  /** @brief Stops the receive task and frees speaker buffers. */
  void end();

  /** @return Configured speaker channel count category. */
  UAC_SPK_Channels speakerChannels();
  /** @return Configured microphone channel count category. */
  UAC_MIC_Channels micChannels();
  /** @return Configured bits per sample. */
  UAC_Bits_Per_Sample bitsPerSample();
  /** @return Bytes per sample in memory (16-bit uses 2; 24/32-bit use 4 in this stack). */
  uint8_t bytesPerSample();
  /** @return Current sample rate in Hz. */
  uint32_t sampleRate();

  /** @brief Reads mute state for a speaker channel.
   *  @param channel Logical channel (@ref UAC_Channel).
   *  @return Current mute flag, or @c false if the channel is invalid.
   */
  bool mute(UAC_Channel channel);
  /** @brief Sets mute locally and notifies the host (UAC2 with interrupt endpoint only).
   *  @param channel Logical channel (@ref UAC_Channel).
   *  @param muted   New mute state.
   *  @return @c true if the interrupt notification was sent, @c false if unavailable or invalid channel.
   */
  bool mute(UAC_Channel channel, bool muted);  // UAC2 Only
  /** @brief Reads volume for a speaker channel in dB.
   *  @param channel Logical channel (@ref UAC_Channel).
   *  @return Volume in dB, or undefined if the channel is invalid (check logs).
   */
  int8_t volume(UAC_Channel channel);
  /** @brief Sets volume locally and notifies the host (UAC2 with interrupt endpoint only).
   *  @param channel   Logical channel (@ref UAC_Channel).
   *  @param volume_db Volume in dB.
   *  @return @c true if the interrupt notification was sent, @c false if unavailable or invalid channel.
   */
  bool volume(UAC_Channel channel, int8_t volume_db);  // UAC2 Only

  /** @brief Scales speaker PCM in @a data by the current volume and mute (software path).
   *
   *  Use when the DAC has no hardware volume control: applies all channels.
   *
   *  @param data Buffer of PCM samples; modified in place. Layout must match bitsPerSample().
   *  @param len  Size of @a data in bytes.
   */
  void applyVolume(void *data, uint16_t len);

  /** @brief Registers a callback for incoming speaker PCM from the host.
   *  @param callback Handler, or @c NULL to clear.
   */
  void onData(arduino_usb_audio_card_data_handler_t callback);
  /** @brief Sends microphone PCM toward the host (USB IN).
   *  @param data PCM buffer.
   *  @param len  Number of bytes to send.
   *  @return Number of bytes accepted, or 0 if the device is not active.
   */
  uint16_t write(const void *data, uint16_t len);

  /** @brief Registers a handler for all audio card events (@ref ARDUINO_USB_AUDIO_CARD_ANY_EVENT). */
  void onEvent(esp_event_handler_t callback);
  /** @brief Registers a handler for a specific event ID.
   *  @param event    Event to subscribe to, or @ref ARDUINO_USB_AUDIO_CARD_ANY_EVENT for all.
   *  @param callback ESP-IDF event handler; @a event_handler_arg will be @c this (the @ref USBAudioCard instance).
   */
  void onEvent(arduino_usb_audio_card_event_t event, esp_event_handler_t callback);

private:
};

#endif /* CONFIG_TINYUSB_AUDIO_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
