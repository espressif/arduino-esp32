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
 * @brief This example demonstrates Zigbee windspeed sensor.
 *
 * The example demonstrates how to use Zigbee library to create a end device wind speed sensor.
 * The wind speed sensor is a Zigbee end device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeWindSpeedSensor.h"

#define BUTTON_PIN                  9  //Boot button for C6/H2
#define WIND_SPEED_SENSOR_ENDPOINT_NUMBER 10

ZigbeeWindSpeedSensor zbWindSpeedSensor = ZigbeeWindSpeedSensor(WIND_SPEED_SENSOR_ENDPOINT_NUMBER);

/************************ Temp sensor *****************************/
static void windspeed_sensor_value_update(void *arg) {
  for (;;) {
    // Read wind speed sensor value
    float tsens_value = windspeedRead();
    log_v("Temperature sensor value: %.2f°C", tsens_value);
    // Update windspeed value in Windspeed sensor EP
    zbWindSpeedSensor.setWindspeed(tsens_value);
    delay(1000);
  }
}

/********************* Arduino functions **************************/
void setup() {

  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Init button switch
  pinMode(BUTTON_PIN, INPUT);

  // Optional: set Zigbee device name and model
  zbWindSpeedSensor.setManufacturerAndModel("Espressif", "ZigbeeWindSpeedSensor");

  // Set minimum and maximum windspeed measurement value (10-50°C is default range for chip windspeed measurement)
  zbWindSpeedSensor.setMinMaxValue(10, 50);

  // Set tolerance for windspeed measurement in °C (lowest possible value is 0.01°C)
  zbWindSpeedSensor.setTolerance(1);

  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbWindSpeedSensor);

  // When all EPs are registered, start Zigbee in End Device mode
  Zigbee.begin();

  // Start Wind speed sensor reading task
  xTaskCreate(windspeed_sensor_value_update, "wind_speed_sensor_update", 2048, NULL, 10, NULL);

  // Set reporting interval for windspeed measurement in seconds, must be called after Zigbee.begin()
  // min_interval and max_interval in seconds, delta (temp change in °C)
  // if min = 1 and max = 0, reporting is sent only when windspeed changes by delta
  // if min = 0 and max = 10, reporting is sent every 10 seconds or windspeed changes by delta
  // if min = 0, max = 10 and delta = 0, reporting is sent every 10 seconds regardless of windspeed change
  zbWindSpeedSensor.setReporting(1, 0, 1);
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(BUTTON_PIN) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.printf("Resetting Zigbee to factory settings, reboot.\n");
        Zigbee.factoryReset();
      }
    }
    zbWindSpeedSensor.reportWindspeed();
  }
  delay(100);
}
