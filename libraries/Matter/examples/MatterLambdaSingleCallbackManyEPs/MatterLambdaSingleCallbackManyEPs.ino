// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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
#include <Matter.h>
#include <Preferences.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

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

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif

  // setup all the OnOff Light endpoint and their lambda callback functions
  for (uint8_t i = 0; i < MAX_LIGHT_NUMBER; i++) {
    pinMode(lightPins[i], OUTPUT);  // set the GPIO function
    OnOffLight[i].begin(false);     // off

    // inline lambda function using capture array index -> it will just print a message in the console
    OnOffLight[i].onChangeOnOff([i](bool state) -> bool {
      // Display message with the specific light name and details
      Serial.printf(
        "Matter App Control: '%s' (OnOffLight[%d], Endpoint %d, GPIO %d) changed to: %s\r\n", lightName[i], i, OnOffLight[i].getEndPointId(), lightPins[i],
        state ? "ON" : "OFF"
      );

      return true;
    });
  }
  // last step, starting Matter Stack
  Matter.begin();
}

void loop() {
  // Check Matter Plugin Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Plugin Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
  } else {
    if (Matter.isDeviceConnected()) {
      Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    } else {
      Serial.println("Matter Node is commissioned. Waiting for the network connection.");
    }
    // wait 3 seconds for the network connection
    delay(3000);
  }

  delay(100);
}
