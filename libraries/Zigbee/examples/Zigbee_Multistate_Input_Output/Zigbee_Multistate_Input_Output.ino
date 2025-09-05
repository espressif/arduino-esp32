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
 * @brief This example demonstrates Zigbee multistate input / output device.
 *
 * The example demonstrates how to use Zigbee library to create a router multistate device.
 * In the example, we have two multistate devices:
 * - zbMultistateDevice: uses defined application states from Zigbee specification
 * - zbMultistateDeviceCustom: uses custom application states (user defined)
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * NOTE: HomeAssistant ZHA does not support multistate input and output clusters yet.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator/router device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee multistate device configuration */
#define MULTISTATE_DEVICE_ENDPOINT_NUMBER 1

uint8_t button = BOOT_PIN;

// zbMultistateDevice will use defined application states
ZigbeeMultistate zbMultistateDevice = ZigbeeMultistate(MULTISTATE_DEVICE_ENDPOINT_NUMBER);

// zbMultistateDeviceCustom will use custom application states (user defined)
ZigbeeMultistate zbMultistateDeviceCustom = ZigbeeMultistate(MULTISTATE_DEVICE_ENDPOINT_NUMBER + 1);

const char *multistate_custom_state_names[6] = {"Off", "On", "UltraSlow", "Slow", "Fast", "SuperFast"};

void onStateChange(uint16_t state) {
  // print the state
  Serial.printf("Received state change: %d\r\n", state);
  // print the state name using the stored state names
  const char *const *state_names = ZB_MULTISTATE_APPLICATION_TYPE_7_STATE_NAMES;
  if (state_names && state < zbMultistateDevice.getMultistateOutputStateNamesLength()) {
    Serial.printf("State name: %s\r\n", state_names[state]);
  }
  // print state index of possible options
  Serial.printf("State index: %d / %d\r\n", state, zbMultistateDevice.getMultistateOutputStateNamesLength() - 1);
}

void onStateChangeCustom(uint16_t state) {
  // print the state
  Serial.printf("Received state change: %d\r\n", state);
  // print the state name using the stored state names
  if (state < zbMultistateDeviceCustom.getMultistateOutputStateNamesLength()) {
    Serial.printf("State name: %s\r\n", multistate_custom_state_names[state]);
  }
  // print state index of possible options
  Serial.printf("State index: %d / %d\r\n", state, zbMultistateDeviceCustom.getMultistateOutputStateNamesLength() - 1);

  Serial.print("Changing to fan mode to: ");
  switch (state) {
    case 0:  Serial.println("Off"); break;
    case 1:  Serial.println("On"); break;
    case 2:  Serial.println("UltraSlow"); break;
    case 3:  Serial.println("Slow"); break;
    case 4:  Serial.println("Fast"); break;
    case 5:  Serial.println("SuperFast"); break;
    default: Serial.println("Invalid state"); break;
  }
}

void setup() {
  log_d("Starting serial");
  Serial.begin(115200);

  // Init button switch
  log_d("Init button switch");
  pinMode(button, INPUT_PULLUP);

  // Optional: set Zigbee device name and model
  log_d("Set Zigbee device name and model");
  zbMultistateDevice.setManufacturerAndModel("Espressif", "ZigbeeMultistateDevice");

  // Set up analog input
  log_d("Add Multistate Input");
  zbMultistateDevice.addMultistateInput();
  log_d("Set Multistate Input Application");
  zbMultistateDevice.setMultistateInputApplication(ZB_MULTISTATE_APPLICATION_TYPE_0_INDEX);
  log_d("Set Multistate Input Description");
  zbMultistateDevice.setMultistateInputDescription("Fan (on/off/auto)");
  zbMultistateDevice.setMultistateInputStates(ZB_MULTISTATE_APPLICATION_TYPE_0_NUM_STATES);

  // Set up analog output
  log_d("Add Multistate Output");
  zbMultistateDevice.addMultistateOutput();
  log_d("Set Multistate Output Application");
  zbMultistateDevice.setMultistateOutputApplication(ZB_MULTISTATE_APPLICATION_TYPE_7_INDEX);
  log_d("Set Multistate Output Description");
  zbMultistateDevice.setMultistateOutputDescription("Light (high/normal/low)");
  zbMultistateDevice.setMultistateOutputStates(ZB_MULTISTATE_APPLICATION_TYPE_7_NUM_STATES);

  // Set up custom output
  log_d("Add Multistate Output");
  zbMultistateDeviceCustom.addMultistateOutput();
  log_d("Set Multistate Output Application");
  zbMultistateDeviceCustom.setMultistateOutputApplication(ZB_MULTISTATE_APPLICATION_TYPE_OTHER_INDEX);
  log_d("Set Multistate Output Description");
  zbMultistateDeviceCustom.setMultistateOutputDescription("Fan (on/off/slow/medium/fast)");
  zbMultistateDeviceCustom.setMultistateOutputStates(5);

  // Set callback function for multistate output change
  log_d("Set callback function for multistate output change");
  zbMultistateDevice.onMultistateOutputChange(onStateChange);
  zbMultistateDeviceCustom.onMultistateOutputChange(onStateChangeCustom);

  // Add endpoints to Zigbee Core
  log_d("Add endpoints to Zigbee Core");
  Zigbee.addEndpoint(&zbMultistateDevice);
  Zigbee.addEndpoint(&zbMultistateDeviceCustom);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in Router Device mode
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
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
  Serial.println("Connected");
}

void loop() {
  // Checking button for factory reset and reporting
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
    // For demonstration purposes, increment the multistate output/input value by 1
    if (zbMultistateDevice.getMultistateOutput() < zbMultistateDevice.getMultistateOutputStateNamesLength() - 1) {
      zbMultistateDevice.setMultistateOutput(zbMultistateDevice.getMultistateOutput() + 1);
      zbMultistateDevice.reportMultistateOutput();
      zbMultistateDevice.setMultistateInput(zbMultistateDevice.getMultistateOutput());
      zbMultistateDevice.reportMultistateInput();
    } else {
      zbMultistateDevice.setMultistateOutput(0);
      zbMultistateDevice.reportMultistateOutput();
      zbMultistateDevice.setMultistateInput(zbMultistateDevice.getMultistateOutput());
      zbMultistateDevice.reportMultistateInput();
    }

    if (zbMultistateDeviceCustom.getMultistateOutput() < zbMultistateDeviceCustom.getMultistateOutputStateNamesLength() - 1) {
      zbMultistateDeviceCustom.setMultistateOutput(zbMultistateDeviceCustom.getMultistateOutput() + 1);
      zbMultistateDeviceCustom.reportMultistateOutput();
    } else {
      zbMultistateDeviceCustom.setMultistateOutput(0);
      zbMultistateDeviceCustom.reportMultistateOutput();
    }
  }
  delay(100);
}
