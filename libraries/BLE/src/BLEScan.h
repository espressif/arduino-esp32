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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEAdvTypes.h"
#include "BLEAdvertisedDevice.h"
#include <memory>
#include <functional>
#include <vector>

/**
 * @brief BLE scanner -- legacy and BLE5 extended/periodic scanning.
 *
 * Singleton shared handle. Access via BLE.getScan().
 */
class BLEScan {
public:
  BLEScan();
  ~BLEScan() = default;
  BLEScan(const BLEScan &) = default;
  BLEScan &operator=(const BLEScan &) = default;
  BLEScan(BLEScan &&) = default;
  BLEScan &operator=(BLEScan &&) = default;

  /**
   * @brief Container for a snapshot of BLE scan results.
   *
   * Supports range-based for loops. Obtained from @ref startBlocking / @ref getResults
   * or passed to the @ref CompleteHandler callback.
   */
  class Results {
  public:
    /**
     * @brief Get the number of devices in the results.
     * @return The device count.
     */
    size_t size() const {
      return _devices.size();
    }

    /**
     * @brief Get a device by index.
     * @param index Zero-based index into the results.
     * @return The advertised device at the given index, or an invalid handle if out of range.
     */
    BLEAdvertisedDevice getDevice(size_t index) const {
      return (index < _devices.size()) ? _devices[index] : BLEAdvertisedDevice();
    }

    /**
     * @brief Check whether the results contain any devices.
     * @return true if at least one device was found.
     */
    explicit operator bool() const {
      return !_devices.empty();
    }

    /**
     * @brief Print all scan results to the log output.
     */
    void dump() const;

    /**
     * @brief Insert a device or replace an existing entry with the same (address, SID).
     * @param device The advertised device to add or update.
     * @note If the existing entry already has a name (merged from a prior scan response)
     *       and the incoming device has none, the existing entry is kept to avoid losing
     *       the scan-response data.
     */
    void appendOrReplace(const BLEAdvertisedDevice &device);

    /**
     * @brief Iterator begin for range-based for loops.
     * @return Pointer to the first device.
     */
    const BLEAdvertisedDevice *begin() const {
      return _devices.data();
    }

    /**
     * @brief Iterator end for range-based for loops.
     * @return Pointer past the last device.
     */
    const BLEAdvertisedDevice *end() const {
      return _devices.data() + _devices.size();
    }

  private:
    std::vector<BLEAdvertisedDevice> _devices;
    friend class BLEScan;
    // Backend-agnostic scan helper (impl/common/BLEScanImpl.h) that merges scan
    // responses into the result list for the backend scan parsers.
    friend struct BLEScanImplCommon;
  };

  /**
   * @brief Check whether this handle references a valid scanner instance.
   * @return true if the handle is backed by an initialized implementation, false otherwise.
   */
  explicit operator bool() const;

  // --- Scan Parameters ---

  /**
   * @brief Set the scan interval.
   * @param intervalMs Interval between scan windows, in milliseconds.
   */
  void setInterval(uint16_t intervalMs);

  /**
   * @brief Set the scan window duration.
   * @param windowMs Duration of each scan window, in milliseconds. Must be <= interval.
   */
  void setWindow(uint16_t windowMs);

  /**
   * @brief Enable or disable active scanning.
   * @param active If true, the scanner sends scan-request PDUs to obtain scan-response data.
   */
  void setActiveScan(bool active);

  /**
   * @brief Enable or disable duplicate filtering.
   * @param filter If true, the stack suppresses repeated advertisements from the same device.
   */
  void setFilterDuplicates(bool filter);

  /**
   * @brief Flush the controller's duplicate-address cache.
   *
   * @note Only meaningful when duplicate filtering is enabled.
   */
  void clearDuplicateCache();

  // --- Start / Stop ---

  /**
   * @brief Start scanning asynchronously.
   * @param durationMs How long to scan, in milliseconds. 0 = scan indefinitely.
   * @param appendToExistingResults If true, append to the existing result set rather than clearing it.
   * @return BTStatus indicating success or error.
   */
  BTStatus start(uint32_t durationMs, bool appendToExistingResults = false);

  /**
   * @brief Start scanning and block until the duration expires.
   * @param durationMs How long to scan, in milliseconds.
   * @return Snapshot of all discovered devices.
   */
  Results startBlocking(uint32_t durationMs);

  /**
   * @brief Stop an in-progress scan.
   * @return BTStatus indicating success or error.
   */
  BTStatus stop();

  /**
   * @brief Check whether a scan is currently running.
   * @return true if scanning is active, false otherwise.
   */
  bool isScanning() const;

  // --- Callbacks ---

  /**
   * @brief Callback invoked for each advertisement received during a scan.
   * @param device The advertised device that was discovered.
   */
  using ResultHandler = std::function<void(const BLEAdvertisedDevice &)>;

  /**
   * @brief Callback invoked when a scan completes (duration expires or manually stopped).
   * @param results Reference to the accumulated scan results.
   */
  using CompleteHandler = std::function<void(Results &)>;

