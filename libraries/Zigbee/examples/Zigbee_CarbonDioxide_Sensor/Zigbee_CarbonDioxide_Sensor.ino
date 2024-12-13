// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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
 * @brief This example demonstrates Zigbee carbon dioxide sensor.
 *
 * The example demonstrates how to use Zigbee library to create a end device carbon dioxide sensor.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee carbon dioxide sensor configuration */
#define CARBON_DIOXIDE_SENSOR_ENDPOINT_NUMBER 10
uint8_t button = BOOT_PIN;

ZigbeeCarbonDioxideSensor zbCarbonDioxideSensor = ZigbeeCarbonDioxideSensor(CARBON_DIOXIDE_SENSOR_ENDPOINT_NUMBER);

void setup() {
  Serial.begin(115200);

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Optional: set Zigbee device name and model
  zbCarbonDioxideSensor.setManufacturerAndModel("Espressif", "ZigbeeCarbonDioxideSensor");

  // Set minimum and maximum carbon dioxide measurement value in ppm
  zbCarbonDioxideSensor.setMinMaxValue(0, 1500);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbCarbonDioxideSensor);

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
  Serial.println();

  // Set reporting interval for carbon dioxide measurement to be done every 30 seconds, must be called after Zigbee.begin()
  // min_interval and max_interval in seconds, delta (carbon dioxide change in ppm)
  // if min = 1 and max = 0, reporting is sent only when carbon dioxide changes by delta
  // if min = 0 and max = 10, reporting is sent every 10 seconds or when carbon dioxide changes by delta
  // if min = 0, max = 10 and delta = 0, reporting is sent every 10 seconds regardless of delta change
  zbCarbonDioxideSensor.setReporting(0, 30, 0);
}

void loop() {
  static uint32_t timeCounter = 0;
  // Read carbon dioxide sensor every 2s
  if (!(timeCounter++ % 20)) {  // delaying for 100ms x 20 = 2s
    // Read sensor value - here is chip temperature used + 300 as a dummy value for demonstration
    uint16_t carbon_dioxide_value = 300 + (uint16_t)temperatureRead();
    Serial.printf("Updating carbon dioxide sensor value to %d ppm\r\n", carbon_dioxide_value);
    zbCarbonDioxideSensor.setCarbonDioxide(carbon_dioxide_value);
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
    zbCarbonDioxideSensor.report();
  }
  delay(100);
}
