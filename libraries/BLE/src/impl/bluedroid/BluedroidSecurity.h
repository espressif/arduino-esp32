/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 chegewara
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

#if BLE_BLUEDROID

#include <esp_gap_ble_api.h>
#include <freertos/semphr.h>

struct BLESecurity::Impl {
  IOCapability ioCap = NoInputNoOutput;
  bool bonding = true;
  bool mitm = false;
  bool secureConnection = false;
  uint32_t staticPassKey = 0;
  bool passKeySet = false;
  bool regenOnConnect = false;
  bool forceAuth = false;
  KeyDist initiatorKeys = KeyDist::EncKey | KeyDist::IdKey;
  KeyDist responderKeys = KeyDist::EncKey | KeyDist::IdKey;
  uint8_t keySize = 16;

  BLESync authSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  PassKeyRequestHandler passKeyRequestCb;
  PassKeyDisplayHandler passKeyDisplayCb;
  ConfirmPassKeyHandler confirmPassKeyCb;
  SecurityRequestHandler securityRequestCb;
  AuthorizationHandler authorizationCb;
  AuthCompleteHandler authCompleteCb;
  BondStoreOverflowHandler bondOverflowCb;

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static Impl *s_instance;

  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void applySecurityParams();
};

#endif /* BLE_BLUEDROID */
