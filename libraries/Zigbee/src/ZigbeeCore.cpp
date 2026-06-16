// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

/* Zigbee Core Functions */

// =====================================================================================
// ESP-Zigbee-SDK v2.x migration (ZigbeeCore) -- STEP 1 of the library migration.
//
// What was migrated here (ZigbeeCore's own responsibilities):
//   - Init/start:      esp_zb_platform_config()+esp_zb_init()  -> esp_zigbee_init(&esp_zigbee_config_t)
//                      esp_zb_start()+esp_zb_stack_main_loop()  -> esp_zigbee_start()+esp_zigbee_launch_mainloop()
//   - Storage:         the "zb_storage" partition is now NVS and is initialized via
//                      nvs_flash_init_partition("zb_storage") (the partitions.csv subtype must be `nvs`).
//   - App signals:     the weak esp_zb_app_signal_handler() override is replaced by
//                      ezb_app_signal_add_handler(); signal type/params via ezb_app_signal_get_type()/
//                      ezb_app_signal_get_params(); there is no more `esp_err_status` (BDB signals carry
//                      ezb_bdb_signal_simple_params_t::status).
//   - Channel mask:    esp_zb_set_primary_network_channel_set() -> ezb_bdb_set_primary_channel_set()
//   - Scan:            esp_zb_zdo_active_scan_request()         -> ezb_nwk_scan() (now streams one beacon
//                      per callback, terminated by a NULL result).
//   - Bindings:        esp_zb_zdo_binding_table_req()           -> ezb_zdo_nwk_mgmt_bind_req() (array-based result).
//   - Retry:           esp_zb_scheduler_alarm() (removed)       -> esp_timer one-shot re-posting into the stack.
//   - Sleepy/battery:  the ZBOSS zb_set_ed_node_descriptor() workaround is replaced by
//                      ezb_af_set_node_power_desc()+ezb_set_rx_on_when_idle().
//
// Boundaries owned by the not-yet-migrated ZCL / ZigbeeEP layer are flagged with `TODO(zb-v2):`.
// This file will not compile until esp-zigbee-lib v2.x is bundled by the toolchain AND the rest of
// the library (ZigbeeTypes/ZigbeeEP/ZigbeeHandlers/src/ep/*) is migrated.
// =====================================================================================

#include "Arduino.h"
#include "ZigbeeCore.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeHandlers.cpp"  // TODO(zb-v2): ZCL action handlers still use the v1 ZCL API.
#include <set>
#include "nvs_flash.h"
#include "esp_timer.h"

static bool edBatteryPowered = false;

ZigbeeCore::ZigbeeCore() {
  _radio_config.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE;  // Use the native 15.4 radio (host_config removed in v2.x)
  _zb_dev_desc = ezb_af_create_device_desc();
  _primary_channel_mask = ZB_TRANSCEIVER_ALL_CHANNELS_MASK;
  _open_network = 0;
  _scan_status = ZB_SCAN_FAILED;
  _scan_result = nullptr;
  _scan_count = 0;
  _begin_timeout = ZB_BEGIN_TIMEOUT_DEFAULT;
  _initialized = false;
  _endpoints_registered = false;
  _stack_running = false;
  _started = false;
  _connected = false;
  _paused = false;
  _scan_duration = 3;  // default scan duration
  _rx_on_when_idle = true;
  _debug = false;
  _allow_multi_endpoint_binding = false;
  _global_default_response_cb = nullptr;  // Initialize global callback to nullptr
  lock = xSemaphoreCreateBinary();
  if (lock == NULL) {
    log_e("Semaphore creation failed");
  }
}

//forward declarations
// v2.x ZCL core-action handler (implemented in ZigbeeHandlers.cpp). The callback now returns void and
// receives a non-const message whose ->out.result field is used to communicate status back to the stack.
static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message);
bool zb_app_signal_handler(const ezb_app_signal_t *signal);
bool zb_apsde_data_indication_handler(const ezb_apsde_data_ind_t *ind);

// ---- Commissioning retry (replacement for the removed esp_zb_scheduler_alarm) -------------------
static esp_timer_handle_t s_comm_retry_timer = nullptr;
static uint8_t s_comm_retry_mode = 0;

static void comm_retry_timer_cb(void *arg) {
  if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
    if (ezb_bdb_start_top_level_commissioning(s_comm_retry_mode) != EZB_ERR_NONE) {
      log_e("Failed to start Zigbee commissioning");
    }
    esp_zigbee_lock_release();
  }
}

static void scheduleCommissioning(uint8_t mode, uint32_t delay_ms) {
  s_comm_retry_mode = mode;
  if (!s_comm_retry_timer) {
    const esp_timer_create_args_t args = {.callback = comm_retry_timer_cb, .arg = nullptr, .name = "zb_comm_retry"};
    if (esp_timer_create(&args, &s_comm_retry_timer) != ESP_OK) {
      log_e("Failed to create commissioning retry timer");
      return;
    }
  }
  esp_timer_stop(s_comm_retry_timer);
  esp_timer_start_once(s_comm_retry_timer, (uint64_t)delay_ms * 1000);
}

bool ZigbeeCore::role(esp_zigbee_device_config_t *role_cfg, bool erase_nvs) {
  if (_initialized) {
    log_w("Zigbee already initialized");
    return true;
  }
  _role = (zigbee_role_t)role_cfg->device_type;
  if (!zigbeeStackInit(role_cfg, erase_nvs)) {
    log_e("ZigbeeCore role() failed");
    return false;
  }
  return true;
}

