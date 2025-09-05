/*
 * BLEUUID.cpp
 *
 *  Created on: Jun 21, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include <string.h>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "BLEUUID.h"
#include "esp32-hal-log.h"

/*****************************************************************************
 *                             Common functions                              *
 *****************************************************************************/

static void memrcpy(uint8_t *target, uint8_t *source, uint32_t size) {
  assert(size > 0);
  target += (size - 1);  // Point target to the last byte of the target data
  while (size > 0) {
    *target = *source;
    target--;
    source++;
    size--;
  }
}  // memrcpy

BLEUUID::BLEUUID() {
  m_valueSet = false;
}  // BLEUUID

BLEUUID::BLEUUID(String value) {
  m_valueSet = true;
  if (value.length() == 4) {
    UUID_LEN(m_uuid) = BLE_UUID_16_BITS;
    UUID_VAL_16(m_uuid) = 0;
    for (int i = 0; i < value.length();) {
      uint8_t MSB = value.c_str()[i];
      uint8_t LSB = value.c_str()[i + 1];

      if (MSB > '9') {
        MSB -= 7;
      }
      if (LSB > '9') {
        LSB -= 7;
      }
      UUID_VAL_16(m_uuid) += (((MSB & 0x0F) << 4) | (LSB & 0x0F)) << (2 - i) * 4;
      i += 2;
    }
  } else if (value.length() == 8) {
    UUID_LEN(m_uuid) = BLE_UUID_32_BITS;
    UUID_VAL_32(m_uuid) = 0;
    for (int i = 0; i < value.length();) {
      uint8_t MSB = value.c_str()[i];
      uint8_t LSB = value.c_str()[i + 1];

      if (MSB > '9') {
        MSB -= 7;
      }
      if (LSB > '9') {
        LSB -= 7;
      }
      UUID_VAL_32(m_uuid) += (((MSB & 0x0F) << 4) | (LSB & 0x0F)) << (6 - i) * 4;
      i += 2;
    }
  } else if (value.length() == 16) {
    UUID_LEN(m_uuid) = BLE_UUID_128_BITS;
    memrcpy(UUID_VAL_128(m_uuid), (uint8_t *)value.c_str(), 16);
  } else if (value.length() == 36) {
    UUID_LEN(m_uuid) = BLE_UUID_128_BITS;
    int n = 0;
    for (int i = 0; i < value.length();) {
      if (value.c_str()[i] == '-') {
        i++;
      }
      uint8_t MSB = value.c_str()[i];
      uint8_t LSB = value.c_str()[i + 1];

      if (MSB > '9') {
        MSB -= 7;
      }
      if (LSB > '9') {
        LSB -= 7;
      }
      UUID_VAL_128(m_uuid)[15 - n++] = ((MSB & 0x0F) << 4) | (LSB & 0x0F);
      i += 2;
    }
  } else {
    log_e("ERROR: UUID value not 2, 4, 16 or 36 bytes");
    m_valueSet = false;
  }
}  //BLEUUID(String)

BLEUUID::BLEUUID(uint8_t *pData, size_t size, bool msbFirst) {
  if (size != 16) {
    log_e("ERROR: UUID length not 16 bytes");
    return;
  }
  UUID_LEN(m_uuid) = BLE_UUID_128_BITS;
  if (msbFirst) {
    memrcpy(UUID_VAL_128(m_uuid), pData, 16);
  } else {
    memcpy(UUID_VAL_128(m_uuid), pData, 16);
  }
  m_valueSet = true;
}  // BLEUUID

BLEUUID::BLEUUID(uint16_t uuid) {
  UUID_LEN(m_uuid) = BLE_UUID_16_BITS;
  UUID_VAL_16(m_uuid) = uuid;
  m_valueSet = true;
}  // BLEUUID

BLEUUID::BLEUUID(uint32_t uuid) {
  UUID_LEN(m_uuid) = BLE_UUID_32_BITS;
  UUID_VAL_32(m_uuid) = uuid;
  m_valueSet = true;
}  // BLEUUID

