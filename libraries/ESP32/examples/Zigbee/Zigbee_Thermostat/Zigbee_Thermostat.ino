// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief This example demonstrates simple Zigbee thermostat.
 *
 * The example demonstrates how to use ESP Zigbee stack to get data from temperature
 * sensor end device and act as an thermostat.
 * The temperature sensor is a Zigbee end device, which is controlled by a Zigbee coordinator (thermostat).
 * Button switch and Zigbee runs in separate tasks.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"

#define ARRAY_LENTH(arr) (sizeof(arr) / sizeof(arr[0]))

/* Switch configuration */
#define GPIO_INPUT_IO_TOGGLE_SWITCH GPIO_NUM_9
#define PAIR_SIZE(TYPE_STR_PAIR)    (sizeof(TYPE_STR_PAIR) / sizeof(TYPE_STR_PAIR[0]))

typedef enum {
  SWITCH_ON_CONTROL,
  SWITCH_OFF_CONTROL,
  SWITCH_ONOFF_TOGGLE_CONTROL,
  SWITCH_LEVEL_UP_CONTROL,
  SWITCH_LEVEL_DOWN_CONTROL,
  SWITCH_LEVEL_CYCLE_CONTROL,
  SWITCH_COLOR_CONTROL,
} switch_func_t;

typedef struct {
  uint8_t pin;
  switch_func_t func;
} switch_func_pair_t;

typedef enum {
  SWITCH_IDLE,
  SWITCH_PRESS_ARMED,
  SWITCH_PRESS_DETECTED,
  SWITCH_PRESSED,
  SWITCH_RELEASE_DETECTED,
} switch_state_t;

static switch_func_pair_t button_func_pair[] = {{GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}};

/* Default Coordinator config */
#define ESP_ZB_ZC_CONFIG()                                                                                        \
  {                                                                                                               \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR, .install_code_policy = INSTALLCODE_POLICY_ENABLE, .nwk_cfg = { \
      .zczr_cfg =                                                                                                 \
        {                                                                                                         \
          .max_children = MAX_CHILDREN,                                                                           \
        },                                                                                                        \
    }                                                                                                             \
  }

#define ESP_ZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ESP_ZB_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

/* Temperature sensor device parameters */
typedef struct temp_sensor_device_params_s {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
} temp_sensor_device_params_t;

typedef struct zbstring_s {
  uint8_t len;
  char data[];
} ESP_ZB_PACKED_STRUCT zbstring_t;

static temp_sensor_device_params_t temp_sensor;

/* Zigbee configuration */
#define MAX_CHILDREN                10                                   /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE   false                                /* enable the install code policy for security */
#define HA_THERMOSTAT_ENDPOINT      1                                    /* esp light switch device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME \
  "\x0B"                  \
  "ESPRESSIF"
#define MODEL_IDENTIFIER "\x09" CONFIG_IDF_TARGET

/********************* Zigbee functions **************************/
static float zb_s16_to_temperature(int16_t value) {
  return 1.0 * value / 100;
}

