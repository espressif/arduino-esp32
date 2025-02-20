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
 * @brief This example demonstrates Zigbee analog input / output device.
 *
 * The example demonstrates how to use Zigbee library to create a end device analog device.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 * Modified by Pat Clay
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee analog device configuration */
#define ANALOG_DEVICE_ENDPOINT_NUMBER 1

uint8_t analogPin = A0;
uint8_t button = BOOT_PIN;

ZigbeeAnalog zbAnalogDevice = ZigbeeAnalog(ANALOG_DEVICE_ENDPOINT_NUMBER);

void onAnalogOutputChange(float analog_output) {
  Serial.printf("Received analog output change: %.1f\r\n", analog_output);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Set analog resolution to 10 bits
  analogReadResolution(10);

  // Optional: set Zigbee device name and model
  zbAnalogDevice.setManufacturerAndModel("Espressif", "ZigbeeAnalogDevice");

  // Add analog clusters to Zigbee Analog according your needs
  zbAnalogDevice.addAnalogInput();
  zbAnalogDevice.addAnalogOutput();

  // If analog output cluster is added, set callback function for analog output change
  zbAnalogDevice.onAnalogOutputChange(onAnalogOutputChange);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbAnalogDevice);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in End Device mode
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
  Serial.println("Connected");

  // Optional: Add reporting for analog input
  zbAnalogDevice.setAnalogInputReporting(0, 30, 10);  // report every 30 seconds if value changes by 10
}

void loop() {
  static uint32_t timeCounter = 0;

  // Read ADC value and update the analog value every 2s
  if (!(timeCounter++ % 20)) {  // delaying for 100ms x 20 = 2s
    float analog = (float)analogRead(analogPin);
    Serial.printf("Updating analog input to %.1f\r\n", analog);
    zbAnalogDevice.setAnalogInput(analog);

    // Analog input supports reporting
    zbAnalogDevice.reportAnalogInput();
  }

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
  }
  delay(100);
}
