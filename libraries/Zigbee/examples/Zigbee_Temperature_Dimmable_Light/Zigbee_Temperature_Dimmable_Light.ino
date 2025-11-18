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
 * @brief This example demonstrates Zigbee Temperature Dimmable light bulb.
 *
 * The example demonstrates how to use Zigbee library to create an end device with
 * temperature dimmable light endpoint.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Hannes Beckmann
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define ZIGBEE_TEMP_LIGHT_ENDPOINT 1
uint8_t led = RGB_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeTemperatureDimmableLight zbTempLight = ZigbeeTemperatureDimmableLight(ZIGBEE_TEMP_LIGHT_ENDPOINT);

/********************* LED functions **************************/
uint16_t kelvinToMireds(uint16_t kelvin) {
  return 1000000 / kelvin;
}

uint16_t miredsToKelvin(uint16_t mireds) {
  return 1000000 / mireds;
}

void setTempLight(bool state, uint8_t level, uint16_t mireds) {
  if (!state) {
    rgbLedWrite(led, 0, 0, 0);
    return;
  }
  float brightness = (float)level / 255;
  // Convert mireds to color temperature (K) and map to white/yellow
  uint16_t kelvin = miredsToKelvin(mireds);
  uint8_t warm = constrain(map(kelvin, 2000, 6500, 255, 0), 0, 255);
  uint8_t cold = constrain(map(kelvin, 2000, 6500, 0, 255), 0, 255);
  rgbLedWrite(led, warm * brightness, warm * brightness, cold * brightness);
}

void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %d seconds", time);
  if (time == 0) {
    zbTempLight.restoreLight();
    return;
  }
  rgbLedWrite(led, 255 * blink, 255 * blink, 255 * blink);
  blink = !blink;
}

void setup() {
  Serial.begin(115200);
  rgbLedWrite(led, 0, 0, 0);
  pinMode(button, INPUT_PULLUP);
  zbTempLight.onLightChange(setTempLight);
  zbTempLight.onIdentify(identify);
  zbTempLight.setManufacturerAndModel("Espressif", "ZBTempLightBulb");
  // High Kelvin -> Low Mireds: Min and Max is switched
  zbTempLight.setMinMaxTemperature(kelvinToMireds(6500), kelvinToMireds(2000));
  Serial.println("Adding ZigbeeTemperatureDimmableLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbTempLight);
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
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
    zbTempLight.setLightLevel(zbTempLight.getLightLevel() + 50);
  }
  delay(100);
}
