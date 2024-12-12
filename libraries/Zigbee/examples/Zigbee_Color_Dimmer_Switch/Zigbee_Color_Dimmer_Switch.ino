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
 * @brief This example demonstrates Zigbee color dimmer switch.
 *
 * The example demonstrates how to use Zigbee library to control a RGB light bulb.
 * The RGB light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator (Switch).
 * To turn on/off the light, push the button on the switch.
 * To change the color or level of the light, send serial commands to the switch.
 *
 * By setting the switch to allow multiple binding, so it can bind to multiple lights.
 * Also every 30 seconds, all bound lights are printed to the serial console.
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

#include "Zigbee.h"

/* Zigbee color dimmer switch configuration */
#define SWITCH_ENDPOINT_NUMBER 5
uint8_t button = BOOT_PIN;

/* Zigbee switch */
ZigbeeColorDimmerSwitch zbSwitch = ZigbeeColorDimmerSwitch(SWITCH_ENDPOINT_NUMBER);

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  //Init button switch
  pinMode(button, INPUT_PULLUP);

  //Optional: set Zigbee device name and model
  zbSwitch.setManufacturerAndModel("Espressif", "ZigbeeSwitch");

  //Optional to allow multiple light to bind to the switch
  zbSwitch.allowMultipleBinding(true);

  //Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbSwitch);

  //Open network for 180 seconds after boot
  Zigbee.setRebootOpenNetwork(180);

  //When all EPs are registered, start Zigbee with ZIGBEE_COORDINATOR mode
  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  Serial.println("Waiting for Light to bound to the switch");
  //Wait for switch to bound to a light:
  while (!zbSwitch.bound()) {
    Serial.printf(".");
    delay(500);
  }
  Serial.println();
}

void loop() {
  // Handle button switch in loop()
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    while (digitalRead(button) == LOW) {
      delay(50);
    }
    // Toggle light
    zbSwitch.lightToggle();
  }
  // Handle serial input to control color and level of the light
  if (Serial.available()) {
    String command = Serial.readString();

    if (command == "on") {
      zbSwitch.lightOn();
    } else if (command == "off") {
      zbSwitch.lightOff();
    } else if (command == "toggle") {
      zbSwitch.lightToggle();
    } else if (command == "red") {
      zbSwitch.setLightColor(255, 0, 0);
    } else if (command == "green") {
      zbSwitch.setLightColor(0, 255, 0);
    } else if (command == "blue") {
      zbSwitch.setLightColor(0, 0, 255);
    } else if (command == "white") {
      zbSwitch.setLightColor(255, 255, 255);
    } else if (command == "color") {
      //wait for color value
      Serial.println("Enter red value (0-255):");
      while (!Serial.available()) {
        delay(100);
      }
      int red = Serial.parseInt();
      Serial.println("Enter green value (0-255):");
      while (!Serial.available()) {
        delay(100);
      }
      int green = Serial.parseInt();
      Serial.println("Enter blue value (0-255):");
      while (!Serial.available()) {
        delay(100);
      }
      int blue = Serial.parseInt();
      zbSwitch.setLightColor(red, green, blue);
    } else if (command == "level") {
      //wait for level value
      Serial.println("Enter level value (0-255):");
      while (!Serial.available()) {
        delay(100);
      }
      int level = Serial.parseInt();
      zbSwitch.setLightLevel(level);
    } else {
      Serial.println("Unknown command");
    }
  }

  // print the bound devices (lights) every 30 seconds
  static uint32_t last_print = 0;
  if (millis() - last_print > 30000) {
    last_print = millis();
    zbSwitch.printBoundDevices(Serial);
  }
}
