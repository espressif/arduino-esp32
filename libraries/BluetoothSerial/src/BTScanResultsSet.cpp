/*
 * BTScanResultsSet.cpp
 *
 *  Created on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include <esp_err.h>

#include "BTAdvertisedDevice.h"
#include "BTScan.h"
//#include "GeneralUtils.h"
#include "esp32-hal-log.h"

class BTAdvertisedDevice;

/**
 * @brief Dump the scan results to the log.
 */
void BTScanResultsSet::dump(Print *print) {
  int cnt = getCount();
  if (print == nullptr) {
    log_v(">> Dump scan results : %d", cnt);
    for (int i = 0; i < cnt; i++) {
      BTAdvertisedDevice *dev = getDevice(i);
      if (dev) {
        log_d("- %d: %s\n", i + 1, dev->toString().c_str());
      } else {
        log_d("- %d is null\n", i + 1);
      }
    }
    log_v("-- dump finished --");
  } else {
    print->printf(">> Dump scan results: %d\n", cnt);
    for (int i = 0; i < cnt; i++) {
      BTAdvertisedDevice *dev = getDevice(i);
      if (dev) {
        print->printf("- %d: %s\n", i + 1, dev->toString().c_str());
      } else {
        print->printf("- %d is null\n", i + 1);
      }
    }
    print->println("-- Dump finished --");
  }
}  // dump

/**
 * @brief Return the count of devices found in the last scan.
 * @return The number of devices found in the last scan.
 */
int BTScanResultsSet::getCount() {
  return m_vectorAdvertisedDevices.size();
}  // getCount

/**
 * @brief Return the specified device at the given index.
 * The index should be between 0 and getCount()-1.
 * @param [in] i The index of the device.
 * @return The device at the specified index.
 */
BTAdvertisedDevice *BTScanResultsSet::getDevice(int i) {
  if (i < 0) {
    return nullptr;
  }

  int x = 0;
  BTAdvertisedDeviceSet *pDev = &m_vectorAdvertisedDevices.begin()->second;
  for (auto it = m_vectorAdvertisedDevices.begin(); it != m_vectorAdvertisedDevices.end(); it++) {
    pDev = &it->second;
    if (x == i) {
      break;
    }
    x++;
  }
  return x == i ? pDev : nullptr;
}

void BTScanResultsSet::clear() {
  //for(auto _dev : m_vectorAdvertisedDevices)
  //	delete _dev.second;
  m_vectorAdvertisedDevices.clear();
}

bool BTScanResultsSet::add(BTAdvertisedDeviceSet advertisedDevice, bool unique) {
  std::string key = std::string(advertisedDevice.getAddress().toString().c_str(), advertisedDevice.getAddress().toString().length());
  if (!unique || m_vectorAdvertisedDevices.count(key) == 0) {
    m_vectorAdvertisedDevices.insert(std::pair<std::string, BTAdvertisedDeviceSet>(key, advertisedDevice));
    return true;
  } else {
    return false;
  }
}

#endif
