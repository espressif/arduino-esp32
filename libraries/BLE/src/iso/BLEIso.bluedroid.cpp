/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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
 * @file BLEIso.bluedroid.cpp
 * @brief Bluedroid host helpers for the shared IDF ISO transport.
 */

#include "core/BLEGuards.h"

#if BLE_BLUEDROID && BLE_ISO_SUPPORTED

#include "iso/BLEIso.bluedroid.h"

#if BLE_GATT_CLIENT_SUPPORTED
#include "client/BLEClient.bluedroid.h"
#endif

uint16_t BLEIsoInternal::resolveAclHandle(uint16_t connHandle) {
#if BLE_GATT_CLIENT_SUPPORTED
  return BLEClientInternal::aclHandle(connHandle);
#else
  return connHandle;
#endif
}

bool BLEIsoInternal::aclHandleLive(uint16_t connHandle) {
#if BLE_GATT_CLIENT_SUPPORTED
  return BLEClientInternal::connLive(connHandle);
#else
  return connHandle != 0xFFFF;
#endif
}

#endif /* BLE_BLUEDROID && BLE_ISO_SUPPORTED */
