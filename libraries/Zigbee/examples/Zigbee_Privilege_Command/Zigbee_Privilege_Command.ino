// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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
 * @brief This example demonstrates Zigbee privilege command handling.
 *
 * The example shows how to use addPrivilegeCommand() and onPrivilegeCommand()
 * for specific ZCL command IDs: those commands are delivered to your callback and
 * are not handled by the stack. Any command you do not register is still handled
 * by the stack as usual.
 *
 * This is useful when controllers (e.g. Philips Hue) send commands like
 * "Off with effect" (0x40) that you want to handle or log in application code.
 *
 * The example creates a dimmable light that registers privilege commands on the
 * On/Off cluster. When a command is registered as a privilege command, the stack
 * no longer handles it — the application is fully responsible for processing it.
 *
 * The example demonstrates both approaches:
 * - "Off with effect" (0x40): registered as privilege command, handled manually
 *   in the callback with custom behavior
 * - Standard On/Off/Toggle (0x00, 0x01, 0x02): left to the stack, which updates
 *   attributes and triggers onLightChange() as usual
 *
 * Use the Zigbee_On_Off_Switch example as the coordinator/sender, or any
 * Zigbee controller (e.g. Zigbee2MQTT, Philips Hue).
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee light configuration */
#define ZIGBEE_LIGHT_ENDPOINT 10
uint8_t led = RGB_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeDimmableLight zbLight = ZigbeeDimmableLight(ZIGBEE_LIGHT_ENDPOINT);

/********************* Zigbee callback functions **************************/
void setLED(bool value, uint8_t level) {
  digitalWrite(led, value);
  Serial.printf("Light state: %s, level: %d\r\n", value ? "ON" : "OFF", level);
}

// Privilege command callback — the stack will NOT handle registered commands,
// so we are fully responsible for processing them here.
void onPrivilegeCommand(const ezb_zcl_manuf_spec_cmd_message_t *message) {
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  Serial.println("--- Privilege command received ---");
  Serial.printf("  Cluster: 0x%04x\r\n", message->info.cluster_id);
  Serial.printf("  Command: 0x%02x\r\n", hdr ? hdr->cmd_id : 0);
  Serial.printf("  Src endpoint: %d, address: 0x%04x\r\n", hdr ? hdr->src_ep : 0, hdr ? hdr->src_addr.u.short_addr : 0);
  if (message->in.payload_size > 0 && message->in.payload) {
    Serial.printf("  Payload (%d bytes):", message->in.payload_size);
    for (uint16_t i = 0; i < message->in.payload_size; i++) {
      Serial.printf(" %02x", message->in.payload[i]);
    }
    Serial.println();
  }
  Serial.println("---------------------------------");

  // Handle "Off with effect" (0x40) manually — implement custom fade-out behavior here
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF && hdr && hdr->cmd_id == 0x40) {
    Serial.println("Handling 'Off with effect' — turning light off");
    zbLight.setLightState(false);
  }
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init LED and turn it OFF
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Initialize Zigbee stack as end device
  if (!Zigbee.role(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Optional: set Zigbee device name and model
  zbLight.setManufacturerAndModel("Espressif", "ZBPrivilegeCmd");

  // Standard On/Off/Toggle (0x00, 0x01, 0x02) are NOT registered as privilege commands,
  // so the stack handles them normally and notifies us via onLightChange().
  zbLight.onLightChange(setLED);

  // Register "Off with effect" (0x40) as a privilege command. The stack will NOT process
  // this command — our callback is fully responsible for handling it.
  zbLight.addPrivilegeCommand(EZB_ZCL_CLUSTER_ID_ON_OFF, 0x40);  // Off with effect

  zbLight.onPrivilegeCommand(onPrivilegeCommand);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbLight);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee
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
    // Toggle light by pressing the button
    zbLight.setLightState(!zbLight.getLightState());
  }
  delay(100);
}