static void esp_zb_buttons_handler(switch_func_pair_t *button_func_pair) {
  if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
    /* Send "read attributes" command to the bound sensor */
    esp_zb_zcl_read_attr_cmd_t read_req;
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    read_req.zcl_basic_cmd.src_endpoint = HA_THERMOSTAT_ENDPOINT;
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

    uint16_t attributes[] = {
      ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID,
      ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
    };
    read_req.attr_number = ARRAY_LENTH(attributes);
    read_req.attr_field = attributes;

    /* Send "configure report attribute" command to the bound sensor */
    esp_zb_zcl_config_report_cmd_t report_cmd;
    report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    report_cmd.zcl_basic_cmd.src_endpoint = HA_THERMOSTAT_ENDPOINT;
    report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

    int16_t report_change = 200; /* report on each 2 degree changes */
    esp_zb_zcl_config_report_record_t records[] = {
      {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .attributeID = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
        .min_interval = 0,
        .max_interval = 10,
        .reportable_change = &report_change,
      },
    };
    report_cmd.record_number = ARRAY_LENTH(records);
    report_cmd.record_field = records;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_config_report_cmd_req(&report_cmd);
    esp_zb_lock_release();
    log_i("Send 'configure reporting' command");

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&read_req);
    esp_zb_lock_release();
    log_i("Send 'read attributes' command");
  }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)user_ctx;

  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    /* Local binding succeeds */
    if (bind_req->req_dst_addr == esp_zb_get_short_address()) {
      log_i("Successfully bind the temperature sensor from address(0x%x) on endpoint(%d)", temp_sensor.short_addr, temp_sensor.endpoint);

      /* Read peer Manufacture Name & Model Identifier */
      esp_zb_zcl_read_attr_cmd_t read_req;
      read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
      read_req.zcl_basic_cmd.src_endpoint = HA_THERMOSTAT_ENDPOINT;
      read_req.zcl_basic_cmd.dst_endpoint = temp_sensor.endpoint;
      read_req.zcl_basic_cmd.dst_addr_u.addr_short = temp_sensor.short_addr;
      read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;

      uint16_t attributes[] = {
        ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
        ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
      };
      read_req.attr_number = ARRAY_LENTH(attributes);
      read_req.attr_field = attributes;

      esp_zb_zcl_read_attr_cmd_req(&read_req);
    }
    if (bind_req->req_dst_addr == temp_sensor.short_addr) {
      log_i("The temperature sensor from address(0x%x) on endpoint(%d) successfully binds us", temp_sensor.short_addr, temp_sensor.endpoint);
    }
    free(bind_req);
  } else {
    /* Bind failed, maybe retry the binding ? */

    // esp_zb_zdo_device_bind_req(bind_req, bind_cb, bind_req);
  }
}

static void user_find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_i("Found temperature sensor");
    /* Store the information of the remote device */
    temp_sensor_device_params_t *sensor = (temp_sensor_device_params_t *)user_ctx;
    sensor->endpoint = endpoint;
    sensor->short_addr = addr;
    esp_zb_ieee_address_by_short(sensor->short_addr, sensor->ieee_addr);
    log_d("Temperature sensor found: short address(0x%x), endpoint(%d)", sensor->short_addr, sensor->endpoint);

    /* 1. Send binding request to the sensor */
    esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(sizeof(esp_zb_zdo_bind_req_param_t), 1);
    bind_req->req_dst_addr = addr;
    log_d("Request temperature sensor to bind us");

    /* populate the src information of the binding */
    memcpy(bind_req->src_address, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bind_req->src_endp = endpoint;
    bind_req->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
    log_d("Bind temperature sensor");

    /* populate the dst information of the binding */
    bind_req->dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    esp_zb_get_long_address(bind_req->dst_address_u.addr_long);
    bind_req->dst_endp = HA_THERMOSTAT_ENDPOINT;

    log_i("Request temperature sensor to bind us");
    esp_zb_zdo_device_bind_req(bind_req, bind_cb, bind_req);

    /* 2. Send binding request to self */
    bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(sizeof(esp_zb_zdo_bind_req_param_t), 1);
    bind_req->req_dst_addr = esp_zb_get_short_address();

    /* populate the src information of the binding */
    esp_zb_get_long_address(bind_req->src_address);
    bind_req->src_endp = HA_THERMOSTAT_ENDPOINT;
    bind_req->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

    /* populate the dst information of the binding */
    bind_req->dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req->dst_address_u.addr_long, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bind_req->dst_endp = endpoint;

    log_i("Bind temperature sensor");
    esp_zb_zdo_device_bind_req(bind_req, bind_cb, bind_req);
  }
}

