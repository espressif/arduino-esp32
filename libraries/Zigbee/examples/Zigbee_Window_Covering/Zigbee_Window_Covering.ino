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

/**
 * @brief This example demonstrates Zigbee Window Covering.
 *
 * The example demonstrates how to use Zigbee library to create a end device window covering device.
 * The window covering is a Zigbee end device, which is moving the blinds (lift+tilt) and reporting
 * its current position to the Zigbee network.
 *
 * Use setCoveringType() to set the type of covering (blind, shade, etc.).
 *
 * The example also demonstrates how to use the button to manually control the lift position.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by hennikul and Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeWindowCovering.h"

#define ZIGBEE_COVERING_ENDPOINT 10
#define BUTTON_PIN               9  // ESP32-C6/H2 Boot button

#define MAX_LIFT 200  // centimeters from open position (0-900)
#define MIN_LIFT 0

#define MAX_TILT 40  // centimeters from open position (0-900)
#define MIN_TILT 0

uint16_t currentLift = MAX_LIFT;
uint8_t currentLiftPercentage = 100;

uint16_t currentTilt = MAX_TILT;
uint8_t currentTiltPercentage = 100;

ZigbeeWindowCovering zbCovering = ZigbeeWindowCovering(ZIGBEE_COVERING_ENDPOINT);

void setup() {
  Serial.begin(115200);

  // Init button for factory reset
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Optional: set Zigbee device name and model
  zbCovering.setManufacturerAndModel("Espressif", "WindowBlinds");

  // Set proper covering type, it defines which attributes are available
  zbCovering.setCoveringType(BLIND_LIFT_AND_TILT);

  // Set configuration: operational, online, not commands_reversed, lift / tilt closed_loop, lift / tilt encoder_controlled
  zbCovering.setConfigStatus(true, true, false, true, true, true, true);

  // Set mode: not motor_reversed, calibration_mode, not maintenance_mode, not leds_on
  zbCovering.setMode(false, true, false, false);

  // Set limits of motion
  zbCovering.setLimits(MIN_LIFT, MAX_LIFT, MIN_TILT, MAX_TILT);

  // Set callback function for open, close, filt and tilt change, stop
  zbCovering.onOpen(fullOpen);
  zbCovering.onClose(fullClose);
  zbCovering.onGoToLiftPercentage(goToLiftPercentage);
  zbCovering.onGoToTiltPercentage(goToTiltPercentage);
  zbCovering.onStop(stopMotor);

  // Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeWindowCovering endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbCovering);

  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  Serial.println("Calling Zigbee.begin()");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Set initial position
  zbCovering.setLiftPercentage(currentLiftPercentage);
  zbCovering.setTiltPercentage(currentTiltPercentage);
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(BUTTON_PIN) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.printf("Resetting Zigbee to factory settings, reboot.\n");
        Zigbee.factoryReset();
        delay(30000);
      }
    }
    // Manual lift control simulation by pressing button
    manualControl();
  }
  delay(500);
}

void fullOpen() {
  /* This is where you would trigger your motor to go to full open state, currentLift should
     be updated until full open has been reached in order to provide feedback to controller of actual position
     The stop can be always called, so the movement can be stopped at any time */

  // Our cover updates instantly!
  currentLift = MAX_LIFT;
  currentLiftPercentage = 100;
  Serial.println("Opening cover");
  // Update the current position
  zbCovering.setLiftPercentage(currentLiftPercentage);
}

void fullClose() {
  /* This is where you would trigger your motor to go to full close state, currentLift should
     be updated until full close has been reached in order to provide feedback to controller of actual position
     The stop can be always called, so the movement can be stopped at any time */

  // Our cover updates instantly!
  currentLift = MIN_LIFT;
  currentLiftPercentage = 0;
  Serial.println("Closing cover");
  // Update the current position
  zbCovering.setLiftPercentage(currentLiftPercentage);
}

void goToLiftPercentage(uint8_t liftPercentage) {
  /* This is where you would trigger your motor to go towards liftPercentage, currentLift should
     be updated until liftPercentage has been reached in order to provide feedback to controller */

  // Our simulated cover updates instantly!
  currentLift = (liftPercentage * MAX_LIFT) / 100;
  currentLiftPercentage = liftPercentage;
  Serial.printf("New requested lift from Zigbee: %d (%d)\n", currentLift, liftPercentage);

  // Update the current position
  zbCovering.setLiftPercentage(currentLiftPercentage);  //or setLiftPosition()
}

void goToTiltPercentage(uint8_t tiltPercentage) {
  /* This is where you would trigger your motor to go towards tiltPercentage, currentTilt should
     be updated until tiltPercentage has been reached in order to provide feedback to controller */

  // Our simulated cover updates instantly!
  currentTilt = (tiltPercentage * MAX_TILT) / 100;
  currentTiltPercentage = tiltPercentage;
  Serial.printf("New requested tilt from Zigbee: %d (%d)\n", currentTilt, tiltPercentage);

  // Update the current position
  zbCovering.setTiltPercentage(currentTiltPercentage);  //or setTiltPosition()
}

void stopMotor() {
  // Motor can be stopped while moving cover toward current target, when stopped the actual position should be updated
  Serial.println("Stopping motor");
  // Update the current position of both lift and tilt
  zbCovering.setLiftPercentage(currentLiftPercentage);
  zbCovering.setTiltPercentage(currentTiltPercentage);
}

void manualControl() {
  // Simulate lift percentage move by increasing it by 20% each time
  currentLiftPercentage += 20;
  if (currentLiftPercentage > 100) {
    currentLiftPercentage = 0;
  }
  zbCovering.setLiftPercentage(currentLiftPercentage);
  // Also setLiftPosition() can be used to set the exact position instead of percentage
}