BLEUUID::BLEUUID(uint32_t first, uint16_t second, uint16_t third, uint64_t fourth) {
  UUID_LEN(m_uuid) = BLE_UUID_128_BITS;
  memcpy(UUID_VAL_128(m_uuid) + 12, &first, 4);
  memcpy(UUID_VAL_128(m_uuid) + 10, &second, 2);
  memcpy(UUID_VAL_128(m_uuid) + 8, &third, 2);
  memcpy(UUID_VAL_128(m_uuid), &fourth, 8);
  m_valueSet = true;
}

uint8_t BLEUUID::bitSize() {
  if (!m_valueSet) {
    return 0;
  }
  switch (UUID_LEN(m_uuid)) {
    case BLE_UUID_16_BITS:  return 16;
    case BLE_UUID_32_BITS:  return 32;
    case BLE_UUID_128_BITS: return 128;
    default:                log_e("Unknown UUID length: %d", UUID_LEN(m_uuid)); return 0;
  }  // End of switch
}  // bitSize

bool BLEUUID::equals(const BLEUUID &uuid) const {
  if (!m_valueSet || !uuid.m_valueSet) {
    return false;
  }

  if (UUID_LEN(uuid.m_uuid) != UUID_LEN(m_uuid)) {
    return uuid.toString() == toString();
  }

  if (UUID_LEN(uuid.m_uuid) == BLE_UUID_16_BITS) {
    return UUID_VAL_16(uuid.m_uuid) == UUID_VAL_16(m_uuid);
  }

  if (UUID_LEN(uuid.m_uuid) == BLE_UUID_32_BITS) {
    return UUID_VAL_32(uuid.m_uuid) == UUID_VAL_32(m_uuid);
  }

  return memcmp(UUID_VAL_128(uuid.m_uuid), UUID_VAL_128(m_uuid), 16) == 0;
}  // equals

BLEUUID BLEUUID::fromString(String _uuid) {
  uint8_t start = 0;
  if (strstr(_uuid.c_str(), "0x") != nullptr) {  // If the string starts with 0x, skip those characters.
    start = 2;
  }
  uint8_t len = _uuid.length() - start;  // Calculate the length of the string we are going to use.

  if (len == 4) {
    uint16_t x = strtoul(_uuid.substring(start, start + len).c_str(), NULL, 16);
    return BLEUUID(x);
  } else if (len == 8) {
    uint32_t x = strtoul(_uuid.substring(start, start + len).c_str(), NULL, 16);
    return BLEUUID(x);
  } else if (len == 36) {
    return BLEUUID(_uuid);
  }
  return BLEUUID();
}  // fromString

BLEUUID BLEUUID::to128() {
  if (!m_valueSet || UUID_LEN(m_uuid) == BLE_UUID_128_BITS) {
    return *this;
  }

  if (UUID_LEN(m_uuid) == BLE_UUID_16_BITS) {
    uint16_t temp = UUID_VAL_16(m_uuid);
    UUID_VAL_128(m_uuid)[15] = 0;
    UUID_VAL_128(m_uuid)[14] = 0;
    UUID_VAL_128(m_uuid)[13] = (temp >> 8) & 0xff;
    UUID_VAL_128(m_uuid)[12] = temp & 0xff;
  } else if (UUID_LEN(m_uuid) == BLE_UUID_32_BITS) {
    uint16_t temp = UUID_VAL_32(m_uuid);
    UUID_VAL_128(m_uuid)[15] = (temp >> 24) & 0xff;
    UUID_VAL_128(m_uuid)[14] = (temp >> 16) & 0xff;
    UUID_VAL_128(m_uuid)[13] = (temp >> 8) & 0xff;
    UUID_VAL_128(m_uuid)[12] = temp & 0xff;
  }

  UUID_VAL_128(m_uuid)[11] = 0x00;
  UUID_VAL_128(m_uuid)[10] = 0x00;
  UUID_VAL_128(m_uuid)[9] = 0x10;
  UUID_VAL_128(m_uuid)[8] = 0x00;
  UUID_VAL_128(m_uuid)[7] = 0x80;
  UUID_VAL_128(m_uuid)[6] = 0x00;
  UUID_VAL_128(m_uuid)[5] = 0x00;
  UUID_VAL_128(m_uuid)[4] = 0x80;
  UUID_VAL_128(m_uuid)[3] = 0x5f;
  UUID_VAL_128(m_uuid)[2] = 0x9b;
  UUID_VAL_128(m_uuid)[1] = 0x34;
  UUID_VAL_128(m_uuid)[0] = 0xfb;

  UUID_LEN(m_uuid) = BLE_UUID_128_BITS;
  return *this;
}  // to128

