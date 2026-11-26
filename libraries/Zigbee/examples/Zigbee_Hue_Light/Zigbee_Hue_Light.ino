// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief On/off light that can join a Philips Hue Bridge.
 *
 * The bridge requires distributed security, its ZLL Trust Center link key, and
 * endpoint device version 1. Replace the placeholder key before pairing.
 * Off with effect turns the light off. This on/off light does not fade.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee router mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define ZIGBEE_LIGHT_ENDPOINT 10
#ifdef RGB_BUILTIN
uint8_t led = RGB_BUILTIN;
#else
uint8_t led = 2;
#endif
uint8_t button = BOOT_PIN;

// Replace these bytes with a Philips Hue Trust Center link key.
// See README.md. This placeholder will not join a Hue Bridge.
static const uint8_t hue_trust_center_key[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

ZigbeeLight zbLight = ZigbeeLight(ZIGBEE_LIGHT_ENDPOINT);

void setLED(bool value) {
  digitalWrite(led, value);
}

void setup() {
  Serial.begin(115200);

  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
  pinMode(button, INPUT_PULLUP);

  if (!Zigbee.role(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  Zigbee.setDistributedSecurity(true);
  if (!Zigbee.setGlobalLinkKey(hue_trust_center_key)) {
    Serial.println("Failed to set the Trust Center link key");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Hue caches version 0 and then refuses the device. Version 1 is required.
  zbLight.setVersion(1);
  zbLight.setManufacturerAndModel("Espressif", "ZBHueLight");
  zbLight.onLightChange(setLED);

  Zigbee.addEndpoint(&zbLight);

  Serial.println("Starting Zigbee...");
  Serial.println("Replace hue_trust_center_key with the Hue ZLL key before pairing.");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Zigbee started successfully!");
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop() {
  if (digitalRead(button) == LOW) {
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    zbLight.setLight(!zbLight.getLightState());
  }
  delay(100);
}
