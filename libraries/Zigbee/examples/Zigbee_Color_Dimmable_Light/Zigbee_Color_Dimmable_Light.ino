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
 * @brief This example demonstrates Zigbee Color Dimmable light bulb with RGB and Temperature support.
 *
 * The example demonstrates how to use Zigbee library to create an end device with
 * color dimmable light end point supporting both RGB (X/Y) and Color Temperature modes.
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

#include "Zigbee.h"

/* Zigbee color dimmable light configuration */
#define ZIGBEE_RGB_LIGHT_ENDPOINT 10
uint8_t led = RGB_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeColorDimmableLight zbColorLight = ZigbeeColorDimmableLight(ZIGBEE_RGB_LIGHT_ENDPOINT);

/********************* Temperature conversion functions **************************/
uint16_t kelvinToMireds(uint16_t kelvin) {
  return 1000000 / kelvin;
}

uint16_t miredsToKelvin(uint16_t mireds) {
  return 1000000 / mireds;
}

/********************* RGB LED functions **************************/
void setRGBLight(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
  if (!state) {
    rgbLedWrite(led, 0, 0, 0);
    return;
  }
  float brightness = (float)level / 255;
  rgbLedWrite(led, red * brightness, green * brightness, blue * brightness);
}

/********************* Temperature LED functions **************************/
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

// Create a task on identify call to handle the identify function
void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %d seconds", time);
  if (time == 0) {
    // If identify time is 0, stop blinking and restore light as it was used for identify
    zbColorLight.restoreLight();
    return;
  }
  rgbLedWrite(led, 255 * blink, 255 * blink, 255 * blink);
  blink = !blink;
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init RMT and leave light OFF
  rgbLedWrite(led, 0, 0, 0);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Enable both XY (RGB) and Temperature color capabilities
  uint16_t capabilities = ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP;
  zbColorLight.setLightColorCapabilities(capabilities);

  // Set callback functions for RGB and Temperature modes
  zbColorLight.onLightChangeRgb(setRGBLight);
  zbColorLight.onLightChangeTemp(setTempLight);

  // Optional: Set callback function for device identify
  zbColorLight.onIdentify(identify);

  // Optional: Set Zigbee device name and model
  zbColorLight.setManufacturerAndModel("Espressif", "ZBColorLightBulb");

  // Set min/max temperature range (High Kelvin -> Low Mireds: Min and Max is switched)
  zbColorLight.setLightColorTemperatureRange(kelvinToMireds(6500), kelvinToMireds(2000));

  // Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbColorLight);

  // When all EPs are registered, start Zigbee in End Device mode
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
  // Checking button for factory reset
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
    // Increase blightness by 50 every time the button is pressed
    zbColorLight.setLightLevel(zbColorLight.getLightLevel() + 50);
  }
  delay(100);
}
