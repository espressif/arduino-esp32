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

// Matter Simple Blinds Example
// Lift-only roller shade. The hub writes Target; this sketch simulates a
// motor at 1% every 200 ms and reports Current + Opening/Closing/Stall.

#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Window Covering Endpoint
MatterWindowCovering WindowBlinds;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// Matter percent: 0 = fully open, 100 = fully closed. begin() starts closed.
static const uint8_t kSimStepPercent = 1;
static const uint32_t kSimStepMs = 200;

static volatile uint8_t simCurrent = 100;
static volatile uint8_t simTarget = 100;
static volatile bool simMoving = false;
static uint32_t simLastStepMs = 0;

static bool setLiftDirection(uint8_t fromPercent, uint8_t toPercent) {
  if (toPercent > fromPercent) {
    Serial.printf("Closing: %u%% -> %u%%\r\n", fromPercent, toPercent);
    return WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::MOVING_DOWN_OR_CLOSE);
  }
  if (toPercent < fromPercent) {
    Serial.printf("Opening: %u%% -> %u%%\r\n", fromPercent, toPercent);
    return WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::MOVING_UP_OR_OPEN);
  }
  Serial.printf("Lift already %u%%\r\n", toPercent);
  if (!WindowBlinds.setCurrentLiftPercent100ths(WindowBlinds.getTargetLiftPercent100ths())) {
    return false;
  }
  simMoving = false;
  return WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);
}

// PRE_UPDATE: liftPercent / getTargetLiftPercent100ths() are the new request.
// Returning true accepts Target. loop() reports Current while the shade moves.
bool onBlindsLift(uint8_t liftPercent) {
  Serial.printf("Window Covering change request: Lift=%u%%\r\n", liftPercent);
  const uint8_t fromPercent = simCurrent;
  simTarget = liftPercent;
  if (!setLiftDirection(fromPercent, liftPercent)) {
    return false;
  }
  if (fromPercent != liftPercent) {
    simMoving = true;
    simLastStepMs = millis();
  }
  return true;
}

static void simulateLiftStep() {
  if (!simMoving) {
    return;
  }
  const uint32_t now = millis();
  if ((now - simLastStepMs) < kSimStepMs) {
    return;
  }
  simLastStepMs = now;

  uint8_t current = simCurrent;
  const uint8_t target = simTarget;
  if (current < target) {
    current = (uint8_t)(current + kSimStepPercent);
    if (current > target) {
      current = target;
    }
  } else if (current > target) {
    current = (uint8_t)(current - kSimStepPercent);
    if (current < target) {
      current = target;
    }
  }
  simCurrent = current;

  if (!WindowBlinds.setCurrentLiftPercent100ths((uint16_t)current * 100)) {
    return;
  }
  if (current != target) {
    return;
  }

  // Last step: match Target 100ths so CHIP / Alexa leave Opening/Closing.
  WindowBlinds.setCurrentLiftPercent100ths(WindowBlinds.getTargetLiftPercent100ths());
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);
  simMoving = false;
  Serial.printf("Lift reached %u%%\r\n", current);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n============================");
  Serial.println("Matter Simple Blinds Example");
  Serial.println("============================\n");

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Window Covering endpoint
  // Using ROLLERSHADE type (lift only, no tilt)
  WindowBlinds.begin(100, 0, MatterWindowCovering::ROLLERSHADE);
  simCurrent = WindowBlinds.getLiftPercentage();
  simTarget = simCurrent;

  // Set up the onGoToLiftPercentage callback - this handles all window covering changes requested by the Matter Controller
  WindowBlinds.onGoToLiftPercentage(onBlindsLift);

  // Start Matter
  matterSetExampleIdentity("Window Covering");
  Matter.begin();
  matterWaitUntilReady();
  Serial.println("Matter started");
}

void loop() {
  matterRestartIfNoFabric();
  simulateLiftStep();
}
