/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 * Copyright 2017 Neil Kolban
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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEAdvTypes.h"
#include "BLEAdvertisementData.h"
#include <memory>
#include <functional>

/**
 * @brief Unified BLE advertising class covering legacy, extended, and periodic advertising.
 *
 * Legacy advertising is the simple default. BLE5 extended/periodic features
 * are available with explicit opt-in and guarded by BLE5_SUPPORTED.
 */
class BLEAdvertising {
public:
  BLEAdvertising();
  ~BLEAdvertising() = default;
  BLEAdvertising(const BLEAdvertising &) = default;
  BLEAdvertising &operator=(const BLEAdvertising &) = default;
  BLEAdvertising(BLEAdvertising &&) = default;
  BLEAdvertising &operator=(BLEAdvertising &&) = default;

  /**
   * @brief Check whether this handle references a valid advertising instance.
   * @return true if the handle is backed by an initialized implementation, false otherwise.
   */
  explicit operator bool() const;

  // --- Legacy Advertising ---

  /**
   * @brief Append a service UUID to the advertisement payload.
   * @param uuid Service UUID to advertise.
   */
  void addServiceUUID(const BLEUUID &uuid);

  /**
   * @brief Remove a service UUID from the advertisement payload.
   * @param uuid Service UUID to remove.
   */
  void removeServiceUUID(const BLEUUID &uuid);

  /**
   * @brief Remove all service UUIDs from the advertisement payload.
   */
  void clearServiceUUIDs();

  /**
   * @brief Set the local name included in the advertisement.
   * @param name Device name string. May be truncated to fit the payload.
   * @note The host stack keeps its own GAP name; the backend may copy this
   *       string for the AD data rather than querying the GAP name API at
   *       broadcast time, so do not assume other getters always match the
   *       on-air name byte-for-byte on every target.
   */
  void setName(const String &name);

  /**
   * @brief Enable or disable sending a scan-response payload.
   * @param enable If true, the stack will transmit scan-response data when queried.
   */
  void setScanResponse(bool enable);

  /**
   * @brief Set the advertising PDU type.
   * @param type Advertising type (connectable, scannable, etc.).
   */
  void setType(BLEAdvType type);

  /**
   * @brief Set the advertising interval range.
   * @param minMs Minimum advertising interval in milliseconds.
   * @param maxMs Maximum advertising interval in milliseconds.
   */
  void setInterval(uint16_t minMs, uint16_t maxMs);

  /**
   * @brief Set the minimum preferred connection interval advertised to centrals.
   * @param minPreferred Minimum preferred interval value (in 1.25 ms units).
   */
  void setMinPreferred(uint16_t minPreferred);

  /**
   * @brief Set the maximum preferred connection interval advertised to centrals.
   * @param maxPreferred Maximum preferred interval value (in 1.25 ms units).
   */
  void setMaxPreferred(uint16_t maxPreferred);

  /**
   * @brief Include or exclude the TX power level in the advertisement.
   * @param include If true, the TX power level is added to the payload.
   */
  void setTxPower(bool include);

  /**
   * @brief Set the GAP appearance value in the advertisement.
   * @param appearance 16-bit appearance value as defined by the Bluetooth SIG.
   */
  void setAppearance(uint16_t appearance);

