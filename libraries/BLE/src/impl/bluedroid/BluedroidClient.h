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

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "impl/common/BLEClientImpl.h"
#include "BLERemoteService.h"
#include "impl/common/BLESync.h"

#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include <atomic>
#include <vector>
#include <memory>

/**
 * @brief Bluedroid GATT-client implementation (@c BLEClient::Impl).
 *
 * Defined in @c impl/bluedroid/, so everything here is Bluedroid-specific; it inherits the
 * stack-agnostic @c BLEClientImplCommon, giving one uniform @c impl.member type.
 */
struct BLEClient::Impl : BLEClientImplCommon {
  // connId and mtu are written on the BTC host task (GATTC connect/disconnect/
  // MTU events) and read on the user task (disconnect/getMTU/getConnInfo), so
  // they are atomic for well-defined concurrent access without locking across
  // callbacks (mirrors the atomic `connected` in the common base).
  std::atomic<uint16_t> connId{0xFFFF};
  esp_gatt_if_t gattcIf = ESP_GATT_IF_NONE;
  uint16_t appId = 0;
  std::atomic<uint16_t> mtu{23};

  // Link security state. Unlike NimBLE (which exposes the live security level via
  // ble_gap_conn_find), Bluedroid has no GATTC query for it, so it is latched here
  // from the AUTH_CMPL GAP event and cleared on (re)connect/disconnect. Written on
  // the BTC host task, read on the user task via getConnInfo(), hence atomic.
  std::atomic<bool> secEncrypted{false};
  std::atomic<bool> secAuthenticated{false};
  std::atomic<bool> secBonded{false};

  BLESync regSync;       // For blocking GATTC app registration
  BLESync connectSync;   // For blocking connect
  BLESync discoverSync;  // For blocking service discovery
  // Only one GATT read/write can be in-flight per client at a time because
  // Bluedroid delivers completions without a per-characteristic token.
  BLESync readSync;
  BLESync writeSync;
  BLESync mtuSync;   // For MTU exchange
  BLESync rssiSync;  // For RSSI read
#if BLE5_SUPPORTED
  // Bridges async GAP PHY/DLE completions to the blocking public setPhy/getPhy/setDataLen APIs.
  BLESync phySync;
  BLESync dataLenSync;
  BLEPhy pendingTxPhy = BLEPhy::PHY_1M;
  BLEPhy pendingRxPhy = BLEPhy::PHY_1M;
  // Cached live PHY for getConnInfo() (updated from READ_PHY / PHY_UPDATE events).
  uint8_t cachedTxPhy = 1;
  uint8_t cachedRxPhy = 1;
#endif

  std::vector<std::shared_ptr<BLERemoteService::Impl>> discoveredServices;

  // Temp buffer for read operations
  std::vector<uint8_t> readBuf;
  esp_gatt_status_t lastReadStatus = ESP_GATT_OK;
  esp_gatt_status_t lastWriteStatus = ESP_GATT_OK;
  int8_t lastRssi = -128;

  ~Impl();

  static uint16_t s_nextAppId;
  static std::vector<BLEClient::Impl *> s_clients;
  static void handleGATTC(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

/**
 * @brief Register a new GATTC application with Bluedroid for @p impl.
 * @return true on successful registration; false on allocation or registration failure.
 */
bool bluedroidRegisterClient(const std::shared_ptr<BLEClient::Impl> &impl);

#endif /* BLE_BLUEDROID */