BLEUUID BLEUUID::to16() {
  if (!m_valueSet || UUID_LEN(m_uuid) == BLE_UUID_16_BITS) {
    return *this;
  }

  if (UUID_LEN(m_uuid) == BLE_UUID_128_BITS) {
    uint8_t base128[] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
    if (memcmp(UUID_VAL_128(m_uuid), base128, sizeof(base128)) == 0) {
      *this = BLEUUID(*(uint16_t *)(UUID_VAL_128(m_uuid) + 12));
    }
  }

  return *this;
}

String BLEUUID::toString() const {
  if (!m_valueSet) {
    return "<NULL>";  // If we have no value, nothing to format.
  }

  if (UUID_LEN(m_uuid) == BLE_UUID_16_BITS) {  // If the UUID is 16bit, pad correctly.
    char hex[9];
    snprintf(hex, sizeof(hex), "%08x", UUID_VAL_16(m_uuid));
    return String(hex) + "-0000-1000-8000-00805f9b34fb";
  }  // End 16bit UUID

  if (UUID_LEN(m_uuid) == BLE_UUID_32_BITS) {  // If the UUID is 32bit, pad correctly.
    char hex[9];
    snprintf(hex, sizeof(hex), "%08lx", UUID_VAL_32(m_uuid));
    return String(hex) + "-0000-1000-8000-00805f9b34fb";
  }  // End 32bit UUID

  // The UUID is not 16bit or 32bit which means that it is 128bit.
  auto size = 37;  // 32 for UUID data, 4 for '-' delimiters and one for a terminator == 37 chars
  char *hex = (char *)malloc(size);
  snprintf(
    hex, size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", UUID_VAL_128(m_uuid)[15], UUID_VAL_128(m_uuid)[14],
    UUID_VAL_128(m_uuid)[13], UUID_VAL_128(m_uuid)[12], UUID_VAL_128(m_uuid)[11], UUID_VAL_128(m_uuid)[10], UUID_VAL_128(m_uuid)[9], UUID_VAL_128(m_uuid)[8],
    UUID_VAL_128(m_uuid)[7], UUID_VAL_128(m_uuid)[6], UUID_VAL_128(m_uuid)[5], UUID_VAL_128(m_uuid)[4], UUID_VAL_128(m_uuid)[3], UUID_VAL_128(m_uuid)[2],
    UUID_VAL_128(m_uuid)[1], UUID_VAL_128(m_uuid)[0]
  );

  String res(hex);
  free(hex);
  return res;
}  // toString

bool BLEUUID::operator==(const BLEUUID &rhs) const {
  return equals(rhs);
}

bool BLEUUID::operator!=(const BLEUUID &rhs) const {
  return !equals(rhs);
}

/*****************************************************************************
 *                            Bluedroid functions                            *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
BLEUUID::BLEUUID(esp_bt_uuid_t uuid) {
  m_uuid = uuid;
  m_valueSet = true;
}  // BLEUUID

BLEUUID::BLEUUID(esp_gatt_id_t gattId) : BLEUUID(gattId.uuid) {}  // BLEUUID

esp_bt_uuid_t *BLEUUID::getNative() {
  if (m_valueSet == false) {
    log_v("<< Return of un-initialized UUID!");
    return nullptr;
  }
  return &m_uuid;
}  // getNative
#endif

/*****************************************************************************
 *                             NimBLE functions                              *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
BLEUUID::BLEUUID(ble_uuid_any_t uuid) {
  m_uuid = uuid;
  m_valueSet = true;
}  // BLEUUID

BLEUUID::BLEUUID(const ble_uuid128_t *uuid) {
  m_uuid.u.type = BLE_UUID_TYPE_128;
  memcpy(m_uuid.u128.value, uuid->value, 16);
  m_valueSet = true;
}  // BLEUUID

const ble_uuid_any_t *BLEUUID::getNative() const {
  if (m_valueSet == false) {
    log_v("<< Return of un-initialized UUID!");
    return nullptr;
  }
  return &m_uuid;
}  // getNative
#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
