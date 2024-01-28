/*
 * BLEEddystoneTLM.cpp
 *
 *  Created on: Mar 12, 2018
 *      Author: pcbreflux
 */

#ifndef _BLEEddystoneTLM_H_
#define _BLEEddystoneTLM_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "BLEUUID.h"
#include <BLEAdvertisedDevice.h>

#define EDDYSTONE_TLM_FRAME_TYPE 0x20
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))
#define ENDIAN_CHANGE_U32(x) ((((x)&0xFF000000) >> 24) + (((x)&0x00FF0000) >> 8)) + ((((x)&0xFF00) << 8) + (((x)&0xFF) << 24))
#define EDDYSTONE_TEMP_U16_TO_FLOAT(tempU16) (((int16_t)ENDIAN_CHANGE_U16(tempU16)) / 256.0f)
#define EDDYSTONE_TEMP_FLOAT_TO_U16(tempFloat) (ENDIAN_CHANGE_U16(((int)((tempFloat)*256))))

/**
 * @brief Representation of a beacon.
 * See:
 * * https://github.com/google/eddystone
 */
class BLEEddystoneTLM {
public:
  BLEEddystoneTLM();
  BLEEddystoneTLM(BLEAdvertisedDevice *advertisedDevice);
  String getData();
  BLEUUID getUUID();
  uint8_t getVersion();
  uint16_t getVolt();
  float getTemp();
  uint16_t getRawTemp();
  uint32_t getCount();
  uint32_t getTime();
  String toString();
  void setData(String data);
  void setUUID(BLEUUID l_uuid);
  void setVersion(uint8_t version);
  void setVolt(uint16_t volt);
  void setTemp(float temp);
  void setCount(uint32_t advCount);
  void setTime(uint32_t tmil);

private:
  BLEUUID beaconUUID;
  struct {
    uint8_t frameType;
    uint8_t version;
    uint16_t volt;
    uint16_t temp;
    uint32_t advCount;
    uint32_t tmil;
  } __attribute__((packed)) m_eddystoneData;
};  // BLEEddystoneTLM

#endif /* SOC_BLE_SUPPORTED */
#endif /* _BLEEddystoneTLM_H_ */
