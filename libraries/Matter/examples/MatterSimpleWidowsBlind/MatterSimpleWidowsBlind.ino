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

// Matter Simple Window Blinds Example
// This is a minimal example that only controls Lift percentage using a single onGoToLiftPercentage() callback

#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Window Covering Endpoint
MatterWindowCovering WindowBlinds;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// Simple callback - handles window Lift change request
bool onBlindLift(uint8_t liftPercent) {
  // This example only uses lift
  Serial.printf("Window Covering change request: Lift=%d%%\r\n", liftPercent);

  // Returning true will store the new Lift value into the Matter Cluster
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("Matter Simple Window Blinds Example");
  Serial.println("========================================\n");

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  // Initialize Window Covering endpoint
  // Using ROLLERSHADE type (lift only, no tilt)
  WindowBlinds.begin(100, 0, MatterWindowCovering::ROLLERSHADE);

  // Set up the onGoToLiftPercentage callback - this handles all window covering changes requested by the Matter Controller
  WindowBlinds.onGoToLiftPercentage(onBlindLift);

  // Start Matter
  Matter.begin();
  Serial.println("Matter started");
  Serial.println();

  // Print commissioning information
  Serial.println("========================================");
  Serial.println("Matter Node is not commissioned yet.");
  Serial.println("Initiate the device discovery in your Matter environment.");
  Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
  Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
  Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  Serial.println("========================================");
}

void loop() {
  delay(100);
}
