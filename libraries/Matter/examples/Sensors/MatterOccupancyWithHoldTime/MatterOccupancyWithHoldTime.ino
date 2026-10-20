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
 * Simulated PIR pulse (1 s) every 2 minutes. CHIP HoldTime (default 30 s)
 * keeps Occupancy occupied after the pulse; the hub then goes vacant.
 * Do not implement HoldTime in the sketch — setOccupancy(false) starts CHIP's timer.
 * Polling the same vacant reading again would restart that timer.
 *
 * The HoldTime value is persisted to Preferences (NVS) and restored on reboot, so the
 * last configured HoldTime value is maintained across device restarts.
 *
 * The onboard button can be kept pressed for 5 seconds to decommission the Matter Node.
 * The example will also show the manual commissioning code and QR code to be used in the Matter environment.
 *
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

#include <Preferences.h>

// HoldTime configuration constants
const uint16_t HOLD_TIME_MIN = 1;       // Minimum HoldTime in seconds (CHIP rejects 0)
const uint16_t HOLD_TIME_MAX = 3600;    // Maximum HoldTime in seconds (1 hour)
const uint16_t HOLD_TIME_DEFAULT = 30;  // Default HoldTime in seconds

// List of Matter Endpoints for this Node
// Matter Occupancy Sensor Endpoint
MatterOccupancySensor OccupancySensor;

// Preferences to store HoldTime value across reboots
Preferences matterPref;
const char *holdTimePrefKey = "HoldTime";

// set your board USER BUTTON pin here - decommissioning only
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Raw PIR pulse. CHIP HoldTime keeps the cluster occupied after this returns false.
bool simulatedHWOccupancySensor() {
  static uint32_t windowStart = millis();
  const uint32_t detectionInterval = 120000;  // Pulse every 2 minutes
  const uint32_t pulseMs = 1000;
  const uint32_t now = millis();

  if ((now - windowStart) >= detectionInterval) {
    windowStart = now;
    Serial.printf("Motion pulse. Cluster stays occupied for HoldTime=%u s\r\n", OccupancySensor.getHoldTime());
  }
  return (now - windowStart) < pulseMs;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Preferences and read stored HoldTime value
  matterPref.begin("MatterPrefs", false);
  uint16_t storedHoldTime = matterPref.getUShort(holdTimePrefKey, HOLD_TIME_DEFAULT);

  // Validate stored value is within limits
  if (storedHoldTime < HOLD_TIME_MIN || storedHoldTime > HOLD_TIME_MAX) {
    uint16_t invalidValue = storedHoldTime;
    storedHoldTime = HOLD_TIME_DEFAULT;
    Serial.printf("Invalid stored HoldTime (%u), using default: %u seconds\n", invalidValue, HOLD_TIME_DEFAULT);
  } else if (storedHoldTime != HOLD_TIME_DEFAULT) {
    Serial.printf("Restored HoldTime from Preferences: %u seconds\n", storedHoldTime);
  }

  // Register callback for HoldTime changes from Matter Controller
  OccupancySensor.onHoldTimeChange([](uint16_t holdTime_seconds) -> bool {
    Serial.printf("HoldTime changed to %u seconds by Matter Controller\n", holdTime_seconds);
    // Store the new HoldTime value to Preferences for persistence across reboots
    matterPref.putUShort(holdTimePrefKey, holdTime_seconds);
    // Return false to reject the controller write. CHIP already owns HoldTime.
    return true;
  });

  // set initial occupancy sensor state as false and connected to a PIR sensor type (default)
  OccupancySensor.begin();

  // HoldTime must be enabled before Matter.begin() so the OccupancySensing cluster
  // is created with the hold-time feature. Later controller writes still work.
  if (!OccupancySensor.setHoldTimeLimits(HOLD_TIME_MIN, HOLD_TIME_MAX, HOLD_TIME_DEFAULT)) {
    Serial.println("Warning: Failed to set HoldTimeLimits");
  } else {
    Serial.printf("HoldTimeLimits set: Min=%u, Max=%u, Default=%u seconds\n", HOLD_TIME_MIN, HOLD_TIME_MAX, HOLD_TIME_DEFAULT);
  }

  if (!OccupancySensor.setHoldTime(storedHoldTime)) {
    Serial.printf("Warning: Failed to set HoldTime to %u seconds\n", storedHoldTime);
  } else {
    Serial.printf("HoldTime set to: %u seconds\n", storedHoldTime);
  }

  // Matter beginning - Last step, after all EndPoints are initialized
  matterSetExampleIdentity("Occupancy Sensor");
  Matter.begin();

  Serial.printf("Initial HoldTime: %u seconds\n", OccupancySensor.getHoldTime());
  matterWaitUntilReady();
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

  OccupancySensor.setOccupancy(simulatedHWOccupancySensor());

  static bool lastHubOccupied = OccupancySensor.isOccupied();
  const bool hubOccupied = OccupancySensor.isOccupied();
  if (hubOccupied != lastHubOccupied) {
    Serial.printf("Hub occupancy: %s\r\n", hubOccupied ? "occupied" : "vacant");
    lastHubOccupied = hubOccupied;
  }

  delay(50);
}
