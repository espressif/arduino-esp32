/* Zigbee Common Functions */
#include "Zigbee_core.h"
#include "Zigbee_handlers.cpp"
#include "Arduino.h"


Zigbee_Core::Zigbee_Core() {
  _radio_config = ZIGBEE_DEFAULT_RADIO_CONFIG();
  _host_config = ZIGBEE_DEFAULT_HOST_CONFIG();
  _zb_ep_list = esp_zb_ep_list_create();
  _primary_channel_mask = ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK;
  _open_network = 0;
}
Zigbee_Core::~Zigbee_Core() {}

//forward declaration
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);

bool Zigbee_Core::begin(esp_zb_cfg_t *role_cfg, bool erase_nvs) {
  if (!zigbeeInit(role_cfg, erase_nvs)){
    return false;
  }
  _role = (zigbee_role_t)role_cfg->esp_zb_role;
  return true;
}

bool Zigbee_Core::begin(zigbee_role_t role, bool erase_nvs) {
  bool status = true;
  switch (role)
  {
    case ZIGBEE_COORDINATOR: {
      _role = ZIGBEE_COORDINATOR;
      esp_zb_cfg_t zb_nwk_cfg = ZIGBEE_DEFAULT_COORDINATOR_CONFIG();
      status = zigbeeInit(&zb_nwk_cfg, erase_nvs);
      break;
    }
    case ZIGBEE_ROUTER: {
      _role = ZIGBEE_ROUTER;
      esp_zb_cfg_t zb_nwk_cfg = ZIGBEE_DEFAULT_ROUTER_CONFIG();
      status = zigbeeInit(&zb_nwk_cfg, erase_nvs);
      break;
    }
    case ZIGBEE_END_DEVICE: {
      _role = ZIGBEE_END_DEVICE;
      esp_zb_cfg_t zb_nwk_cfg = ZIGBEE_DEFAULT_ED_CONFIG();
      status = zigbeeInit(&zb_nwk_cfg, erase_nvs);
      break;
    }
    default:
      log_e("Invalid Zigbee Role");
      return false;
  }
  return status;
}

void Zigbee_Core::addEndpoint(Zigbee_EP *ep) {
  ep_objects.push_back(ep);
  
  log_d("Endpoint: %d, Device ID: 0x%04x", ep->_endpoint, ep->_device_id);
  //Register clusters and ep_list to the Zigbee_Core class's ep_list
  if (ep->_ep_config.endpoint == 0 || ep->_cluster_list == nullptr) {
    log_e("Endpoint config or Cluster list is not initialized, EP not added to Zigbee_Core's EP list");
    return;
  }

  esp_zb_ep_list_add_ep(_zb_ep_list, ep->_cluster_list, ep->_ep_config);
}

static void esp_zb_task(void *pvParameters) {
  /* initialize Zigbee stack */
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_main_loop_iteration();
}

// Zigbee core init function
bool Zigbee_Core::zigbeeInit(esp_zb_cfg_t *zb_cfg, bool erase_nvs) {
  // Zigbee platform configuration
  esp_zb_platform_config_t platform_config = {
    .radio_config = _radio_config,
    .host_config = _host_config,
  };

  esp_err_t err  = esp_zb_platform_config(&platform_config);
  if (err != ESP_OK) {
    log_e("Failed to configure Zigbee platform");
    return false;
  }

  // Initialize Zigbee stack
  log_d("Initialize Zigbee stack");
  esp_zb_init(zb_cfg);

  // Register all Zigbee EPs in list 
  log_d("Register all Zigbee EPs in list");
  err = esp_zb_device_register(_zb_ep_list);
  if (err != ESP_OK) {
    log_e("Failed to register Zigbee EPs");
    return false;
  }
  
  //print the list of Zigbee EPs from ep_objects
  log_i("List of registered Zigbee EPs:");
  for (std::list<Zigbee_EP*>::iterator it = ep_objects.begin(); it != ep_objects.end(); ++it) {
    log_i("Endpoint: %d, Device ID: 0x%04x", (*it)->_endpoint, (*it)->_device_id); //TODO: Idea device id -> device name
  }

  // Register Zigbee action handler
  esp_zb_core_action_handler_register(zb_action_handler);
  err = esp_zb_set_primary_network_channel_set(_primary_channel_mask);
  if (err != ESP_OK) {
    log_e("Failed to set primary network channel mask");
    return false;
  }

  //Erase NVRAM before creating connection to new Coordinator
  if (erase_nvs) {
    esp_zb_nvram_erase_at_start(true);
  }

  // Create Zigbee task and start Zigbee stack
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

  return true;
}