  /**
   * @brief Configure the scan-request and connect whitelist filters.
   * @param scanRequestWhitelistOnly If true, only accept scan requests from whitelisted devices.
   * @param connectWhitelistOnly If true, only accept connections from whitelisted devices.
   */
  void setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly);

  /**
   * @brief Reset all legacy advertising parameters to defaults.
   */
  void reset();

  /**
   * @brief Replace the advertisement payload with custom data.
   * @param data Pre-built advertisement data to use.
   */
  void setAdvertisementData(const BLEAdvertisementData &data);

  /**
   * @brief Replace the scan-response payload with custom data.
   * @param data Pre-built scan-response data to use.
   */
  void setScanResponseData(const BLEAdvertisementData &data);

  /**
   * @brief Start legacy advertising.
   * @param durationMs Duration in milliseconds. 0 = advertise indefinitely.
   * @return BTStatus indicating success or error.
   */
  BTStatus start(uint32_t durationMs = 0);

  /**
   * @brief Stop legacy advertising.
   * @return BTStatus indicating success or error.
   */
  BTStatus stop();

  /**
   * @brief Check whether legacy advertising is currently active.
   * @return true if advertising is running, false otherwise.
   */
  bool isAdvertising() const;

  // --- Extended Advertising (BLE5) ---

  /**
   * @brief Parameters for a BLE 5 extended advertising set (one instance).
   */
  struct ExtAdvConfig {
    uint8_t instance = 0;                                ///< Advertising instance index.
    BLEAdvType type = BLEAdvType::ConnectableScannable;  ///< PDU type for this instance.
    BLEPhy primaryPhy = BLEPhy::PHY_1M;                  ///< Primary advertising PHY.
    BLEPhy secondaryPhy = BLEPhy::PHY_1M;                ///< Secondary (aux) advertising PHY.
    int8_t txPower = 127;                                ///< TX power in dBm (127 = no preference).
    uint16_t intervalMin = 0x30;                         ///< Minimum advertising interval (controller units).
    uint16_t intervalMax = 0x30;                         ///< Maximum advertising interval (controller units).
    uint8_t channelMap = 0x07;                           ///< Bitmask of advertising channels (37/38/39).
    uint8_t sid = 0;                                     ///< Advertising Set Identifier.
    bool anonymous = false;                              ///< If true, omit the advertiser address.
    bool includeTxPower = false;                         ///< If true, include TX power in the header.
    bool scanReqNotify = false;                          ///< If true, notify on scan requests.
  };

  /**
   * @brief Configure an extended advertising instance.
   * @param config Configuration parameters for the instance.
   * @return BTStatus indicating success or error.
   * @note Requires BLE5_SUPPORTED.
   */
  BTStatus configureExtended(const ExtAdvConfig &config);

  /**
   * @brief Set the advertisement data for an extended advertising instance.
   * @param instance Advertising instance index.
   * @param data Advertisement payload.
   * @return BTStatus indicating success or error.
   */
  BTStatus setExtAdvertisementData(uint8_t instance, const BLEAdvertisementData &data);

  /**
   * @brief Set the scan-response data for an extended advertising instance.
   * @param instance Advertising instance index.
   * @param data Scan-response payload.
   * @return BTStatus indicating success or error.
   */
  BTStatus setExtScanResponseData(uint8_t instance, const BLEAdvertisementData &data);

  /**
   * @brief Set a random address for an extended advertising instance.
   * @param instance Advertising instance index.
   * @param addr Address to assign to the instance.
   * @return BTStatus indicating success or error.
   */
  BTStatus setExtInstanceAddress(uint8_t instance, const BTAddress &addr);

  /**
   * @brief Start an extended advertising instance.
   * @param instance Advertising instance index.
   * @param durationMs Duration in milliseconds. 0 = advertise indefinitely.
   * @param maxEvents Maximum number of advertising events. 0 = no limit.
   * @return BTStatus indicating success or error.
   */
  BTStatus startExtended(uint8_t instance, uint32_t durationMs = 0, uint8_t maxEvents = 0);

  /**
   * @brief Stop an extended advertising instance.
   * @param instance Advertising instance index.
   * @return BTStatus indicating success or error.
   */
  BTStatus stopExtended(uint8_t instance);

  /**
   * @brief Remove an extended advertising instance and free its resources.
   * @param instance Advertising instance index to remove.
   * @return BTStatus indicating success or error.
   */
  BTStatus removeExtended(uint8_t instance);

  /**
   * @brief Remove all extended advertising instances.
   * @return BTStatus indicating success or error.
   */
  BTStatus clearExtended();

  // --- Periodic Advertising (BLE5) ---

  /**
   * @brief Parameters for BLE 5 periodic advertising on an extended set.
   */
  struct PeriodicAdvConfig {
    uint8_t instance = 0;         ///< Extended advertising instance to attach to.
    uint16_t intervalMin = 0;     ///< Minimum periodic interval (1.25 ms units).
    uint16_t intervalMax = 0;     ///< Maximum periodic interval (1.25 ms units).
    bool includeTxPower = false;  ///< If true, include TX power in periodic PDUs.
  };

  /**
   * @brief Configure periodic advertising on an extended instance.
   * @param config Periodic advertising parameters.
   * @return BTStatus indicating success or error.
   */
  BTStatus configurePeriodicAdv(const PeriodicAdvConfig &config);

  /**
   * @brief Set the payload for periodic advertising.
   * @param instance Extended advertising instance index.
   * @param data Periodic advertising payload.
   * @return BTStatus indicating success or error.
   */
  BTStatus setPeriodicAdvData(uint8_t instance, const BLEAdvertisementData &data);

  /**
   * @brief Start periodic advertising on an extended instance.
   * @param instance Extended advertising instance index.
   * @return BTStatus indicating success or error.
   */
  BTStatus startPeriodicAdv(uint8_t instance);

  /**
   * @brief Stop periodic advertising on an extended instance.
   * @param instance Extended advertising instance index.
   * @return BTStatus indicating success or error.
   */
  BTStatus stopPeriodicAdv(uint8_t instance);

  // --- Event Handlers ---

  /**
   * @brief Callback invoked when an advertising instance completes (duration or maxEvents reached).
   * @param instance The advertising instance index that completed.
   */
  using CompleteHandler = std::function<void(uint8_t instance)>;

  /**
   * @brief Register a handler for advertising completion events.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onComplete(CompleteHandler handler);

  /**
   * @brief Remove all registered advertising callbacks.
   */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAdvertising(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEClass;
};

#endif /* BLE_ENABLED */
