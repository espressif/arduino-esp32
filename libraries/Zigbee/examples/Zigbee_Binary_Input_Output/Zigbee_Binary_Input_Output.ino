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
 * @brief This example demonstrates Zigbee binary input/output device.
 *
 * The example demonstrates how to use Zigbee library to create an end device binary sensor/switch device.
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

/* Zigbee binary sensor device configuration */
#define BINARY_DEVICE_ENDPOINT_NUMBER 1

uint8_t button = BOOT_PIN;

ZigbeeBinary zbBinaryFan = ZigbeeBinary(BINARY_DEVICE_ENDPOINT_NUMBER);
ZigbeeBinary zbBinaryZone = ZigbeeBinary(BINARY_DEVICE_ENDPOINT_NUMBER + 1);
ZigbeeBinary zbBinaryHumidifier = ZigbeeBinary(BINARY_DEVICE_ENDPOINT_NUMBER + 2);

bool zoneStatus = false;

void fanSwitch(bool state) {
  Serial.println("Fan switch changed to: " + String(state));
  // Switch Fan status input signaling the fan status has changed
  zbBinaryFan.setBinaryInput(state);
  zbBinaryFan.reportBinaryInput();
}

void humidifierSwitch(bool state) {
  Serial.println("Humidifier switch changed to: " + String(state));
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Set analog resolution to 10 bits
  analogReadResolution(10);

  // Optional: set Zigbee device name and model
  zbBinaryFan.setManufacturerAndModel("Espressif", "ZigbeeBinarySensor");

  // Set up binary fan status input + switch output (HVAC)
  zbBinaryFan.addBinaryInput();
  zbBinaryFan.setBinaryInputApplication(BINARY_INPUT_APPLICATION_TYPE_HVAC_FAN_STATUS);
  zbBinaryFan.setBinaryInputDescription("Fan Status");

  zbBinaryFan.addBinaryOutput();
  zbBinaryFan.setBinaryOutputApplication(BINARY_OUTPUT_APPLICATION_TYPE_HVAC_FAN);
  zbBinaryFan.setBinaryOutputDescription("Fan Switch");

  zbBinaryFan.onBinaryOutputChange(fanSwitch);

  // Set up binary zone armed input (Security)
  zbBinaryZone.addBinaryInput();
  zbBinaryZone.setBinaryInputApplication(BINARY_INPUT_APPLICATION_TYPE_SECURITY_ZONE_ARMED);
  zbBinaryZone.setBinaryInputDescription("Zone Armed");

  // Set up binary humidifier output (HVAC)
  zbBinaryHumidifier.addBinaryOutput();
  zbBinaryHumidifier.setBinaryOutputApplication(BINARY_OUTPUT_APPLICATION_TYPE_HVAC_HUMIDIFIER);
  zbBinaryHumidifier.setBinaryOutputDescription("Humidifier Switch");

  zbBinaryHumidifier.onBinaryOutputChange(humidifierSwitch);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbBinaryFan);
  Zigbee.addEndpoint(&zbBinaryZone);
  Zigbee.addEndpoint(&zbBinaryHumidifier);

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
}

void loop() {
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

    // Toggle fan
    zbBinaryFan.setBinaryOutput(!zbBinaryFan.getBinaryOutput());
    zbBinaryFan.reportBinaryOutput();

    // Toggle zone
    zoneStatus = !zoneStatus;
    zbBinaryZone.setBinaryInput(zoneStatus);
    zbBinaryZone.reportBinaryInput();

    // Toggle humidifier
    zbBinaryHumidifier.setBinaryOutput(!zbBinaryHumidifier.getBinaryOutput());
    zbBinaryHumidifier.reportBinaryOutput();
  }
  delay(100);
}
