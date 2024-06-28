/* Common Class for Zigbee End Point */

#include "Zigbee_ep.h"
#include "Arduino.h"

uint8_t Zigbee_EP::_endpoint = 0;
bool Zigbee_EP::_is_bound = false;

/* Zigbee End Device Class */
Zigbee_EP::Zigbee_EP(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) {
    _endpoint = endpoint;
    _cb = cb;
    _ep_config.endpoint = 0;
    _cluster_list = nullptr;
}

Zigbee_EP::~Zigbee_EP() {

}
