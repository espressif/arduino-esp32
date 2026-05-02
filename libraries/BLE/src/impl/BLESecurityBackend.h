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

#pragma once

/**
 * @file
 * @brief Includes the backend-specific @c BLESecurity::Impl definition.
 */

#include "impl/BLEGuards.h"

#if BLE_ENABLED

#if BLE_NIMBLE
#include "impl/nimble/NimBLESecurity.h"
#elif BLE_BLUEDROID
#include "impl/bluedroid/BluedroidSecurity.h"
#endif

#include "BLESecurity.h"

class BLESecurityBackend {
public:
  static void notifyAuthComplete(BLESecurity &sec, const BLEConnInfo &conn, bool success) {
    sec.notifyAuthComplete(conn, success);
  }
  static bool notifyAuthorization(BLESecurity &sec, const BLEConnInfo &conn, uint16_t attrHandle, bool isRead) {
    return sec.notifyAuthorization(conn, attrHandle, isRead);
  }
  static uint32_t resolvePasskeyForDisplay(BLESecurity &sec, const BLEConnInfo &conn) {
    return sec.resolvePasskeyForDisplay(conn);
  }
  static uint32_t resolvePasskeyForInput(BLESecurity &sec, const BLEConnInfo &conn) {
    return sec.resolvePasskeyForInput(conn);
  }
  static bool resolveNumericComparison(BLESecurity &sec, const BLEConnInfo &conn, uint32_t numcmp) {
    return sec.resolveNumericComparison(conn, numcmp);
  }
  static bool notifyBondOverflow(BLESecurity &sec, const BTAddress &oldest) {
    return sec.notifyBondOverflow(oldest);
  }
};

#endif /* BLE_ENABLED */