bool ZigbeeCore::role(zigbee_role_t role, bool erase_nvs) {
  if (_initialized) {
    log_w("Zigbee already initialized");
    return true;
  }
  esp_zigbee_device_config_t zb_nwk_cfg;
  switch (role) {
    case ZIGBEE_COORDINATOR: zb_nwk_cfg = ZIGBEE_DEFAULT_COORDINATOR_CONFIG(); break;
    case ZIGBEE_ROUTER:      zb_nwk_cfg = ZIGBEE_DEFAULT_ROUTER_CONFIG();      break;
    case ZIGBEE_END_DEVICE:  zb_nwk_cfg = ZIGBEE_DEFAULT_ED_CONFIG();          break;
    default: log_e("Invalid Zigbee role"); return false;
  }
  _role = role;
  if (!zigbeeStackInit(&zb_nwk_cfg, erase_nvs)) {
    log_e("ZigbeeCore role() failed");
    return false;
  }
  return true;
}

bool ZigbeeCore::begin() {
  if (_stack_running) {
    log_w("Zigbee stack already running");
    return _started;
  }
  if (!_initialized) {
    log_e("Zigbee.begin(): role() must be called before begin()");
    return false;
  }

  // Attach each endpoint descriptor to the device descriptor (deferred from addEndpoint()).
  if (!_endpoints_registered) {
    for (ZigbeeEP *ep : ep_objects) {
      ezb_err_t ret = ezb_af_device_add_endpoint_desc(_zb_dev_desc, ep->_ep_desc);
      if (ret != EZB_ERR_NONE) {
        log_e("Failed to add endpoint %u to device descriptor: 0x%x", ep->_endpoint, ret);
        return false;
      }
    }
    if (!registerEndpoints()) {
      log_e("ZigbeeCore begin failed to register endpoints");
      return false;
    }
  }
  if (!zigbeeStartStack()) {
    log_e("ZigbeeCore begin failed to start stack");
    return false;
  }
  if (xSemaphoreTake(lock, _begin_timeout) != pdTRUE) {
    log_e("ZigbeeCore begin timed out");
    if (_role != ZIGBEE_COORDINATOR) {
      resetNVRAMChannelMask();
    }
    return false;
  }
  return _started;
}

bool ZigbeeCore::addEndpoint(ZigbeeEP *ep) {
  if (_endpoints_registered) {
    log_e("Cannot add endpoint after Zigbee.begin()");
    return false;
  }
  if (ep->_ep_desc == nullptr) {
    log_e("Endpoint %u descriptor was not created in the constructor", ep->_endpoint);
    return false;
  }
  if (ep->_ep_config.ep_id == 0) {
    ep->_ep_config.ep_id = ep->_endpoint;
  }
  ep_objects.push_back(ep);
  log_d("Endpoint %u added (Device ID: 0x%04x) — will be registered in begin()", ep->_endpoint, ep->_device_id);
  return true;
}

bool ZigbeeCore::registerEndpoints() {
  if (_endpoints_registered) {
    return true;
  }
  if (ep_objects.empty()) {
    log_w("No Zigbee EPs to register");
    return false;
  }

  log_d("Register Zigbee device descriptor");
  if (ezb_af_device_desc_register(_zb_dev_desc) != EZB_ERR_NONE) {
    log_e("Failed to register Zigbee device descriptor");
    return false;
  }

  // SDK order: register device descriptor, then ZCL core action handler.
  ezb_zcl_core_action_handler_register(zb_action_handler);

  for (std::list<ZigbeeEP *>::iterator it = ep_objects.begin(); it != ep_objects.end(); ++it) {
    log_i("Device type: %s, Endpoint: %u, Device ID: 0x%04x", getDeviceTypeString((*it)->_device_id), (*it)->_endpoint, (*it)->_device_id);
    if ((*it)->_power_source == ZB_POWER_SOURCE_BATTERY) {
      edBatteryPowered = true;
    }
  }

  _endpoints_registered = true;
  return true;
}

static void esp_zb_task(void *pvParameters) {
  ezb_bdb_set_scan_duration(Zigbee.getScanDuration());

  /* initialize and start Zigbee stack (no-autostart: we drive commissioning from the signal handler) */
  ESP_ERROR_CHECK(esp_zigbee_start(false));

  // Battery powered end devices: advertise a disposable-battery node power descriptor.
  // Replaces the old ZBOSS zb_set_ed_node_descriptor() workaround (the power source and transfer
  // size node-descriptor fields are now generated from the device's settings in v2.x).
  if (((zigbee_role_t)Zigbee.getRole() == ZIGBEE_END_DEVICE) && edBatteryPowered) {
    ezb_af_node_power_desc_t power_desc = {};
    power_desc.current_power_mode = EZB_AF_NODE_POWER_MODE_SYNC_ON_WHEN_IDLE;
    power_desc.available_power_sources = EZB_AF_NODE_POWER_SOURCE_DISPOSABLE_BATTERY;
    power_desc.current_power_source = EZB_AF_NODE_POWER_SOURCE_DISPOSABLE_BATTERY;
    power_desc.current_power_source_level = EZB_AF_NODE_POWER_SOURCE_LEVEL_100_PERCENT;
    ezb_af_set_node_power_desc(&power_desc);
  }
  ezb_set_rx_on_when_idle(Zigbee.getRxOnWhenIdle());

  esp_zigbee_launch_mainloop();
}

