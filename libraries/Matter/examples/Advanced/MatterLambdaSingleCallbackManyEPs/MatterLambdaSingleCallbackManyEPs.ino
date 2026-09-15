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
  This example creates 6 on-off light endpoints that share the same onChangeOnOff() callback code.
  It uses Lambda Function with an extra Lambda Capture information that links the Endpoint to its individual information.
  After the Matter example is commissioned, the expected Serial output shall be similar to this:

Matter App Control: 'Room 1' (OnOffLight[0], Endpoint 1, GPIO 2) changed to: OFF
Matter App Control: 'Room 1' (OnOffLight[0], Endpoint 1, GPIO 2) changed to: ON
Matter App Control: 'Room 5' (OnOffLight[4], Endpoint 5, GPIO 10) changed to: ON
Matter App Control: 'Room 2' (OnOffLight[1], Endpoint 2, GPIO 4) changed to: ON
Matter App Control: 'Room 4' (OnOffLight[3], Endpoint 4, GPIO 8) changed to: ON
Matter App Control: 'Room 6' (OnOffLight[5], Endpoint 6, GPIO 12) changed to: ON
Matter App Control: 'Room 3' (OnOffLight[2], Endpoint 3, GPIO 6) changed to: ON
Matter App Control: 'Room 5' (OnOffLight[4], Endpoint 5, GPIO 10) changed to: OFF
*/

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"


//number of On-Off Lights:
const uint8_t MAX_LIGHT_NUMBER = 6;

// array of OnOffLight endpoints
MatterOnOffLight OnOffLight[MAX_LIGHT_NUMBER];

// all pins, one for each on-off light
uint8_t lightPins[MAX_LIGHT_NUMBER] = {2, 4, 6, 8, 10, 12};  // must replace it by the real pin for the target SoC and application

// friendly OnOffLights names used for printing a message in the callback
const char *lightName[MAX_LIGHT_NUMBER] = {
  "Room 1", "Room 2", "Room 3", "Room 4", "Room 5", "Room 6",
};

// simple setup() function
void setup() {
  Serial.begin(115200);  // callback will just print a message in the console

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // setup all the OnOff Light endpoint and their lambda callback functions
  for (uint8_t i = 0; i < MAX_LIGHT_NUMBER; i++) {
    pinMode(lightPins[i], OUTPUT);  // set the GPIO function
    OnOffLight[i].begin(false);     // off

    // inline lambda function using capture array index -> it will just print a message in the console
    OnOffLight[i].onChangeOnOff([i](bool state) -> bool {
      // Display message with the specific light name and details
      Serial.printf(
        "Matter App Control: '%s' (OnOffLight[%u], Endpoint %u, GPIO %u) changed to: %s\r\n", lightName[i], i, OnOffLight[i].getEndPointId(), lightPins[i],
        state ? "ON" : "OFF"
      );

      return true;
    });
  }
  // last step, starting Matter Stack
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();
}
