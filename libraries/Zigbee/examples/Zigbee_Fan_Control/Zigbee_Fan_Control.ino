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
 * @brief This example demonstrates simple Zigbee fan control.
 *
 * The example demonstrates how to use Zigbee library to create a router device fan control.
 * The fan control is a Zigbee router device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee light bulb configuration */
#define ZIGBEE_FAN_CONTROL_ENDPOINT 1

#ifdef RGB_BUILTIN
uint8_t led = RGB_BUILTIN;  // To demonstrate the current fan control mode
#else
uint8_t led = 2;
#endif

uint8_t button = BOOT_PIN;

ZigbeeFanControl zbFanControl = ZigbeeFanControl(ZIGBEE_FAN_CONTROL_ENDPOINT);

/********************* fan control callback function **************************/
void setFan(ZigbeeFanMode mode) {
  switch (mode) {
    case FAN_MODE_OFF:
      rgbLedWrite(led, 0, 0, 0);  // Off
      Serial.println("Fan mode: OFF");
      break;
    case FAN_MODE_LOW:
      rgbLedWrite(led, 0, 0, 255);  // Blue
      Serial.println("Fan mode: LOW");
      break;
    case FAN_MODE_MEDIUM:
      rgbLedWrite(led, 255, 255, 0);  // Yellow
      Serial.println("Fan mode: MEDIUM");
      break;
    case FAN_MODE_HIGH:
      rgbLedWrite(led, 255, 0, 0);  // Red
      Serial.println("Fan mode: HIGH");
      break;
    case FAN_MODE_ON:
      rgbLedWrite(led, 255, 255, 255);  // White
      Serial.println("Fan mode: ON");
      break;
    default: log_e("Unhandled fan mode: %d", mode); break;
  }
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init LED that will be used to indicate the current fan control mode
  rgbLedWrite(led, 0, 0, 0);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  //Optional: set Zigbee device name and model
  zbFanControl.setManufacturerAndModel("Espressif", "ZBFanControl");

  // Set the fan mode sequence to LOW_MED_HIGH
  zbFanControl.setFanModeSequence(FAN_MODE_SEQUENCE_LOW_MED_HIGH);

  // Set callback function for fan mode change
  zbFanControl.onFanModeChange(setFan);

  //Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeFanControl endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbFanControl);

  // When all EPs are registered, start Zigbee in ROUTER mode
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
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
  delay(100);
}