  /**
   * @brief Register a handler called for every advertisement received.
   * @param callback The handler to invoke, or nullptr to clear.
   */
  void onResult(ResultHandler callback);

  /**
   * @brief Register a handler called when the scan completes.
   * @param callback The handler to invoke, or nullptr to clear.
   */
  void onComplete(CompleteHandler callback);

  /**
   * @brief Remove all registered scan callbacks.
   */
  void resetCallbacks();

  // --- Result Management ---

  /**
   * @brief Retrieve a snapshot of the current scan results.
   * @return Copy of the accumulated scan results.
   */
  Results getResults();

  /**
   * @brief Discard all accumulated scan results.
   */
  void clearResults();

  /**
   * @brief Remove a specific device from the result set.
   * @param address BLE address of the device to erase.
   */
  void erase(const BTAddress &address);

  // --- Extended Scanning (BLE5) ---

  /**
   * @brief Scan timing and PHY choice for @ref startExtended.
   */
  struct ExtScanConfig {
    BLEPhy phy = BLEPhy::PHY_1M;  ///< PHY to scan on.
    uint16_t interval = 0;        ///< Scan interval in milliseconds (0 = use the value from setInterval()).
    uint16_t window = 0;          ///< Scan window in milliseconds (0 = use the value from setWindow()).
  };

  /**
   * @brief Start a BLE5 extended scan.
   * @param durationMs Scan duration in milliseconds. 0 = indefinite.
   * @param codedConfig Optional scan parameters for the Coded PHY. nullptr to skip.
   * @param uncodedConfig Optional scan parameters for the 1M / 2M PHY. nullptr to skip.
   * @return BTStatus indicating success or error.
   * @note Requires BLE5_SUPPORTED.
   */
  BTStatus startExtended(uint32_t durationMs, const ExtScanConfig *codedConfig = nullptr, const ExtScanConfig *uncodedConfig = nullptr);

  /**
   * @brief Stop a running BLE5 extended scan.
   * @return BTStatus indicating success or error.
   */
  BTStatus stopExtended();

  // --- Periodic Sync (BLE5) ---

  /**
   * @brief Create a synchronization with a periodic advertiser.
   * @param addr Address of the periodic advertiser.
   * @param sid Advertising SID to synchronize with.
   * @param skipCount Max number of periodic events the receiver may skip. 0 = none.
   * @param timeoutMs Synchronization supervision timeout in milliseconds.
   * @return BTStatus indicating success or error.
   */
  BTStatus createPeriodicSync(const BTAddress &addr, uint8_t sid, uint16_t skipCount = 0, uint16_t timeoutMs = 10000);

  /**
   * @brief Cancel a pending periodic sync creation.
   * @return BTStatus indicating success or error.
   */
  BTStatus cancelPeriodicSync();

  /**
   * @brief Terminate an established periodic sync.
   * @param syncHandle Handle returned by the stack when sync was established.
   * @return BTStatus indicating success or error.
   */
  BTStatus terminatePeriodicSync(uint16_t syncHandle);

  /**
   * @brief Callback invoked when periodic sync is established.
   * @param syncHandle Stack-assigned sync handle.
   * @param sid Advertising SID.
   * @param addr Advertiser address.
   * @param phy PHY used for the periodic train.
   * @param interval Periodic advertising interval.
   */
  using PeriodicSyncHandler = std::function<void(uint16_t syncHandle, uint8_t sid, const BTAddress &addr, BLEPhy phy, uint16_t interval)>;

  /**
   * @brief Callback invoked for each periodic advertising report.
   * @param syncHandle Sync handle identifying the periodic train.
   * @param rssi Received signal strength indicator.
   * @param txPower Transmit power level reported by the advertiser.
   * @param data Pointer to the periodic advertising payload.
   * @param len Length of the payload in bytes.
   */
  using PeriodicReportHandler = std::function<void(uint16_t syncHandle, int8_t rssi, int8_t txPower, const uint8_t *data, size_t len)>;

  /**
   * @brief Callback invoked when a periodic sync is lost (supervision timeout).
   * @param syncHandle Sync handle that was lost.
   */
  using PeriodicLostHandler = std::function<void(uint16_t syncHandle)>;

  /**
   * @brief Register a handler for periodic sync establishment events.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onPeriodicSync(PeriodicSyncHandler handler);

  /**
   * @brief Register a handler for periodic advertising report events.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onPeriodicReport(PeriodicReportHandler handler);

  /**
   * @brief Register a handler for periodic sync lost events.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onPeriodicLost(PeriodicLostHandler handler);

  struct Impl;

private:
  explicit BLEScan(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  /**
   * @brief Internal teardown hook: terminate every periodic sync the library is
   *        tracking. Called by BLEClass::end() while the stack is still up so
   *        applications are not required to call terminatePeriodicSync() before
   *        BLE.end(). No-op on backends without BLE5 periodic-sync support.
   */
  void terminateAllPeriodicSyncs();

  friend class BLEClass;
};

#endif /* BLE_ENABLED */