static void find_temperature_sensor(esp_zb_zdo_match_desc_req_param_t *param, esp_zb_zdo_match_desc_callback_t user_cb, void *user_ctx) {
  uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT};
  param->profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  param->num_in_clusters = 1;
  param->num_out_clusters = 0;
  param->cluster_list = cluster_list;
  esp_zb_zdo_match_cluster(param, user_cb, (void *)&temp_sensor);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
  esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      log_i("Zigbee stack initialized");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        log_i("Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
        if (esp_zb_bdb_is_factory_new()) {
          log_i("Start network formation");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        } else {
          log_i("Device rebooted");
          log_i("Openning network for joining for %d seconds", 180);
          esp_zb_bdb_open_network(180);
        }
      } else {
        log_e("Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
      }
      break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
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
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      if (err_status == ESP_OK) {
        log_i("Network steering started");
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
      dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
      log_i("New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
      esp_zb_zdo_match_desc_req_param_t cmd_req;
      cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
      cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
      find_temperature_sensor(&cmd_req, user_find_cb, NULL);
      break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
      if (err_status == ESP_OK) {
        if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
          log_i("Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
        } else {
          log_w("Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
        }
      }
      break;
    default: log_i("ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status)); break;
  }
}

static void esp_app_zb_attribute_handler(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute) {
  /* Basic cluster attributes */
  if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
    if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING && attribute->data.value) {
      zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
      char *string = (char *)malloc(zbstr->len + 1);
      memcpy(string, zbstr->data, zbstr->len);
      string[zbstr->len] = '\0';
      log_i("Peer Manufacturer is \"%s\"", string);
      free(string);
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING && attribute->data.value) {
      zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
      char *string = (char *)malloc(zbstr->len + 1);
      memcpy(string, zbstr->data, zbstr->len);
      string[zbstr->len] = '\0';
      log_i("Peer Model is \"%s\"", string);
      free(string);
    }
  }

  /* Temperature Measurement cluster attributes */
  if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
      int16_t value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      log_i("Measured Value is %.2f degrees Celsius", zb_s16_to_temperature(value));
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
      int16_t min_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      log_i("Min Measured Value is %.2f degrees Celsius", zb_s16_to_temperature(min_value));
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
      int16_t max_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      log_i("Max Measured Value is %.2f degrees Celsius", zb_s16_to_temperature(max_value));
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t tolerance = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      log_i("Tolerance is %.2f degrees Celsius", 1.0 * tolerance / 100);
    }
  }
}

static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message) {
  if (!message) {
    log_e("Empty message");
  }
  if (message->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->status);
  }
  log_i(
    "Received report from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)", message->src_address.u.short_addr, message->src_endpoint,
    message->dst_endpoint, message->cluster
  );
  esp_app_zb_attribute_handler(message->cluster, &message->attribute);
  return ESP_OK;
}

static esp_err_t zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message) {
  if (!message) {
    log_e("Empty message");
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
  }
  log_i(
    "Read attribute response: from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)", message->info.src_address.u.short_addr,
    message->info.src_endpoint, message->info.dst_endpoint, message->info.cluster
  );

  esp_zb_zcl_read_attr_resp_variable_t *variable = message->variables;
  while (variable) {
    log_i(
      "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)", variable->status, message->info.cluster,
      variable->attribute.id, variable->attribute.data.type, variable->attribute.data.value ? *(uint8_t *)variable->attribute.data.value : 0
    );
    if (variable->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
      esp_app_zb_attribute_handler(message->info.cluster, &variable->attribute);
    }

    variable = variable->next;
  }

  return ESP_OK;
}

static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message) {
  if (!message) {
    log_e("Empty message");
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
  }
  esp_zb_zcl_config_report_resp_variable_t *variable = message->variables;
  while (variable) {
    log_i(
      "Configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)", variable->status, message->info.cluster, variable->direction,
      variable->attribute_id
    );
    variable = variable->next;
  }

  return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  switch (callback_id) {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:            ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message); break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:     ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message); break;
    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID: ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message); break;
    default:                                       log_w("Receive Zigbee action(0x%x) callback", callback_id); break;
  }
  return ret;
}

