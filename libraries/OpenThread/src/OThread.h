// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

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
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include <openthread/thread.h>
#include <openthread/link.h>
#include <openthread/netdata.h>
#include <openthread/ip6.h>
#include <openthread/dataset_ftd.h>
#if CONFIG_OPENTHREAD_JOINER
#include <openthread/joiner.h>
#endif
#if CONFIG_OPENTHREAD_COMMISSIONER
#include <openthread/commissioner.h>
#endif
#include <esp_openthread.h>
#include "esp_openthread_lock.h"

#include <Arduino.h>
#include "IPAddress.h"
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/** @brief Thread device role reported by OpenThread::otGetDeviceRole(). */
typedef enum {
  OT_ROLE_DISABLED = 0,  ///< The Thread stack is disabled.
  OT_ROLE_DETACHED = 1,  ///< Not currently participating in a Thread network/partition.
  OT_ROLE_CHILD = 2,     ///< The Thread Child role.
  OT_ROLE_ROUTER = 3,    ///< The Thread Router role.
  OT_ROLE_LEADER = 4,    ///< The Thread Leader role.
} ot_device_role_t;

/** @brief Human-readable role names indexed by ot_device_role_t. */
extern const char *otRoleString[];

/**
 * @brief Builder for a Thread Operational Dataset (network name, key, channel, …).
 *
 * Configure the parameters, then hand the dataset to OpenThread::commitDataSet()
 * (or apply() directly) to provision a network without using CLI commands.
 */
class DataSet {
public:
  /** @brief Construct an empty dataset. */
  DataSet();

  /** @brief Reset all fields to their default (unset) values. */
  void clear();

  /** @brief Initialize a new complete dataset with sensible random defaults. */
  void initNew();

  /** @brief Access the underlying OpenThread dataset structure. */
  const otOperationalDataset &getDataset() const;

  // Setters

  /** @brief Set the network name. @param name Null-terminated network name. */
  void setNetworkName(const char *name);

  /** @brief Set the Extended PAN ID. @param extPanId 8-byte array. */
  void setExtendedPanId(const uint8_t *extPanId);

  /** @brief Set the network key. @param key 16-byte (OT_NETWORK_KEY_SIZE) array. */
  void setNetworkKey(const uint8_t *key);

  /** @brief Set the IEEE 802.15.4 channel. @param channel Channel 11..26. */
  void setChannel(uint8_t channel);

  /** @brief Set the PAN ID. @param panId 16-bit PAN ID. */
  void setPanId(uint16_t panId);

  // Getters

  /** @brief Get the network name. */
  const char *getNetworkName() const;

  /** @brief Get the Extended PAN ID (8 bytes). */
  const uint8_t *getExtendedPanId() const;

  /** @brief Get the network key (16 bytes). */
  const uint8_t *getNetworkKey() const;

  /** @brief Get the IEEE 802.15.4 channel. */
  uint8_t getChannel() const;

  /** @brief Get the PAN ID. */
  uint16_t getPanId() const;

  /**
   * @brief Apply this dataset to an OpenThread instance as the active dataset.
   * @param instance Target OpenThread instance.
   */
  void apply(otInstance *instance);

private:
  otOperationalDataset mDataset;
};

/**
 * @brief Arduino front-end for the native OpenThread stack and Thread network.
 *
 * Wraps the OpenThread C API with typed methods to initialize the stack, form or
 * join a network, query state, and run the Joiner/Commissioner roles. A global
 * instance named `OThread` is provided.
 */
class OpenThread {
public:
  /** @brief Indicates whether the OpenThread stack is running. */
  static bool otStarted;

  /** @brief Get the current Thread device role. */
  static ot_device_role_t otGetDeviceRole();

  /** @brief Get the current Thread device role as a string (e.g. "Leader"). */
  static const char *otGetStringDeviceRole();

  /**
   * @brief Print network information (name, channel, PAN ID, addresses) to a stream.
   * @param output Destination stream (e.g. Serial).
   */
  static void otPrintNetworkInformation(Stream &output);

  OpenThread();
  ~OpenThread();

  /** @brief true if the OpenThread stack is running. */
  operator bool() const;

  /**
   * @brief Initialize the OpenThread stack.
   * @param OThreadAutoStart When true, auto-start Thread using the NVS dataset.
   */
  static void begin(bool OThreadAutoStart = true);

  /** @brief Deinitialize the OpenThread stack and release resources.
   *
   * Stops CLI, CoAP servers, and pending clients before exiting the worker
   * mainloop. Application code should also call ``OThreadUDP.stop()`` on open
   * sockets and destroy CoAP client objects before ``end()``; ``end()`` repeats
   * CLI/CoAP cleanup as a safety net.
   */
  static void end();

  /** @brief Start the Thread network (equivalent to CLI "thread start"). */
  void start();

  /** @brief Stop the Thread network (equivalent to CLI "thread stop"). */
  void stop();

  /** @brief Bring up the Thread network interface (equivalent to "ifconfig up"). */
  void networkInterfaceUp();

