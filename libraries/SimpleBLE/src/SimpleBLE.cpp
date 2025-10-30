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

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

#include "SimpleBLE.h"
#include "esp32-hal-log.h"

#if defined(SOC_BLE_SUPPORTED)
#include "esp_bt.h"
#endif

/***************************************************************************
 *                           Bluedroid includes                           *
 ***************************************************************************/
#if defined(CONFIG_BLUEDROID_ENABLED)
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#endif

/***************************************************************************
 *                           NimBLE includes                              *
 ***************************************************************************/
#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <store/config/ble_store_config.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>

#ifdef CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE
#include <esp_nimble_hci.h>
#endif

// Forward declaration
extern "C" void ble_store_config_init(void);
#endif

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#include "esp32-hal-hosted.h"
#endif

/***************************************************************************
 *                           Bluedroid data structures                    *
 ***************************************************************************/
#if defined(CONFIG_BLUEDROID_ENABLED)
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
  .peer_addr =
    {
      0x00,
    },
  .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
#endif

/***************************************************************************
 *                           NimBLE data structures                       *
 ***************************************************************************/
#if defined(CONFIG_NIMBLE_ENABLED)
static struct ble_hs_adv_fields _nimble_adv_fields;
static struct ble_gap_adv_params _nimble_adv_params = {
  .conn_mode = BLE_GAP_CONN_MODE_NON,
  .disc_mode = BLE_GAP_DISC_MODE_GEN,
  .itvl_min = 512,
  .itvl_max = 1024,
  .channel_map = 0,
  .filter_policy = 0,
  .high_duty_cycle = 0,
};

// Global variables for NimBLE synchronization
static bool _nimble_synced = false;
#endif

// Global state tracking
static bool _ble_initialized = false;

/***************************************************************************
 *                           Bluedroid callbacks                          *
 ***************************************************************************/
#if defined(CONFIG_BLUEDROID_ENABLED)
static void _on_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
    esp_ble_gap_start_advertising(&_adv_params);
  }
}
#endif

/***************************************************************************
 *                           NimBLE callbacks                             *
 ***************************************************************************/
#if defined(CONFIG_NIMBLE_ENABLED)
static void _nimble_host_task(void *param) {
  // This function will be called to run the BLE host
  nimble_port_run();
  // Should never reach here unless nimble_port_stop() is called
  nimble_port_freertos_deinit();
}

static void _nimble_on_reset(int reason) {
  log_i("NimBLE reset; reason=%d", reason);
}

static void _nimble_on_sync(void) {
  log_i("NimBLE sync complete");
  _nimble_synced = true;
}

static int _nimble_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE: log_d("BLE_GAP_EVENT_ADV_COMPLETE"); break;
    default:                         break;
  }
  return 0;
}
#endif

/***************************************************************************
 *                      Forward declarations                               *
 ***************************************************************************/
static bool _init_gap(const char *name);
static bool _stop_gap();
static bool _update_advertising(const char *name);

/***************************************************************************
 *                           Initialization functions                     *
 ***************************************************************************/
