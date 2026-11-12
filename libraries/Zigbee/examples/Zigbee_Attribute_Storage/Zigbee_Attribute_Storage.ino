// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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
 * @brief This example demonstrates Zigbee persistent attribute storage.
 *
 * Analog Input PresentValue is marked persistent and stored in the Zigbee NVS
 * dataset. Reboot the board and the last written value is restored.
 *
 * Short press BOOT: increment and store PresentValue.
 * Long press BOOT (3s): factory reset (clears Zigbee NVS, including persisted attrs).
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode (coordinator/router)
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator/router device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define ANALOG_DEVICE_ENDPOINT 10
uint8_t button = BOOT_PIN;

ZigbeeAnalog zbAnalog = ZigbeeAnalog(ANALOG_DEVICE_ENDPOINT);

static void printPersistedValue() {
  float value = 0;
  if (zbAnalog.getAnalogInput(value)) {
    Serial.printf("Persistent PresentValue: %.1f\r\n", value);
  } else {
    Serial.println("Failed to read PresentValue");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(button, INPUT_PULLUP);

  if (!Zigbee.role(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to init!");
    delay(1000);
    ESP.restart();
  }

  zbAnalog.setManufacturerAndModel("Espressif", "ZBAttrStorage");
  zbAnalog.addAnalogInput();
  zbAnalog.setAnalogInputApplication(EZB_ZCL_AI_COUNT_UNITLESS_OTHER);

  if (!zbAnalog.setAttributePersistent(EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID)) {
    Serial.println("Failed to mark PresentValue persistent");
  }

  Zigbee.addEndpoint(&zbAnalog);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    ESP.restart();
  }
  Serial.println("Zigbee started");

  printPersistedValue();
  Serial.println("Short press BOOT to increment PresentValue, long press to factory reset");
}

void loop() {
  if (digitalRead(button) == LOW) {
    delay(100);
    unsigned long start = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - start) > 3000) {
        Serial.println("Factory resetting (clears persisted attributes)");
        delay(200);
        Zigbee.factoryReset();
      }
    }
    float value = 0;
    if (!zbAnalog.getAnalogInput(value)) {
      value = 0;
    }
    value += 1.0f;
    if (zbAnalog.setAnalogInput(value)) {
      Serial.printf("Stored PresentValue: %.1f (reboot to verify persist)\r\n", value);
    } else {
      Serial.println("Failed to set PresentValue");
    }
  }
  delay(100);
}