// Zigbee stack init: esp_zigbee_init() + commissioning setup (no endpoint registration yet).
bool ZigbeeCore::zigbeeStackInit(esp_zigbee_device_config_t *zb_cfg, bool erase_nvs) {
  // The persistent data partition is now NVS (was a FAT partition in v1.x). The partitions.csv
  // entry for `zb_storage` must use subtype `nvs`, and the partition must be initialized as NVS.
  esp_err_t nvs_err = nvs_flash_init_partition("zb_storage");
  if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND || erase_nvs) {
    nvs_flash_erase_partition("zb_storage");
    nvs_err = nvs_flash_init_partition("zb_storage");
  }
  if (nvs_err != ESP_OK) {
    log_e("Failed to initialize zb_storage NVS partition: %s", esp_err_to_name(nvs_err));
    return false;
  }

  // v2.x bundles the platform + device configuration into a single esp_zigbee_init() call.
  esp_zigbee_config_t zb_init_cfg = {
    .device_config = *zb_cfg,
    .platform_config =
      {
        .storage_partition_name = "zb_storage",
        .radio_config = _radio_config,
      },
  };

  log_d("Initialize Zigbee stack");
  esp_err_t err = esp_zigbee_init(&zb_init_cfg);
  if (err != ESP_OK) {
    log_e("Failed to initialize Zigbee stack: %s", esp_err_to_name(err));
    return false;
  }

  // SDK order: esp_zigbee_init(), then commissioning setup, then endpoint registration in begin().
  if (ezb_app_signal_add_handler(zb_app_signal_handler) != EZB_ERR_NONE) {
    log_e("Failed to register Zigbee application signal handler");
    return false;
  }

  ezb_aps_secur_enable_distributed_security(false);

  if (ezb_bdb_set_primary_channel_set(_primary_channel_mask) != EZB_ERR_NONE) {
    log_e("Failed to set primary network channel mask");
    return false;
  }

  if (ezb_bdb_set_secondary_channel_set(ZB_TRANSCEIVER_ALL_CHANNELS_MASK) != EZB_ERR_NONE) {
    log_e("Failed to set secondary network channel mask");
    return false;
  }

  // Register APSDE-DATA indication handler to catch bind/unbind requests.
  ezb_apsde_data_indication_handler_register(zb_apsde_data_indication_handler);

  // NOTE(zb-v2): esp_zb_nvram_erase_at_start() is removed. NVRAM erase on join is handled above
  // by erasing the zb_storage NVS partition when `erase_nvs` is set.

  _initialized = true;
  return true;
}

bool ZigbeeCore::zigbeeStartStack() {
  if (_stack_running) {
    return true;
  }
  if (xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL) != pdPASS) {
    log_e("Failed to create Zigbee task");
    return false;
  }
  _stack_running = true;
  return true;
}

void ZigbeeCore::setRadioConfig(esp_zigbee_radio_config_t config) {
  if (_stack_running) {
    log_e("Zigbee.setRadioConfig() must be called before Zigbee.begin()");
    return;
  }
  _radio_config = config;
}

esp_zigbee_radio_config_t ZigbeeCore::getRadioConfig() {
  return _radio_config;
}

void ZigbeeCore::setPrimaryChannelMask(uint32_t mask) {
  if (_stack_running) {
    log_e("Zigbee.setPrimaryChannelMask() must be called before Zigbee.begin()");
    return;
  }
  _primary_channel_mask = mask;
}

void ZigbeeCore::setScanDuration(uint8_t duration) {
  if (duration < 1 || duration > 4) {
    log_e("Invalid scan duration, must be between 1 and 4");
    return;
  }
  _scan_duration = duration;
}

void ZigbeeCore::setRebootOpenNetwork(uint8_t time) {
  _open_network = time;
}

void ZigbeeCore::openNetwork(uint8_t time) {
  if (_stack_running && !_paused) {
    log_v("Opening network for joining for %u seconds", time);
    ezb_bdb_open_network(time);
  }
}

void ZigbeeCore::closeNetwork() {
  if (_stack_running && !_paused) {
    log_v("Closing network");
    ezb_bdb_close_network();
  }
}

// v2.x application signal handler. Registered via ezb_app_signal_add_handler().
// Returns true if the signal was fully handled, false to let the stack continue its default handling.
bool zb_app_signal_handler(const ezb_app_signal_t *signal) {
  ezb_app_signal_type_t sig_type = ezb_app_signal_get_type(signal);
  const void *p_params = ezb_app_signal_get_params(signal);

  switch (sig_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:  // Common
      log_i("Zigbee stack initialized");
      log_d("Zigbee channel mask: 0x%08" PRIx32, ezb_get_channel_mask());
      // In v2.x the initialization (rejoin to previous network) does not run automatically;
      // the application must trigger it explicitly.
      ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
      break;
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:  // Common
    case EZB_BDB_SIGNAL_DEVICE_REBOOT:       // Common
    {
      const ezb_bdb_signal_simple_params_t *bdb = (const ezb_bdb_signal_simple_params_t *)p_params;
      bool ok = (bdb != nullptr) && (bdb->status == EZB_BDB_STATUS_SUCCESS);
      if (ok) {
        log_i("Device started up in %s factory-reset mode", ezb_bdb_is_factory_new() ? "" : "non");
        if (ezb_bdb_is_factory_new()) {
          if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
            log_i("Start network formation");
            ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_FORMATION);
          } else {
            log_i("Start network steering");
            ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
            Zigbee._started = true;
            xSemaphoreGive(Zigbee.lock);
          }
        } else {
          log_i("Device rebooted");
          Zigbee._started = true;
          xSemaphoreGive(Zigbee.lock);
          if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR && Zigbee._open_network > 0) {
            log_i("Opening network for joining for %u seconds", Zigbee._open_network);
            ezb_bdb_open_network(Zigbee._open_network);
          } else {
            // Save the channel mask in case of reboot which may be on a different channel after a change in the network
            Zigbee.setNVRAMChannelMask(1 << ezb_get_current_channel());
            Zigbee._connected = true;  // Coordinator is always connected
          }
          Zigbee.searchBindings();
        }
      } else {
        /* commissioning failed */
        log_w("Commissioning failed (status: %u), trying again...", bdb ? bdb->status : 0xFF);
        scheduleCommissioning(EZB_BDB_MODE_INITIALIZATION, 500);
      }
      break;
    }
    case EZB_BDB_SIGNAL_FORMATION:  // Coordinator
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        const ezb_bdb_signal_simple_params_t *bdb = (const ezb_bdb_signal_simple_params_t *)p_params;
        if (bdb && bdb->status == EZB_BDB_STATUS_SUCCESS) {
          ezb_extpanid_t extended_pan_id;
          ezb_get_extended_panid(&extended_pan_id);
          log_i(
            "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04x, Channel:%u, Short Address: 0x%04x)",
            extended_pan_id.u8[7], extended_pan_id.u8[6], extended_pan_id.u8[5], extended_pan_id.u8[4], extended_pan_id.u8[3], extended_pan_id.u8[2],
            extended_pan_id.u8[1], extended_pan_id.u8[0], ezb_get_panid(), ezb_get_current_channel(), ezb_get_short_address()
          );
          Zigbee._connected = true;
          ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        } else {
          log_i("Restart network formation (status: %u)", bdb ? bdb->status : 0xFF);
          scheduleCommissioning(EZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
      }
      break;
    case EZB_BDB_SIGNAL_STEERING:  // Router and End Device
    {
      const ezb_bdb_signal_simple_params_t *bdb = (const ezb_bdb_signal_simple_params_t *)p_params;
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        if (bdb && bdb->status == EZB_BDB_STATUS_SUCCESS) {
          log_i("Network steering started");
        }
        Zigbee._started = true;
        xSemaphoreGive(Zigbee.lock);
      } else {
        if (bdb && bdb->status == EZB_BDB_STATUS_SUCCESS) {
          ezb_extpanid_t extended_pan_id;
          ezb_get_extended_panid(&extended_pan_id);
          log_i(
            "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04x, Channel:%u, Short Address: 0x%04x)",
            extended_pan_id.u8[7], extended_pan_id.u8[6], extended_pan_id.u8[5], extended_pan_id.u8[4], extended_pan_id.u8[3], extended_pan_id.u8[2],
            extended_pan_id.u8[1], extended_pan_id.u8[0], ezb_get_panid(), ezb_get_current_channel(), ezb_get_short_address()
          );
          Zigbee._connected = true;
          // Set channel mask, so that the device will re-join the network faster after reboot (scan only on the current channel)
          Zigbee.setNVRAMChannelMask(1 << ezb_get_current_channel());
        } else {
          log_i("Network steering was not successful (status: %u)", bdb ? bdb->status : 0xFF);
          scheduleCommissioning(EZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
      }
      break;
    }
    case EZB_ZDO_SIGNAL_DEVICE_ANNCE:  // Coordinator
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        const ezb_zdo_signal_device_annce_params_t *dev_annce_params = (const ezb_zdo_signal_device_annce_params_t *)p_params;
        log_i("New device commissioned or rejoined (short: 0x%04x)", dev_annce_params->short_addr);
        // TODO(zb-v2): ZigbeeEP::findEndpoint() must be migrated to accept ezb_zdo_match_desc_req_t
        // and to fill field.profile_id / cluster_list / cb before issuing ezb_zdo_match_desc_req().
        ezb_zdo_match_desc_req_t cmd_req = {};
        cmd_req.dst_nwk_addr = dev_annce_params->short_addr;
        cmd_req.field.nwk_addr_of_interest = dev_annce_params->short_addr;
        log_v("Device capabilities: 0x%02x", dev_annce_params->capability);
        // for each endpoint in the list call the findEndpoint function if not bounded or allowed to bind multiple devices
        for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
          log_d("Checking endpoint %u", (*it)->getEndpoint());
          if (!(*it)->epUseManualBinding()) {
            if (!(*it)->bound() || (*it)->epAllowMultipleBinding()) {
              bool found = false;
              std::list<zb_device_params_t *> bound_devices = (*it)->getBoundDevices();
              for (std::list<zb_device_params_t *>::iterator device = bound_devices.begin(); device != bound_devices.end(); ++device) {
                if (((*device)->short_addr == dev_annce_params->short_addr) || (memcmp((*device)->ieee_addr, dev_annce_params->device_addr.u8, 8) == 0)) {
                  found = true;
                  log_d("Device already bound to endpoint %u", (*it)->getEndpoint());
                  break;
                }
              }
              if (!found) {
                log_d("Device not bound to endpoint %u and it is free to bound!", (*it)->getEndpoint());
                (*it)->findEndpoint(&cmd_req);
                log_d("Endpoint %u is searching for device", (*it)->getEndpoint());
                if (!Zigbee.allowMultiEndpointBinding()) {  // If multi endpoint binding is not allowed, break the loop to keep backwards compatibility
                  break;
                }
              }
            }
          }
        }
      }
      break;
    case EZB_ZDO_SIGNAL_DEVICE_UPDATE:  // Router
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_ROUTER) {
        const ezb_zdo_signal_device_update_params_t *dev_update_params = (const ezb_zdo_signal_device_update_params_t *)p_params;
        log_i("New device commissioned or rejoined (short: 0x%04x)", dev_update_params->short_addr);
        ezb_zdo_match_desc_req_t cmd_req = {};
        cmd_req.dst_nwk_addr = dev_update_params->short_addr;
        cmd_req.field.nwk_addr_of_interest = dev_update_params->short_addr;
        // for each endpoint in the list call the findEndpoint function if not bounded or allowed to bind multiple devices
        for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
          log_d("Checking endpoint %u", (*it)->getEndpoint());
          if (!(*it)->epUseManualBinding()) {
            if (!(*it)->bound() || (*it)->epAllowMultipleBinding()) {
              bool found = false;
              std::list<zb_device_params_t *> bound_devices = (*it)->getBoundDevices();
              for (std::list<zb_device_params_t *>::iterator device = bound_devices.begin(); device != bound_devices.end(); ++device) {
                if (((*device)->short_addr == dev_update_params->short_addr) || (memcmp((*device)->ieee_addr, dev_update_params->device_addr.u8, 8) == 0)) {
                  found = true;
                  log_d("Device already bound to endpoint %u", (*it)->getEndpoint());
                  break;
                }
              }
              if (!found) {
                log_d("Device not bound to endpoint %u and it is free to bound!", (*it)->getEndpoint());
                (*it)->findEndpoint(&cmd_req);
                log_d("Endpoint %u is searching for device", (*it)->getEndpoint());
                if (!Zigbee.allowMultiEndpointBinding()) {  // If multi endpoint binding is not allowed, break the loop to keep backwards compatibility
                  break;
                }
              }
            }
          }
        }
      }
      break;
    case EZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:  // Coordinator
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        const ezb_nwk_signal_permit_join_status_params_t *permit = (const ezb_nwk_signal_permit_join_status_params_t *)p_params;
        if (permit && permit->duration) {
          log_i("Network(0x%04x) is open for %u seconds", ezb_get_panid(), permit->duration);
        } else {
          log_i("Network(0x%04x) closed, devices joining not allowed.", ezb_get_panid());
        }
      }
      break;
    case EZB_ZDO_SIGNAL_LEAVE:  // End Device + Router
      // Received signal to leave the network
      if ((zigbee_role_t)Zigbee.getRole() != ZIGBEE_COORDINATOR) {
        const ezb_zdo_signal_leave_params_t *leave_params = (const ezb_zdo_signal_leave_params_t *)p_params;
        log_v("Signal to leave the network, leave type: %u", leave_params->leave_type);
        if (leave_params->leave_type == EZB_ZDO_LEAVE_TYPE_RESET) {  // Leave without rejoin -> Factory reset
          log_i("Leave without rejoin, factory reset the device");
          Zigbee.factoryReset(true);
        } else {  // Leave with rejoin -> Rejoin the network, only reboot the device
          log_i("Leave with rejoin, only reboot the device");
          ESP.restart();
        }
      }
      break;
    default: log_v("ZDO signal: %s (0x%x)", ezb_app_signal_to_string(sig_type), (unsigned int)sig_type); break;
  }
  return false;
}

// APS DATA INDICATION HANDLER to catch bind/unbind requests
bool zb_apsde_data_indication_handler(const ezb_apsde_data_ind_t *ind) {
  if (Zigbee.getDebugMode()) {
    uint16_t src_short = (ind->src_address.addr_mode == EZB_ADDR_MODE_SHORT) ? ind->src_address.u.short_addr : 0xFFFF;
    log_d("APSDE INDICATION - Received APSDE-DATA indication, status: %u", ind->status);
    log_d(
      "APSDE INDICATION - dst_endpoint: %u, src_endpoint: %u, cluster_id: 0x%04x, profile_id: 0x%04x, asdu_length: %u, src_short_addr: 0x%04x, "
      "security_status: %u, lqi: %d",
      ind->dst_endpoint, ind->src_endpoint, ind->cluster_id, ind->profile_id, ind->asdu_length, src_short, ind->security_status, ind->lqi
    );
  }
  if (ind->status == 0x00) {
    // Catch bind/unbind requests to update the bound devices list
    if (ind->cluster_id == 0x21 || ind->cluster_id == 0x22) {
      Zigbee.searchBindings();
    }
  } else {
    log_e("APSDE INDICATION - Invalid status of APSDE-DATA indication, error code: %u", ind->status);
  }
  return false;  //False to let the stack process the message as usual
}

void ZigbeeCore::factoryReset(bool restart) {
  if (restart) {
    log_v("Factory resetting Zigbee stack, device will reboot");
    esp_zigbee_factory_reset();  // noreturn in v2.x
  } else {
    // TODO(zb-v2): esp_zb_zcl_reset_nvram_to_factory_default() (ZCL) has no direct v2.x equivalent
    // available here; revisit when the ZCL layer is migrated.
    log_w("Factory reset without restart is not yet supported on the v2.x SDK");
  }
}

// v2.x active scan results stream one beacon per callback, terminated by a NULL result.
void ZigbeeCore::scanCompleteCallback(ezb_nwk_active_scan_result_t *result, void *user_ctx) {
  if (result == nullptr) {
    // Scan finished
    log_v("Zigbee network scan complete, found %u networks", Zigbee._scan_count);
    Zigbee._scan_status = (int16_t)Zigbee._scan_count;
    return;
  }

  // Append this beacon to the result buffer
  zigbee_scan_result_t *tmp = (zigbee_scan_result_t *)realloc(Zigbee._scan_result, (Zigbee._scan_count + 1) * sizeof(zigbee_scan_result_t));
  if (tmp == nullptr) {
    log_e("Failed to allocate memory for scan result");
    return;
  }
  Zigbee._scan_result = tmp;
  Zigbee._scan_result[Zigbee._scan_count] = *result;
  log_v(
    "Network %u: PAN ID: 0x%04x, Permit Joining: %s, Channel: %u, Router Capacity: %s, End Device Capacity: %s", Zigbee._scan_count, result->panid,
    result->permit_join ? "Yes" : "No", result->channel_number, result->router_capacity ? "Yes" : "No", result->enddev_capacity ? "Yes" : "No"
  );
  Zigbee._scan_count++;
}

void ZigbeeCore::scanNetworks(uint32_t channel_mask, uint8_t scan_duration) {
  if (!started()) {
    log_e("Zigbee stack is not started, cannot scan networks");
    return;
  }
  if (_scan_status == ZB_SCAN_RUNNING) {
    log_w("Scan already in progress, ignoring new scan request");
    return;
  }
  log_v("Starting to scan Zigbee networks");

  // Reset previous results
  if (_scan_result != nullptr) {
    free(_scan_result);
    _scan_result = nullptr;
  }
  _scan_count = 0;

  ezb_nwk_scan_req_t req = {};
  req.scan_type = EZB_NWK_SCAN_TYPE_ACTIVE;
  req.scan_duration = scan_duration;
  req.scan_channels = channel_mask;
  req.active_scan_cb = scanCompleteCallback;
  req.user_ctx = nullptr;

  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_e("Failed to start scanning Zigbee networks: failed to acquire Zigbee lock");
    return;
  }
  ezb_nwk_scan(&req);
  esp_zigbee_lock_release();
  _scan_status = ZB_SCAN_RUNNING;
}

int16_t ZigbeeCore::scanComplete() {
  return _scan_status;
}

zigbee_scan_result_t *ZigbeeCore::getScanResult() {
  return _scan_result;
}

void ZigbeeCore::scanDelete() {
  if (_scan_result != nullptr) {
    free(_scan_result);
    _scan_result = nullptr;
  }
  _scan_count = 0;
  _scan_status = ZB_SCAN_FAILED;
}

// Recall bounded devices from the binding table after reboot or when requested.
// v2.x: ezb_zdo_nwk_mgmt_bind_req() returns the binding table as an array (rsp->binding_table_list)
// rather than the v1 linked list, and chunking advances field.start_index.
void ZigbeeCore::bindingTableCb(const ezb_zdo_nwk_mgmt_bind_req_result_t *result, void *user_ctx) {
  ezb_zdo_nwk_mgmt_bind_req_t *req = (ezb_zdo_nwk_mgmt_bind_req_t *)user_ctx;

  if (result->error != EZB_ERR_NONE || result->rsp == nullptr) {
    log_e("Binding table request failed with error: 0x%x", result->error);
    free(req);
    return;
  }

  const ezb_zdp_nwk_mgmt_bind_rsp_field_t *rsp = result->rsp;
  log_d("Binding table info: total %u, index %u, count %u", rsp->binding_table_entries, rsp->start_index, rsp->binding_table_list_count);

  if (rsp->binding_table_entries == 0) {
    log_d("No binding table entries found");
    // Clear all bound devices since there are no entries
    for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
      log_d("Clearing bound devices for EP %u", (*it)->getEndpoint());
      (*it)->clearBoundDevices();
    }
    free(req);
    return;
  }

  // Track found devices using both short and IEEE addresses
  struct DeviceIdentifier {
    uint8_t endpoint;
    uint16_t short_addr;
    uint8_t ieee_addr[8];
    bool is_ieee;

    bool operator<(const DeviceIdentifier &other) const {
      if (endpoint != other.endpoint) {
        return endpoint < other.endpoint;
      }
      if (is_ieee != other.is_ieee) {
        return is_ieee < other.is_ieee;
      }
      if (is_ieee) {
        return memcmp(ieee_addr, other.ieee_addr, 8) < 0;
      }
      return short_addr < other.short_addr;
    }
  };
  static std::set<DeviceIdentifier> found_devices;
  static std::vector<ezb_zdp_nwk_mgmt_bind_table_entry_t> all_records;

  // If this is the first chunk (index 0), clear the previous data
  if (rsp->start_index == 0) {
    found_devices.clear();
    all_records.clear();
  }

  // Add current records to our collection
  for (int i = 0; i < rsp->binding_table_list_count; i++) {
    const ezb_zdp_nwk_mgmt_bind_table_entry_t *record = &rsp->binding_table_list[i];
    log_d(
      "Processing record %d: src_ep %u, dst_ep %u, cluster_id 0x%04x, dst_addr_mode %u", i, record->src_ep, record->dst_ep, record->cluster_id,
      record->dst_addr_mode
    );
    all_records.push_back(*record);
  }

  // If this is not the last chunk, request the next one
  if (rsp->start_index + rsp->binding_table_list_count < rsp->binding_table_entries) {
    log_d(
      "Requesting next chunk of binding table (current index: %u, count: %u, total: %u)", rsp->start_index, rsp->binding_table_list_count,
      rsp->binding_table_entries
    );
    req->field.start_index = rsp->start_index + rsp->binding_table_list_count;
    ezb_zdo_nwk_mgmt_bind_req(req);
    return;
  }

  // This is the last chunk, process all records
  log_d("Processing final chunk of binding table, total records: %lu", (unsigned long)all_records.size());
  for (const auto &record : all_records) {
    DeviceIdentifier dev_id;
    dev_id.endpoint = record.src_ep;
    dev_id.is_ieee = (record.dst_addr_mode == EZB_APS_ADDR_MODE_64_ENDP_PRESENT);

    if (dev_id.is_ieee) {
      memcpy(dev_id.ieee_addr, record.dst_addr.extended_addr.u8, 8);
      dev_id.short_addr = 0xFFFF;  // Invalid short address
    } else {
      dev_id.short_addr = record.dst_addr.short_addr;
      memset(dev_id.ieee_addr, 0, 8);
    }

    found_devices.insert(dev_id);
  }

  // Now process each endpoint and update its bound devices
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    log_d("Processing endpoint %u", (*it)->getEndpoint());
    std::list<zb_device_params_t *> bound_devices = (*it)->getBoundDevices();
    std::list<zb_device_params_t *> devices_to_remove;

    // First, identify devices that need to be removed
    for (std::list<zb_device_params_t *>::iterator dev_it = bound_devices.begin(); dev_it != bound_devices.end(); ++dev_it) {
      DeviceIdentifier dev_id;
      dev_id.endpoint = (*it)->getEndpoint();
      bool found = false;

      // Check if device exists with short address
      if ((*dev_it)->short_addr != 0xFFFF) {
        dev_id.is_ieee = false;
        dev_id.short_addr = (*dev_it)->short_addr;
        memset(dev_id.ieee_addr, 0, 8);
        if (found_devices.find(dev_id) != found_devices.end()) {
          found = true;
        }
      }

      // Check if device exists with IEEE address
      if (!found) {
        dev_id.is_ieee = true;
        memcpy(dev_id.ieee_addr, (*dev_it)->ieee_addr, 8);
        dev_id.short_addr = 0xFFFF;
        if (found_devices.find(dev_id) != found_devices.end()) {
          found = true;
        }
      }

      if (!found) {
        devices_to_remove.push_back(*dev_it);
      }
    }

    // Remove devices that are no longer in the binding table
    for (std::list<zb_device_params_t *>::iterator dev_it = devices_to_remove.begin(); dev_it != devices_to_remove.end(); ++dev_it) {
      (*it)->removeBoundDevice(*dev_it);
      free(*dev_it);
    }

    // Now add new devices from the binding table
    for (const auto &record : all_records) {
      if (record.src_ep == (*it)->getEndpoint()) {
        log_d("Processing binding record for EP %u", record.src_ep);
        zb_device_params_t *device = (zb_device_params_t *)calloc(1, sizeof(zb_device_params_t));
        if (!device) {
          log_e("Failed to allocate memory for device params");
          continue;
        }
        device->endpoint = record.dst_ep;

        bool is_ieee = (record.dst_addr_mode == EZB_APS_ADDR_MODE_64_ENDP_PRESENT);
        if (is_ieee) {
          memcpy(device->ieee_addr, record.dst_addr.extended_addr.u8, 8);
          device->short_addr = 0xFFFF;
        } else {
          device->short_addr = record.dst_addr.short_addr;
          memset(device->ieee_addr, 0, 8);
        }

        // Check if device already exists
        bool device_exists = false;
        for (std::list<zb_device_params_t *>::iterator dev_it = bound_devices.begin(); dev_it != bound_devices.end(); ++dev_it) {
          if (is_ieee) {
            if (memcmp((*dev_it)->ieee_addr, device->ieee_addr, 8) == 0) {
              device_exists = true;
              break;
            }
          } else {
            if ((*dev_it)->short_addr == device->short_addr) {
              device_exists = true;
              break;
            }
          }
        }

        if (!device_exists) {
          (*it)->addBoundDevice(device);
          log_d(
            "Device bound to EP %u -> device endpoint: %u, %s: %s", record.src_ep, device->endpoint, is_ieee ? "ieee addr" : "short addr",
            is_ieee ? formatIEEEAddress(device->ieee_addr) : formatShortAddress(device->short_addr)
          );
        } else {
          log_d("Device already exists, freeing allocated memory");
          free(device);  // Free the device if it already exists
        }
      }
    }
  }

  log_d("Filling bounded devices finished");
  free(req);
}

void ZigbeeCore::searchBindings() {
  ezb_zdo_nwk_mgmt_bind_req_t *mb_req = (ezb_zdo_nwk_mgmt_bind_req_t *)calloc(1, sizeof(ezb_zdo_nwk_mgmt_bind_req_t));
  if (!mb_req) {
    log_e("Failed to allocate memory for binding table request");
    return;
  }
  mb_req->dst_nwk_addr = ezb_get_short_address();
  mb_req->field.start_index = 0;
  mb_req->cb = bindingTableCb;
  mb_req->user_ctx = mb_req;
  log_d("Requesting binding table for address 0x%04x", mb_req->dst_nwk_addr);
  ezb_zdo_nwk_mgmt_bind_req(mb_req);
}

void ZigbeeCore::resetNVRAMChannelMask() {
  _primary_channel_mask = ZB_TRANSCEIVER_ALL_CHANNELS_MASK;
  ezb_set_channel_mask(_primary_channel_mask);
  // NOTE(zb-v2): the ZBOSS zb_nvram_write_dataset() call is removed; persistence is handled by NVS.
  log_v("Channel mask reset to all channels");
}

void ZigbeeCore::setNVRAMChannelMask(uint32_t mask) {
  _primary_channel_mask = mask;
  ezb_set_channel_mask(_primary_channel_mask);
  log_v("Channel mask set to 0x%08" PRIx32, mask);
}

void ZigbeeCore::stop() {
  if (_stack_running && !_paused) {
    vTaskSuspend(xTaskGetHandle("Zigbee_main"));
    log_v("Zigbee stack stopped");
    _paused = true;
  }
}

void ZigbeeCore::start() {
  if (_stack_running && _paused) {
    vTaskResume(xTaskGetHandle("Zigbee_main"));
    log_v("Zigbee stack started");
    _paused = false;
  }
}