static bool _init_gap(const char *name) {
  if (_ble_initialized) {
    log_d("BLE already initialized, skipping");
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
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
  _ble_initialized = true;
  return true;
#elif defined(CONFIG_NIMBLE_ENABLED)
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  // Initialize esp-hosted transport for BLE HCI when explicitly enabled
  if (!hostedInitBLE()) {
    log_e("Failed to initialize ESP-Hosted for BLE");
    return false;
  }
#endif

  esp_err_t errRc = nimble_port_init();
  if (errRc != ESP_OK) {
    log_e("nimble_port_init: rc=%d", errRc);
    return false;
  }

  // Configure NimBLE host
  ble_hs_cfg.reset_cb = _nimble_on_reset;
  ble_hs_cfg.sync_cb = _nimble_on_sync;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 1;

  // Set device name
  errRc = ble_svc_gap_device_name_set(name);
  if (errRc != ESP_OK) {
    log_e("ble_svc_gap_device_name_set: rc=%d", errRc);
    return false;
  }

  // Configure advertising data
  memset(&_nimble_adv_fields, 0, sizeof(_nimble_adv_fields));
  _nimble_adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  _nimble_adv_fields.name = (uint8_t *)name;
  _nimble_adv_fields.name_len = strlen(name);
  _nimble_adv_fields.name_is_complete = 1;
  _nimble_adv_fields.tx_pwr_lvl_is_present = 1;

  // Initialize store configuration
  ble_store_config_init();

  // Start the host task
  nimble_port_freertos_init(_nimble_host_task);

  // Wait for sync
  int sync_timeout = 1000;  // 10 seconds timeout
  while (!_nimble_synced && sync_timeout > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
    sync_timeout--;
  }

  if (!_nimble_synced) {
    log_e("NimBLE sync timeout");
    return false;
  }

  // Set advertising data
  errRc = ble_gap_adv_set_fields(&_nimble_adv_fields);
  if (errRc != ESP_OK) {
    log_e("ble_gap_adv_set_fields: rc=%d", errRc);
    return false;
  }

  // Start advertising
  errRc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &_nimble_adv_params, _nimble_gap_event, NULL);
  if (errRc != ESP_OK) {
    log_e("ble_gap_adv_start: rc=%d", errRc);
    return false;
  }

  _ble_initialized = true;
  return true;
#else
  log_e("No BLE stack enabled");
  return false;
#endif
}

static bool _stop_gap() {
  if (!_ble_initialized) {
    log_d("BLE not initialized, nothing to stop");
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  if (btStarted()) {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    btStop();
  }
  _ble_initialized = false;
  return true;
#elif defined(CONFIG_NIMBLE_ENABLED)
  // Stop advertising
  ble_gap_adv_stop();

  // Stop NimBLE
  int rc = nimble_port_stop();
  if (rc != ESP_OK) {
    log_e("nimble_port_stop: rc=%d", rc);
  }

  nimble_port_deinit();
  _nimble_synced = false;
  _ble_initialized = false;

  return true;
#else
  return true;
#endif
}

static bool _update_advertising(const char *name) {
  if (!_ble_initialized) {
    log_e("BLE not initialized");
    return false;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  // Stop current advertising
  esp_ble_gap_stop_advertising();

  // Set new device name
  if (esp_ble_gap_set_device_name(name)) {
    log_e("gap_set_device_name failed");
    return false;
  }

  // Restart advertising with new name
  if (esp_ble_gap_config_adv_data(&_adv_config)) {
    log_e("gap_config_adv_data failed");
    return false;
  }

  return true;
#elif defined(CONFIG_NIMBLE_ENABLED)
  // Stop current advertising
  ble_gap_adv_stop();

  // Set new device name
  int errRc = ble_svc_gap_device_name_set(name);
  if (errRc != ESP_OK) {
    log_e("ble_svc_gap_device_name_set: rc=%d", errRc);
    return false;
  }

  // Update advertising fields with new name
  _nimble_adv_fields.name = (uint8_t *)name;
  _nimble_adv_fields.name_len = strlen(name);
  _nimble_adv_fields.name_is_complete = 1;

  // Set new advertising data
  errRc = ble_gap_adv_set_fields(&_nimble_adv_fields);
  if (errRc != ESP_OK) {
    log_e("ble_gap_adv_set_fields: rc=%d", errRc);
    return false;
  }

  // Restart advertising
  errRc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &_nimble_adv_params, _nimble_gap_event, NULL);
  if (errRc != ESP_OK) {
    log_e("ble_gap_adv_start: rc=%d", errRc);
    return false;
  }

  return true;
#else
  log_e("No BLE stack enabled");
  return false;
#endif
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

  // If already initialized, just update advertising data
  if (_ble_initialized) {
    return _update_advertising(local_name.c_str());
  }

  return _init_gap(local_name.c_str());
}

void SimpleBLE::end() {
  _stop_gap();
}

#endif  // CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED
#endif  // SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE
