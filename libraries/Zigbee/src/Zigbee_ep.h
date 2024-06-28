/* Common Class for Zigbee End Device */

#pragma once

#include "Zigbee_core.h"

/* Zigbee End Device Class */
class Zigbee_EP {
  public:
    Zigbee_EP(uint8_t endpoint = 10, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message) = NULL);
    //Zigbee_EP(uint8_t endpoint = 10, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message) = NULL, void *arg = NULL);
    ~Zigbee_EP();

    virtual void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) = 0;

    void (*_cb)(const esp_zb_zcl_set_attr_value_message_t *message);
    //void *_arg;

    static uint8_t _endpoint;
    esp_zb_ha_standard_devices_t _device_id; //type of device
    uint8_t _version;

    esp_zb_endpoint_config_t _ep_config;
    esp_zb_cluster_list_t *_cluster_list;

    // Set ep config and cluster list
    void set_ep_config(esp_zb_endpoint_config_t ep_config, esp_zb_cluster_list_t *cluster_list) {
        _ep_config = ep_config;
        _cluster_list = cluster_list;
    }

    static bool _is_bound;
    static bool is_bound() { return _is_bound; }

    friend class ZigbeeCore;

    // List of Zigbee EP classes 
    friend class ZigbeeLight;

};

