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
 */

// Matter Manager
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Matter Temperature Controlled Cabinet Endpoint
MatterTemperatureControlledCabinet TemperatureCabinet;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control - decommision the Matter Node
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

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
    Serial.printf("Temperature setpoint updated to: %.02f°C (Range: %.02f°C to %.02f°C)\r\n", 
                  tempState.currentSetpoint, minTemp, maxTemp);
  } else {
    Serial.printf("Failed to update temperature setpoint to: %.02f°C\r\n", tempState.currentSetpoint);
  }
}

// Print current temperature status
void printTemperatureStatus() {
  Serial.printf("Current Temperature Setpoint: %.02f°C (Range: %.02f°C to %.02f°C)\r\n", 
                TemperatureCabinet.getTemperatureSetpoint(),
                TemperatureCabinet.getMinTemperature(),
                TemperatureCabinet.getMaxTemperature());
}

// Handle button press for decommissioning
void handleButtonPress() {
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning Temperature Controlled Cabinet Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissining again, reboot takes a second or so
  }
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

  // Initialize Temperature Controlled Cabinet with:
  // - Initial setpoint: 4.0°C (typical refrigerator temperature)
  // - Min temperature: -10.0°C
  // - Max temperature: 10.0°C
  // - Step: 0.5°C (optional, for temperature_step feature)
  TemperatureCabinet.begin(4.0, -10.0, 10.0, 0.5);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Temperature Controlled Cabinet Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // Print initial configuration
  Serial.println("\nTemperature Controlled Cabinet Configuration:");
  Serial.printf("  Setpoint: %.02f°C\n", TemperatureCabinet.getTemperatureSetpoint());
  Serial.printf("  Min Temperature: %.02f°C\n", TemperatureCabinet.getMinTemperature());
  Serial.printf("  Max Temperature: %.02f°C\n", TemperatureCabinet.getMaxTemperature());
  Serial.printf("  Step: %.02f°C\n", TemperatureCabinet.getStep());
}

void loop() {
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
