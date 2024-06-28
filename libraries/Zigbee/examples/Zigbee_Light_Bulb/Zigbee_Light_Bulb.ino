// Copyright 2023 Espressif Systems (Shanghai) PTE LTD
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
 * @brief This example demonstrates simple Zigbee light bulb.
 *
 * The example demonstrates how to use ESP Zigbee stack to create a end device light bulb.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee_core.h"
#include "ep/ep_on_off_light.h"

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"

#define LED_PIN RGB_BUILTIN
#define ZIGBEE_LIGHT_ENDPOINT       10                                   /* esp light bulb device endpoint, used to process light controlling commands */

/* Handle the light attribute */
// User callback for Zigbee actions to handle turning on/off the light called by Zigbee stack in zb_attribute_handler()

static void light_cb(const esp_zb_zcl_set_attr_value_message_t *message) {
  bool light_state = 0;
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state;
      log_i("Light sets to %s", light_state ? "On" : "Off");
      neopixelWrite(LED_PIN, 255 * light_state, 255 * light_state, 255 * light_state);  // Toggle light
    }
  }
  return;
}

ZigbeeLight zbLight = ZigbeeLight(ZIGBEE_LIGHT_ENDPOINT, light_cb);

/********************* Arduino functions **************************/
void setup() {
  // Init RMT and leave light OFF
  neopixelWrite(LED_PIN, 0, 0, 0);

  //Add endpoint to Zigbee Core
  log_d("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbLight);
  
  // When all EPs are registered, start Zigbee. By default acts as Zigbee_End_Device
  log_d("Calling Zigbee.begin()");
  Zigbee.begin();
}

void loop() {
  //empty, zigbee running in task
}