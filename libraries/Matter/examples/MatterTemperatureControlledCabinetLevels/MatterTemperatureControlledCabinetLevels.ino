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
 * This example demonstrates the Temperature Level mode of the Matter Temperature Controlled Cabinet Device.
 *
 * This example will create a Matter Device which can be commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 *
 * The example will create a Matter Temperature Controlled Cabinet Device using temperature_level feature.
 * The Temperature Controlled Cabinet can be controlled via Matter controllers to set
 * temperature levels from a predefined array of supported levels.
 *
 * This mode is mutually exclusive with temperature_number mode.
 * See MatterTemperatureControlledCabinet example for temperature setpoint control.
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

// Button control - decommission the Matter Node
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Temperature levels array - these represent different temperature presets
// Example: 0 = Off, 1 = Low, 2 = Medium, 3 = High, 4 = Maximum
// The actual temperature values are application-specific
uint8_t supportedLevels[] = {0, 1, 2, 3, 4};
const uint16_t levelCount = sizeof(supportedLevels) / sizeof(supportedLevels[0]);
const uint8_t initialLevel = 2;  // Start with level 2 (Medium)

// Temperature level control state
struct LevelControlState {
  bool initialized;
  bool increasing;
  uint16_t currentLevelIndex;
  uint8_t initialLevel;
  bool levelReachedIncreasing;
  bool levelReachedDecreasing;
};

static LevelControlState levelState = {
  .initialized = false, .increasing = true, .currentLevelIndex = 0, .initialLevel = 0, .levelReachedIncreasing = false, .levelReachedDecreasing = false
};

// Initialize level control state
void initLevelControl() {
  if (!levelState.initialized) {
    uint8_t currentLevel = TemperatureCabinet.getSelectedTemperatureLevel();
    levelState.initialLevel = currentLevel;
    // Find the index of current level in supportedLevels array
    for (uint16_t i = 0; i < levelCount; i++) {
      if (supportedLevels[i] == currentLevel) {
        levelState.currentLevelIndex = i;
        break;
      }
    }
    levelState.initialized = true;
  }
}

// Check and log when initial level is reached/overpassed
void checkLevelReached(uint8_t newLevel, bool isIncreasing, bool directionChanged) {
  if (directionChanged) {
    // Reset flags when direction changes
    levelState.levelReachedIncreasing = false;
    levelState.levelReachedDecreasing = false;
    return;
  }

  if (isIncreasing && !levelState.levelReachedIncreasing && newLevel >= levelState.initialLevel) {
    Serial.printf("*** Temperature level %u reached/overpassed while increasing ***\r\n", levelState.initialLevel);
    levelState.levelReachedIncreasing = true;
  } else if (!isIncreasing && !levelState.levelReachedDecreasing && newLevel <= levelState.initialLevel) {
    Serial.printf("*** Temperature level %u reached/overpassed while decreasing ***\r\n", levelState.initialLevel);
    levelState.levelReachedDecreasing = true;
  }
}

// Update temperature level with cycling logic
void updateTemperatureLevel() {
  // Cycle through supported levels in both directions
  bool directionChanged = false;

  if (levelState.increasing) {
    levelState.currentLevelIndex++;
    if (levelState.currentLevelIndex >= levelCount) {
      levelState.currentLevelIndex = levelCount - 1;
      levelState.increasing = false;  // Reverse direction
      directionChanged = true;
    }
  } else {
    if (levelState.currentLevelIndex == 0) {
      levelState.currentLevelIndex = 0;
      levelState.increasing = true;  // Reverse direction
      directionChanged = true;
    } else {
      levelState.currentLevelIndex--;
    }
  }

  uint8_t newLevel = supportedLevels[levelState.currentLevelIndex];

  // Check if initial level has been reached or overpassed
  checkLevelReached(newLevel, levelState.increasing, directionChanged);

  // Update the temperature level
  if (TemperatureCabinet.setSelectedTemperatureLevel(newLevel)) {
    Serial.printf("Temperature level updated to: %u (Supported Levels: ", newLevel);
    for (uint16_t i = 0; i < levelCount; i++) {
      Serial.printf("%u", supportedLevels[i]);
      if (i < levelCount - 1) {
        Serial.print(", ");
      }
    }
    Serial.println(")");
  } else {
    Serial.printf("Failed to update temperature level to: %u\r\n", newLevel);
  }
}

// Print current level status
void printLevelStatus() {
  uint8_t currentLevel = TemperatureCabinet.getSelectedTemperatureLevel();
  Serial.printf("Current Temperature Level: %u (Supported Levels: ", currentLevel);
  for (uint16_t i = 0; i < levelCount; i++) {
    Serial.printf("%u", supportedLevels[i]);
    if (i < levelCount - 1) {
      Serial.print(", ");
    }
  }
  Serial.println(")");
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
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
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

  // Initialize Temperature Controlled Cabinet with temperature_level feature:
  // - supportedLevels: Array of temperature level values (0-255)
  // - levelCount: Number of levels in the array
  // - initialLevel: Initial selected temperature level
  //
  // Note: This mode is mutually exclusive with temperature_number mode.
  // See MatterTemperatureControlledCabinet example for temperature setpoint control.
  if (!TemperatureCabinet.begin(supportedLevels, levelCount, initialLevel)) {
    Serial.println("Failed to initialize Temperature Controlled Cabinet!");
    while (1) {
      delay(1000);
    }
  }

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
  Serial.println("\nTemperature Controlled Cabinet Configuration (Temperature Level Mode):");
  Serial.printf("  Selected Level: %u\n", TemperatureCabinet.getSelectedTemperatureLevel());
  Serial.printf("  Supported Levels Count: %u\n", TemperatureCabinet.getSupportedTemperatureLevelsCount());
  Serial.print("  Supported Levels: ");
  for (uint16_t i = 0; i < levelCount; i++) {
    Serial.printf("%u", supportedLevels[i]);
    if (i < levelCount - 1) {
      Serial.print(", ");
    }
  }
  Serial.println();
}

void loop() {
  static uint32_t timeCounter = 0;
  static uint32_t lastUpdateTime = 0;

  // Initialize level control state on first run
  initLevelControl();

  // Update temperature level dynamically every 1 second
  uint32_t currentTime = millis();
  if (currentTime - lastUpdateTime >= 1000) {  // 1 second interval
    lastUpdateTime = currentTime;
    updateTemperatureLevel();
  }

  // Print the current temperature level every 5s
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    printLevelStatus();
  }

  // Handle button press for decommissioning
  handleButtonPress();

  delay(500);
}
