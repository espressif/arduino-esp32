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
 * @brief This example demonstrates simple Zigbee multi-light switch.
 *
 * The example demonstrates how to use Zigbee library to control multiple light bulbs.
 * The light bulbs are Zigbee devices, which are controlled by a Zigbee coordinator/router (Multi-Switch).
 * Settings are stored in NVS to not be lost after power loss.
 * Configuring and controlling the lights is done via serial input.
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
#include <Preferences.h>

#define ZIGBEE_ROLE ZIGBEE_ROUTER  // ZIGBEE_ROUTER for HomeAssistant integration, ZIGBEE_COORDINATOR for running own network

/* Zigbee switch configuration */
#define SWITCH_ENDPOINT_NUMBER 1

uint8_t button = BOOT_PIN;

ZigbeeSwitch zbSwitch = ZigbeeSwitch(SWITCH_ENDPOINT_NUMBER);

int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// To be stored in NVS to not be lost after power loss
Preferences prefs;

zb_device_params_t light_1;
zb_device_params_t light_2;
zb_device_params_t light_3;

void storeLightParams(zb_device_params_t *light, int light_number) {
  char key[10];
  snprintf(key, sizeof(key), "light_%d", light_number);
  prefs.putBytes(key, light, sizeof(zb_device_params_t));
}

