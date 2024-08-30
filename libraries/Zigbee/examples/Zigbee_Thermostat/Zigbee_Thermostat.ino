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

#include "Zigbee_core.h"
#include "ep/ep_thermostat.h"

#define BUTTON_PIN 9 // Boot button for C6/H2
#define THERMOSTAT_ENDPOINT_NUMBER 5

class MyZigbeeThermostat : public ZigbeeThermostat {
  public:
    // Constructor that passes parameters to the base class constructor
    MyZigbeeThermostat(uint8_t endpoint) : ZigbeeThermostat(endpoint) {}

    // Override virtual functions from ZigbeeThermostat to handle temperature sensor data
    void temperatureRead(float temp) override {
      if (temperature != temp) {
        Serial.printf("Temperature sensor value changed to: %.2f°C\n", temp);
        temperature = temp;
      }
    }

    void temperatureMin(float temp) override {
      Serial.printf("Temperature sensor min value: %.2f°C\n", temp);
      min_temperature = temp;
    }

    void temperatureMax(float temp) override {
      Serial.printf("Temperature sensor max value: %.2f°C\n", temp);
      max_temperature = temp;
    }

    void temperatureTolerance(float tolerance) override {
      Serial.printf("Temperature sensor tolerance: %.2f°C\n", tolerance);
      temp_tolerance = tolerance;
    }

    float temperature;
    float max_temperature;
    float min_temperature;
    float temp_tolerance;
};

MyZigbeeThermostat zbThermostat = MyZigbeeThermostat(THERMOSTAT_ENDPOINT_NUMBER);

/********************* Arduino functions **************************/
void setup() {
  
  Serial.begin(115200);

  // Init button switch
  pinMode(BUTTON_PIN, INPUT);

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
  while(!zbThermostat.is_bound()) 
  {
    Serial.printf(".");
    delay(500);
  }

  // Get temperature sensor value to update min and max values
  zbThermostat.getTemperature();

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
    int temp_percent = (int)((zbThermostat.temperature - zbThermostat.min_temperature) / (zbThermostat.max_temperature - zbThermostat.min_temperature) * 100);
    Serial.printf("Temperature: %.2f°C (%d '%')\n", zbThermostat.temperature, temp_percent);
  }
}
