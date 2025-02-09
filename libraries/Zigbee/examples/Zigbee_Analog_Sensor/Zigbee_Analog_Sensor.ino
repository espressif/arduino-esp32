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
 * @brief This example demonstrates Zigbee analog sensor.
 *
 * The example demonstrates how to use Zigbee library to create a end device analog sensor.
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

/* Zigbee analog sensor configuration */
#define ANALOG_SENSOR_ENDPOINT_NUMBER 10

uint8_t button = BOOT_PIN;

ZigbeeAnalogSensor zbAnalogSensor = ZigbeeAnalogSensor(ANALOG_SENSOR_ENDPOINT_NUMBER);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  
  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Optional: set Zigbee device name and model
  zbAnalogSensor.setManufacturerAndModel("WekaBuilt", "ZigbeePowerSensor");

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbAnalogSensor);

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

  // Set reporting interval for measurement in seconds, must be called after Zigbee.begin()
  // min_interval and max_interval in seconds, delta 
  // if min = 1 and max = 0, reporting is sent only when temperature changes by delta
  // if min = 0 and max = 10, reporting is sent every 10 seconds or temperature changes by delta
  // if min = 0, max = 10 and delta = 0, reporting is sent every 10 seconds regardless of delta change
  zbAnalogSensor.setReporting(0, 2, 0);
  Serial.println("reporting set");

}

void loop() {
  static uint32_t timeCounter = 0;

  // Read flow and pressure sensors every 2s
  if (!(timeCounter++ % 20)) {  // delaying for 100ms x 20 = 2s
    float tsens_value = temperatureRead();
    Serial.printf("Updating voltage value to %.2f°C\r\n", tsens_value);
    zbAnalogSensor.setAnalog(tsens_value);
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
  zbAnalogSensor.report();

}
