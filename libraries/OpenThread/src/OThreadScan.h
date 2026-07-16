// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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
#include "sdkconfig.h"

#if SOC_IEEE802154_SUPPORTED && CONFIG_OPENTHREAD_ENABLED

#include <Arduino.h>
#include <vector>
#include <openthread/link.h>
#include <openthread/thread.h>
#include "OThread.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @file OThreadScan.h
 * @brief MLE Thread network discovery for the OpenThread Native API.
 *
 * Wraps `otThreadDiscover()` (ESP-IDF OpenThread CLI `discover`). Each result
 * includes Thread identity fields (network name, Extended PAN ID, joinable flag)
 * and IEEE 802.15.4 link fields (extended address, PAN ID, channel, RSSI, LQI).
 *
 * This is the same primitive Matter uses to list Thread networks during
 * commissioning.
 */

/** @brief Discovery still in progress (same convention as @c WIFI_SCAN_RUNNING). */
#define OT_DISCOVER_RUNNING (-1)

/** @brief Discovery failed or was not started (same convention as @c WIFI_SCAN_FAILED). */
#define OT_DISCOVER_FAILED (-2)

/** @brief Default overall discovery timeout (ms) for blocking and async completion. */
#ifndef OT_DISCOVER_DEFAULT_TIMEOUT_MS
#define OT_DISCOVER_DEFAULT_TIMEOUT_MS 30000
#endif

/**
 * @brief Optional filters passed to otThreadDiscover().
 *
 * Defaults match the ESP-IDF CLI `discover` command (no PAN / joiner / EUI-64
 * filtering). Set @c panIdFilter to a specific PAN ID, or leave at
 * @c OT_PANID_BROADCAST (0xffff) to accept any network.
 */
struct OThreadDiscoverFilters {
  uint16_t panIdFilter = OT_PANID_BROADCAST;  ///< Broadcast PAN disables filtering.
  bool joinerOnly = false;                    ///< Joiner flag in Discovery Request TLV.
  bool eui64Filter = false;                   ///< Filter responses on EUI-64.
};

/**
 * @brief One discovered Thread network.
 *
 * Populated from `otActiveScanResult` on each MLE Discovery Response.
 */
struct OThreadNetworkInfo {
  char networkName[OT_NETWORK_NAME_MAX_SIZE + 1];  ///< Null-terminated Thread network name.
  uint8_t extendedPanId[OT_EXT_PAN_ID_SIZE];      ///< Extended PAN ID (8 bytes).
  uint16_t panId;                                  ///< IEEE 802.15.4 PAN ID.
  uint8_t extAddress[OT_EXT_ADDRESS_SIZE];         ///< Responder extended address.
  uint8_t channel;                                 ///< IEEE 802.15.4 channel (11..26).
  int8_t rssi;                                     ///< Received signal strength (dBm).
  uint8_t lqi;                                     ///< Link Quality Indicator.
  uint8_t threadVersion;                           ///< Thread version (4-bit MLE value).
  bool joinable;                                   ///< Joining permitted (when known).
  bool nativeCommissioner;                         ///< Native Commissioner flag.

  /** @brief Network name as an Arduino @c String. */
  String networkNameStr() const {
    return String(networkName);
  }

  /** @brief Extended PAN ID as a lowercase hex string (16 characters). */
  String extendedPanIdStr() const;

  /** @brief Extended address as a lowercase hex string (16 characters). */
  String extAddressStr() const;
};

/** @brief Called for each network found during discovery. */
typedef void (*OThreadDiscoverResultCallback)(const OThreadNetworkInfo &info, void *context);

/**
 * @brief Called when discovery finishes.
 * @param resultCount Number of networks collected, or @ref OT_DISCOVER_FAILED.
 * @param error       OpenThread error from the start request, or @c OT_ERROR_NONE on success.
 */
typedef void (*OThreadDiscoverCompleteCallback)(int16_t resultCount, otError error, void *context);

