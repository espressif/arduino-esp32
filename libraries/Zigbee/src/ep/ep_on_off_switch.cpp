#include "ep_on_off_switch.h"
#include <Arduino.h>
//#include "esp_zigbee_zcl_common.h"

typedef struct light_bulb_device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t  endpoint;
    uint16_t short_addr;
} light_bulb_device_params_t;

// Definition and initialization of the static member variables


ZigbeeSwitch::ZigbeeSwitch(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) : Zigbee_EP(endpoint, cb) {
    _device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID;
    _version = 0;

    esp_zb_on_off_switch_cfg_t switch_cfg = ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();
    _cluster_list = esp_zb_on_off_switch_clusters_create(&switch_cfg);

    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = _version
    };
}

void ZigbeeSwitch::bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_i("Bound successfully!");
    if (user_ctx) {
      light_bulb_device_params_t *light = (light_bulb_device_params_t *)user_ctx;
      log_i("The light originating from address(0x%x) on endpoint(%d)", light->short_addr, light->endpoint);
      free(light);
    }
    _is_bound = true;
  }
}

void ZigbeeSwitch::find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        log_d("Found light endpoint");
        esp_zb_zdo_bind_req_param_t bind_req;
        light_bulb_device_params_t *light = (light_bulb_device_params_t *)malloc(sizeof(light_bulb_device_params_t));
        light->endpoint = endpoint;
        light->short_addr = addr;
        esp_zb_ieee_address_by_short(light->short_addr, light->ieee_addr);
        esp_zb_get_long_address(bind_req.src_address);
        bind_req.src_endp = _endpoint; //_dev_endpoint;
        bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        memcpy(bind_req.dst_address_u.addr_long, light->ieee_addr, sizeof(esp_zb_ieee_addr_t));
        bind_req.dst_endp = endpoint;
        bind_req.req_dst_addr = esp_zb_get_short_address();
        log_i("Try to bind On/Off");
        esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void *)light);
    } else {
        log_d("No light endpoint found");
    }
}

// find on_off light endpoint
void ZigbeeSwitch::find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
    esp_zb_zdo_match_desc_req_param_t on_off_req = {
        .dst_nwk_addr = cmd_req->dst_nwk_addr,
        .addr_of_interest = cmd_req->addr_of_interest,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .num_in_clusters = 1,
        .num_out_clusters = 1,
        .cluster_list = cluster_list,
    };

    esp_zb_zdo_match_cluster(&on_off_req, find_cb, NULL);
}
