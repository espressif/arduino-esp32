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

#include "impl/common/BLEServerImpl.h"
#include "impl/common/BLESync.h"

#include <esp_gatts_api.h>
#include <esp_gap_ble_api.h>
#include <esp_timer.h>
#include <vector>
#include <utility>
#include <memory>

/**
 * @brief Bluedroid GATT-server implementation (@c BLEServer::Impl).
 *
 * Defined in @c impl/bluedroid/, so everything here is Bluedroid-specific; it inherits the
 * stack-agnostic @c BLEServerImplCommon, giving one uniform @c impl.member type. Layer is
 * disclosed by file/type: members on @c BLEServer::Impl are Bluedroid, members on
 * @c BLEServerImplCommon are shared.
 */
struct BLEServer::Impl : BLEServerImplCommon {
  esp_gatt_if_t gattsIf = ESP_GATT_IF_NONE;
  uint16_t appId = 0;

  // Prepared (long) write buffer per ATT bearer (keyed by conn_id).
  // BT Core Spec requires prepared writes to be maintained per-connection. A flat
  // vector keyed by conn_id (mirrors BLEServerImplCommon::connections) avoids
  // pulling the <unordered_map> template into every Bluedroid build; the number of
  // concurrent connections is small so the linear scan is cheap.
  struct PrepWrite {
    uint16_t handle;
    uint16_t offset;
    std::vector<uint8_t> data;
  };
  std::vector<std::pair<uint16_t, std::vector<PrepWrite>>> prepWrites;

  // Return the fragment list for connId, creating an empty one if absent.
  std::vector<PrepWrite> &prepWritesFor(uint16_t connId);
  // Return the fragment list for connId, or nullptr if none is buffered.
  std::vector<PrepWrite> *findPrepWrites(uint16_t connId);
  // Drop any buffered fragments for connId (no-op if absent).
  void erasePrepWrites(uint16_t connId);

  BLESync regSync;
  BLESync createSync;
  BLESync connectSync;
#if BLE5_SUPPORTED
  // Bridges async GAP PHY/DLE completions to the blocking public setPhy/getPhy/setDataLen APIs.
  BLESync phySync;
  BLESync dataLenSync;
  BLEPhy pendingTxPhy = BLEPhy::PHY_1M;
  BLEPhy pendingRxPhy = BLEPhy::PHY_1M;
#endif

  // Used during start() to pass handle results from async GATTS events
  uint16_t *pendingHandle = nullptr;

  // Timer for deferred advertising restart (avoids blocking the BTC task)
  esp_timer_handle_t advRestartTimer = nullptr;

  ~Impl() {
    if (advRestartTimer) {
      esp_timer_delete(advRestartTimer);
    }
  }

  static BLEServer::Impl *s_instance;
  static void invalidate();
  static void handleGATTS(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

/**
 * @brief Register the GATTS application with Bluedroid for @p server.
 * @return true on successful registration; false on registration failure or timeout.
 */
bool bluedroidRegisterGattsApp(const std::shared_ptr<BLEServer::Impl> &server);

#endif /* BLE_BLUEDROID */
