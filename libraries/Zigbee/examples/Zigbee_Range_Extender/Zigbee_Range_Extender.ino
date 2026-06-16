// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief This example demonstrates simple Zigbee Range Extender (router).
 *
 * The example demonstrates how to use Zigbee library to create a Zigbee network ragbe extender (router).
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator/router mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee light bulb configuration */
#define USE_CUSTOM_ZIGBEE_CONFIG 1
#define ZIGBEE_EXTENDER_ENDPOINT 1

#ifndef LED_BUILTIN
#define LED_BUILTIN 4
#endif

uint8_t led = LED_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeRangeExtender zbExtender = ZigbeeRangeExtender(ZIGBEE_EXTENDER_ENDPOINT);

/************************** Identify ******************************/
// Create a task on identify call to handle the identify function
void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %u seconds", time);
  if (time == 0) {
    digitalWrite(led, LOW);
    return;
  }
  digitalWrite(led, blink);
  blink = !blink;
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init LED and turn it OFF (if LED_PIN == RGB_BUILTIN, the rgbLedWrite() will be used under the hood)
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Initialize Zigbee stack as router (must be before EP setters)
#if USE_CUSTOM_ZIGBEE_CONFIG
  esp_zigbee_device_config_t zigbeeConfig = ZIGBEE_DEFAULT_ROUTER_CONFIG();
  zigbeeConfig.nwk_cfg.zczr_cfg.max_children = 20;  // 10 is default
  if (!Zigbee.role(&zigbeeConfig)) {
#else
  if (!Zigbee.role(ZIGBEE_ROUTER)) {
#endif
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Optional: Set callback function for device identify
  zbExtender.onIdentify(identify);

  // Optional: Set Zigbee device name and model
  zbExtender.setManufacturerAndModel("Espressif", "ZigbeeRangeExtender");

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbExtender);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }
}
