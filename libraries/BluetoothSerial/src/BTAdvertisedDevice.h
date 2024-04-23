/*
 * BTAdvertisedDevice.h
 *
 *  Created on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#ifndef __BTADVERTISEDDEVICE_H__
#define __BTADVERTISEDDEVICE_H__

#include "BTAddress.h"
#include <string>

class BTAdvertisedDevice {
public:
  virtual ~BTAdvertisedDevice() = default;

  virtual BTAddress getAddress() = 0;
  virtual uint32_t getCOD() const = 0;
  virtual std::string getName() const = 0;
  virtual int8_t getRSSI() const = 0;

  virtual bool haveCOD() const = 0;
  virtual bool haveName() const = 0;
  virtual bool haveRSSI() const = 0;

  virtual std::string toString() = 0;
};

class BTAdvertisedDeviceSet : public virtual BTAdvertisedDevice {
public:
  BTAdvertisedDeviceSet();
  //~BTAdvertisedDeviceSet() = default;

  BTAddress getAddress();
  uint32_t getCOD() const;
  std::string getName() const;
  int8_t getRSSI() const;

  bool haveCOD() const;
  bool haveName() const;
  bool haveRSSI() const;

  std::string toString();

  void setAddress(BTAddress address);
  void setCOD(uint32_t cod);
  void setName(std::string name);
  void setRSSI(int8_t rssi);

  bool m_haveCOD;
  bool m_haveName;
  bool m_haveRSSI;

  BTAddress m_address = BTAddress((uint8_t *)"\0\0\0\0\0\0");
  uint32_t m_cod;
  std::string m_name;
  int8_t m_rssi;
};

#endif