  /** @brief Bring down the Thread network interface (equivalent to "ifconfig down"). */
  void networkInterfaceDown();

  /**
   * @brief Commit a dataset to the OpenThread instance as the active dataset.
   * @param dataset Dataset describing the network parameters.
   */
  void commitDataSet(const DataSet &dataset);

  /**
   * @brief Whether a committed Active Operational Dataset exists.
   *
   * The dataset may have been loaded from NVS during begin() or committed by the
   * application. Useful to decide whether to resume an existing Thread network or
   * provision a new one.
   * @return true if an active dataset is present.
   */
  bool hasActiveDataset() const;

  /** @brief Get the node network name. */
  String getNetworkName() const;

  /** @brief Get the node Extended PAN ID (8 bytes). */
  const uint8_t *getExtendedPanId() const;

  /** @brief Get the node network key (16 bytes). */
  const uint8_t *getNetworkKey() const;

  /** @brief Get the node IEEE 802.15.4 channel. */
  uint8_t getChannel() const;

  /** @brief Get the node PAN ID. */
  uint16_t getPanId() const;

  /**
   * @name Live setters
   * Applied directly on the running OpenThread instance. Useful when joining via
   * Commissioner/Joiner where the full Active Operational Dataset is not built
   * locally (see commitDataSet()/DataSet for the offline path). Stop the Thread
   * protocol before changing these on an already-attached device.
   * @{
   */
  /** @brief Set the IEEE 802.15.4 channel. @param channel Channel 11..26. */
  otError setChannel(uint8_t channel);
  /** @brief Set the PAN ID. @param panid 16-bit PAN ID. */
  otError setPanId(uint16_t panid);
  /** @brief Set the Extended PAN ID. @param extpanid 8-byte array. */
  otError setExtendedPanId(const uint8_t *extpanid);
  /** @brief Set the network key. @param key 16-byte array. */
  otError setNetworkKey(const uint8_t *key);
  /** @brief Set the network name. @param name Null-terminated network name. */
  otError setNetworkName(const char *name);
  /** @} */

  /**
   * @brief Factory-assigned EUI-64 (HW-derived 802.15.4 extended address).
   * @param out Receives the 8-byte EUI-64.
   * @return true on success.
   */
  bool getEui64(uint8_t out[8]) const;

  /** @brief Factory-assigned EUI-64 as a hex String. */
  String getEui64() const;

  /** @brief Currently configured 802.15.4 extended address (may differ from EUI-64). */
  const uint8_t *getExtendedAddress() const;

  /** @brief Thread protocol version (e.g. 4 == 1.3). */
  uint16_t getThreadVersion() const;

  /** @brief Human-readable OpenThread stack version string. */
  String getVersionString() const;

  /** @brief Radio transmit power in dBm. @return Power, or INT8_MIN on failure. */
  int8_t getTxPower() const;

  /** @brief Sleepy End Device data poll period in milliseconds. */
  uint32_t getPollPeriod() const;

  // Joiner (Thread Commissioning) ---------------------------------------
#if CONFIG_OPENTHREAD_JOINER
  /**
   * @brief Synchronously run the Joiner state machine to obtain a dataset.
   *
   * Requires `CONFIG_OPENTHREAD_JOINER=y`. The IPv6 stack must be up
   * (networkInterfaceUp()) and the Thread protocol must NOT be started yet. On
   * success the active dataset is provisioned by the commissioner; call start()
   * afterwards to enable Thread.
   * @param pskd            Pre-Shared Key for the Device (6..32 base32-thread chars).
   * @param timeoutMs       How long to block before giving up (ms).
   * @param provisioningUrl Optional provisioning URL.
   * @param vendorName      Optional vendor name.
   * @param vendorModel     Optional vendor model.
   * @param vendorSwVersion Optional vendor software version.
   * @param vendorData      Optional vendor-specific data.
   * @return OT_ERROR_NONE on success, otherwise an OpenThread error.
   */
  otError startJoiner(
    const char *pskd, uint32_t timeoutMs = 30000, const char *provisioningUrl = nullptr, const char *vendorName = nullptr, const char *vendorModel = nullptr,
    const char *vendorSwVersion = nullptr, const char *vendorData = nullptr
  );
  /** @brief Abort an in-progress Joiner operation. */
  void stopJoiner();
  /** @brief Get the live Joiner state machine state. */
  otJoinerState getJoinerState() const;
  /** @brief Get the device's Joiner ID (extended address used during commissioning). */
  const otExtAddress *getJoinerId() const;
#endif /* CONFIG_OPENTHREAD_JOINER */

