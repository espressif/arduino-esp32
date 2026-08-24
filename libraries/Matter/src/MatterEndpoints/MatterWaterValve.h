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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <functional>

using namespace chip::app::Clusters;

// Matter Water Valve endpoint (device type 0x0042) - Valve Configuration and Control cluster.
//
// CurrentState/TargetState/OpenDuration/RemainingDuration are managed by the CHIP cluster
// implementation itself, so they cannot be read/written through
// getAttributeVal()/setAttributeVal()/updateAttributeVal(). Instead, Open/Close commands (whether
// they come from a Matter controller or from calling open()/close() locally) are routed through a
// Delegate that bridges into the onOpen()/onClose() user callbacks, and the internal countdown for
// a timed open automatically ticks RemainingDuration down to 0 and closes the valve - no manual
// countdown handling is required in the sketch.
class MatterWaterValve : public MatterEndPoint {
public:
  // ValveStateEnum values (from Matter spec)
  enum ValveState_t {
    VALVE_STATE_CLOSED = (uint8_t)ValveConfigurationAndControl::ValveStateEnum::kClosed,
    VALVE_STATE_OPEN = (uint8_t)ValveConfigurationAndControl::ValveStateEnum::kOpen,
    VALVE_STATE_TRANSITIONING = (uint8_t)ValveConfigurationAndControl::ValveStateEnum::kTransitioning,
  };

  // ValveFaultBitmap values (from Matter spec) - combine with bitwise OR
  enum ValveFault_t {
    VALVE_FAULT_GENERAL_FAULT = (uint16_t)ValveConfigurationAndControl::ValveFaultBitmap::kGeneralFault,
    VALVE_FAULT_BLOCKED = (uint16_t)ValveConfigurationAndControl::ValveFaultBitmap::kBlocked,
    VALVE_FAULT_LEAKING = (uint16_t)ValveConfigurationAndControl::ValveFaultBitmap::kLeaking,
    VALVE_FAULT_NOT_CONNECTED = (uint16_t)ValveConfigurationAndControl::ValveFaultBitmap::kNotConnected,
    VALVE_FAULT_SHORT_CIRCUIT = (uint16_t)ValveConfigurationAndControl::ValveFaultBitmap::kShortCircuit,
    VALVE_FAULT_CURRENT_EXCEEDED = (uint16_t)ValveConfigurationAndControl::ValveFaultBitmap::kCurrentExceeded,
  };

  MatterWaterValve();
  ~MatterWaterValve();
  // begin Matter Water Valve endpoint. defaultOpenDurationSeconds (0 = none) is reported as the
  // DefaultOpenDuration attribute and used by open() (no argument) when set.
  bool begin(uint32_t defaultOpenDurationSeconds = 0);
  // this will just stop processing Water Valve Matter events
  void end();

  // Opens the valve indefinitely, or for defaultOpenDurationSeconds (set in begin()) if configured.
  bool open();
  // Opens the valve for durationSeconds, after which it closes automatically (0 = open indefinitely).
  bool open(uint32_t durationSeconds);
  // Closes the valve.
  bool close();

  // returns the valve's current state
  ValveState_t getCurrentState() {
    return currentState;
  }
  // returns the valve's target state
  ValveState_t getTargetState() {
    return targetState;
  }
  // returns true if the valve is currently open
  bool isOpen() {
    return currentState == VALVE_STATE_OPEN;
  }

  // returns the duration in seconds used for the current/last timed open operation (0 = opened indefinitely)
  uint32_t getOpenDuration() {
    return openDuration;
  }
  // returns the default open duration in seconds used by open() when no explicit duration is given (0 = none configured)
  uint32_t getDefaultOpenDuration() {
    return defaultOpenDuration;
  }
  // returns the remaining duration in seconds of the current timed open operation, updated automatically once per second
  uint32_t getRemainingDuration() {
    return remainingDuration;
  }

  // returns the valve's currently reported fault bitmap (combination of ValveFault_t values)
  uint16_t getValveFault() {
    return valveFault;
  }
  // reports a fault condition (combination of ValveFault_t values, 0 to clear)
  bool setValveFault(uint16_t fault);

  // User Callback for whenever the valve is commanded open, either by a Matter controller or by calling open()
  // locally. It should perform the physical action; return false to signal it could not be completed.
  using EndPointOpenCB = std::function<bool()>;
  void onOpen(EndPointOpenCB onOpenCB) {
    _onOpenCB = onOpenCB;
  }

  // User Callback for whenever the valve is commanded closed, either by a Matter controller, by calling close()
  // locally, or automatically when a timed open operation elapses. It should perform the physical action.
  using EndPointCloseCB = std::function<bool()>;
  void onClose(EndPointCloseCB onCloseCB) {
    _onCloseCB = onCloseCB;
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  ValveState_t currentState = VALVE_STATE_CLOSED;
  ValveState_t targetState = VALVE_STATE_CLOSED;
  uint32_t openDuration = 0;
  uint32_t defaultOpenDuration = 0;
  uint32_t remainingDuration = 0;
  uint16_t valveFault = 0;

  EndPointOpenCB _onOpenCB = NULL;
  EndPointCloseCB _onCloseCB = NULL;

private:
  // Bridges the Valve Configuration and Control cluster's Delegate interface to this class; defined in the .cpp
  // so callers don't need the CHIP cluster delegate headers. Declared as a nested class so it can reach the
  // protected/private members above directly.
  class ValveDelegate;
  ValveDelegate *delegate = nullptr;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