void Zigbee_Core::setRadioConfig(esp_zb_radio_config_t config) {
  _radio_config = config;
}

esp_zb_radio_config_t Zigbee_Core::getRadioConfig() {
  return _radio_config;
}

void Zigbee_Core::setHostConfig(esp_zb_host_config_t config) {
  _host_config = config;
}

esp_zb_host_config_t Zigbee_Core::getHostConfig() {
  return _host_config;
}

void Zigbee_Core::setPrimaryChannelMask(uint32_t mask) {
  _primary_channel_mask = mask;
}

void Zigbee_Core::setRebootOpenNetwork(uint8_t time) {
  _open_network = time;
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

  log_d("Zigbee role: %d", Zigbee._role);
  //main switch
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP: // Common
      log_i("Zigbee stack initialized");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START: // Common
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT: // Common
      if (err_status == ESP_OK) {
        log_i("Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
        if (esp_zb_bdb_is_factory_new()) {
          // Role specific code
          if ((zigbee_role_t)Zigbee._role == ZIGBEE_COORDINATOR) {
            log_i("Start network formation");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
          } else {
            log_i("Start network steering");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
          }
          //-----------------

        } else {
          log_i("Device rebooted");

          // Implement opening network for joining after reboot if set
          if ((zigbee_role_t)Zigbee._role == ZIGBEE_COORDINATOR && Zigbee._open_network > 0) {
            log_i("Openning network for joining for %d seconds", Zigbee._open_network);
            esp_zb_bdb_open_network(Zigbee._open_network);
          }
        }
      } else {
        /* commissioning failed */
        log_e("Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
      }
      break;
    case ESP_ZB_BDB_SIGNAL_FORMATION: // Coordinator
      if ((zigbee_role_t)Zigbee._role == ZIGBEE_COORDINATOR) {
        if (err_status == ESP_OK) {
          esp_zb_ieee_addr_t extended_pan_id;
          esp_zb_get_extended_pan_id(extended_pan_id);
          log_i(
            "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
            extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2], extended_pan_id[1],
            extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address()
          );
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
          log_i("Restart network formation (status: %s)", esp_err_to_name(err_status));
          esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING: // Router and End Device
      if ((zigbee_role_t)Zigbee._role == ZIGBEE_COORDINATOR) {
        if (err_status == ESP_OK) {
          log_i("Network steering started");
        }
      } 
      else {
        if (err_status == ESP_OK) {
          esp_zb_ieee_addr_t extended_pan_id;
          esp_zb_get_extended_pan_id(extended_pan_id);
          log_i(
            "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
            extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2], extended_pan_id[1],
            extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address()
          );
        } else {
          log_i("Network steering was not successful (status: %s)", esp_err_to_name(err_status));
          esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: // Coordinator
      if ((zigbee_role_t)Zigbee._role == ZIGBEE_COORDINATOR) {
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        log_i("New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        esp_zb_zdo_match_desc_req_param_t cmd_req;
        cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
        cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
        log_i("Device capabilities: 0x%02x", dev_annce_params->capability);
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

        //TODO: Save the device short address and endpoint to the list ????????

        // for each endpoint in the list call the find_endpoint function if not bounded or allowed to bind multiple devices
        for (std::list<Zigbee_EP*>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
          if (!(*it)->_is_bound || (*it)->_allow_multiple_binding) {
            (*it)->find_endpoint(&cmd_req);
          }
        }
      }
      break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: // Coordinator
      if ((zigbee_role_t)Zigbee._role == ZIGBEE_COORDINATOR) {
        if (err_status == ESP_OK) {
          if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
            log_i("Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
          } else {
            log_i("Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
          }
        }
      }
      break;
    default: log_i("ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status)); break;
  }
}

void Zigbee_Core::factoryReset() {
  log_v("Factory reseting Zigbee stack, device will reboot");
  esp_zb_factory_reset();
}


// TODO: Implement scanning network
// /**
//  * @brief   Active scan available network.
//  *
//  * Network discovery service for scanning available network
//  *
//  * @param[in] channel_mask Valid channel mask is from 0x00000800 (only channel 11) to 0x07FFF800 (all channels from 11 to 26)
//  * @param[in] scan_duration Time to spend scanning each channel
//  * @param[in] user_cb   A user callback to get the active scan result please refer to esp_zb_zdo_scan_complete_callback_t
//  */
// void esp_zb_zdo_active_scan_request(uint32_t channel_mask, uint8_t scan_duration, esp_zb_zdo_scan_complete_callback_t user_cb);

// NOTE: Binding functions to not forget
// void esp_zb_zdo_device_bind_req(esp_zb_zdo_bind_req_param_t *cmd_req, esp_zb_zdo_bind_callback_t user_cb, void *user_ctx)
// {
//     uint8_t output = 0;
//     uint16_t outlen = sizeof(uint8_t);

//     typedef struct {
//         esp_zb_zdo_bind_req_param_t bind_req;
//         esp_zb_user_cb_t            bind_usr;
//     } esp_zb_zdo_bind_req_t;

//     esp_zb_zdo_bind_req_t zdo_data = {
//         .bind_usr = {
//             .user_ctx = (uint32_t)user_ctx,
//             .user_cb = (uint32_t)user_cb,
//         },
//     };

//     memcpy(&zdo_data.bind_req, cmd_req, sizeof(esp_zb_zdo_bind_req_param_t));
//     esp_host_zb_output(ESP_ZNSP_ZDO_BIND_SET, &zdo_data, sizeof(esp_zb_zdo_bind_req_t), &output, &outlen);
// }

// Function to convert enum value to string
const char* Zigbee_Core::getDeviceTypeString(esp_zb_ha_standard_devices_t deviceId) {
    switch (deviceId) {
        case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID: return "General On/Off switch";
        case ESP_ZB_HA_LEVEL_CONTROL_SWITCH_DEVICE_ID: return "Level Control Switch";
        case ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID: return "General On/Off output";
        case ESP_ZB_HA_LEVEL_CONTROLLABLE_OUTPUT_DEVICE_ID: return "Level Controllable Output";
        case ESP_ZB_HA_SCENE_SELECTOR_DEVICE_ID: return "Scene Selector";
        case ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID: return "Configuration Tool";
        case ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID: return "Remote Control";
        case ESP_ZB_HA_COMBINED_INTERFACE_DEVICE_ID: return "Combined Interface";
        case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID: return "Range Extender";
        case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID: return "Mains Power Outlet";
        case ESP_ZB_HA_DOOR_LOCK_DEVICE_ID: return "Door lock client";
        case ESP_ZB_HA_DOOR_LOCK_CONTROLLER_DEVICE_ID: return "Door lock controller";
        case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID: return "Simple Sensor device";
        case ESP_ZB_HA_CONSUMPTION_AWARENESS_DEVICE_ID: return "Consumption Awareness Device";
        case ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID: return "Home Gateway";
        case ESP_ZB_HA_SMART_PLUG_DEVICE_ID: return "Smart plug";
        case ESP_ZB_HA_WHITE_GOODS_DEVICE_ID: return "White Goods";
        case ESP_ZB_HA_METER_INTERFACE_DEVICE_ID: return "Meter Interface";
        case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID: return "On/Off Light Device";
        case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID: return "Dimmable Light Device";
        case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID: return "Color Dimmable Light Device";
        case ESP_ZB_HA_DIMMER_SWITCH_DEVICE_ID: return "Dimmer Switch Device";
        case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID: return "Color Dimmer Switch Device";
        case ESP_ZB_HA_SHADE_DEVICE_ID: return "Shade";
        case ESP_ZB_HA_SHADE_CONTROLLER_DEVICE_ID: return "Shade controller";
        case ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID: return "Window Covering client";
        case ESP_ZB_HA_WINDOW_COVERING_CONTROLLER_DEVICE_ID: return "Window Covering controller";
        case ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID: return "Heating/Cooling Unit device";
        case ESP_ZB_HA_THERMOSTAT_DEVICE_ID: return "Thermostat Device";
        case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID: return "Temperature Sensor";
        case ESP_ZB_HA_IAS_CONTROL_INDICATING_EQUIPMENT_ID: return "IAS Control and Indicating Equipment";
        case ESP_ZB_HA_IAS_ANCILLARY_CONTROL_EQUIPMENT_ID: return "IAS Ancillary Control Equipment";
        case ESP_ZB_HA_IAS_ZONE_ID: return "IAS Zone";
        case ESP_ZB_HA_IAS_WARNING_DEVICE_ID: return "IAS Warning Device";
        case ESP_ZB_HA_TEST_DEVICE_ID: return "Custom HA device for test";
        case ESP_ZB_HA_CUSTOM_TUNNEL_DEVICE_ID: return "Custom Tunnel device";
        case ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID: return "Custom Attributes Device";
        default: return "Unknown device type";
    }
}

Zigbee_Core Zigbee = Zigbee_Core();
