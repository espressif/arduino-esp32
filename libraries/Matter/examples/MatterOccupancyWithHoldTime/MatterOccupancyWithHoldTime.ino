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
 * This example is an example code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 *
 * The example will create a Matter Occupancy Sensor Device.
 * The Occupancy Sensor will be simulated to toggle occupancy every 2 minutes.
 * When occupancy is detected, the sensor holds the "occupied" state for the HoldTime duration
 * (default: 30 seconds, configurable via Matter Controller). After HoldTime expires,
 * the sensor automatically switches to "unoccupied" state.
 *
 * The HoldTime attribute allows you to adjust how long the active status is retained
 * after the person leaves. The HoldTime can be changed by a Matter Controller, and the
 * onHoldTimeChange() callback is used to update the simulated sensor functionality.
 *
 * The HoldTime value is persisted to Preferences (NVS) and restored on reboot, so the
 * last configured HoldTime value is maintained across device restarts.
 *
 * The onboard button can be kept pressed for 5 seconds to decommission the Matter Node.
 * The example will also show the manual commissioning code and QR code to be used in the Matter environment.
 *
 */

// Matter Manager
#include <Matter.h>
// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif
#include <Preferences.h>

// HoldTime configuration constants
const uint16_t HOLD_TIME_MIN = 0;        // Minimum HoldTime in seconds
const uint16_t HOLD_TIME_MAX = 3600;     // Maximum HoldTime in seconds (1 hour)
const uint16_t HOLD_TIME_DEFAULT = 30;   // Default HoldTime in seconds

// List of Matter Endpoints for this Node
// Matter Occupancy Sensor Endpoint
MatterOccupancySensor OccupancySensor;

// Preferences to store HoldTime value across reboots
Preferences matterPref;
const char *holdTimePrefKey = "HoldTime";

// set your board USER BUTTON pin here - decommissioning only
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Simulated Occupancy Sensor with HoldTime support
// When occupancy is detected, it holds the "occupied" state for HoldTime seconds
// After HoldTime expires, it automatically switches to "unoccupied"
// 
// Behavior for different HoldTime vs detectionInterval relationships:
// - holdTime_ms < detectionInterval: State switches to unoccupied after HoldTime, then waits for next detection
// - holdTime_ms == detectionInterval: If detections keep coming, timer resets (continuous occupancy)
// - holdTime_ms > detectionInterval: If detections keep coming, timer resets (continuous occupancy)
//   If detections stop, HoldTime expires after the last detection
bool simulatedHWOccupancySensor() {
  static bool occupancyState = false;
  static uint32_t lastDetectionTime = 0;
  static uint32_t lastDetectionEvent = millis();
  const uint32_t detectionInterval = 120000;  // Simulate detection every 2 minutes
  
  // Get current HoldTime from the sensor (can be changed by Matter Controller)
  uint32_t holdTime_ms = OccupancySensor.getHoldTime() * 1000;  // Convert seconds to milliseconds
  
  // Check HoldTime expiration FIRST (before processing new detections)
  // This ensures HoldTime can expire even if a detection occurs in the same iteration
  if (occupancyState && (millis() - lastDetectionTime > holdTime_ms)) {
    occupancyState = false;
    // Reset detection interval counter so next detection can happen immediately
    // This makes the simulation more responsive after the room becomes unoccupied
    lastDetectionEvent = millis();
    Serial.println("HoldTime expired. Switching to unoccupied state.");
  }
  
  // Simulate periodic occupancy detection (e.g., motion detected)
  // Check this AFTER HoldTime expiration so new detections can immediately re-trigger occupancy
  if (millis() - lastDetectionEvent > detectionInterval) {
    // New detection event occurred
    lastDetectionEvent = millis();
    
    if (!occupancyState) {
      // Transition from unoccupied to occupied - start hold timer
      occupancyState = true;
      lastDetectionTime = millis();
      Serial.printf("Occupancy detected! Holding state for %u seconds (HoldTime)\n", OccupancySensor.getHoldTime());
    } else {
      // Already occupied - new detection extends the hold period by resetting the timer
      // This simulates continuous occupancy (person still present)
      lastDetectionTime = millis();
      Serial.printf("Occupancy still detected. Resetting hold timer to %u seconds (HoldTime)\n", OccupancySensor.getHoldTime());
    }
  }
  
  return occupancyState;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
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
    // The callback can return false to reject the change, or true to accept it
    // In this case, we always accept the change and update the simulator
    return true;
  });

  // set initial occupancy sensor state as false and connected to a PIR sensor type (default)
  OccupancySensor.begin();

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  
  // Set HoldTimeLimits after Matter.begin() (optional, but recommended for validation)
  if (!OccupancySensor.setHoldTimeLimits(HOLD_TIME_MIN, HOLD_TIME_MAX, HOLD_TIME_DEFAULT)) {
    Serial.println("Warning: Failed to set HoldTimeLimits");
  } else {
    Serial.printf("HoldTimeLimits set: Min=%u, Max=%u, Default=%u seconds\n", 
                  HOLD_TIME_MIN, HOLD_TIME_MAX, HOLD_TIME_DEFAULT);
  }
  
  // Set initial HoldTime (use stored value if valid, otherwise use default)
  // This must be done after Matter.begin() because setHoldTime() requires the Matter event loop
  if (!OccupancySensor.setHoldTime(storedHoldTime)) {
    Serial.printf("Warning: Failed to set HoldTime to %u seconds\n", storedHoldTime);
  } else {
    Serial.printf("HoldTime set to: %u seconds\n", storedHoldTime);
  }
  
  Serial.printf("Initial HoldTime: %u seconds\n", OccupancySensor.getHoldTime());

  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Occupancy Sensor Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }
}

void loop() {
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (button_state && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning Occupancy Sensor Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissining again, reboot takes a second or so
  }

  // Check Simulated Occupancy Sensor and set Matter Attribute
  OccupancySensor.setOccupancy(simulatedHWOccupancySensor());

  delay(50);
}
