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
 * @brief This example demonstrates Zigbee temperature sensor.
 *
 * The example demonstrates how to use Zigbee library to create a end device temperature sensor.
 * The temperature sensor is a Zigbee end device, which is controlled by a Zigbee coordinator.
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

/* Zigbee flow + pressure sensor configuration */
#define FLOW_SENSOR_ENDPOINT_NUMBER 10
#define PRESSURE_SENSOR_ENDPOINT_NUMBER 11

uint8_t button = BOOT_PIN;

ZigbeeFlowSensor zbFlowSensor = ZigbeeFlowSensor(FLOW_SENSOR_ENDPOINT_NUMBER);
ZigbeePressureSensor zbPressureSensor = ZigbeePressureSensor(PRESSURE_SENSOR_ENDPOINT_NUMBER);

/************************ Temp sensor *****************************/
static void sensors_reading(void *arg) {
  for (;;) {
    // Read Pressure and Flow sensors value - here is chip temperature used as a dummy value for demonstration
    float flow_value = temperatureRead();
    uint16_t pressure_value = (uint16_t)temperatureRead()*100; //*100 for demonstration so the value is in 1-3hPa
    Serial.printf("Updating flow sensor value to %.2f\r\n", flow_value);
    zbFlowSensor.setFlow(flow_value);
    Serial.printf("Updating pressure sensor value to %d\r\n", pressure_value);
    zbPressureSensor.setPressure(pressure_value);
    delay(1000);
  }
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Optional: set Zigbee device name and model
  zbFlowSensor.setManufacturerAndModel("Espressif", "ZigbeeFlowSensor");

  // Set minimum and maximum temperature measurement value (10-50°C is default range for chip temperature measurement)
  zbFlowSensor.setMinMaxValue(0, 100);

  // Set tolerance for temperature measurement in °C (lowest possible value is 0.01°C)
  zbFlowSensor.setTolerance(1);

  // Optional: set Zigbee device name and model
  zbPressureSensor.setManufacturerAndModel("Espressif", "ZigbeePressureSensor");

  // Set minimum and maximum temperature measurement value (10-50°C is default range for chip temperature measurement)
  zbPressureSensor.setMinMaxValue(0, 30);

  // Set tolerance for temperature measurement in °C (lowest possible value is 0.01°C)
  zbPressureSensor.setTolerance(1);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbFlowSensor);
  Zigbee.addEndpoint(&zbPressureSensor);

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

  // Start Flow and Pressure sensor reading task
  xTaskCreate(sensors_reading, "flow_pressure_sensors_read", 2048, NULL, 10, NULL);

  // Set reporting interval for flow and pressure measurement in seconds, must be called after Zigbee.begin()
  // min_interval and max_interval in seconds, delta (pressure change in Pa, flow change in 0,1 m3/h)
  // if min = 1 and max = 0, reporting is sent only when temperature changes by delta
  // if min = 0 and max = 10, reporting is sent every 10 seconds or temperature changes by delta
  // if min = 0, max = 10 and delta = 0, reporting is sent every 10 seconds regardless of delta change
  zbFlowSensor.setReporting(0, 30, 1);
  zbPressureSensor.setReporting(0, 30, 1);
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
    zbFlowSensor.report();
    zbPressureSensor.report();
  }
  delay(100);
}
