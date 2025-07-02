/*
 * BLEEddystoneURL.cpp
 *
 *  Created on: Mar 12, 2018
 *      Author: pcbreflux
 *  Upgraded on: Feb 20, 2023
 *      By: Tomas Pilny
 *
 */

#ifndef _BLEEddystoneURL_H_
#define _BLEEddystoneURL_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "BLEUUID.h"
#include <BLEAdvertisedDevice.h>
#include "esp_bt.h"

#define EDDYSTONE_URL_FRAME_TYPE 0x10

extern String EDDYSTONE_URL_PREFIX[];
extern String EDDYSTONE_URL_SUFFIX[];

/**
 * @brief Representation of a beacon.
 * See:
 * * https://github.com/google/eddystone
 */
class BLEEddystoneURL {
public:
  BLEEddystoneURL();
  BLEEddystoneURL(BLEAdvertisedDevice *advertisedDevice);
  String getData();
  String getFrame();
  BLEUUID getUUID();
  int8_t getPower();
  String getURL();
  String getPrefix();
  String getSuffix();
  String getDecodedURL();
  void setData(String data);
  void setUUID(BLEUUID l_uuid);
  void setPower(int8_t advertisedTxPower);
  void setPower(esp_power_level_t advertisedTxPower);
  void setURL(String url);
  int setSmartURL(String url);

private:
  uint8_t
    lengthURL;  // Describes the length of the URL part including prefix and optional suffix - max 18 B (excluding TX power, frame type and preceding header)
  struct {
    int8_t advertisedTxPower;
    uint8_t url[18];  // Byte [0] is for prefix. Last valid byte **can** contain suffix - i.e. the next byte after the URL
  } __attribute__((packed)) m_eddystoneData;
  void _initHeadder();
  char BLEHeadder[12];
};  // BLEEddystoneURL

#endif /* SOC_BLE_SUPPORTED */
#endif /* _BLEEddystoneURL_H_ */
