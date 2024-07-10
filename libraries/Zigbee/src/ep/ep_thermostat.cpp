#include "ep_temperature_sensor.h"

ZigbeeThermostat::ZigbeeThermostat(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) : Zigbee_EP(endpoint, cb) {
    _device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID;
    _version = 0;

    esp_zb_thermostat_cfg_t thermostat_cfg = ESP_ZB_DEFAULT_THERMOSTAT_CONFIG();
    _cluster_list = esp_zb_thermostat_clusters_create(&thermostat_cfg);
    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
        .app_device_version = _version
    };
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

static void find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
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

void ZigbeeThermostat::find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT};
    param->profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    param->num_in_clusters = 1;
    param->num_out_clusters = 0;
    param->cluster_list = cluster_list;
    esp_zb_zdo_match_cluster(param, user_cb, (void *)&temp_sensor);
}