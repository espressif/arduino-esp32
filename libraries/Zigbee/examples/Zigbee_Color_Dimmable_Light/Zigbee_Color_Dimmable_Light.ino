// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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
 * @brief This example demonstrates Zigbee Color Dimmable light bulb.
 *
 * The example demonstrates how to use Zigbee library to create an end device with
 * color dimmable light end point.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator.
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

#include "ZigbeeCore.h"
#include "ep/ZigbeeColorDimmableLight.h"

#define LED_PIN               RGB_BUILTIN
#define BUTTON_PIN            9  // C6/H2 Boot button
#define ZIGBEE_LIGHT_ENDPOINT 10

ZigbeeColorDimmableLight zbColorLight = ZigbeeColorDimmableLight(ZIGBEE_LIGHT_ENDPOINT);

/********************* RGB LED functions **************************/
void setRGBLight(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
  float brightness = (float)level / 255;
  rgbLedWrite(LED_PIN, red * brightness, green * brightness, blue * brightness);
}

// Create a task on identify call to handle the identify function
void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %d seconds", time);
  if (time == 0) {
    // If identify time is 0, stop blinking and restore light as it was used for identify
    zbColorLight.restoreLight();
    return;
  }
  rgbLedWrite(LED_PIN, 255 * blink, 255 * blink, 255 * blink);
  blink = !blink;
}

/********************* Arduino functions **************************/
void setup() {
  // Init RMT and leave light OFF
  rgbLedWrite(LED_PIN, 0, 0, 0);

  // Init button for factory reset
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Set callback function for light change
  zbColorLight.onLightChange(setRGBLight);

  // Optional: Set callback function for device identify
  zbColorLight.onIdentify(identify);

  // Optional: Set Zigbee device name and model
  zbColorLight.setManufacturerAndModel("Espressif", "ZBColorLightBulb");

  // Add endpoint to Zigbee Core
  log_d("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbColorLight);

  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  log_d("Calling Zigbee.begin()");
  Zigbee.begin();
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
        Serial.printf("Resetting Zigbee to factory settings, reboot.\n");
        Zigbee.factoryReset();
      }
    }
  }
  delay(100);
}
