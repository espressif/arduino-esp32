// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include <stdint.h>
#include <esp_event.h>
#ifdef __cplusplus
extern "C"
{
#endif

/** ESP RainMaker Common Event Base */
ESP_EVENT_DECLARE_BASE(RMAKER_COMMON_EVENT);

typedef enum {
    /** Node reboot has been triggered. The associated event data is the time in seconds
     * (type: uint8_t) after which the node will reboot. Note that this time may not be
     * accurate as the events are received asynchronously.*/
    RMAKER_EVENT_REBOOT,
    /** Wi-Fi credentials reset. Triggered after calling esp_rmaker_wifi_reset() */
    RMAKER_EVENT_WIFI_RESET,
    /** Node reset to factory defaults. Triggered after calling esp_rmaker_factory_reset() */
    RMAKER_EVENT_FACTORY_RESET,
    /** Connected to MQTT Broker */
    RMAKER_MQTT_EVENT_CONNECTED,
    /** Disconnected from MQTT Broker */
    RMAKER_MQTT_EVENT_DISCONNECTED,
    /** MQTT message published successfully */
    RMAKER_MQTT_EVENT_PUBLISHED,
} esp_rmaker_mqtt_event_t;
#ifdef __cplusplus
}
#endif
