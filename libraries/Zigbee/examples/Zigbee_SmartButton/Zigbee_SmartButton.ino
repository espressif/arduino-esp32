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
 * @brief This example demonstrates simple Zigbee light switch.
 *
 * The example demonstrates how to use Zigbee library to control a light bulb.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator (Switch).
 * Button switch and Zigbee runs in separate tasks.
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

/* Zigbee Smart button () configuration */
#define SWITCH_1_ENDPOINT_NUMBER 5 // SINGLE PRESS
#define SWITCH_2_ENDPOINT_NUMBER 6 // DOUBLE PRESS
#define SWITCH_3_ENDPOINT_NUMBER 7 // LONG PRESS

uint8_t button = BOOT_PIN;

// Timing thresholds (in milliseconds)
const uint16_t debounceTime = 100; // Debounce time
const uint16_t doublePressTime = 200; // Max time between double presses
const uint16_t longPressTime = 1000; // Time to detect a long press
const uint16_t rebootPressTime = 5000; // Time to detect a long press for reboot

// Variables to track button state
int buttonState = HIGH;      // Current state of the button (HIGH means not pressed)
int lastButtonState = HIGH;  // Previous state of the button

// Timing variables
unsigned long lastDebounceTime = 0;   // Last time the button state changed
unsigned long firstPressTime = 0;    // Time of the first press in a sequence
unsigned long buttonPressStart = 0;  // Time when the button was first pressed down

// Flags
bool singlePressDetected = false;
bool doublePressDetected = false;
bool longPressDetected = false;
bool rebootPressDetected = false;
bool longPressHandled = false;
bool doublePressHandled = false;

ZigbeeSmartButton zbSmartButton1 = ZigbeeSmartButton(SWITCH_1_ENDPOINT_NUMBER);
ZigbeeSmartButton zbSmartButton2 = ZigbeeSmartButton(SWITCH_2_ENDPOINT_NUMBER);
ZigbeeSmartButton zbSmartButton3 = ZigbeeSmartButton(SWITCH_3_ENDPOINT_NUMBER);

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Configure button switch
  pinMode(button, INPUT_PULLUP);

  //Optional: set Zigbee device name and model
  zbSmartButton1.setManufacturerAndModel("Espressif", "ZigbeeSmartButton");
  zbSmartButton2.setManufacturerAndModel("Espressif", "ZigbeeSmartButton");
  zbSmartButton3.setManufacturerAndModel("Espressif", "ZigbeeSmartButton");

  //Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbSmartButton1);
  Zigbee.addEndpoint(&zbSmartButton2);
  Zigbee.addEndpoint(&zbSmartButton3);

  // // When all EPs are registered, start Zigbee with ZIGBEE_COORDINATOR mode
  // if (!Zigbee.begin()) {
  //   Serial.println("Zigbee failed to start!");
  //   Serial.println("Short press the button to reboot, long press to factory reset Zigbee.");
  //   // on button press reboot, on long press factory reset
  //   while(true){
  //     if (digitalRead(button) == LOW) {
  //       delay(100);
  //       int startTime = millis();
  //       while (digitalRead(button) == LOW) {
  //         delay(50);
  //         if ((millis() - startTime) > 3000) {
  //           Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
  //           delay(1000);
  //           Zigbee.factoryReset();
  //         }
  //       }
  //       ESP.restart();
  //     }
  //     delay(1);
  //   }
  // }
  // Serial.println("Connecting to network");
  // while (!Zigbee.connected()) {
  //   Serial.print(".");
  //   delay(100);
  // }
  // Serial.println();
}

void loop() {
  // Read the button state
  int reading = digitalRead(button);

  // Debounce handling
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceTime) {
    // If the state has stabilized, update button state
    if (reading != buttonState) {
      buttonState = reading;

      // Detect press events
      if (buttonState == LOW) {
        buttonPressStart = millis();

        if (millis() - firstPressTime <= doublePressTime && firstPressTime != 0) {
          doublePressDetected = true;
          firstPressTime = 0; // Reset for the next sequence
        } else {
          firstPressTime = millis();
        }
      } else {
        // Button released
        unsigned long pressDuration = millis() - buttonPressStart;

        if (!longPressHandled && pressDuration >= rebootPressTime) {
          rebootPressDetected = true;
        } else if (!longPressHandled && pressDuration >= longPressTime) {
          longPressDetected = true;
        } else if (!doublePressHandled && !doublePressDetected && pressDuration < longPressTime) {
          singlePressDetected = true;
        }

        longPressHandled = false; // Reset for next press

        // // If button is pressed again in a short time, it's a double press so reset the single press flag
        // if (singlePressDetected){
        //   // wait for the double press time window

        // }
      }
    }
  }

  // Handle events
  if (singlePressDetected  && !doublePressDetected) {
    Serial.println("Single press detected");
    singlePressDetected = false;
    // zbSmartButton1.toggle();
  }

  if (doublePressDetected) {
    Serial.println("Double press detected");
    doublePressDetected = false;
    doublePressHandled = true;
    // zbSmartButton2.toggle();
  }

  if (longPressDetected) {
    Serial.println("Long press detected");
    longPressDetected = false;
    longPressHandled = true;
    // zbSmartButton3.toggle();
  }

  if (rebootPressDetected) {
    Serial.println("Reboot press detected. Rebooting...");
    rebootPressDetected = false;
    longPressHandled = true;
    // Zigbee.factoryReset();
  }

  lastButtonState = reading;
}
