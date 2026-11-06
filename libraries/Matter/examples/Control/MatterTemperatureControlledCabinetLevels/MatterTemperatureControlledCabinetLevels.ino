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
 * begin() takes a uint8 array of application level values plus optional string labels.
 * Matter SupportedTemperatureLevels is a list of those strings. Hubs write
 * SelectedTemperatureLevel as an index into that list. set/getSelectedTemperatureLevel() still
 * use the uint8 values from the array, not the index.
 *
 * Values here are 10/20/30/40/50 so the value is not the same as the Matter index. Labels are
 * Off/Low/Medium/High/Max. Serial prints value, index, and hub name so you can tell them apart.
 * Label pointers are not copied; these string literals stay valid for the life of the firmware.
 *
 * This mode is mutually exclusive with temperature_number mode.
 * See MatterTemperatureControlledCabinet example for temperature setpoint control.
 *
 * Demo: after commission, the sketch cycles the level every 1 s (can fight the hub).
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

// Application level values (not 0..N-1, so they are distinct from the Matter list index).
// Hubs see the string list "Off" / "Low" / "Medium" / "High" / "Max" and write index 0..4.
uint8_t supportedLevels[] = {10, 20, 30, 40, 50};
const char *supportedLevelLabels[] = {"Off", "Low", "Medium", "High", "Max"};
const uint16_t levelCount = sizeof(supportedLevels) / sizeof(supportedLevels[0]);
const uint8_t initialLevel = 30;  // Medium — Matter stores this as index 2

// Returns the Matter list index for an application level value, or -1 if unknown.
int16_t indexOfLevel(uint8_t level) {
  for (uint16_t i = 0; i < levelCount; i++) {
    if (supportedLevels[i] == level) {
      return (int16_t)i;
    }
  }
  return -1;
}

void printSupportedLevels() {
  for (uint16_t i = 0; i < levelCount; i++) {
    Serial.printf("[%u]=%u(\"%s\")", i, supportedLevels[i], supportedLevelLabels[i]);
    if (i < levelCount - 1) {
      Serial.print(", ");
    }
  }
}

void printLevelMapping(const char *prefix, uint8_t level) {
  int16_t index = indexOfLevel(level);
  const char *label = (index >= 0) ? supportedLevelLabels[index] : "?";
  Serial.printf("%s value %u, Matter index %d, hub label \"%s\"\r\n", prefix, level, (int)index, label);
}

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
    int16_t index = indexOfLevel(currentLevel);
    levelState.currentLevelIndex = (index >= 0) ? (uint16_t)index : 0;
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
    printLevelMapping("Temperature level updated:", newLevel);
  } else {
    Serial.printf("Failed to update temperature level to value %u\r\n", newLevel);
  }
}

// Print current level status
void printLevelStatus() {
  printLevelMapping("Current temperature level:", TemperatureCabinet.getSelectedTemperatureLevel());
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

  // temperature_level mode: pass application values plus hub names. Matter stores
  // SelectedTemperatureLevel as the index of initialLevel in the array.
  // Mutually exclusive with temperature_number mode (see MatterTemperatureControlledCabinet).
  if (!TemperatureCabinet.begin(supportedLevels, supportedLevelLabels, levelCount, initialLevel)) {
    Serial.println("Failed to initialize Temperature Controlled Cabinet!");
    while (1) {
      delay(1000);
    }
  }

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();

  Serial.println("\nTemperature Controlled Cabinet Configuration (Temperature Level Mode):");
  Serial.printf("  Arduino getSelectedTemperatureLevel() = %u\r\n", TemperatureCabinet.getSelectedTemperatureLevel());
  Serial.printf("  Matter SelectedTemperatureLevel index = %d\r\n", (int)indexOfLevel(TemperatureCabinet.getSelectedTemperatureLevel()));
  Serial.printf("  SupportedTemperatureLevels count = %u\r\n", TemperatureCabinet.getSupportedTemperatureLevelsCount());
  Serial.print("  List [index]=value(\"hub label\"): ");
  printSupportedLevels();
  Serial.println();
  Serial.println("  Hub SetTemperature writes an index (0..4). Arduino setters/getters use the uint8 value.");
}

void loop() {
  matterRestartIfNoFabric();

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
