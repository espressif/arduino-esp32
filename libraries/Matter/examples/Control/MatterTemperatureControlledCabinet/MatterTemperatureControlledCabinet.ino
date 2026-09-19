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
 * This example demonstrates the Temperature Number mode of the Matter Temperature Controlled Cabinet Device.
 *
 * This example will create a Matter Device which can be commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 *
 * The example will create a Matter Temperature Controlled Cabinet Device using temperature_number feature.
 * The Temperature Controlled Cabinet can be controlled via Matter controllers to set
 * temperature setpoint with min/max limits and optional step control.
 *
 * This mode is mutually exclusive with temperature_level mode.
 * See MatterTemperatureControlledCabinetLevels example for temperature level control.
 *
 * Demo: after commission, the sketch cycles the setpoint every 1 s (can fight the hub).
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Matter Temperature Controlled Cabinet Endpoint
MatterTemperatureControlledCabinet TemperatureCabinet;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Temperature control state
struct TemperatureControlState {
  bool initialized;
  bool increasing;
  double currentSetpoint;
  double initialSetpoint;
  bool setpointReachedIncreasing;
  bool setpointReachedDecreasing;
};

static TemperatureControlState tempState = {
  .initialized = false,
  .increasing = true,
  .currentSetpoint = 0.0,
  .initialSetpoint = 0.0,
  .setpointReachedIncreasing = false,
  .setpointReachedDecreasing = false
};

// Initialize temperature control state
void initTemperatureControl() {
  if (!tempState.initialized) {
    tempState.currentSetpoint = TemperatureCabinet.getTemperatureSetpoint();
    tempState.initialSetpoint = tempState.currentSetpoint;
    tempState.initialized = true;
  }
}

// Check and log when initial setpoint is reached/overpassed
void checkSetpointReached(double newSetpoint, bool isIncreasing, bool directionChanged) {
  if (directionChanged) {
    // Reset flags when direction changes
    tempState.setpointReachedIncreasing = false;
    tempState.setpointReachedDecreasing = false;
    return;
  }

  if (isIncreasing && !tempState.setpointReachedIncreasing && newSetpoint >= tempState.initialSetpoint) {
    Serial.printf("*** Temperature setpoint %.02f°C reached/overpassed while increasing ***\r\n", tempState.initialSetpoint);
    tempState.setpointReachedIncreasing = true;
  } else if (!isIncreasing && !tempState.setpointReachedDecreasing && newSetpoint <= tempState.initialSetpoint) {
    Serial.printf("*** Temperature setpoint %.02f°C reached/overpassed while decreasing ***\r\n", tempState.initialSetpoint);
    tempState.setpointReachedDecreasing = true;
  }
}

// Update temperature setpoint with cycling logic
void updateTemperatureSetpoint() {
  double minTemp = TemperatureCabinet.getMinTemperature();
  double maxTemp = TemperatureCabinet.getMaxTemperature();
  double step = TemperatureCabinet.getStep();

  // Calculate next setpoint based on direction and step
  bool directionChanged = false;

  if (tempState.increasing) {
    tempState.currentSetpoint += step;
    if (tempState.currentSetpoint >= maxTemp) {
      tempState.currentSetpoint = maxTemp;
      tempState.increasing = false;  // Reverse direction
      directionChanged = true;
    }
  } else {
    tempState.currentSetpoint -= step;
    if (tempState.currentSetpoint <= minTemp) {
      tempState.currentSetpoint = minTemp;
      tempState.increasing = true;  // Reverse direction
      directionChanged = true;
    }
  }

  // Check if setpoint has been reached or overpassed
  checkSetpointReached(tempState.currentSetpoint, tempState.increasing, directionChanged);

  // Update the temperature setpoint
  if (TemperatureCabinet.setTemperatureSetpoint(tempState.currentSetpoint)) {
    Serial.printf("Temperature setpoint updated to: %.02f°C (Range: %.02f°C to %.02f°C)\r\n", tempState.currentSetpoint, minTemp, maxTemp);
  } else {
    Serial.printf("Failed to update temperature setpoint to: %.02f°C\r\n", tempState.currentSetpoint);
  }
}

// Print current temperature status
void printTemperatureStatus() {
  Serial.printf(
    "Current Temperature Setpoint: %.02f°C (Range: %.02f°C to %.02f°C)\r\n", TemperatureCabinet.getTemperatureSetpoint(),
    TemperatureCabinet.getMinTemperature(), TemperatureCabinet.getMaxTemperature()
  );
}

// Handle button press for decommissioning
void handleButtonPress() {
  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Temperature Controlled Cabinet Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Temperature Controlled Cabinet with:
  // - Initial setpoint: 4.0°C (typical refrigerator temperature)
  // - Min temperature: -10.0°C
  // - Max temperature: 10.0°C
  // - Step: 0.5°C (optional, for temperature_step feature)
  TemperatureCabinet.begin(4.0, -10.0, 10.0, 0.5);

  // Matter beginning - Last step, after all EndPoints are initialized
  matterSetExampleIdentity("Temp Cabinet");
  Matter.begin();
  matterWaitUntilReady();

  // Print initial configuration
  Serial.println("\nTemperature Controlled Cabinet Configuration:");
  Serial.printf("  Setpoint: %.02f°C\n", TemperatureCabinet.getTemperatureSetpoint());
  Serial.printf("  Min Temperature: %.02f°C\n", TemperatureCabinet.getMinTemperature());
  Serial.printf("  Max Temperature: %.02f°C\n", TemperatureCabinet.getMaxTemperature());
  Serial.printf("  Step: %.02f°C\n", TemperatureCabinet.getStep());
}

void loop() {
  matterRestartIfNoFabric();

  static uint32_t timeCounter = 0;
  static uint32_t lastUpdateTime = 0;

  // Initialize temperature control state on first run
  initTemperatureControl();

  // Update temperature setpoint dynamically every 1 second
  uint32_t currentTime = millis();
  if (currentTime - lastUpdateTime >= 1000) {  // 1 second interval
    lastUpdateTime = currentTime;
    updateTemperatureSetpoint();
  }

  // Print the current temperature setpoint every 5s
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    printTemperatureStatus();
  }

  // Handle button press for decommissioning
  handleButtonPress();

  delay(500);
}
