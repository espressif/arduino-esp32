/* Common Class for Zigbee End Point */

#include "Zigbee_ep.h"

uint8_t Zigbee_EP::_endpoint = 0;
bool Zigbee_EP::_is_bound = false;

/* Zigbee End Device Class */
Zigbee_EP::Zigbee_EP(uint8_t endpoint) {
    _endpoint = endpoint;
    _ep_config.endpoint = 0;
    _cluster_list = nullptr;
}

Zigbee_EP::~Zigbee_EP() {

}
