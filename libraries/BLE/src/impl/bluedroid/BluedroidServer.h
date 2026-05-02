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

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLEServer.h"
#include "impl/BLESync.h"

#include <esp_gatts_api.h>
#include <esp_gap_ble_api.h>
#include <esp_timer.h>
#include "impl/BLEMutex.h"
#include <unordered_map>
#include <vector>
#include <memory>

struct BLEServer::Impl : std::enable_shared_from_this<BLEServer::Impl> {
  bool started = false;
  bool advertiseOnDisconnect = true;
  esp_gatt_if_t gattsIf = ESP_GATT_IF_NONE;
  uint16_t appId = 0;

  std::vector<std::shared_ptr<BLEService::Impl>> services;

  BLEServer::ConnectHandler onConnectCb = nullptr;
  BLEServer::DisconnectHandler onDisconnectCb = nullptr;
  BLEServer::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEServer::ConnParamsHandler onConnParamsCb = nullptr;
  BLEServer::IdentityHandler onIdentityCb = nullptr;

  std::vector<std::pair<uint16_t, BLEConnInfo>> connections;

  // Prepared (long) write buffer per ATT bearer (keyed by conn_id).
  // BT Core Spec requires prepared writes to be maintained per-connection.
  struct PrepWrite {
    uint16_t handle;
    uint16_t offset;
    std::vector<uint8_t> data;
  };
  std::unordered_map<uint16_t, std::vector<PrepWrite>> prepWrites;

  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLESync regSync;
  BLESync createSync;
  BLESync connectSync;

  // Used during start() to pass handle results from async GATTS events
  uint16_t *pendingHandle = nullptr;

  // Timer for deferred advertising restart (avoids blocking the BTC task)
  esp_timer_handle_t advRestartTimer = nullptr;

  ~Impl() {
    if (advRestartTimer) {
      esp_timer_delete(advRestartTimer);
    }
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  void connSet(uint16_t connHandle, const BLEConnInfo &connInfo);
  void connErase(uint16_t connHandle);
  BLEConnInfo *connFind(uint16_t connHandle);

  static BLEServer::Impl *s_instance;
  static void invalidate();
  static void handleGATTS(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  static BLEServer makeHandle(Impl *impl);
};

#endif /* BLE_BLUEDROID */
