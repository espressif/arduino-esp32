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

typedef enum {
  OT_ROLE_DISABLED = 0,  ///< The Thread stack is disabled.
  OT_ROLE_DETACHED = 1,  ///< Not currently participating in a Thread network/partition.
  OT_ROLE_CHILD = 2,     ///< The Thread Child role.
  OT_ROLE_ROUTER = 3,    ///< The Thread Router role.
  OT_ROLE_LEADER = 4,    ///< The Thread Leader role.
} ot_device_role_t;
extern const char *otRoleString[];

class DataSet {
public:
  DataSet();
  void clear();
  void initNew();
  const otOperationalDataset &getDataset() const;

  // Setters
  void setNetworkName(const char *name);
  void setExtendedPanId(const uint8_t *extPanId);
  void setNetworkKey(const uint8_t *key);
  void setChannel(uint8_t channel);
  void setPanId(uint16_t panId);

  // Getters
  const char *getNetworkName() const;
  const uint8_t *getExtendedPanId() const;
  const uint8_t *getNetworkKey() const;
  uint8_t getChannel() const;
  uint16_t getPanId() const;

  // Apply the dataset to the OpenThread instance
  void apply(otInstance *instance);

private:
  otOperationalDataset mDataset;
};

class OpenThread {
public:
  static bool otStarted;
  static ot_device_role_t otGetDeviceRole();
  static const char *otGetStringDeviceRole();
  static void otPrintNetworkInformation(Stream &output);

  OpenThread();
  ~OpenThread();
  // returns true if OpenThread Stack is running
  operator bool() const;

  // Initialize OpenThread
  static void begin(bool OThreadAutoStart = true);

  // Initialize OpenThread
  static void end();

  // Start the Thread network
  void start();

  // Stop the Thread network
  void stop();

  // Start Thread Network Interface
  void networkInterfaceUp();

  // Stop Thread Network Interface
  void networkInterfaceDown();

  // Set the dataset
  void commitDataSet(const DataSet &dataset);

  // Returns true when OpenThread has a committed Active Operational Dataset.
  // The dataset may have been loaded from NVS during begin(), or committed by
  // the application. This is useful when deciding whether to resume an existing
  // Thread network or provision a new one.
  bool hasActiveDataset() const;

  // Get the Node Network Name
  String getNetworkName() const;

  // Get the Node Extended PAN ID
  const uint8_t *getExtendedPanId() const;

  // Get the Node Network Key
  const uint8_t *getNetworkKey() const;

  // Get the Node Channel
  uint8_t getChannel() const;

  // Get the Node PAN ID
  uint16_t getPanId() const;

  // Live setters applied directly on the running OpenThread instance.
  // Useful when joining via Commissioner/Joiner where the full Active
  // Operational Dataset is not built locally (see commitDataSet/DataSet for
  // the offline path). The Thread protocol should typically be stopped before
  // changing these parameters on an already-attached device.
  otError setChannel(uint8_t channel);
  otError setPanId(uint16_t panid);
  otError setExtendedPanId(const uint8_t *extpanid);
  otError setNetworkKey(const uint8_t *key);
  otError setNetworkName(const char *name);

  // Factory-assigned EUI-64 (IEEE 802.15.4 extended address derived from HW)
  bool getEui64(uint8_t out[8]) const;
  String getEui64() const;

  // Currently configured 802.15.4 extended address (may differ from EUI-64)
  const uint8_t *getExtendedAddress() const;

  // Thread protocol version (e.g. 4 == 1.3)
  uint16_t getThreadVersion() const;

  // Human-readable OpenThread stack version string
  String getVersionString() const;

  // Radio transmit power in dBm. Returns INT8_MIN on failure.
  int8_t getTxPower() const;

  // Sleepy End Device data poll period in milliseconds
  uint32_t getPollPeriod() const;

  // Joiner (Thread Commissioning) ---------------------------------------
  // Requires CONFIG_OPENTHREAD_JOINER=y in sdkconfig.
  // Synchronously runs the Joiner state machine. The OpenThread IPv6 stack
  // must be up (networkInterfaceUp()) and the Thread protocol must NOT be
  // started yet. On success, the active dataset is provisioned by the
  // commissioner and Thread can be started with start().
#if CONFIG_OPENTHREAD_JOINER
  otError startJoiner(
    const char *pskd, const char *provisioningUrl = nullptr, const char *vendorName = nullptr, const char *vendorModel = nullptr,
    const char *vendorSwVersion = nullptr, const char *vendorData = nullptr, uint32_t timeoutMs = 30000
  );
  void stopJoiner();
  otJoinerState getJoinerState() const;
  const otExtAddress *getJoinerId() const;
#endif /* CONFIG_OPENTHREAD_JOINER */

  // Commissioner (Thread Commissioning leader side) ---------------------
  // Requires CONFIG_OPENTHREAD_COMMISSIONER=y in sdkconfig.
  // Petitions the network to become the active Commissioner. Blocks until
  // OT_COMMISSIONER_STATE_ACTIVE is reached or the timeout fires. The
  // device must already be attached to a Thread network (e.g. as Leader).
#if CONFIG_OPENTHREAD_COMMISSIONER
  otError startCommissioner(uint32_t timeoutMs = 30000);
  // Adds a Joiner entry that will be accepted by this commissioner.
  // Passing nullptr for eui64 accepts any joiner ("*" wildcard in ot-ctl).
  // timeoutSec is the lifetime of the joiner entry on the commissioner.
  otError addJoiner(const char *pskd, const otExtAddress *eui64 = nullptr, uint32_t timeoutSec = 120);
  void stopCommissioner();
  otCommissionerState getCommissionerState() const;
#endif /* CONFIG_OPENTHREAD_COMMISSIONER */

  // Get the OpenThread instance
  otInstance *getInstance();

  // Get the current dataset
  const DataSet &getCurrentDataSet() const;

  // Get the Mesh Local Prefix
  const otMeshLocalPrefix *getMeshLocalPrefix() const;

  // Get the Mesh-Local EID
  IPAddress getMeshLocalEid() const;

  // Get the Thread Leader RLOC
  IPAddress getLeaderRloc() const;

  // Get the Node RLOC
  IPAddress getRloc() const;

  // Get the RLOC16 ID
  uint16_t getRloc16() const;

  // Address management with caching
  size_t getUnicastAddressCount() const;
  IPAddress getUnicastAddress(size_t index) const;
  std::vector<IPAddress> getAllUnicastAddresses() const;

  size_t getMulticastAddressCount() const;
  IPAddress getMulticastAddress(size_t index) const;
  std::vector<IPAddress> getAllMulticastAddresses() const;

  // Cache management
  void clearUnicastAddressCache() const;
  void clearMulticastAddressCache() const;
  void clearAllAddressCache() const;

private:
  static otInstance *mInstance;
  static DataSet mCurrentDataset;        // Current dataset being used by the OpenThread instance.
  static otNetworkKey mNetworkKey;       // Static storage to persist after function return
  static otExtAddress mFactoryEui64;     // Cached factory-assigned EUI-64

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

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_OPENTHREAD)
extern OpenThread OThread;
#endif

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
