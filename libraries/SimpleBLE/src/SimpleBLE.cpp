// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sdkconfig.h"

#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include "SimpleBLE.h"
#include "esp32-hal-log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

static esp_ble_adv_data_t _adv_config = {
  .set_scan_rsp = false,
  .include_name = true,
  .include_txpower = true,
  .min_interval = 512,
  .max_interval = 1024,
  .appearance = 0,
  .manufacturer_len = 0,
  .p_manufacturer_data = NULL,
  .service_data_len = 0,
  .p_service_data = NULL,
  .service_uuid_len = 0,
  .p_service_uuid = NULL,
  .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
};

static esp_ble_adv_params_t _adv_params = {
  .adv_int_min = 512,
  .adv_int_max = 1024,
  .adv_type = ADV_TYPE_NONCONN_IND,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .peer_addr = {
    0x00,
  },
  .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void _on_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
    esp_ble_gap_start_advertising(&_adv_params);
  }
}

static bool _init_gap(const char *name) {
  if (!btStarted() && !btStart()) {
    log_e("btStart failed");
    return false;
  }
  esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
  if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    if (esp_bluedroid_init()) {
      log_e("esp_bluedroid_init failed");
      return false;
    }
  }
  if (bt_state != ESP_BLUEDROID_STATUS_ENABLED) {
    if (esp_bluedroid_enable()) {
      log_e("esp_bluedroid_enable failed");
      return false;
    }
  }
  if (esp_ble_gap_set_device_name(name)) {
    log_e("gap_set_device_name failed");
    return false;
  }
  if (esp_ble_gap_config_adv_data(&_adv_config)) {
    log_e("gap_config_adv_data failed");
    return false;
  }
  if (esp_ble_gap_register_callback(_on_gap)) {
    log_e("gap_register_callback failed");
    return false;
  }
  return true;
}

static bool _stop_gap() {
  if (btStarted()) {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    btStop();
  }
  return true;
}

/*
 * BLE Arduino
 *
 * */

SimpleBLE::SimpleBLE() {
  local_name = "esp32";
}

SimpleBLE::~SimpleBLE(void) {
  _stop_gap();
}

bool SimpleBLE::begin(String localName) {
  if (localName.length()) {
    local_name = localName;
  }
  return _init_gap(local_name.c_str());
}

void SimpleBLE::end() {
  _stop_gap();
}

#endif
