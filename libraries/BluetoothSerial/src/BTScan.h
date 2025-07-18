/*
 * BTScan.h
 *
 *  Created on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#pragma once
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if SOC_BT_SUPPORTED && defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include <map>
#include <string>
#include <Print.h>
#include "BTAddress.h"
#include "BTAdvertisedDevice.h"

class BTAdvertisedDevice;
class BTAdvertisedDeviceSet;

class BTScanResults {
public:
  virtual ~BTScanResults() = default;

  virtual void dump(Print *print = nullptr) = 0;
  virtual int getCount() = 0;
  virtual BTAdvertisedDevice *getDevice(int i) = 0;
};

class BTScanResultsSet : public BTScanResults {
public:
  void dump(Print *print = nullptr);
  int getCount();
  BTAdvertisedDevice *getDevice(int i);

  bool add(BTAdvertisedDeviceSet advertisedDevice, bool unique = true);
  void clear();

  std::map<std::string, BTAdvertisedDeviceSet> m_vectorAdvertisedDevices;
};

#endif
