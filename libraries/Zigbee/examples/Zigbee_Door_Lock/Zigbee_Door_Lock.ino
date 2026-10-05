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
 * @brief This example demonstrates a simple Zigbee door lock.
 *
 * The example demonstrates how to use Zigbee library to create an end device door lock.
 * The door lock is a Zigbee end device, which is controlled by a Zigbee coordinator (Lock / Unlock commands)
 * and reports its LockState attribute to the Zigbee network.
 *
 * The LED shows the current state (green = locked/secured, red = unlocked/unsecured).
 * A short press of the BOOT button simulates a local operation of the lock (toggles the state and reports it),
 * a long press (more than 3 seconds) performs a factory reset of the Zigbee stack.
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

/* Zigbee door lock configuration */
#define ZIGBEE_DOOR_LOCK_ENDPOINT 1

#ifdef RGB_BUILTIN
uint8_t led = RGB_BUILTIN;  // To demonstrate the current lock state
#else
uint8_t led = 2;
#endif

uint8_t button = BOOT_PIN;

ZigbeeDoorLock zbDoorLock = ZigbeeDoorLock(ZIGBEE_DOOR_LOCK_ENDPOINT);

/********************* LED indication **************************/
void showLockState(bool locked) {
  if (locked) {
    rgbLedWrite(led, 0, 64, 0);  // Green - locked (secured)
  } else {
    rgbLedWrite(led, 64, 0, 0);  // Red - unlocked (unsecured)
  }
}

/********************* Door lock callbacks **************************/
// Called when Lock Door command is received from the Zigbee network.
// Return true when the lock was successfully locked, false to reject the command.
bool lockDoor() {
  /* This is where you would drive your lock mechanism (motor, relay, ...) */
  Serial.println("Zigbee command: lock door");
  showLockState(true);
  return true;
}

// Called when Unlock Door command is received from the Zigbee network.
// Return true when the lock was successfully unlocked, false to reject the command.
bool unlockDoor() {
  /* This is where you would drive your lock mechanism (motor, relay, ...) */
  Serial.println("Zigbee command: unlock door");
  showLockState(false);
  return true;
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init LED that will be used to indicate the current lock state
  rgbLedWrite(led, 0, 0, 0);

  // Init button for local lock operation and factory reset
  pinMode(button, INPUT_PULLUP);

  // Initialize Zigbee stack as end device
  if (!Zigbee.role(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Optional: set Zigbee device name and model
  zbDoorLock.setManufacturerAndModel("Espressif", "ZBDoorLock");

  // Optional: set the lock type (default is dead bolt)
  zbDoorLock.setLockType(DOOR_LOCK_TYPE_DEAD_BOLT);

  // Set callback functions for the lock and unlock commands
  zbDoorLock.onLock(lockDoor);
  zbDoorLock.onUnlock(unlockDoor);

  // Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbDoorLock);

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

  // Set and report the initial state of the lock
  zbDoorLock.setLocked();
  showLockState(true);
}

void loop() {
  // Checking button for local lock operation and factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    // Short press: simulate local operation of the lock
    if (zbDoorLock.isLocked()) {
      Serial.println("Local operation: unlock door");
      zbDoorLock.setUnlocked();
      showLockState(false);
    } else {
      Serial.println("Local operation: lock door");
      zbDoorLock.setLocked();
      showLockState(true);
    }
  }
  delay(100);
}
