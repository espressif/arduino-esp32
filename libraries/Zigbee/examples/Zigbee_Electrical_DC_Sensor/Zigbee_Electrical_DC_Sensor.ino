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
 * @brief This example demonstrates a Zigbee DC electrical measurement sensor.
 *
 * The example shows how to use the Zigbee library to create an end device that measures
 * DC voltage, current and power using the Electrical Measurement cluster.
 *
 * The device reports:
 * - DC voltage in millivolts (0-5000mV)
 * - DC current in milliamps (0-1000mA)
 * - DC power in milliwatts (0-5000mW)
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

/* Zigbee DC measurement device configuration */
#define DC_ELECTRICAL_MEASUREMENT_ENDPOINT_NUMBER 1

uint8_t analogPin = A0;
uint8_t button = BOOT_PIN;

ZigbeeElectricalMeasurement zbElectricalMeasurement = ZigbeeElectricalMeasurement(DC_ELECTRICAL_MEASUREMENT_ENDPOINT_NUMBER);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Set analog resolution to 10 bits
  analogReadResolution(10);

  // Optional: set Zigbee device name and model
  zbElectricalMeasurement.setManufacturerAndModel("Espressif", "ZigbeeElectricalMeasurementDC");

  // Add analog clusters to Zigbee Analog according your needs
  zbElectricalMeasurement.addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE);
  zbElectricalMeasurement.addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT);
  zbElectricalMeasurement.addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_POWER);

  // // Optional: set Min/max values for the measurements
  zbElectricalMeasurement.setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 0, 5000);  // 0-5.000V
  zbElectricalMeasurement.setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, 0, 1000);  // 0-1.000A
  zbElectricalMeasurement.setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE_POWER, 0, 5000);    // 0-5.000W

  // // Optional: set Multiplier/Divisor for the measurements
  zbElectricalMeasurement.setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 1, 1000);  // 1/1000 = 0.001V (1 unit of measurement = 0.001V = 1mV)
  zbElectricalMeasurement.setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, 1, 1000);  // 1/1000 = 0.001A (1 unit of measurement = 0.001A = 1mA)
  zbElectricalMeasurement.setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE_POWER, 1, 1000);    // 1/1000 = 0.001W (1 unit of measurement = 0.001W = 1mW)

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbElectricalMeasurement);

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

  // Optional: Add reporting for DC measurements (this is overridden by HomeAssistant ZHA if connected to its network)
  zbElectricalMeasurement.setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 0, 30, 10);  // report every 30 seconds if value changes by 10 (0.1V)
  zbElectricalMeasurement.setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, 0, 30, 10);  // report every 30 seconds if value changes by 10 (0.1A)
  zbElectricalMeasurement.setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE_POWER, 0, 30, 10);    // report every 30 seconds if value changes by 10 (0.1W)
}

void loop() {
  static uint32_t timeCounter = 0;
  static uint8_t randomizer = 0;
  // Read ADC value and update the analog value every 2s
  if (!(timeCounter++ % 20)) {  // delaying for 100ms x 20 = 2s
    int16_t voltage_mv = (int16_t)(analogReadMilliVolts(analogPin));
    int16_t current_ma = randomizer;                    //0-255mA
    int16_t power_mw = voltage_mv * current_ma / 1000;  //calculate power in mW
    Serial.printf("Updating DC voltage to %d mV, current to %d mA, power to %d mW\r\n", voltage_mv, current_ma, power_mw);
    zbElectricalMeasurement.setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, voltage_mv);
    zbElectricalMeasurement.setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, current_ma);
    zbElectricalMeasurement.setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_POWER, power_mw);
    // Analog input supports reporting
    zbElectricalMeasurement.reportDC(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE);
    zbElectricalMeasurement.reportDC(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT);
    zbElectricalMeasurement.reportDC(ZIGBEE_DC_MEASUREMENT_TYPE_POWER);

    randomizer += 10;
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
