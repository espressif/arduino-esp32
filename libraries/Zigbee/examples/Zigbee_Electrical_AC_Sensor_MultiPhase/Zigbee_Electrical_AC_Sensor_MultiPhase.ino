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
 * @brief This example demonstrates Zigbee electrical AC measurement device with multi-phase support.
 *
 * The example demonstrates how to use Zigbee library to create a router device that measures
 * AC electrical parameters like voltage, current, power, power factor and frequency across
 * three phases (A, B, C). This allows monitoring of three-phase power systems commonly used
 * in industrial and commercial applications.
 *
 * The device measures:
 * - Per phase: voltage, current, power, power factor
 * - Shared: frequency (common across all phases)
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

// Recommended to use Router mode, as this type of device is expected to be mains powered.
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator/router mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee AC measurement device configuration */
#define AC_ELECTRICAL_MEASUREMENT_ENDPOINT_NUMBER 1

uint8_t analogPin = A0;
uint8_t button = BOOT_PIN;

ZigbeeElectricalMeasurement zbElectricalMeasurement = ZigbeeElectricalMeasurement(AC_ELECTRICAL_MEASUREMENT_ENDPOINT_NUMBER);

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
  zbElectricalMeasurement.setManufacturerAndModel("Espressif", "ZigbeeElectricalMeasurementAC");

  // Add analog clusters to Zigbee Analog according your needs
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_A);
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_A);
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_A);

  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_B);
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_B);
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_B);

  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_C);
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_C);
  zbElectricalMeasurement.addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_C);

  zbElectricalMeasurement.addACMeasurement(
    ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY, ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC
  );  // frequency is not phase specific (shared)

  // Recommended: set Multiplier/Divisor for the measurements (common for all phases)
  zbElectricalMeasurement.setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, 1, 100);     // 1/100 = 0.01V (1 unit of measurement = 0.01V = 10mV)
  zbElectricalMeasurement.setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, 1, 1000);    // 1/1000 = 0.001A (1 unit of measurement = 0.001A = 1mA)
  zbElectricalMeasurement.setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, 1, 10);        // 1/10 = 0.1W (1 unit of measurement = 0.1W = 100mW)
  zbElectricalMeasurement.setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY, 1, 1000);  // 1/1000 = 0.001Hz (1 unit of measurement = 0.001Hz = 1mHz)

  // Optional: set Min/max values for the measurements
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_A, 0, 30000);  // 0-300.00V
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_A, 0, 10000);  // 0-10.000A
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_A, 0, 32000);    // 0-3200.0W

  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_B, 0, 30000);  // 0-300.00V
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_B, 0, 10000);  // 0-10.000A
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_B, 0, 32000);    // 0-3200.0W

  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_C, 0, 30000);  // 0-300.00V
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_C, 0, 10000);  // 0-10.000A
  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_C, 0, 32000);    // 0-3200.0W

  zbElectricalMeasurement.setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY, ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, 0, 65000);  // 0-65.000Hz

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbElectricalMeasurement);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in Router mode
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
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
}

void loop() {
  static uint32_t timeCounter = 0;
  static uint8_t randomizer = 0;
  // Read ADC value and update the analog value every 2s
  if (!(timeCounter++ % 20)) {                                  // delaying for 100ms x 20 = 2s
    uint16_t voltage = 23000 + randomizer;                      // 230.00 V
    uint16_t current = analogReadMilliVolts(analogPin);         // demonstrates approx 0-3.3A
    int16_t power = ((voltage / 100) * (current / 1000) * 10);  //calculate power in W
    uint16_t frequency = 50135;                                 // 50.000 Hz
    Serial.printf("Updating AC voltage to %d (0.01V), current to %d (mA), power to %d (0.1W), frequency to %d (mHz)\r\n", voltage, current, power, frequency);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_A, voltage);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_A, current);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_A, power);

    // Phase B demonstrates phase shift
    current += 500;
    voltage += 500;
    power = ((voltage / 100) * (current / 1000) * 10);  //calculate power in W

    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_B, voltage);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_B, current);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_B, power);

    // Phase C demonstrates phase shift
    current += 500;
    voltage += 500;
    power = ((voltage / 100) * (current / 1000) * 10);  //calculate power in W

    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_C, voltage);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_C, current);
    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_C, power);

    zbElectricalMeasurement.setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY, ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, frequency);

    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_A);
    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_A);
    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_A);

    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_B);
    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_B);
    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_B);

    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_PHASE_TYPE_C);
    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_PHASE_TYPE_C);
    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_PHASE_TYPE_C);

    zbElectricalMeasurement.reportAC(ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY, ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC);

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