/**
 * @brief Discover nearby Thread networks (MLE discover).
 *
 * Global instance `OThreadScan` is provided (same pattern as `OThreadUDP`).
 *
 * Blocking flow:
 * @code
 * OThread.begin(false);
 * OThread.networkInterfaceUp();
 *
 * int n = OThreadScan.discoverNetworks();
 * if (n >= 0) {
 *   for (int i = 0; i < n; ++i) {
 *     const OThreadNetworkInfo &net = OThreadScan.getResult(i);
 *     Serial.println(net.networkNameStr());
 *   }
 *   OThreadScan.scanDelete();
 * }
 * @endcode
 *
 * Async flow:
 * @code
 * OThreadScan.discoverNetworks(true);
 * // ... other work in loop() ...
 * int16_t status = OThreadScan.scanComplete();
 * @endcode
 *
 * Streaming flow:
 * @code
 * OThreadScan.onResult([](const OThreadNetworkInfo &info, void *) {
 *   Serial.println(info.networkNameStr());
 * });
 * OThreadScan.discoverNetworks(true);
 * @endcode
 */
class OThreadScanClass {
public:
  OThreadScanClass();
  ~OThreadScanClass();

  /** @brief Set the maximum time to wait for blocking discovery or async completion. */
  void setScanTimeout(uint32_t ms);

  /**
   * @brief Limit discovery to one channel.
   * @param channel IEEE 802.15.4 channel (11..26), or @c 0 for all supported channels.
   */
  void setChannel(uint8_t channel);

  /** @brief Configure PAN / joiner / EUI-64 filters for the next discovery. */
  void setDiscoverFilters(const OThreadDiscoverFilters &filters);

  /** @brief Register a per-network callback (optional; OpenThread streams results). */
  void onResult(OThreadDiscoverResultCallback callback, void *context = nullptr);

  /** @brief Register a completion callback (optional). */
  void onComplete(OThreadDiscoverCompleteCallback callback, void *context = nullptr);

  /**
   * @brief Start MLE Thread network discovery.
   *
   * @param async When @c false, blocks until discovery completes or
   *              @ref setScanTimeout() expires. When @c true, returns immediately
   *              with @ref OT_DISCOVER_RUNNING on success.
   * @return Network count on success (blocking), @ref OT_DISCOVER_RUNNING,
   *         or @ref OT_DISCOVER_FAILED.
   *
   * @note Requires the IPv6 interface to be up (@c OThread.networkInterfaceUp()).
   *       Thread does not need to be started; this matches Matter pre-commission
   *       discovery. For best results, at least one Leader/Router should be
   *       active nearby.
   */
  int16_t discoverNetworks(bool async = false);

  /**
   * @brief Poll async discovery state.
   * @return Result count when done, @ref OT_DISCOVER_RUNNING while busy,
   *         or @ref OT_DISCOVER_FAILED on error / timeout / no scan triggered.
   */
  int16_t scanComplete();

  /** @brief Release stored results and free associated memory. */
  void scanDelete();

  /** @brief @c true while an MLE discovery scan is in progress. */
  bool isDiscoverInProgress() const;

  /** @brief Number of entries from the last completed discovery. */
  uint8_t getResultCount() const;

  /**
   * @brief Access one collected result by index.
   * @return Reference to internal storage (valid until @ref scanDelete()).
   */
  const OThreadNetworkInfo &getResult(uint8_t index) const;

  /**
   * @brief Copy one result into @p info.
   * @return @c false if @p index is out of range.
   */
  bool getResult(uint8_t index, OThreadNetworkInfo &info) const;

  /**
   * @name Index-based convenience getters
   * @brief Per-field accessors for a discovery result at @p index.
   *
   * Each method delegates to @ref getResult(). Valid after a completed
   * discovery until @ref scanDelete(). Out-of-range @p index yields empty
   * strings, zero values, or @c false as documented per method.
   * @{
   */

  /**
   * @brief Thread network name for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return Network name, or an empty string if @p index is out of range.
   */
  String networkName(uint8_t index);