  // Commissioner (Thread Commissioning leader side) ---------------------
#if CONFIG_OPENTHREAD_COMMISSIONER
  /**
   * @brief Petition the network to become the active Commissioner.
   *
   * Requires `CONFIG_OPENTHREAD_COMMISSIONER=y`. Blocks until
   * OT_COMMISSIONER_STATE_ACTIVE is reached or the timeout fires. The device must
   * already be attached to a Thread network (e.g. as Leader).
   * @param timeoutMs How long to block before giving up (ms).
   * @return OT_ERROR_NONE on success, otherwise an OpenThread error.
   */
  otError startCommissioner(uint32_t timeoutMs = 30000);
  /**
   * @brief Add a Joiner entry this commissioner will accept.
   * @param pskd       Pre-Shared Key for the Device the joiner must present.
   * @param timeoutSec Lifetime of the joiner entry on the commissioner (seconds).
   * @param eui64      Specific joiner EUI-64, or nullptr to accept any joiner.
   * @return OT_ERROR_NONE on success, otherwise an OpenThread error.
   */
  otError addJoiner(const char *pskd, uint32_t timeoutSec = 120, const otExtAddress *eui64 = nullptr);
  /** @brief Stop acting as Commissioner. */
  void stopCommissioner();
  /** @brief Get the live Commissioner state. */
  otCommissionerState getCommissionerState() const;
#endif /* CONFIG_OPENTHREAD_COMMISSIONER */

  /** @brief Get the underlying OpenThread instance for direct native API calls. */
  otInstance *getInstance();

  /** @brief Get the current dataset tracked by this instance. */
  const DataSet &getCurrentDataSet() const;

  /** @brief Get the Mesh-Local prefix. */
  const otMeshLocalPrefix *getMeshLocalPrefix() const;

  /** @brief Get the Mesh-Local EID address. */
  IPAddress getMeshLocalEid() const;

  /** @brief Get the Thread Leader RLOC address. */
  IPAddress getLeaderRloc() const;

  /** @brief Get this node's RLOC address. */
  IPAddress getRloc() const;

  /** @brief Get this node's RLOC16 identifier. */
  uint16_t getRloc16() const;

  /**
   * @name IPv6 address management (cached)
   * @{
   */
  /** @brief Number of unicast addresses on the interface. */
  size_t getUnicastAddressCount() const;
  /** @brief Get a unicast address by index (< getUnicastAddressCount()). */
  IPAddress getUnicastAddress(size_t index) const;
  /** @brief Get all unicast addresses as a vector. */
  std::vector<IPAddress> getAllUnicastAddresses() const;

  /** @brief Number of multicast addresses on the interface. */
  size_t getMulticastAddressCount() const;
  /** @brief Get a multicast address by index (< getMulticastAddressCount()). */
  IPAddress getMulticastAddress(size_t index) const;
  /** @brief Get all multicast addresses as a vector. */
  std::vector<IPAddress> getAllMulticastAddresses() const;
  /** @} */

  /**
   * @name Multicast group membership
   * Subscribe/unsubscribe the Thread interface to IPv6 multicast groups so the
   * node receives datagrams sent to them (e.g. a CoAP/UDP group address).
   * @{
   */
  /**
   * @brief Subscribe the interface to an IPv6 multicast group.
   * @param group IPv6 multicast address (first byte 0xFF).
   * @return true on success or if already subscribed; false on error or if the
   *         stack is not running / @p group is not an IPv6 multicast address.
   */
  bool subscribeMulticast(const IPAddress &group);
  /**
   * @brief Unsubscribe the interface from an IPv6 multicast group.
   * @param group IPv6 multicast address previously passed to subscribeMulticast().
   * @return true on success; false on error or if the stack is not running.
   */
  bool unsubscribeMulticast(const IPAddress &group);
  /** @} */

  /**
   * @name Address cache management
   * Call when the role changes to force address re-resolution.
   * @{
   */
  /** @brief Clear the cached unicast addresses. */
  void clearUnicastAddressCache() const;
  /** @brief Clear the cached multicast addresses. */
  void clearMulticastAddressCache() const;
  /** @brief Clear all cached addresses. */
  void clearAllAddressCache() const;
  /** @} */

private:
  static otInstance *mInstance;
  static DataSet mCurrentDataset;     // Current dataset being used by the OpenThread instance.
  static otNetworkKey mNetworkKey;    // Static storage to persist after function return
  static otExtAddress mFactoryEui64;  // Cached factory-assigned EUI-64

  // Joiner synchronization
#if CONFIG_OPENTHREAD_JOINER
  static SemaphoreHandle_t mJoinerSemaphore;
  static otError mJoinerResult;
  static void joinerCallback(otError aError, void *aContext);
#endif /* CONFIG_OPENTHREAD_JOINER */

  // Commissioner synchronization
#if CONFIG_OPENTHREAD_COMMISSIONER
  static SemaphoreHandle_t mCommissionerSemaphore;
  static otCommissionerState mCommissionerLastState;
  static void commissionerStateCallback(otCommissionerState aState, void *aContext);
#endif /* CONFIG_OPENTHREAD_COMMISSIONER */

  // Address caching for performance (user-controlled)
  mutable std::vector<IPAddress> mCachedUnicastAddresses;
  mutable std::vector<IPAddress> mCachedMulticastAddresses;

  // Internal cache management
  void populateUnicastAddressCache() const;
  void populateMulticastAddressCache() const;
};

/** @brief Global OpenThread instance. */
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_OPENTHREAD)
extern OpenThread OThread;
#endif

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
