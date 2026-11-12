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

// Matter Simple Blinds Example
// This is a minimal example that only controls Lift percentage using a single onGoToLiftPercentage() callback

#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Window Covering Endpoint
MatterWindowCovering WindowBlinds;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// Simple callback - handles window Lift change request
bool onBlindsLift(uint8_t liftPercent) {
  // This example only uses lift
  Serial.printf("Window Covering change request: Lift=%u%%\r\n", liftPercent);

  // Returning true will store the new Lift value into the Matter Cluster
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n============================");
  Serial.println("Matter Simple Blinds Example");
  Serial.println("============================\n");

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Window Covering endpoint
  // Using ROLLERSHADE type (lift only, no tilt)
  WindowBlinds.begin(100, 0, MatterWindowCovering::ROLLERSHADE);

  // Set up the onGoToLiftPercentage callback - this handles all window covering changes requested by the Matter Controller
  WindowBlinds.onGoToLiftPercentage(onBlindsLift);

  // Start Matter
  matterSetExampleIdentity("Window Covering");
  Matter.begin();
  matterWaitUntilReady();
  Serial.println("Matter started");
}

void loop() {
  matterRestartIfNoFabric();

  delay(100);
}
