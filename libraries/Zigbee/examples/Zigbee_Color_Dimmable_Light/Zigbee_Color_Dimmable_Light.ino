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

#include "Zigbee_core.h"
#include "ep/ep_color_dimmable_light.h"

#define LED_PIN RGB_BUILTIN
#define BUTTON_PIN 9  // C6/H2 Boot button
#define ZIGBEE_LIGHT_ENDPOINT       10                                   /* esp light bulb device endpoint, used to process light controlling commands */

class MyZigbeeColorLight : public ZigbeeColorDimmableLight {
public:
    // Constructor that passes parameters to the base class constructor
    MyZigbeeColorLight(uint8_t endpoint) : ZigbeeColorDimmableLight(endpoint) {}

    // Override the set_on_off function
    void setOnOff(bool value) override {
      if (value == false) {
        rgbLedWrite(LED_PIN, 0, 0, 0);  // Turn off light
      } else {
        updateLight(); // Turn on light on last color and level
      }
    }

    // Override the set_level function
    void setLevel(uint8_t level) override {
      if (level == 0) {
        rgbLedWrite(LED_PIN, 0, 0, 0);  // Turn off light and dont update ratio
        return;
      }
      _ratio = (float)level / 255;
      updateLight();
    }

    // Override the set_color function
    void setColor(uint8_t red, uint8_t green, uint8_t blue) override {
      _red = red;
      _green = green;
      _blue = blue;
      updateLight();
    }

    void updateLight() {
      rgbLedWrite(LED_PIN, _red * _ratio, _green * _ratio, _blue * _ratio);  // Update light
    }
private:
    // Add your custom attributes and methods here
    float _ratio = 1.0;
    uint8_t _red = 255;
    uint8_t _green = 255;
    uint8_t _blue = 255;

};

MyZigbeeColorLight zbColorLight = MyZigbeeColorLight(ZIGBEE_LIGHT_ENDPOINT);

/********************* Arduino functions **************************/
void setup() {
  // Init RMT and leave light OFF
  rgbLedWrite(LED_PIN, 0, 0, 0);

  // Init button for factory reset
  pinMode(BUTTON_PIN, INPUT);

  //Optional: set Zigbee device name and model
  zbColorLight.setManufacturerAndModel("Espressif", "ZBColorLightBulb");

  //Add endpoint to Zigbee Core
  log_d("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbColorLight);
  
  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  log_d("Calling Zigbee.begin()");
  Zigbee.begin();
}

void loop() {
  // Cheking button for factory reset
  if (digitalRead(BUTTON_PIN) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.printf("Reseting Zigbee to factory settings, reboot.\n");
        Zigbee.factoryReset();
      }
    }
  }
  delay(100);
}