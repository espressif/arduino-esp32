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
 * @brief This example demonstrates Zigbee illuminance sensor.
 *
 * The example demonstrates how to use Zigbee library to create a end device illuminance sensor.
 * The illuminance sensor is a Zigbee end device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by MikaFromTheRoof (https://github.com/MikaFromTheRoof)
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define ZIGBEE_ILLUMINANCE_SENSOR_ENDPOINT 9
uint8_t button = BOOT_PIN;
uint8_t illuminance_sensor_pin = 6;  // Insert the analog pin to which the sensor (e.g. photoresistor) is connected

ZigbeeIlluminanceSensor zbIlluminanceSensor = ZigbeeIlluminanceSensor(ZIGBEE_ILLUMINANCE_SENSOR_ENDPOINT);

/********************* Illuminance sensor **************************/
static void illuminance_sensor_value_update(void *arg) {
  for (;;) {
    // read the raw analog value from the sensor
    int lsens_analog_raw = analogRead(illuminance_sensor_pin);
    Serial.printf("[Illuminance Sensor] raw analog value: %d\r\n", lsens_analog_raw);

    // conversion into zigbee raw illuminance value (typically between 0 in darkness and 50000 in direct sunlight)
    // depends on the value range of the raw analog sensor values and will need calibration for correct lux values
    // for demonstration purpose map the 12-bit ADC value (0-4095) to Zigbee illuminance range (0-50000)
    int lsens_illuminance_raw = map(lsens_analog_raw, 0, 4095, 0, 50000);
    Serial.printf("[Illuminance Sensor] raw illuminance value: %d\r\n", lsens_illuminance_raw);

    // according to zigbee documentation the formular 10^(lsens_illuminance_raw/10000)-1 can be used to calculate lux value from raw illuminance value
    // Note: Zigbee2MQTT seems to be using the formular 10^(lsens_illuminance_raw/10000) instead (without -1)
    int lsens_illuminance_lux = round(pow(10, (lsens_illuminance_raw / 10000.0)) - 1);
    Serial.printf("[Illuminance Sensor] lux value: %d lux\r\n", lsens_illuminance_lux);

    // Update illuminance in illuminance sensor EP
    zbIlluminanceSensor.setIlluminance(lsens_illuminance_raw);  // use raw illuminance here!

    delay(1000);  // reduce delay (in ms), if you want your device to react more quickly to changes in illuminance
  }
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Optional: configure analog input
  analogSetAttenuation(ADC_11db);  // set analog to digital converter (ADC) attenuation to 11 dB (up to ~3.3V input)
  analogReadResolution(12);        // set analog read resolution to 12 bits (value range from 0 to 4095), 12 is default

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Optional: Set Zigbee device name and model
  zbIlluminanceSensor.setManufacturerAndModel("Espressif", "ZigbeeIlluminanceSensor");

  // Optional: Set power source (choose between ZB_POWER_SOURCE_MAINS and ZB_POWER_SOURCE_BATTERY), defaults to unknown
  zbIlluminanceSensor.setPowerSource(ZB_POWER_SOURCE_MAINS);

  // Set minimum and maximum for raw illuminance value (0 min and 50000 max equals to 0 lux - 100,000 lux)
  zbIlluminanceSensor.setMinMaxValue(0, 50000);

  // Optional: Set tolerance for raw illuminance value
  zbIlluminanceSensor.setTolerance(1);

  // Add endpoint to Zigbee Core
  Serial.println("Adding Zigbee illuminance sensor endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbIlluminanceSensor);

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

  // Start illuminance sensor reading task
  xTaskCreate(illuminance_sensor_value_update, "illuminance_sensor_update", 2048, NULL, 10, NULL);

  // Set reporting schedule for illuminance value measurement in seconds, must be called after Zigbee.begin()
  // min_interval and max_interval in seconds, delta
  // if min = 1 and max = 0, delta = 1000, reporting is sent when raw illuminance value changes by 1000, but at most once per second
  // if min = 0 and max = 10, delta = 1000, reporting is sent every 10 seconds or if raw illuminance value changes by 1000
  // if min = 0, max = 10 and delta = 0, reporting is sent every 10 seconds regardless of illuminance change
  // Note: On pairing with Zigbee Home Automation or Zigbee2MQTT the reporting schedule will most likely be overwritten with their default settings
  zbIlluminanceSensor.setReporting(1, 0, 1000);
}

/********************* Main loop **************************/
void loop() {
  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3 secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    // force report of illuminance when button is pressed
    zbIlluminanceSensor.report();
  }
  delay(100);
}
