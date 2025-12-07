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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/window-covering-server/window-covering-server.h>

using namespace chip::app::Clusters;

// Matter Window Covering endpoint with Lift and Tilt control

class MatterWindowCovering : public MatterEndPoint {
public:
  // Window Covering Type constants (from Matter spec)
  enum WindowCoveringType_t {
    ROLLERSHADE = (uint8_t) WindowCovering::Type::kRollerShade,                                  // Roller Shade (LIFT support)
    ROLLERSHADE_2_MOTOR = (uint8_t) WindowCovering::Type::kRollerShade2Motor,                    // Roller Shade 2 Motor (LIFT support)
    ROLLERSHADE_EXTERIOR = (uint8_t) WindowCovering::Type::kRollerShadeExterior,                 // Roller Shade Exterior (LIFT support)
    ROLLERSHADE_EXTERIOR_2_MOTOR = (uint8_t) WindowCovering::Type::kRollerShadeExterior2Motor,   // Roller Shade Exterior 2 Motor (LIFT support)
    DRAPERY = (uint8_t) WindowCovering::Type::kDrapery,                                          // Drapery (LIFT support)
    AWNING = (uint8_t) WindowCovering::Type::kAwning,                                            // Awning (LIFT support)
    SHUTTER = (uint8_t) WindowCovering::Type::kShutter,                                          // Shutter (TILT support)
    BLIND_TILT_ONLY = (uint8_t) WindowCovering::Type::kTiltBlindTiltOnly,                        // Blind Tilt Only (TILT support)
    BLIND_LIFT_AND_TILT = (uint8_t) WindowCovering::Type::kTiltBlindLiftAndTilt,                 // Blind Lift and Tilt (LIFT and TILT support)
    PROJECTOR_SCREEN = (uint8_t) WindowCovering::Type::kProjectorScreen,                         // Projector Screen (LIFT support)
  };

  // Operational State constants (from Matter spec)
  enum OperationalState_t {
    STALL = (uint8_t)WindowCovering::OperationalState::Stall,                                    // Currently not moving
    MOVING_UP_OR_OPEN = (uint8_t)WindowCovering::OperationalState::MovingUpOrOpen,               // Is currently opening
    MOVING_DOWN_OR_CLOSE = (uint8_t)WindowCovering::OperationalState::MovingDownOrClose,         // Is currently closing
  };

  // Operational Status field constants (from Matter spec)
  enum OperationalStatusField_t {
    GLOBAL = (uint8_t) WindowCovering::OperationalStatus::kGlobal,                              // Global operational state field 0x03 (bits 0-1)
    LIFT = (uint8_t) WindowCovering::OperationalStatus::kLift,                                  // Lift operational state field 0x0C (bits 2-3)
    TILT = (uint8_t) WindowCovering::OperationalStatus::kTilt,                                  // Tilt operational state field 0x30 (bits 4-5)
  };

  MatterWindowCovering();
  ~MatterWindowCovering();
  virtual bool begin(uint8_t liftPercent = 100, uint8_t tiltPercent = 0, WindowCoveringType_t coveringType = ROLLERSHADE);
  void end();  // this will just stop processing Matter events

  // Lift position control
  bool setLiftPosition(uint16_t liftPosition);
  uint16_t getLiftPosition();
  bool setLiftPercentage(uint8_t liftPercent);
  uint8_t getLiftPercentage();
  bool setTargetLiftPercent100ths(uint16_t liftPercent100ths);
  uint16_t getTargetLiftPercent100ths();

  // Lift limit control
  bool setInstalledOpenLimitLift(uint16_t openLimit);
  uint16_t getInstalledOpenLimitLift();
  bool setInstalledClosedLimitLift(uint16_t closedLimit);
  uint16_t getInstalledClosedLimitLift();

  // Tilt position control
  bool setTiltPosition(uint16_t tiltPosition);
  uint16_t getTiltPosition();
  bool setTiltPercentage(uint8_t tiltPercent);
  uint8_t getTiltPercentage();
  bool setTargetTiltPercent100ths(uint16_t tiltPercent100ths);
  uint16_t getTargetTiltPercent100ths();

  // Tilt limit control
  bool setInstalledOpenLimitTilt(uint16_t openLimit);
  uint16_t getInstalledOpenLimitTilt();
  bool setInstalledClosedLimitTilt(uint16_t closedLimit);
  uint16_t getInstalledClosedLimitTilt();

  // Window covering type
  bool setCoveringType(WindowCoveringType_t coveringType);
  WindowCoveringType_t getCoveringType();

  // Operational status control (full bitmap)
  bool setOperationalStatus(uint8_t operationalStatus);
  uint8_t getOperationalStatus();

  // Operational state control (individual fields)
  bool setOperationalState(OperationalStatusField_t field, OperationalState_t state);
  OperationalState_t getOperationalState(OperationalStatusField_t field);

  // User Callback for whenever the window covering is opened
  using EndPointOpenCB = std::function<bool()>;
  void onOpen(EndPointOpenCB onChangeCB) {
    _onOpenCB = onChangeCB;
  }

  // User Callback for whenever the window covering is closed
  using EndPointCloseCB = std::function<bool()>;
  void onClose(EndPointCloseCB onChangeCB) {
    _onCloseCB = onChangeCB;
  }

  // User Callback for whenever the lift percentage is changed
  using EndPointLiftCB = std::function<bool(uint8_t)>;
  void onGoToLiftPercentage(EndPointLiftCB onChangeCB) {
    _onGoToLiftPercentageCB = onChangeCB;
  }

  // User Callback for whenever the tilt percentage is changed
  using EndPointTiltCB = std::function<bool(uint8_t)>;
  void onGoToTiltPercentage(EndPointTiltCB onChangeCB) {
    _onGoToTiltPercentageCB = onChangeCB;
  }

  // User Callback for whenever the window covering is stopped
  using EndPointStopCB = std::function<bool()>;
  void onStop(EndPointStopCB onChangeCB) {
    _onStopCB = onChangeCB;
  }

  // User Callback for whenever any parameter is changed
  using EndPointCB = std::function<bool(uint8_t, uint8_t)>;
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  // used to update the state of the window covering using the current Matter internal state
  // It is necessary to set a user callback function using onChange() to handle the physical window covering state
  void updateAccessory();

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  uint8_t currentLiftPercent = 0;
  uint16_t currentLiftPosition = 0;
  uint8_t currentTiltPercent = 0;
  uint16_t currentTiltPosition = 0;
  WindowCoveringType_t coveringType = ROLLERSHADE;

  EndPointOpenCB _onOpenCB = NULL;
  EndPointCloseCB _onCloseCB = NULL;
  EndPointLiftCB _onGoToLiftPercentageCB = NULL;
  EndPointTiltCB _onGoToTiltPercentageCB = NULL;
  EndPointStopCB _onStopCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */

