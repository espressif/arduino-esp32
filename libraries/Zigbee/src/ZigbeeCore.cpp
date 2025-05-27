/* Zigbee Core Functions */

#include "ZigbeeCore.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeHandlers.cpp"
#include "Arduino.h"
#include <set>

#ifdef __cplusplus
extern "C" {
#endif
#include "zboss_api.h"
extern zb_ret_t zb_nvram_write_dataset(zb_nvram_dataset_types_t t);                            // rejoin scanning workaround
extern void zb_set_ed_node_descriptor(bool power_src, bool rx_on_when_idle, bool alloc_addr);  // sleepy device power mode workaround
#ifdef __cplusplus
}
#endif

static bool edBatteryPowered = false;

ZigbeeCore::ZigbeeCore() {
  _radio_config.radio_mode = ZB_RADIO_MODE_NATIVE;                   // Use the native 15.4 radio
  _host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;  // Disable host connection
  _zb_ep_list = esp_zb_ep_list_create();
  _primary_channel_mask = ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK;
  _open_network = 0;
  _scan_status = ZB_SCAN_FAILED;
  _begin_timeout = ZB_BEGIN_TIMEOUT_DEFAULT;
  _started = false;
  _connected = false;
  _scan_duration = 3;  // default scan duration
  _rx_on_when_idle = true;
  _debug = false;
  if (!lock) {
    lock = xSemaphoreCreateBinary();
    if (lock == NULL) {
      log_e("Semaphore creation failed");
    }
  }
}

//forward declaration
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);

bool ZigbeeCore::begin(esp_zb_cfg_t *role_cfg, bool erase_nvs) {
  if (!zigbeeInit(role_cfg, erase_nvs)) {
    log_e("ZigbeeCore begin failed");
    return false;
  }
  _role = (zigbee_role_t)role_cfg->esp_zb_role;
  if (xSemaphoreTake(lock, _begin_timeout) != pdTRUE) {
    log_e("ZigbeeCore begin failed or timeout");
    if (_role != ZIGBEE_COORDINATOR) {  // Only End Device and Router can rejoin
      resetNVRAMChannelMask();
    }
  }
  return started();
}

bool ZigbeeCore::begin(zigbee_role_t role, bool erase_nvs) {
  bool status = true;
  switch (role) {
    case ZIGBEE_COORDINATOR:
    {
      _role = ZIGBEE_COORDINATOR;
      esp_zb_cfg_t zb_nwk_cfg = ZIGBEE_DEFAULT_COORDINATOR_CONFIG();
      status = zigbeeInit(&zb_nwk_cfg, erase_nvs);
      break;
    }
    case ZIGBEE_ROUTER:
    {
      _role = ZIGBEE_ROUTER;
      esp_zb_cfg_t zb_nwk_cfg = ZIGBEE_DEFAULT_ROUTER_CONFIG();
      status = zigbeeInit(&zb_nwk_cfg, erase_nvs);
      break;
    }
    case ZIGBEE_END_DEVICE:
    {
      _role = ZIGBEE_END_DEVICE;
      esp_zb_cfg_t zb_nwk_cfg = ZIGBEE_DEFAULT_ED_CONFIG();
      status = zigbeeInit(&zb_nwk_cfg, erase_nvs);
      break;
    }
    default: log_e("Invalid Zigbee Role"); return false;
  }
  if (!status || xSemaphoreTake(lock, _begin_timeout) != pdTRUE) {
    log_e("ZigbeeCore begin failed or timeout");
    if (_role != ZIGBEE_COORDINATOR) {  // Only End Device and Router can rejoin
      resetNVRAMChannelMask();
    }
  }
  return started();
}

bool ZigbeeCore::addEndpoint(ZigbeeEP *ep) {
  ep_objects.push_back(ep);

  log_d("Endpoint: %d, Device ID: 0x%04x", ep->_endpoint, ep->_device_id);
  //Register clusters and ep_list to the ZigbeeCore class's ep_list
  if (ep->_ep_config.endpoint == 0 || ep->_cluster_list == nullptr) {
    log_e("Endpoint config or Cluster list is not initialized, EP not added to ZigbeeCore's EP list");
    return false;
  }
  esp_err_t ret = ESP_OK;
  if (ep->_device_id == ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID) {
    ret = esp_zb_ep_list_add_gateway_ep(_zb_ep_list, ep->_cluster_list, ep->_ep_config);
  } else {
    ret = esp_zb_ep_list_add_ep(_zb_ep_list, ep->_cluster_list, ep->_ep_config);
  }
  if (ret != ESP_OK) {
    log_e("Failed to add endpoint: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

static void esp_zb_task(void *pvParameters) {
  esp_zb_bdb_set_scan_duration(Zigbee.getScanDuration());

  /* initialize Zigbee stack */
  ESP_ERROR_CHECK(esp_zb_start(false));

  //NOTE: This is a workaround to make battery powered devices to be discovered as battery powered
  if (((zigbee_role_t)Zigbee.getRole() == ZIGBEE_END_DEVICE) && edBatteryPowered) {
    zb_set_ed_node_descriptor(0, Zigbee.getRxOnWhenIdle(), 1);
  }

  esp_zb_stack_main_loop();
}

// Zigbee core init function
bool ZigbeeCore::zigbeeInit(esp_zb_cfg_t *zb_cfg, bool erase_nvs) {
  // Zigbee platform configuration
  esp_zb_platform_config_t platform_config = {
    .radio_config = _radio_config,
    .host_config = _host_config,
  };

  esp_err_t err = esp_zb_platform_config(&platform_config);
  if (err != ESP_OK) {
    log_e("Failed to configure Zigbee platform");
    return false;
  }

  // Initialize Zigbee stack
  log_d("Initialize Zigbee stack");
  esp_zb_init(zb_cfg);

  // Register all Zigbee EPs in list
  if (ep_objects.empty()) {
    log_w("No Zigbee EPs to register");
  } else {
    log_d("Register all Zigbee EPs in list");
    err = esp_zb_device_register(_zb_ep_list);
    if (err != ESP_OK) {
      log_e("Failed to register Zigbee EPs");
      return false;
    }

    //print the list of Zigbee EPs from ep_objects
    log_i("List of registered Zigbee EPs:");
    for (std::list<ZigbeeEP *>::iterator it = ep_objects.begin(); it != ep_objects.end(); ++it) {
      log_i("Device type: %s, Endpoint: %d, Device ID: 0x%04x", getDeviceTypeString((*it)->_device_id), (*it)->_endpoint, (*it)->_device_id);
      if ((*it)->_power_source == ZB_POWER_SOURCE_BATTERY) {
        edBatteryPowered = true;
      }
    }
  }
  // Register Zigbee action handler
  esp_zb_core_action_handler_register(zb_action_handler);
  err = esp_zb_set_primary_network_channel_set(_primary_channel_mask);
  if (err != ESP_OK) {
    log_e("Failed to set primary network channel mask");
    return false;
  }

  // Register APSDATA INDICATION handler to catch bind/unbind requests
  esp_zb_aps_data_indication_handler_register(zb_apsde_data_indication_handler);

  //Erase NVRAM before creating connection to new Coordinator
  if (erase_nvs) {
    esp_zb_nvram_erase_at_start(true);
  }

  // Create Zigbee task and start Zigbee stack
  xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL);

  return true;
}

void ZigbeeCore::setRadioConfig(esp_zb_radio_config_t config) {
  _radio_config = config;
}

esp_zb_radio_config_t ZigbeeCore::getRadioConfig() {
  return _radio_config;
}

void ZigbeeCore::setHostConfig(esp_zb_host_config_t config) {
  _host_config = config;
}

esp_zb_host_config_t ZigbeeCore::getHostConfig() {
  return _host_config;
}

void ZigbeeCore::setPrimaryChannelMask(uint32_t mask) {
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
  if (started()) {
    log_v("Opening network for joining for %d seconds", time);
    esp_zb_bdb_open_network(time);
  }
}

void ZigbeeCore::closeNetwork() {
  if (started()) {
    log_v("Closing network");
    esp_zb_bdb_close_network();
  }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  //common variables
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
  //coordinator variables
  esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
  esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
  //router variables
  esp_zb_zdo_signal_device_update_params_t *dev_update_params = NULL;

  //main switch
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:  // Common
      log_i("Zigbee stack initialized");
      log_d("Zigbee channel mask: 0x%08x", esp_zb_get_channel_mask());
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:  // Common
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:       // Common
      if (err_status == ESP_OK) {
        log_i("Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
        if (esp_zb_bdb_is_factory_new()) {
          // Role specific code
          if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
            log_i("Start network formation");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
          } else {
            log_i("Start network steering");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            Zigbee._started = true;
            xSemaphoreGive(Zigbee.lock);
          }
        } else {
          log_i("Device rebooted");
          Zigbee._started = true;
          xSemaphoreGive(Zigbee.lock);
          if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR && Zigbee._open_network > 0) {
            log_i("Opening network for joining for %d seconds", Zigbee._open_network);
            esp_zb_bdb_open_network(Zigbee._open_network);
          } else {
            // Save the channel mask to NVRAM in case of reboot which may be on a different channel after a change in the network
            Zigbee.setNVRAMChannelMask(1 << esp_zb_get_current_channel());
            Zigbee._connected = true;  // Coordinator is always connected
          }
          Zigbee.searchBindings();
        }
      } else {
        /* commissioning failed */
        log_w("Commissioning failed, trying again...", esp_err_to_name(err_status));
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 500);
      }
      break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:  // Coordinator
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        if (err_status == ESP_OK) {
          esp_zb_ieee_addr_t extended_pan_id;
          esp_zb_get_extended_pan_id(extended_pan_id);
          log_i(
            "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
            extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2], extended_pan_id[1],
            extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address()
          );
          Zigbee._connected = true;
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
          log_i("Restart network formation (status: %s)", esp_err_to_name(err_status));
          esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:  // Router and End Device
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        if (err_status == ESP_OK) {
          log_i("Network steering started");
        }
        Zigbee._started = true;
        xSemaphoreGive(Zigbee.lock);
      } else {
        if (err_status == ESP_OK) {
          esp_zb_ieee_addr_t extended_pan_id;
          esp_zb_get_extended_pan_id(extended_pan_id);
          log_i(
            "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
            extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2], extended_pan_id[1],
            extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address()
          );
          Zigbee._connected = true;
          // Set channel mask and write to NVRAM, so that the device will re-join the network faster after reboot (scan only on the current channel)
          Zigbee.setNVRAMChannelMask(1 << esp_zb_get_current_channel());
        } else {
          log_i("Network steering was not successful (status: %s)", esp_err_to_name(err_status));
          esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:  // Coordinator
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        log_i("New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        esp_zb_zdo_match_desc_req_param_t cmd_req;
        cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
        cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
        log_v("Device capabilities: 0x%02x", dev_annce_params->capability);
        /*
            capability:
            Bit 0 – Alternate PAN Coordinator
            Bit 1 – Device type: 1- ZigBee Router; 0 – End Device
            Bit 2 – Power Source: 1 Main powered
            Bit 3 – Receiver on when Idle
            Bit 4 – Reserved
            Bit 5 – Reserved
            Bit 6 – Security capability
            Bit 7 – Reserved
        */
        // for each endpoint in the list call the findEndpoint function if not bounded or allowed to bind multiple devices
        for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
          log_d("Checking endpoint %d", (*it)->getEndpoint());
          if (!(*it)->epUseManualBinding()) {
            if (!(*it)->bound() || (*it)->epAllowMultipleBinding()) {
              // Check if the device is already bound
              bool found = false;
              // Get the list of devices bound to the EP
              std::list<zb_device_params_t *> bound_devices = (*it)->getBoundDevices();
              for (std::list<zb_device_params_t *>::iterator device = bound_devices.begin(); device != bound_devices.end(); ++device) {
                if (((*device)->short_addr == dev_annce_params->device_short_addr) || (memcmp((*device)->ieee_addr, dev_annce_params->ieee_addr, 8) == 0)) {
                  found = true;
                  log_d("Device already bound to endpoint %d", (*it)->getEndpoint());
                  break;
                }
              }
              if (!found) {
                log_d("Device not bound to endpoint %d and it is free to bound!", (*it)->getEndpoint());
                (*it)->findEndpoint(&cmd_req);
                log_d("Endpoint %d is searching for device", (*it)->getEndpoint());
                break;  // Only one endpoint per device
              }
            }
          }
        }
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:  // Router
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_ROUTER) {
        dev_update_params = (esp_zb_zdo_signal_device_update_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        log_i("New device commissioned or rejoined (short: 0x%04hx)", dev_update_params->short_addr);
        esp_zb_zdo_match_desc_req_param_t cmd_req;
        cmd_req.dst_nwk_addr = dev_update_params->short_addr;
        cmd_req.addr_of_interest = dev_update_params->short_addr;
        // for each endpoint in the list call the findEndpoint function if not bounded or allowed to bind multiple devices
        for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
          log_d("Checking endpoint %d", (*it)->getEndpoint());
          if (!(*it)->epUseManualBinding()) {
            if (!(*it)->bound() || (*it)->epAllowMultipleBinding()) {
              // Check if the device is already bound
              bool found = false;
              // Get the list of devices bound to the EP
              std::list<zb_device_params_t *> bound_devices = (*it)->getBoundDevices();
              for (std::list<zb_device_params_t *>::iterator device = bound_devices.begin(); device != bound_devices.end(); ++device) {
                if (((*device)->short_addr == dev_update_params->short_addr) || (memcmp((*device)->ieee_addr, dev_update_params->long_addr, 8) == 0)) {
                  found = true;
                  log_d("Device already bound to endpoint %d", (*it)->getEndpoint());
                  break;
                }
              }
              log_d("Device not bound to endpoint %d and it is free to bound!", (*it)->getEndpoint());
              if (!found) {
                (*it)->findEndpoint(&cmd_req);
                log_d("Endpoint %d is searching for device", (*it)->getEndpoint());
                break;  // Only one endpoint per device
              }
            }
          }
        }
      }
      break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:  // Coordinator
      if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_COORDINATOR) {
        if (err_status == ESP_OK) {
          if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
            log_i("Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
          } else {
            log_i("Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
          }
        }
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:  // End Device + Router
      // Received signal to leave the network
      if ((zigbee_role_t)Zigbee.getRole() != ZIGBEE_COORDINATOR) {
        leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        log_v("Signal to leave the network, leave type: %d", leave_params->leave_type);
        if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {  // Leave without rejoin -> Factory reset
          log_i("Leave without rejoin, factory reset the device");
          Zigbee.factoryReset(true);
        } else {  // Leave with rejoin -> Rejoin the network, only reboot the device
          log_i("Leave with rejoin, only reboot the device");
          ESP.restart();
        }
      }
      break;
    default: log_v("ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status)); break;
  }
}

// APS DATA INDICATION HANDLER to catch bind/unbind requests
bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind) {
  if (Zigbee.getDebugMode()) {
    log_d("APSDE INDICATION - Received APSDE-DATA indication, status: %d", ind.status);
    log_d(
      "APSDE INDICATION - dst_endpoint: %d, src_endpoint: %d, dst_addr_mode: %d, src_addr_mode: %d, cluster_id: 0x%04x, asdu_length: %d", ind.dst_endpoint,
      ind.src_endpoint, ind.dst_addr_mode, ind.src_addr_mode, ind.cluster_id, ind.asdu_length
    );
    log_d(
      "APSDE INDICATION - dst_short_addr: 0x%04x, src_short_addr: 0x%04x, profile_id: 0x%04x, security_status: %d, lqi: %d, rx_time: %d", ind.dst_short_addr,
      ind.src_short_addr, ind.profile_id, ind.security_status, ind.lqi, ind.rx_time
    );
  }
  if (ind.status == 0x00) {
    // Catch bind/unbind requests to update the bound devices list
    if (ind.cluster_id == 0x21 || ind.cluster_id == 0x22) {
      Zigbee.searchBindings();
    }
  } else {
    log_e("APSDE INDICATION - Invalid status of APSDE-DATA indication, error code: %d", ind.status);
  }
  return false;  //False to let the stack process the message as usual
}

void ZigbeeCore::factoryReset(bool restart) {
  if (restart) {
    log_v("Factory resetting Zigbee stack, device will reboot");
    esp_zb_factory_reset();
  } else {
    log_v("Factory resetting Zigbee NVRAM to factory default");
    log_w("The device will not reboot, to take effect please reboot the device manually");
    esp_zb_zcl_reset_nvram_to_factory_default();
  }
}

void ZigbeeCore::scanCompleteCallback(esp_zb_zdp_status_t zdo_status, uint8_t count, esp_zb_network_descriptor_t *nwk_descriptor) {
  log_v("Zigbee network scan complete");
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_v("Found %d networks", count);
    //print Zigbee networks
    for (int i = 0; i < count; i++) {
      log_v(
        "Network %d: PAN ID: 0x%04hx, Permit Joining: %s, Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, Channel: %d, Router Capacity: %s, End "
        "Device Capacity: %s",
        i, nwk_descriptor[i].short_pan_id, nwk_descriptor[i].permit_joining ? "Yes" : "No", nwk_descriptor[i].extended_pan_id[7],
        nwk_descriptor[i].extended_pan_id[6], nwk_descriptor[i].extended_pan_id[5], nwk_descriptor[i].extended_pan_id[4], nwk_descriptor[i].extended_pan_id[3],
        nwk_descriptor[i].extended_pan_id[2], nwk_descriptor[i].extended_pan_id[1], nwk_descriptor[i].extended_pan_id[0], nwk_descriptor[i].logic_channel,
        nwk_descriptor[i].router_capacity ? "Yes" : "No", nwk_descriptor[i].end_device_capacity ? "Yes" : "No"
      );
    }
    //save scan result and update scan status
    //copy network descriptor to _scan_result to keep the data after the callback
    Zigbee._scan_result = (esp_zb_network_descriptor_t *)malloc(count * sizeof(esp_zb_network_descriptor_t));
    memcpy(Zigbee._scan_result, nwk_descriptor, count * sizeof(esp_zb_network_descriptor_t));
    Zigbee._scan_status = count;
  } else {
    log_e("Failed to scan Zigbee network (status: 0x%x)", zdo_status);
    Zigbee._scan_status = ZB_SCAN_FAILED;
    Zigbee._scan_result = nullptr;
  }
}

void ZigbeeCore::scanNetworks(u_int32_t channel_mask, u_int8_t scan_duration) {
  if (!started()) {
    log_e("Zigbee stack is not started, cannot scan networks");
    return;
  }
  log_v("Scanning Zigbee networks");
  esp_zb_zdo_active_scan_request(channel_mask, scan_duration, scanCompleteCallback);
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
  _scan_status = ZB_SCAN_FAILED;
}

// Recall bounded devices from the binding table after reboot or when requested
void ZigbeeCore::bindingTableCb(const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx) {
  esp_zb_zdo_mgmt_bind_param_t *req = (esp_zb_zdo_mgmt_bind_param_t *)user_ctx;
  esp_zb_zdp_status_t zdo_status = (esp_zb_zdp_status_t)table_info->status;
  log_d("Binding table callback for address 0x%04x with status %d", req->dst_addr, zdo_status);

  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    // Print binding table log simple
    log_d("Binding table info: total %d, index %d, count %d", table_info->total, table_info->index, table_info->count);

    if (table_info->total == 0) {
      log_d("No binding table entries found");
      // Clear all bound devices since there are no entries
      for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
        log_d("Clearing bound devices for EP %d", (*it)->getEndpoint());
        (*it)->clearBoundDevices();
      }
      free(req);
      return;
    }

    // Create a set to track found devices using both short and IEEE addresses
    struct DeviceIdentifier {
      uint8_t endpoint;
      uint16_t short_addr;
      esp_zb_ieee_addr_t ieee_addr;
      bool is_ieee;

      bool operator<(const DeviceIdentifier &other) const {
        if (endpoint != other.endpoint) {
          return endpoint < other.endpoint;
        }
        if (is_ieee != other.is_ieee) {
          return is_ieee < other.is_ieee;
        }
        if (is_ieee) {
          return memcmp(ieee_addr, other.ieee_addr, sizeof(esp_zb_ieee_addr_t)) < 0;
        }
        return short_addr < other.short_addr;
      }
    };
    static std::set<DeviceIdentifier> found_devices;
    static std::vector<esp_zb_zdo_binding_table_record_t> all_records;

    // If this is the first chunk (index 0), clear the previous data
    if (table_info->index == 0) {
      found_devices.clear();
      all_records.clear();
    }

    // Add current records to our collection
    esp_zb_zdo_binding_table_record_t *record = table_info->record;
    for (int i = 0; i < table_info->count; i++) {
      log_d(
        "Processing record %d: src_endp %d, dst_endp %d, cluster_id 0x%04x, dst_addr_mode %d", i, record->src_endp, record->dst_endp, record->cluster_id,
        record->dst_addr_mode
      );
      all_records.push_back(*record);
      record = record->next;
    }

    // If this is not the last chunk, request the next one
    if (table_info->index + table_info->count < table_info->total) {
      log_d("Requesting next chunk of binding table (current index: %d, count: %d, total: %d)", table_info->index, table_info->count, table_info->total);
      req->start_index = table_info->index + table_info->count;
      esp_zb_zdo_binding_table_req(req, bindingTableCb, req);
    } else {
      // This is the last chunk, process all records
      log_d("Processing final chunk of binding table, total records: %d", all_records.size());
      for (const auto &record : all_records) {

        DeviceIdentifier dev_id;
        dev_id.endpoint = record.src_endp;
        dev_id.is_ieee = (record.dst_addr_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT);

        if (dev_id.is_ieee) {
          memcpy(dev_id.ieee_addr, record.dst_address.addr_long, sizeof(esp_zb_ieee_addr_t));
          dev_id.short_addr = 0xFFFF;  // Invalid short address
        } else {
          dev_id.short_addr = record.dst_address.addr_short;
          memset(dev_id.ieee_addr, 0, sizeof(esp_zb_ieee_addr_t));
        }

        // Track this device as found
        found_devices.insert(dev_id);
      }

      // Now process each endpoint and update its bound devices
      for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
        log_d("Processing endpoint %d", (*it)->getEndpoint());
        std::list<zb_device_params_t *> bound_devices = (*it)->getBoundDevices();
        std::list<zb_device_params_t *> devices_to_remove;

        // First, identify devices that need to be removed
        for (std::list<zb_device_params_t *>::iterator dev_it = bound_devices.begin(); dev_it != bound_devices.end(); ++dev_it) {
          DeviceIdentifier dev_id;
          dev_id.endpoint = (*it)->getEndpoint();

          // Create both short and IEEE address identifiers for the device
          bool found = false;

          // Check if device exists with short address
          if ((*dev_it)->short_addr != 0xFFFF) {
            dev_id.is_ieee = false;
            dev_id.short_addr = (*dev_it)->short_addr;
            memset(dev_id.ieee_addr, 0, sizeof(esp_zb_ieee_addr_t));
            if (found_devices.find(dev_id) != found_devices.end()) {
              found = true;
            }
          }

          // Check if device exists with IEEE address
          if (!found) {
            dev_id.is_ieee = true;
            memcpy(dev_id.ieee_addr, (*dev_it)->ieee_addr, sizeof(esp_zb_ieee_addr_t));
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
          if (record.src_endp == (*it)->getEndpoint()) {
            log_d("Processing binding record for EP %d", record.src_endp);
            zb_device_params_t *device = (zb_device_params_t *)calloc(1, sizeof(zb_device_params_t));
            if (!device) {
              log_e("Failed to allocate memory for device params");
              continue;
            }
            device->endpoint = record.dst_endp;

            bool is_ieee = (record.dst_addr_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT);
            if (is_ieee) {
              memcpy(device->ieee_addr, record.dst_address.addr_long, sizeof(esp_zb_ieee_addr_t));
              device->short_addr = 0xFFFF;
            } else {
              device->short_addr = record.dst_address.addr_short;
              memset(device->ieee_addr, 0, sizeof(esp_zb_ieee_addr_t));
            }

            // Check if device already exists
            bool device_exists = false;
            for (std::list<zb_device_params_t *>::iterator dev_it = bound_devices.begin(); dev_it != bound_devices.end(); ++dev_it) {
              if (is_ieee) {
                if (memcmp((*dev_it)->ieee_addr, device->ieee_addr, sizeof(esp_zb_ieee_addr_t)) == 0) {
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
                "Device bound to EP %d -> device endpoint: %d, %s: %s", record.src_endp, device->endpoint, is_ieee ? "ieee addr" : "short addr",
                is_ieee ? formatIEEEAddress(device->ieee_addr) : formatShortAddress(device->short_addr)
              );
            } else {
              log_d("Device already exists, freeing allocated memory");
              free(device);  // Free the device if it already exists
            }
          }
        }
      }

      // Print bound devices
      log_d("Filling bounded devices finished");
      free(req);
    }
  } else {
    log_e("Binding table request failed with status: %d", zdo_status);
    free(req);
  }
}

void ZigbeeCore::searchBindings() {
  esp_zb_zdo_mgmt_bind_param_t *mb_req = (esp_zb_zdo_mgmt_bind_param_t *)malloc(sizeof(esp_zb_zdo_mgmt_bind_param_t));
  mb_req->dst_addr = esp_zb_get_short_address();
  mb_req->start_index = 0;
  log_d("Requesting binding table for address 0x%04x", mb_req->dst_addr);
  esp_zb_zdo_binding_table_req(mb_req, bindingTableCb, (void *)mb_req);
}

void ZigbeeCore::resetNVRAMChannelMask() {
  _primary_channel_mask = ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK;
  esp_zb_set_channel_mask(_primary_channel_mask);
  zb_nvram_write_dataset(ZB_NVRAM_COMMON_DATA);
  log_v("Channel mask reset to all channels");
}

void ZigbeeCore::setNVRAMChannelMask(uint32_t mask) {
  _primary_channel_mask = mask;
  esp_zb_set_channel_mask(_primary_channel_mask);
  zb_nvram_write_dataset(ZB_NVRAM_COMMON_DATA);
  log_v("Channel mask set to 0x%08x", mask);
}

// Function to convert enum value to string
const char *ZigbeeCore::getDeviceTypeString(esp_zb_ha_standard_devices_t deviceId) {
  switch (deviceId) {
    case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID:              return "General On/Off switch";
    case ESP_ZB_HA_LEVEL_CONTROL_SWITCH_DEVICE_ID:       return "Level Control Switch";
    case ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID:              return "General On/Off output";
    case ESP_ZB_HA_LEVEL_CONTROLLABLE_OUTPUT_DEVICE_ID:  return "Level Controllable Output";
    case ESP_ZB_HA_SCENE_SELECTOR_DEVICE_ID:             return "Scene Selector";
    case ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID:         return "Configuration Tool";
    case ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID:             return "Remote Control";
    case ESP_ZB_HA_COMBINED_INTERFACE_DEVICE_ID:         return "Combined Interface";
    case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:             return "Range Extender";
    case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID:         return "Mains Power Outlet";
    case ESP_ZB_HA_DOOR_LOCK_DEVICE_ID:                  return "Door lock client";
    case ESP_ZB_HA_DOOR_LOCK_CONTROLLER_DEVICE_ID:       return "Door lock controller";
    case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID:              return "Simple Sensor device";
    case ESP_ZB_HA_CONSUMPTION_AWARENESS_DEVICE_ID:      return "Consumption Awareness Device";
    case ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID:               return "Home Gateway";
    case ESP_ZB_HA_SMART_PLUG_DEVICE_ID:                 return "Smart plug";
    case ESP_ZB_HA_WHITE_GOODS_DEVICE_ID:                return "White Goods";
    case ESP_ZB_HA_METER_INTERFACE_DEVICE_ID:            return "Meter Interface";
    case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:               return "On/Off Light Device";
    case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID:             return "Dimmable Light Device";
    case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID:       return "Color Dimmable Light Device";
    case ESP_ZB_HA_DIMMER_SWITCH_DEVICE_ID:              return "Dimmer Switch Device";
    case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID:        return "Color Dimmer Switch Device";
    case ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID:               return "Light Sensor";
    case ESP_ZB_HA_SHADE_DEVICE_ID:                      return "Shade";
    case ESP_ZB_HA_SHADE_CONTROLLER_DEVICE_ID:           return "Shade controller";
    case ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID:            return "Window Covering client";
    case ESP_ZB_HA_WINDOW_COVERING_CONTROLLER_DEVICE_ID: return "Window Covering controller";
    case ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID:       return "Heating/Cooling Unit device";
    case ESP_ZB_HA_THERMOSTAT_DEVICE_ID:                 return "Thermostat Device";
    case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID:         return "Temperature Sensor";
    case ESP_ZB_HA_IAS_CONTROL_INDICATING_EQUIPMENT_ID:  return "IAS Control and Indicating Equipment";
    case ESP_ZB_HA_IAS_ANCILLARY_CONTROL_EQUIPMENT_ID:   return "IAS Ancillary Control Equipment";
    case ESP_ZB_HA_IAS_ZONE_ID:                          return "IAS Zone";
    case ESP_ZB_HA_IAS_WARNING_DEVICE_ID:                return "IAS Warning Device";
    case ESP_ZB_HA_TEST_DEVICE_ID:                       return "Custom HA device for test";
    case ESP_ZB_HA_CUSTOM_TUNNEL_DEVICE_ID:              return "Custom Tunnel device";
    case ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID:                return "Custom Attributes Device";
    default:                                             return "Unknown device type";
  }
}

ZigbeeCore Zigbee = ZigbeeCore();

#endif  // CONFIG_ZB_ENABLED
