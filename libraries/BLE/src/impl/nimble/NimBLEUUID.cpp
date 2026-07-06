/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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

/**
 * @file NimBLEUUID.cpp
 * @brief Conversions between the public BLEUUID and NimBLE's ble_uuid_any_t.
 */

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLEUUID.h"

// BLEUUID stores 128-bit UUIDs big-endian; NimBLE stores them little-endian.
void nimbleUuidFromPublic(const BLEUUID &uuid, ble_uuid_any_t &out) {
  switch (uuid.bitSize()) {
    case 16:
    {
      out.u.type = BLE_UUID_TYPE_16;
      out.u16.value = uuid.toUint16();
      break;
    }
    case 32:
    {
      out.u.type = BLE_UUID_TYPE_32;
      out.u32.value = uuid.toUint32();
      break;
    }
    case 128:
    {
      out.u.type = BLE_UUID_TYPE_128;
      const uint8_t *be = uuid.data();
      for (int i = 0; i < 16; i++) {
        out.u128.value[15 - i] = be[i];
      }
      break;
    }
    default:
    {
      BLEUUID u128 = uuid.to128();
      out.u.type = BLE_UUID_TYPE_128;
      const uint8_t *be = u128.data();
      for (int i = 0; i < 16; i++) {
        out.u128.value[15 - i] = be[i];
      }
      break;
    }
  }
}

BLEUUID nimbleUuidToPublic(const ble_uuid_any_t &u) {
  if (u.u.type == BLE_UUID_TYPE_16) {
    return BLEUUID(static_cast<uint16_t>(u.u16.value));
  } else if (u.u.type == BLE_UUID_TYPE_32) {
    return BLEUUID(static_cast<uint32_t>(u.u32.value));
  } else {
    return BLEUUID(u.u128.value, 16, true);
  }
}

#endif /* BLE_NIMBLE */