void loadLightParams(zb_device_params_t *light, int light_number) {
  char key[10];
  snprintf(key, sizeof(key), "light_%d", light_number);
  prefs.getBytes(key, light, sizeof(zb_device_params_t));
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Initialize Preferences
  prefs.begin("lights", false);  // false means read/write mode

  // Load saved light parameters
  loadLightParams(&light_1, 1);
  loadLightParams(&light_2, 2);
  loadLightParams(&light_3, 3);

  // Init button switch
  pinMode(button, INPUT_PULLUP);

  // Set Zigbee device name and model
  zbSwitch.setManufacturerAndModel("Espressif", "ZBMultiSwitch");

  // Set binding settings depending on the role
  if (ZIGBEE_ROLE == ZIGBEE_COORDINATOR) {
    zbSwitch.allowMultipleBinding(true);  // To allow binding multiple lights to the switch
  } else {
    zbSwitch.setManualBinding(true);  //Set manual binding to true, so binding is done on Home Assistant side
  }

  // Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeSwitch endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbSwitch);

  // When all EPs are registered, start Zigbee with given role
  if (!Zigbee.begin(ZIGBEE_ROLE)) {
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
  // Handle button switch in loop()
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    while (digitalRead(button) == LOW) {
      delay(50);
    }
    // Print bound devices
    Serial.println("Bound devices:");
    zbSwitch.printBoundDevices(Serial);
    Serial.println("Lights configured:");
    Serial.printf("Light 1: %d %s\n", light_1.endpoint, Zigbee.formatIEEEAddress(light_1.ieee_addr));
    Serial.printf("Light 2: %d %s\n", light_2.endpoint, Zigbee.formatIEEEAddress(light_2.ieee_addr));
    Serial.printf("Light 3: %d %s\n", light_3.endpoint, Zigbee.formatIEEEAddress(light_3.ieee_addr));
  }
  // Handle serial input to configure and control the lights
  if (Serial.available()) {
    String command = Serial.readString();
    Serial.println("Command: " + command);

    if (command == "config") {
      //wait for light number, endpoint and ieee address
      Serial.println("Enter light number (1-3):");
      while (!Serial.available()) {
        delay(100);
      }
      int light_number = Serial.parseInt();
      Serial.println("Enter endpoint:");
      while (!Serial.available()) {
        delay(100);
      }
      int endpoint = Serial.parseInt();
      Serial.println("Enter ieee address:");
      while (!Serial.available()) {
        delay(100);
      }
      String ieee_address = Serial.readStringUntil('\n');
      ieee_address.trim();
      //convert ieee address to uint8_t array (format in string is 00:00:00:00:00:00:00:00)
      uint8_t ieee_address_array[8];
      int index = 0;
      bool valid = true;

      // Check if the string has the correct format (8 hex pairs with colons)
      if (ieee_address.length() != 23) {  // 8 pairs * 2 + 7 colons
        Serial.println("Invalid IEEE address format. Expected format: 00:00:00:00:00:00:00:00");
        valid = false;
      } else {
        for (int i = 0; i < ieee_address.length() && index < 8 && valid; i += 3) {
          // Check for colon at expected positions
          if (i > 0 && ieee_address.charAt(i - 1) != ':') {
            valid = false;
            break;
          }
          // Convert two hex characters to a byte
          char hex[3] = {ieee_address.charAt(i), ieee_address.charAt(i + 1), '\0'};
          char *endptr;
          long value = strtol(hex, &endptr, 16);
          if (*endptr != '\0' || value < 0 || value > 255) {
            valid = false;
            break;
          }
          // Store bytes in reverse order to match Zigbee standard
          ieee_address_array[7 - index++] = (uint8_t)value;
        }
      }

      if (!valid || index != 8) {
        Serial.println("Invalid IEEE address. Please enter a valid address in format: 00:00:00:00:00:00:00:00");
        return;
      }
      //set the light parameters
      if (light_number == 1) {
        light_1.endpoint = endpoint;
        memcpy(light_1.ieee_addr, ieee_address_array, 8);
        storeLightParams(&light_1, 1);
      } else if (light_number == 2) {
        light_2.endpoint = endpoint;
        memcpy(light_2.ieee_addr, ieee_address_array, 8);
        storeLightParams(&light_2, 2);
      } else if (light_number == 3) {
        light_3.endpoint = endpoint;
        memcpy(light_3.ieee_addr, ieee_address_array, 8);
        storeLightParams(&light_3, 3);
      }
      Serial.printf("Light %d configured\n", light_number);
    } else if (command == "remove") {
      //wait for light number
      Serial.println("Enter light number (1-3):");
      while (!Serial.available()) {
        delay(100);
      }
      int light_number = Serial.parseInt();
      uint8_t ieee_address_empty[8] = {0, 0, 0, 0, 0, 0, 0, 0};
      if (light_number == 1) {
        light_1.endpoint = 0;
        memcpy(light_1.ieee_addr, ieee_address_empty, 8);
        storeLightParams(&light_1, 1);
      } else if (light_number == 2) {
        light_2.endpoint = 0;
        memcpy(light_2.ieee_addr, ieee_address_empty, 8);
        storeLightParams(&light_2, 2);
      } else if (light_number == 3) {
        light_3.endpoint = 0;
        memcpy(light_3.ieee_addr, ieee_address_empty, 8);
        storeLightParams(&light_3, 3);
      }
      Serial.printf("Light %d removed\n", light_number);
    } else if (command == "on") {
      Serial.println("  --> SIG Input : All Lights ON");
      zbSwitch.lightOn();
    } else if (command == "off") {
      Serial.println("  --> SIG Input : All Lights OFF");
      zbSwitch.lightOff();
    } else if (command == "toggle") {
      Serial.println("  --> SIG Input : All Lights Toggle");
      zbSwitch.lightToggle();
    } else if (command == "1on") {
      Serial.println("  --> SIG Input : Light 1 ON");
      zbSwitch.lightOn(light_1.endpoint, light_1.ieee_addr);
    } else if (command == "1off") {
      Serial.println("  --> SIG Input : Light 1 OFF");
      zbSwitch.lightOff(light_1.endpoint, light_1.ieee_addr);
    } else if (command == "1toggle") {
      Serial.println("  --> SIG Input : Light 1 Toggle");
      zbSwitch.lightToggle(light_1.endpoint, light_1.ieee_addr);
    } else if (command == "2on") {
      Serial.println("  --> SIG Input : Light 2 ON");
      zbSwitch.lightOn(light_2.endpoint, light_2.ieee_addr);
    } else if (command == "2off") {
      Serial.println("  --> SIG Input : Light 2 OFF");
      zbSwitch.lightOff(light_2.endpoint, light_2.ieee_addr);
    } else if (command == "2toggle") {
      Serial.println("  --> SIG Input : Light 2 Toggle");
      zbSwitch.lightToggle(light_2.endpoint, light_2.ieee_addr);
    } else if (command == "3on") {
      Serial.println("  --> SIG Input : Light 3 ON");
      zbSwitch.lightOn(light_3.endpoint, light_3.ieee_addr);
    } else if (command == "3off") {
      Serial.println("  --> SIG Input : Light 3 OFF");
      zbSwitch.lightOff(light_3.endpoint, light_3.ieee_addr);
    } else if (command == "3toggle") {
      Serial.println("  --> SIG Input : Light 3 Toggle");
      zbSwitch.lightToggle(light_3.endpoint, light_3.ieee_addr);
    } else if (command == "freset") {
      Serial.println("  --> SIG Input : Factory Reset!");
      delay(1500);
      Zigbee.factoryReset();
    } else if (command == "open_network") {
      Serial.println("  --> SIG Input : Open Network");
      if (ZIGBEE_ROLE == ZIGBEE_COORDINATOR) {
        Zigbee.openNetwork(180);
      } else {
        Serial.println("Open network is only available for coordinator role");
      }
    } else {
      Serial.println("Unknown command");
    }
  }
}
