// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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
#include <esp_openthread.h>
#include <Arduino.h>

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

private:
  static otInstance *mInstance;
  DataSet mCurrentDataSet;
};

extern OpenThread OThread;

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
