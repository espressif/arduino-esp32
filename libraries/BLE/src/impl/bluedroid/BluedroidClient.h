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

#include "BLEClient.h"
#include "BLERemoteService.h"
#include "impl/BLESync.h"

#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include "impl/BLEMutex.h"
#include <vector>
#include <memory>

struct BLEClient::Impl : std::enable_shared_from_this<BLEClient::Impl> {
  uint16_t connId = 0xFFFF;
  BTAddress peerAddress;
  bool connected = false;
  esp_gatt_if_t gattcIf = ESP_GATT_IF_NONE;
  uint16_t appId = 0;
  uint16_t mtu = 23;

  BLESync regSync;       // For blocking GATTC app registration
  BLESync connectSync;   // For blocking connect
  BLESync discoverSync;  // For blocking service discovery
  // Only one GATT read/write can be in-flight per client at a time because
  // Bluedroid delivers completions without a per-characteristic token.
  BLESync readSync;
  BLESync writeSync;
  BLESync mtuSync;       // For MTU exchange
  BLESync rssiSync;      // For RSSI read
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl();

  BLEClient::ConnectHandler onConnectCb = nullptr;
  BLEClient::DisconnectHandler onDisconnectCb = nullptr;
  BLEClient::ConnectFailHandler onConnectFailCb = nullptr;
  BLEClient::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEClient::ConnParamsReqHandler onConnParamsReqCb = nullptr;
  BLEClient::IdentityHandler onIdentityCb = nullptr;

  std::vector<std::shared_ptr<BLERemoteService::Impl>> discoveredServices;

  // Temp buffer for read operations
  std::vector<uint8_t> readBuf;
  esp_gatt_status_t lastReadStatus = ESP_GATT_OK;
  esp_gatt_status_t lastWriteStatus = ESP_GATT_OK;
  int8_t lastRssi = -128;

  static uint16_t s_nextAppId;
  static std::vector<Impl *> s_clients;
  static void handleGATTC(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  static BLEClient makeHandle(Impl *impl);
};

#endif /* BLE_BLUEDROID */