static esp_zb_cluster_list_t *custom_thermostat_clusters_create(esp_zb_thermostat_cfg_t *thermostat) {
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(thermostat->basic_cfg));
  ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)MANUFACTURER_NAME));
  ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)MODEL_IDENTIFIER));
  ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
  ESP_ERROR_CHECK(
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&(thermostat->identify_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE)
  );
  ESP_ERROR_CHECK(
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE)
  );
  ESP_ERROR_CHECK(
    esp_zb_cluster_list_add_thermostat_cluster(cluster_list, esp_zb_thermostat_cluster_create(&(thermostat->thermostat_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE)
  );
  /* Add temperature measurement cluster for attribute reporting */
  ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, esp_zb_temperature_meas_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
  return cluster_list;
}

static esp_zb_ep_list_t *custom_thermostat_ep_create(uint8_t endpoint_id, esp_zb_thermostat_cfg_t *thermostat) {
  esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
  esp_zb_endpoint_config_t endpoint_config = {
    .endpoint = endpoint_id, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID, .app_device_version = 0
  };
  esp_zb_ep_list_add_ep(ep_list, custom_thermostat_clusters_create(thermostat), endpoint_config);
  return ep_list;
}

static void esp_zb_task(void *pvParameters) {
  /* Initialize Zigbee stack */
  esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
  esp_zb_init(&zb_nwk_cfg);
  /* Create customized thermostat endpoint */
  esp_zb_thermostat_cfg_t thermostat_cfg = ESP_ZB_DEFAULT_THERMOSTAT_CONFIG();
  esp_zb_ep_list_t *esp_zb_thermostat_ep = custom_thermostat_ep_create(HA_THERMOSTAT_ENDPOINT, &thermostat_cfg);
  /* Register the device */
  esp_zb_device_register(esp_zb_thermostat_ep);

  esp_zb_core_action_handler_register(zb_action_handler);
  esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_main_loop_iteration();
}

/********************* GPIO functions **************************/
static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  xQueueSendFromISR(gpio_evt_queue, (switch_func_pair_t *)arg, NULL);
}

static void switch_gpios_intr_enabled(bool enabled) {
  for (int i = 0; i < PAIR_SIZE(button_func_pair); ++i) {
    if (enabled) {
      enableInterrupt((button_func_pair[i]).pin);
    } else {
      disableInterrupt((button_func_pair[i]).pin);
    }
  }
}

/********************* Arduino functions **************************/
void setup() {
  // Init Zigbee
  esp_zb_platform_config_t config = {
    .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
    .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };

  ESP_ERROR_CHECK(esp_zb_platform_config(&config));

  // Init button switch
  for (int i = 0; i < PAIR_SIZE(button_func_pair); i++) {
    pinMode(button_func_pair[i].pin, INPUT_PULLUP);
    /* create a queue to handle gpio event from isr */
    gpio_evt_queue = xQueueCreate(10, sizeof(switch_func_pair_t));
    if (gpio_evt_queue == 0) {
      log_e("Queue was not created and must not be used");
      while (1);
    }
    attachInterruptArg(button_func_pair[i].pin, gpio_isr_handler, (void *)(button_func_pair + i), FALLING);
  }

  // Start Zigbee task
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}

void loop() {
  // Handle button switch in loop()
  uint8_t pin = 0;
  switch_func_pair_t button_func_pair;
  static switch_state_t switch_state = SWITCH_IDLE;
  bool evt_flag = false;

  /* check if there is any queue received, if yes read out the button_func_pair */
  if (xQueueReceive(gpio_evt_queue, &button_func_pair, portMAX_DELAY)) {
    pin = button_func_pair.pin;
    switch_gpios_intr_enabled(false);
    evt_flag = true;
  }
  while (evt_flag) {
    bool value = digitalRead(pin);
    switch (switch_state) {
      case SWITCH_IDLE:           switch_state = (value == LOW) ? SWITCH_PRESS_DETECTED : SWITCH_IDLE; break;
      case SWITCH_PRESS_DETECTED: switch_state = (value == LOW) ? SWITCH_PRESS_DETECTED : SWITCH_RELEASE_DETECTED; break;
      case SWITCH_RELEASE_DETECTED:
        switch_state = SWITCH_IDLE;
        /* callback to button_handler */
        (*esp_zb_buttons_handler)(&button_func_pair);
        break;
      default: break;
    }
    if (switch_state == SWITCH_IDLE) {
      switch_gpios_intr_enabled(true);
      evt_flag = false;
      break;
    }
    delay(10);
  }
}
