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
 * @brief This example demonstrates Zigbee temperature sensor Sleepy device.
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
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeTempSensor.h"
#include <ctime>

#define BUTTON_PIN                  9  //Boot button for C6/H2
#define TEMP_SENSOR_ENDPOINT_NUMBER 10

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30         /* Time ESP32 will go to sleep (in seconds) */

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

/************************ Temp sensor *****************************/
void meausureAndSleep(){
  // Measure temperature sensor value
  float tsens_value = temperatureRead();

  // Initialize random seed
  std::srand(static_cast<unsigned int>(std::time(0)));

  // Calculate the 1% fluctuation range
  float fluctuation = tsens_value * 0.01f;

  // Generate a random number between -1% and +1%
  float randomFactor = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f;

  // Apply the fluctuation to the original temperature
  tsens_value = tsens_value + randomFactor * fluctuation;

  // Update temperature value in Temperature sensor EP
  zbTempSensor.setTemperature(tsens_value);

  // Report temperature value
  zbTempSensor.reportTemperature();

  // Set battery percentage to 80% and report it
  zbTempSensor.setBatteryPercentage(80);
  zbTempSensor.reportBatteryPercentage();

  // Put device to deep sleep
  esp_deep_sleep_start();
}

/********************* Arduino functions **************************/
void setup() {
  // Init button switch
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Optional: set Zigbee device name and model
  zbTempSensor.setManufacturerAndModel("Espressif", "SleepyZigbeeTempSensor");

  // Set minimum and maximum temperature measurement value (10-50°C is default range for chip temperature measurement)
  zbTempSensor.setMinMaxValue(10, 50);

  // Set tolerance for temperature measurement in °C (lowest possible value is 0.01°C)
  zbTempSensor.setTolerance(1);

  // Set power source to battery and battery percentage to 100%
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100);

  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbTempSensor);

  // Configure the wake up source and set to wake up every 5 seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Create a custom Zigbee configuration for End Device
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;

  // When all EPs are registered, start Zigbee in End Device mode
  Zigbee.begin(&zigbeeConfig, false);

  // Wait for Zigbee to start
  while(!Zigbee.isStarted()){
    delay(100);
  }

  // Delay 5s to allow establishing connection with coordinator, needed for sleepy devices
  delay(5000);
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
        Zigbee.factoryReset();
      }
    }
  }

  // Call the function to measure temperature and put the device to sleep
  meausureAndSleep();
}