// Function to convert enum value to string
// TODO(zb-v2): the HA device id enum lives in ezbee/zha.h in v2.x; verify the identifiers still match.
const char *ZigbeeCore::getDeviceTypeString(uint16_t deviceId) {
  switch (deviceId) {
    case EZB_ZHA_ON_OFF_SWITCH_DEVICE_ID:              return "General On/Off switch";
    case EZB_ZHA_LEVEL_CONTROL_SWITCH_DEVICE_ID:       return "Level Control Switch";
    case EZB_ZHA_ON_OFF_OUTPUT_DEVICE_ID:              return "General On/Off output";
    case EZB_ZHA_LEVEL_CONTROLLABLE_OUTPUT_DEVICE_ID:  return "Level Controllable Output";
    case EZB_ZHA_SCENE_SELECTOR_DEVICE_ID:             return "Scene Selector";
    case EZB_ZHA_CONFIGURATION_TOOL_DEVICE_ID:         return "Configuration Tool";
    case EZB_ZHA_REMOTE_CONTROL_DEVICE_ID:             return "Remote Control";
    case EZB_ZHA_COMBINED_INTERFACE_DEVICE_ID:         return "Combined Interface";
    case EZB_ZHA_RANGE_EXTENDER_DEVICE_ID:             return "Range Extender";
    case EZB_ZHA_MAINS_POWER_OUTLET_DEVICE_ID:         return "Mains Power Outlet";
    case EZB_ZHA_DOOR_LOCK_DEVICE_ID:                  return "Door lock client";
    case EZB_ZHA_DOOR_LOCK_CONTROLLER_DEVICE_ID:       return "Door lock controller";
    case EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID:              return "Simple Sensor device";
    case EZB_ZHA_CONSUMPTION_AWARENESS_DEVICE_ID:      return "Consumption Awareness Device";
    case EZB_ZHA_HOME_GATEWAY_DEVICE_ID:               return "Home Gateway";
    case EZB_ZHA_SMART_PLUG_DEVICE_ID:                 return "Smart plug";
    case EZB_ZHA_WHITE_GOODS_DEVICE_ID:                return "White Goods";
    case EZB_ZHA_METER_INTERFACE_DEVICE_ID:            return "Meter Interface";
    case EZB_ZHA_ON_OFF_LIGHT_DEVICE_ID:               return "On/Off Light Device";
    case EZB_ZHA_DIMMABLE_LIGHT_DEVICE_ID:             return "Dimmable Light Device";
    case EZB_ZHA_COLOR_DIMMABLE_LIGHT_DEVICE_ID:       return "Color Dimmable Light Device";
    case EZB_ZHA_ON_OFF_LIGHT_SWITCH_DEVICE_ID:        return "On/Off Light Switch Device";
    case EZB_ZHA_DIMMER_SWITCH_DEVICE_ID:              return "Dimmer Switch Device";
    case EZB_ZHA_COLOR_DIMMER_SWITCH_DEVICE_ID:        return "Color Dimmer Switch Device";
    case EZB_ZHA_LIGHT_SENSOR_DEVICE_ID:               return "Light Sensor";
    case EZB_ZHA_OCCUPANCY_SENSOR_DEVICE_ID:           return "Occupancy Sensor";
    case EZB_ZHA_SHADE_DEVICE_ID:                      return "Shade";
    case EZB_ZHA_SHADE_CONTROLLER_DEVICE_ID:           return "Shade controller";
    case EZB_ZHA_WINDOW_COVERING_DEVICE_ID:            return "Window Covering client";
    case EZB_ZHA_WINDOW_COVERING_CONTROLLER_DEVICE_ID: return "Window Covering controller";
    case EZB_ZHA_HEATING_COOLING_UNIT_DEVICE_ID:       return "Heating/Cooling Unit device";
    case EZB_ZHA_THERMOSTAT_DEVICE_ID:                 return "Thermostat Device";
    case EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID:         return "Temperature Sensor";
    case EZB_ZHA_PUMP_DEVICE_ID:                       return "Pump Device";
    case EZB_ZHA_PUMP_CONTROLLER_DEVICE_ID:            return "Pump Controller Device";
    case EZB_ZHA_PRESSURE_SENSOR_DEVICE_ID:            return "Pressure Sensor Device";
    case EZB_ZHA_FLOW_SENSOR_DEVICE_ID:                return "Flow Sensor Device";
    case EZB_ZHA_MINI_SPLIT_AC_DEVICE_ID:              return "Mini Split AC Device";
    case EZB_ZHA_IAS_CONTROL_INDICATING_EQUIPMENT_ID:  return "IAS Control and Indicating Equipment";
    case EZB_ZHA_IAS_ANCILLARY_CONTROL_EQUIPMENT_ID:   return "IAS Ancillary Control Equipment";
    case EZB_ZHA_IAS_ZONE_ID:                          return "IAS Zone";
    case EZB_ZHA_IAS_WARNING_DEVICE_ID:                return "IAS Warning Device";
    case EZB_ZHA_CUSTOM_GATEWAY_DEVICE_ID:             return "Custom Gateway Device";
    default:                                           return "Unknown device type";
  }
}

void ZigbeeCore::callDefaultResponseCallback(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status, uint8_t endpoint, uint16_t cluster) {
  if (_global_default_response_cb) {
    _global_default_response_cb(resp_to_cmd, status, endpoint, cluster);
  }
}

ZigbeeCore Zigbee = ZigbeeCore();

#endif  // CONFIG_ZB_ENABLED
