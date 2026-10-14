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

/*
 * This example is an example code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Matter Light Sensor Endpoint
MatterLightSensor SimulatedLightSensor;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Simulate an illuminance sensor - add your preferred illuminance sensor library code here
// Returns the illuminance in lux; the endpoint converts it to the Matter-style logarithmic value internally.
float getSimulatedIlluminance() {
  static float simulatedIlluminanceHWSensor = 100.0;

  // it will increase from 100lx to 300lx in 10lx steps to simulate an illuminance sensor
  simulatedIlluminanceHWSensor = simulatedIlluminanceHWSensor + 10.0;
  if (simulatedIlluminanceHWSensor > 300) {
    simulatedIlluminanceHWSensor = 100;
  }

  return simulatedIlluminanceHWSensor;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // set initial illuminance sensor measurement
  // Simulated Sensor - it shall initially print 2150lx and then move to the 100lx to 300lx illuminance range
  SimulatedLightSensor.begin(2150.0);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  static uint32_t timeCounter = 0;

  // Print the current illuminance value every 5s
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    // Print the current illuminance value
    Serial.printf("Current Illuminance is %.02flx\r\n", SimulatedLightSensor.getIlluminance());
    // Update Illuminance from the (Simulated) Hardware Sensor
    // Matter APP shall display the updated illuminance value
    SimulatedLightSensor.setIlluminance(getSimulatedIlluminance());
  }

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Light Sensor Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(500);
}
