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

#pragma once

#include "BLESecurity.h"
#include "impl/BLESync.h"

#if BLE_NIMBLE

#include <host/ble_hs.h>

struct BLESecurity::Impl {
  BLESync authSync;
  SemaphoreHandle_t mtx = nullptr;
  IOCapability ioCap = NoInputNoOutput;
  bool bonding = false;
  bool mitm = false;
  bool sc = true;
  bool forceAuth = false;
  bool staticPassKey = false;
  bool regenOnConnect = false;
  uint32_t passKey = 0;
  uint8_t keySize = 16;
  uint8_t initKeyDist = BLE_SM_PAIR_KEY_DIST_ENC;
  uint8_t respKeyDist = BLE_SM_PAIR_KEY_DIST_ENC;

  PassKeyRequestHandler passKeyRequestCb = nullptr;
  PassKeyDisplayHandler passKeyDisplayCb = nullptr;
  ConfirmPassKeyHandler confirmPassKeyCb = nullptr;
  SecurityRequestHandler securityRequestCb = nullptr;
  AuthorizationHandler authorizationCb = nullptr;
  AuthCompleteHandler authCompleteCb = nullptr;
  BondStoreOverflowHandler bondOverflowCb = nullptr;

  void applyToHost() const;
};

#endif /* BLE_NIMBLE */