  /**
   * @brief IEEE 802.15.4 PAN ID for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return PAN ID, or @c 0 if @p index is out of range.
   */
  uint16_t panId(uint8_t index);

  /**
   * @brief Extended PAN ID as a lowercase hex string for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return 16-character hex string, or empty if @p index is out of range.
   */
  String extendedPanIdStr(uint8_t index);

  /**
   * @brief IEEE 802.15.4 extended address as a hex string for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return 16-character hex string, or empty if @p index is out of range.
   */
  String extAddressStr(uint8_t index);

  /**
   * @brief Copy the extended address for result @p index.
   * @param index   Result index (0 .. getResultCount() - 1).
   * @param address Output buffer (8 bytes, @c OT_EXT_ADDRESS_SIZE).
   * @return @c true on success, @c false if @p index is out of range.
   */
  bool extAddress(uint8_t index, uint8_t address[OT_EXT_ADDRESS_SIZE]);

  /**
   * @brief Copy the Extended PAN ID for result @p index.
   * @param index    Result index (0 .. getResultCount() - 1).
   * @param extPanId Output buffer (8 bytes, @c OT_EXT_PAN_ID_SIZE).
   * @return @c true on success, @c false if @p index is out of range.
   */
  bool extendedPanId(uint8_t index, uint8_t extPanId[OT_EXT_PAN_ID_SIZE]);

  /**
   * @brief IEEE 802.15.4 channel for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return Channel (11..26), or @c 0 if @p index is out of range.
   */
  uint8_t channel(uint8_t index);

  /**
   * @brief Received signal strength for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return RSSI in dBm, or @c 0 if @p index is out of range.
   */
  int8_t rssi(uint8_t index);

  /**
   * @brief Link Quality Indicator for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return LQI value, or @c 0 if @p index is out of range.
   */
  uint8_t lqi(uint8_t index);

  /**
   * @brief Join-permitted flag for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return @c true when joining is permitted, @c false when not joinable or
   *         @p index is out of range.
   */
  bool isJoinable(uint8_t index);

  /**
   * @brief Thread version field for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return 4-bit MLE Thread version, or @c 0 if @p index is out of range.
   */
  uint8_t threadVersion(uint8_t index);

  /**
   * @brief Native Commissioner flag for result @p index.
   * @param index Result index (0 .. getResultCount() - 1).
   * @return @c true when the responder advertises a native Commissioner,
   *         @c false otherwise or if @p index is out of range.
   */
  bool isNativeCommissioner(uint8_t index);

  /** @} */

  /**
   * @brief Raw OpenThread result for advanced use.
   * @return Pointer into internal storage, or @c nullptr if out of range.
   *         Valid until @ref scanDelete().
   */
  const otActiveScanResult *getActiveScanResult(uint8_t index) const;

private:
  static void handleDiscoverResult(otActiveScanResult *aResult, void *aContext);
  void onDiscoverResult(otActiveScanResult *aResult);
  static OThreadNetworkInfo fromActiveScanResult(const otActiveScanResult &in);
  bool discoverChannelMask(uint32_t &mask) const;
  /** Allocates `_doneSem` on first use (not in the global ctor / static init). */
  bool ensureDoneSem();

  uint32_t _timeoutMs;
  uint8_t _channel;
  OThreadDiscoverFilters _filters;
  OThreadDiscoverResultCallback _resultCb;
  void *_resultCtx;
  OThreadDiscoverCompleteCallback _completeCb;
  void *_completeCtx;

  bool _async;
  bool _inProgress;
  bool _triggered;
  bool _done;
  uint32_t _startedMs;
  otError _startError;

  std::vector<OThreadNetworkInfo> _results;
  std::vector<otActiveScanResult> _rawResults;
  SemaphoreHandle_t _doneSem;
};

/** @brief Global Thread network discovery helper. */
extern OThreadScanClass OThreadScan;

#endif /* SOC_IEEE802154_SUPPORTED && CONFIG_OPENTHREAD_ENABLED */
