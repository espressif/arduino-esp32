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
 * @brief This example demonstrates simple Zigbee thermostat.
 *
 * The example demonstrates how to use Zigbee library to get data from temperature
 * sensor end device and act as an thermostat.
 * The temperature sensor is a Zigbee end device, which is controlled by a Zigbee coordinator (thermostat).
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

#include "ZigbeeCore.h"
#include "ep/ZigbeeThermostat.h"

#define BUTTON_PIN                 9  // Boot button for C6/H2
#define THERMOSTAT_ENDPOINT_NUMBER 5

ZigbeeThermostat zbThermostat = ZigbeeThermostat(THERMOSTAT_ENDPOINT_NUMBER);

// Save temperature sensor data
float sensor_temp;
float sensor_max_temp;
float sensor_min_temp;
float sensor_tolerance;

/****************** Temperature sensor handling *******************/
void recieveSensorTemp(float temperature) {
  Serial.printf("Temperature sensor value: %.2f°C\n", temperature);
  sensor_temp = temperature;
}

void recieveSensorConfig(float min_temp, float max_temp, float tolerance) {
  Serial.printf("Temperature sensor settings: min %.2f°C, max %.2f°C, tolerance %.2f°C\n", min_temp, max_temp, tolerance);
  sensor_min_temp = min_temp;
  sensor_max_temp = max_temp;
  sensor_tolerance = tolerance;
}
/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Init button switch
  pinMode(BUTTON_PIN, INPUT);

  // Set callback functions for temperature and configuration receive
  zbThermostat.onTempRecieve(recieveSensorTemp);
  zbThermostat.onConfigRecieve(recieveSensorConfig);

  //Optional: set Zigbee device name and model
  zbThermostat.setManufacturerAndModel("Espressif", "ZigbeeThermostat");

  //Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbThermostat);

  //Open network for 180 seconds after boot
  Zigbee.setRebootOpenNetwork(180);

  // When all EPs are registered, start Zigbee with ZIGBEE_COORDINATOR mode
  Zigbee.begin(ZIGBEE_COORDINATOR);

  Serial.println("Waiting for Temperature sensor to bound to the switch");

  //Wait for switch to bound to a light:
  while (!zbThermostat.isBound()) {
    Serial.printf(".");
    delay(500);
  }

  // Get temperature sensor configuration
  zbThermostat.getSensorSettings();
  Serial.println();
}

void loop() {
  // Handle button switch in loop()
  if (digitalRead(BUTTON_PIN) == LOW) {  // Push button pressed

    // Key debounce handling
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
    }

    // Set reporting interval for temperature sensor
    zbThermostat.setTemperatureReporting(0, 10, 2);
  }

  // Print temperature sensor data each 10 seconds
  static uint32_t last_print = 0;
  if (millis() - last_print > 10000) {
    last_print = millis();
    int temp_percent = (int)((sensor_temp - sensor_min_temp) / (sensor_max_temp - sensor_min_temp) * 100);
    Serial.printf("Loop temperature info: %.2f°C (%d %%)\n", sensor_temp, temp_percent);
  }
}
