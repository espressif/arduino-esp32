// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief This example demonstrates OTA support on light bulb.
 *
 * The example demonstrates how to use Zigbee library to create a end device light bulb with OTA support.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee light bulb configuration */
#define ZIGBEE_LIGHT_ENDPOINT 1
uint8_t led = RGB_BUILTIN;
uint8_t button = BOOT_PIN;

/* Zigbee OTA configuration
 * These #defines are also read by tools/zb_ota_image.py when you use
 * Sketch -> Export compiled Binary (with Tools -> Zigbee Mode enabled).
 * Bump OTA_UPGRADE_RUNNING_FILE_VERSION for each new image you export for ZHA.
 */
#define OTA_UPGRADE_MANUFACTURER         0x1001
#define OTA_UPGRADE_IMAGE_TYPE           0x1011
#define OTA_UPGRADE_RUNNING_FILE_VERSION 0x01010100  // Version of this firmware (used as OTA file version on Export)
// DownloadedFileVersion: 0xFFFFFFFF = none stored. Must NOT equal the ZHA image version or the
// stack silently skips Image Block Request after Query approve (no START / RECEIVING).
#define OTA_UPGRADE_DOWNLOADED_FILE_VERSION 0xFFFFFFFF
#define OTA_UPGRADE_HW_VERSION              0x0101

/* Print running FW version on Serial every N ms (easy to see after OTA) */
#define FW_VERSION_PRINT_INTERVAL_MS 10000

ZigbeeLight zbLight = ZigbeeLight(ZIGBEE_LIGHT_ENDPOINT);

volatile bool otaRunning = false;

/********************* Callbacks *************************/
void otaActiveCallback(bool otaActive) {
  otaRunning = otaActive;
  if (otaActive) {
    Serial.println("OTA started");
  }
}

void otaCompleteCallback(bool success) {
  Serial.println(success ? "OTA finished" : "OTA aborted");
}

/********************* RGB LED functions **************************/
void setLED(bool value) {
  digitalWrite(led, value);
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init LED and turn it OFF (if LED_PIN == RGB_BUILTIN, the rgbLedWrite() will be used under the hood)
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Initialize Zigbee stack as end device
  if (!Zigbee.role(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Optional: set Zigbee device name and model
  zbLight.setManufacturerAndModel("Espressif", "ZBLightBulb");

  // Set callback function for light change
  zbLight.onLightChange(setLED);

  // Add OTA client to the light bulb
  zbLight.addOTAClient(
    OTA_UPGRADE_RUNNING_FILE_VERSION, OTA_UPGRADE_DOWNLOADED_FILE_VERSION, OTA_UPGRADE_HW_VERSION, OTA_UPGRADE_MANUFACTURER, OTA_UPGRADE_IMAGE_TYPE
  );

  // Optional: OTA active flag + distinct finished vs aborted
  zbLight.onOTAStateChange(otaActiveCallback);
  zbLight.onOTAComplete(otaCompleteCallback);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbLight);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Discover the OTA server and Query Next Image; further queries every hour. ZHA Update also works via Image Notify.
  zbLight.requestOTAUpdate();
}

void loop() {
  static uint32_t last_fw_print_ms = 0;
  if (millis() - last_fw_print_ms >= FW_VERSION_PRINT_INTERVAL_MS) {
    last_fw_print_ms = millis();
    Serial.printf("Running FW version: 0x%08lX\n", (unsigned long)OTA_UPGRADE_RUNNING_FILE_VERSION);
  }

  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        if (otaRunning) {
          Serial.println("OTA in progress, cannot reset now");
          break;
        }
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    // Toggle light by pressing the button
    zbLight.setLight(!zbLight.getLightState());
  }
  delay(100);
}
