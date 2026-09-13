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
 *
 * The example will create a Matter Occupancy Sensor Device.
 * The Occupancy Sensor will be simulated to change its state every 2 minutes.
 *
 * The onboard button can be kept pressed for 5 seconds to decommission the Matter Node.
 * The example will also show the manual commissioning code and QR code to be used in the Matter environment.
 *
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Matter Occupancy Sensor Endpoint
MatterOccupancySensor OccupancySensor;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// set your board USER BUTTON pin here - decommissioning only
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  // set initial occupancy sensor state as false and connected to a PIR sensor type (default)
  OccupancySensor.begin();

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();

}

bool simulatedHWOccupancySensor() {
  // Simulated Occupancy Sensor
  static bool occupancyState = false;
  static uint32_t lastTime = millis();
  const uint32_t occupancyTimeout = 120000;  // 2 minutes to toggle the state

  // Simulate a Occupancy Sensor state change every 2 minutes
  if (millis() - lastTime > occupancyTimeout) {
    occupancyState = !occupancyState;
    lastTime = millis();
  }
  return occupancyState;
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Occupancy Sensor Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  // Check Simulated Occupancy Sensor and set Matter Attribute
  OccupancySensor.setOccupancy(simulatedHWOccupancySensor());

  delay(50);
}
