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

/*
 * This example is the smallest code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * It doesn't control any light, but the ESP32 will send debug messages indicating the Matter activity.
 * Therefore, turning DEBUG Level may be useful to follow Matter Accessory and Controller messages.
 */

// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// Single On/Off Light Endpoint - at least one per node
MatterOnOffLight OnOffLight;

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

void setup() {
  WiFi.enableIPv6(true);
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  if (!Matter.isDeviceCommissioned()) {
    log_i("Matter Node is not commissioned yet.");
    log_i("Initiate the device discovery in your Matter environment.");
    log_i("Commission it to your Matter hub with the manual pairing code or QR code");
    log_i("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    log_i("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

void loop() {
  delay(500);
}
