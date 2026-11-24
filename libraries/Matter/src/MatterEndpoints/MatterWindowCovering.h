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

/**
 * @brief Matter window covering endpoint with lift and/or tilt control (blinds, shades).
 *
 * Matter percentage semantics: 0 = fully open, 100 = fully closed. @c begin()
 * advertises only the features that type actually has. Unused lift or tilt
 * arguments and calibration are ignored. Lift setters return false on
 * shutter/tilt-only types; tilt setters return false on lift-only types.
 */
class MatterWindowCovering : public MatterEndPoint {
public:
  /**
   * @brief Window covering type. Determines which features (lift, tilt, or both) are enabled at begin().
   */
  enum WindowCoveringType_t {
    ROLLERSHADE = (uint8_t)WindowCovering::Type::kRollerShade,                                 ///< Roller Shade (lift).
    ROLLERSHADE_2_MOTOR = (uint8_t)WindowCovering::Type::kRollerShade2Motor,                   ///< Roller Shade 2 Motor (lift).
    ROLLERSHADE_EXTERIOR = (uint8_t)WindowCovering::Type::kRollerShadeExterior,                ///< Roller Shade Exterior (lift).
    ROLLERSHADE_EXTERIOR_2_MOTOR = (uint8_t)WindowCovering::Type::kRollerShadeExterior2Motor,  ///< Roller Shade Exterior 2 Motor (lift).
    DRAPERY = (uint8_t)WindowCovering::Type::kDrapery,                                         ///< Drapery (lift).
    AWNING = (uint8_t)WindowCovering::Type::kAwning,                                           ///< Awning (lift).
    SHUTTER = (uint8_t)WindowCovering::Type::kShutter,                                         ///< Shutter (tilt).
    BLIND_TILT_ONLY = (uint8_t)WindowCovering::Type::kTiltBlindTiltOnly,                       ///< Blind Tilt Only (tilt).
    BLIND_LIFT_AND_TILT = (uint8_t)WindowCovering::Type::kTiltBlindLiftAndTilt,                ///< Blind Lift and Tilt (lift and tilt).
    PROJECTOR_SCREEN = (uint8_t)WindowCovering::Type::kProjectorScreen,                        ///< Projector Screen (lift).
  };

  /**
   * @brief Operational state for GLOBAL / LIFT / TILT fields.
   */
  enum OperationalState_t {
    STALL = (uint8_t)WindowCovering::OperationalState::Stall,                             ///< Currently not moving.
    MOVING_UP_OR_OPEN = (uint8_t)WindowCovering::OperationalState::MovingUpOrOpen,        ///< Currently opening.
    MOVING_DOWN_OR_CLOSE = (uint8_t)WindowCovering::OperationalState::MovingDownOrClose,  ///< Currently closing.
  };

  /**
   * @brief Operational status bitmap fields. GLOBAL cannot be set directly.
   */
  enum OperationalStatusField_t {
    GLOBAL = (uint8_t)WindowCovering::OperationalStatus::kGlobal,  ///< Global operational state (bits 0–1).
    LIFT = (uint8_t)WindowCovering::OperationalStatus::kLift,      ///< Lift operational state (bits 2–3).
    TILT = (uint8_t)WindowCovering::OperationalStatus::kTilt,      ///< Tilt operational state (bits 4–5).
  };

  /**
   * @brief Local motor range used to convert between physical motor units and Matter percentages.
   *
   * Stored in firmware only; not published as a Matter cluster attribute in ESP-Matter 1.5.
   */
  struct PositionCalibration {
    uint16_t open = 0;       ///< Physical units when fully open.
    uint16_t closed = 65534; ///< Physical units when fully closed.
  };

  /**
   * @brief Create a new Matter window covering endpoint.
   */
  MatterWindowCovering();
  /**
   * @brief Destroy the window covering endpoint object.
   */
  ~MatterWindowCovering();
  /**
   * @brief Initialize the Matter window covering endpoint.
   *
   * @param liftPercent Initial lift percentage (0–100, default 0 = fully open).
   * @param tiltPercent Initial tilt percentage (0–100, default 0 = fully open).
   * @param coveringType Window covering type (default ROLLERSHADE). Determines lift and/or tilt features.
   * @param liftCalibration Optional local motor range for lift. Not a Matter attribute in ESP-Matter 1.5.
   * @param tiltCalibration Optional local motor range for tilt. Not a Matter attribute in ESP-Matter 1.5.
   * @return true if successful, false otherwise.
   * @note Matter percentage semantics: 0 = fully open, 100 = fully closed.
   */
  virtual bool begin(
    uint8_t liftPercent = 0, uint8_t tiltPercent = 0, WindowCoveringType_t coveringType = ROLLERSHADE, const PositionCalibration *liftCalibration = nullptr,
    const PositionCalibration *tiltCalibration = nullptr
  );
  /**
   * @brief Stop processing Matter window covering events.
   */
  void end();

  /**
   * @brief Set lift position in local motor units (converted via calibration).
   * @param liftPosition Lift position in local motor units (e.g. centimeters).
   * @return true if successful, false otherwise.
   */
  bool setLiftPosition(uint16_t liftPosition);
  /**
   * @brief Get the current lift position in local motor units.
   * @return Current lift position.
   */
  uint16_t getLiftPosition();
  /**
   * @brief Set current lift position as a percentage (updates CurrentPositionLiftPercent100ths).
   * @param liftPercent Lift percentage (0–100, 0 = fully open, 100 = fully closed).
   * @return true if successful, false otherwise (including SHUTTER and BLIND_TILT_ONLY).
   * @note When the device reaches the target, call setOperationalState(LIFT, STALL).
   */
  bool setLiftPercentage(uint8_t liftPercent);
  /**
   * @brief Get the current lift percentage.
   * @return Lift percentage (0–100, Matter semantics).
   */
  uint8_t getLiftPercentage();
  /**
   * @brief Set current lift position in Matter percent100ths (0–10000). Primary API for reporting actual position.
   * @param liftPercent100ths Current lift position (0 = open, 10000 = closed).
   * @return true if successful, false otherwise.
   */
  bool setCurrentLiftPercent100ths(uint16_t liftPercent100ths);
  /**
   * @brief Get the current lift position in percent100ths.
   * @return Current lift position (0–10000).
   */
  uint16_t getCurrentLiftPercent100ths();
  /**
   * @brief Set the target lift position in percent100ths.
   * @param liftPercent100ths Target lift position (0–10000, 0 = open, 10000 = closed).
   * @return true if successful, false otherwise.
   * @note This sets the target; update actual position with setLiftPercentage() / setCurrentLiftPercent100ths().
   */
  bool setTargetLiftPercent100ths(uint16_t liftPercent100ths);
  /**
   * @brief Get the current target lift position in percent100ths.
   *
   * During onGoToLiftPercentage() (PRE_UPDATE) this returns the incoming request
   * cached from the write, which is the value to copy into the current lift position
   * for an instant move.
   *
   * @return Target lift position (0–10000).
   */
  uint16_t getTargetLiftPercent100ths();

  /**
   * @brief Set the local open limit for lift motor calibration.
   * @param openLimit Open limit in motor units (e.g. centimeters).
   * @return true if successful, false otherwise.
   */
  bool setInstalledOpenLimitLift(uint16_t openLimit);
  /**
   * @brief Get the local open limit for lift motor calibration.
   * @return Open limit in motor units.
   */
  uint16_t getInstalledOpenLimitLift();
  /**
   * @brief Set the local closed limit for lift motor calibration.
   * @param closedLimit Closed limit in motor units (e.g. centimeters).
   * @return true if successful, false otherwise.
   */
  bool setInstalledClosedLimitLift(uint16_t closedLimit);
  /**
   * @brief Get the local closed limit for lift motor calibration.
   * @return Closed limit in motor units.
   */
  uint16_t getInstalledClosedLimitLift();
  /**
   * @brief Set the local lift motor calibration range.
   * @param calibration Open/closed physical units.
   * @return true if successful, false otherwise.
   */
  bool setLiftCalibration(const PositionCalibration &calibration);
  /**
   * @brief Get the local lift motor calibration range.
   * @return Current lift calibration.
   */
  PositionCalibration getLiftCalibration();

  /**
   * @brief Set tilt position (rotation converted via installed limits).
   * @param tiltPosition Tilt position (absolute value for conversion, not a physical unit).
   * @return true if successful, false otherwise.
   */
  bool setTiltPosition(uint16_t tiltPosition);
  /**
   * @brief Get the current tilt position (absolute value for conversion, not a physical unit).
   * @return Current tilt position.
   */
  uint16_t getTiltPosition();
  /**
   * @brief Set current tilt position as a percentage (updates CurrentPositionTiltPercent100ths).
   * @param tiltPercent Tilt percentage (0–100, 0 = fully open, 100 = fully closed).
   * @return true if successful, false otherwise.
   * @note When the device reaches the target, call setOperationalState(TILT, STALL).
   */
  bool setTiltPercentage(uint8_t tiltPercent);
  /**
   * @brief Get the current tilt percentage.
   * @return Tilt percentage (0–100, Matter semantics).
   */
  uint8_t getTiltPercentage();
  /**
   * @brief Set current tilt position in Matter percent100ths (0–10000).
   * @param tiltPercent100ths Current tilt position (0 = open, 10000 = closed).
   * @return true if successful, false otherwise.
   */
  bool setCurrentTiltPercent100ths(uint16_t tiltPercent100ths);
  /**
   * @brief Get the current tilt position in percent100ths.
   * @return Current tilt position (0–10000).
   */
  uint16_t getCurrentTiltPercent100ths();
  /**
   * @brief Set the target tilt position in percent100ths.
   * @param tiltPercent100ths Target tilt position (0–10000, 0 = open, 10000 = closed).
   * @return true if successful, false otherwise.
   */
  bool setTargetTiltPercent100ths(uint16_t tiltPercent100ths);
  /**
   * @brief Get the current target tilt position in percent100ths.
   * @return Target tilt position (0–10000).
   */
  uint16_t getTargetTiltPercent100ths();

  /**
   * @brief Set the local open limit for tilt motor calibration.
   * @param openLimit Open limit value for tilt conversion.
   * @return true if successful, false otherwise.
   * @note Tilt is a rotation, not a linear measurement.
   */
  bool setInstalledOpenLimitTilt(uint16_t openLimit);
  /**
   * @brief Get the local open limit for tilt motor calibration.
   * @return Open limit for tilt conversion.
   */
  uint16_t getInstalledOpenLimitTilt();
  /**
   * @brief Set the local closed limit for tilt motor calibration.
   * @param closedLimit Closed limit value for tilt conversion.
   * @return true if successful, false otherwise.
   */
  bool setInstalledClosedLimitTilt(uint16_t closedLimit);
  /**
   * @brief Get the local closed limit for tilt motor calibration.
   * @return Closed limit for tilt conversion.
   */
  uint16_t getInstalledClosedLimitTilt();
  /**
   * @brief Set the local tilt motor calibration range.
   * @param calibration Open/closed conversion range.
   * @return true if successful, false otherwise.
   */
  bool setTiltCalibration(const PositionCalibration &calibration);
  /**
   * @brief Get the local tilt motor calibration range.
   * @return Current tilt calibration.
   */
  PositionCalibration getTiltCalibration();

  /**
   * @brief Set the window covering Type attribute.
   *
   * Lift/Tilt FeatureMap bits are fixed at begin(). A type that needs a different
   * feature set (for example shutter to roller shade) is rejected; recreate the
   * endpoint instead. Types in the same family succeed.
   *
   * @param coveringType Window covering type.
   * @return true if successful, false otherwise.
   */
  bool setCoveringType(WindowCoveringType_t coveringType);
  /**
   * @brief Get the current window covering type.
   * @return Current covering type.
   */
  WindowCoveringType_t getCoveringType();

  /**
   * @brief Set the full operational status bitmap.
   * @param operationalStatus Full operational status bitmap value.
   * @return true if successful, false otherwise.
   * @note Prefer setOperationalState() to set individual field states.
   */
  bool setOperationalStatus(uint8_t operationalStatus);
  /**
   * @brief Get the full operational status bitmap.
   * @return Current operational status bitmap.
   */
  uint8_t getOperationalStatus();

  /**
   * @brief Set the operational state for LIFT or TILT. GLOBAL is updated automatically (LIFT > TILT).
   * @param field Field to set (LIFT or TILT). GLOBAL cannot be set directly.
   * @param state Operational state (STALL, MOVING_UP_OR_OPEN, or MOVING_DOWN_OR_CLOSE).
   * @return true if successful, false otherwise.
   */
  bool setOperationalState(OperationalStatusField_t field, OperationalState_t state);
  /**
   * @brief Get the operational state for a specific field.
   * @param field Field to get (GLOBAL, LIFT, or TILT).
   * @return Operational state for that field.
   */
  OperationalState_t getOperationalState(OperationalStatusField_t field);

  /**
   * @brief Callback invoked when the UpOrOpen command is received (target 0% = fully open).
   * @return true if handled successfully.
   */
  using EndPointOpenCB = std::function<bool()>;
  /**
   * @brief Register a callback for the UpOrOpen command.
   * @param onChangeCB Function to call when UpOrOpen is received.
   */
  void onOpen(EndPointOpenCB onChangeCB) {
    _onOpenCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when the DownOrClose command is received (target 100% = fully closed).
   * @return true if handled successfully.
   */
  using EndPointCloseCB = std::function<bool()>;
  /**
   * @brief Register a callback for the DownOrClose command.
   * @param onChangeCB Function to call when DownOrClose is received.
   */
  void onClose(EndPointCloseCB onChangeCB) {
    _onCloseCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when TargetPositionLiftPercent100ths changes.
   *
   * Returning true accepts the target write. The sketch must then report
   * CurrentPosition (and Stall) when the covering actually arrives — CHIP leaves
   * OperationalStatus at Opening/Closing until Current equals Target.
   *
   * @param liftPercent Target lift percentage (0–100, 0 = open, 100 = closed).
   * @return true to accept the target write.
   */
  using EndPointLiftCB = std::function<bool(uint8_t)>;
  /**
   * @brief Register a callback for target lift percentage changes.
   * @param onChangeCB Function to call when target lift percentage changes.
   */
  void onGoToLiftPercentage(EndPointLiftCB onChangeCB) {
    _onGoToLiftPercentageCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when TargetPositionTiltPercent100ths changes.
   * @param tiltPercent Target tilt percentage (0–100, 0 = open, 100 = closed).
   * @return true to accept the target write.
   */
  using EndPointTiltCB = std::function<bool(uint8_t)>;
  /**
   * @brief Register a callback for target tilt percentage changes.
   * @param onChangeCB Function to call when target tilt percentage changes.
   */
  void onGoToTiltPercentage(EndPointTiltCB onChangeCB) {
    _onGoToTiltPercentageCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when the StopMotion command is received.
   * @return true if handled successfully.
   */
  using EndPointStopCB = std::function<bool()>;
  /**
   * @brief Register a callback for the StopMotion command.
   * @param onChangeCB Function to call when StopMotion is received.
   */
  void onStop(EndPointStopCB onChangeCB) {
    _onStopCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when CurrentPosition lift or tilt attributes change.
   *
   * Not automatically called when Matter commands execute (those modify TargetPosition).
   *
   * @param liftPercent Current lift percentage (0–100).
   * @param tiltPercent Current tilt percentage (0–100).
   * @return true if handled successfully.
   */
  using EndPointCB = std::function<bool(uint8_t, uint8_t)>;
  /**
   * @brief Register a callback for current position attribute changes.
   * @param onChangeCB Function to call when current position attributes change.
   */
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  /**
   * @brief Apply the current Matter internal state to the accessory.
   *
   * Calls the registered @ref onChange callback with the current state.
   */
  void updateAccessory();

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Can be overridden by the application if necessary.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully.
   */
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  uint8_t currentLiftPercent = 0;
  uint16_t currentLiftPercent100ths = 0;
  uint16_t currentLiftPosition = 0;
  uint16_t targetLiftPercent100ths = 0;
  uint8_t currentTiltPercent = 0;
  uint16_t currentTiltPercent100ths = 0;
  uint16_t currentTiltPosition = 0;
  uint16_t targetTiltPercent100ths = 0;
  uint16_t installedOpenLimitLift = 0;
  uint16_t installedClosedLimitLift = 65534;
  uint16_t installedOpenLimitTilt = 0;
  uint16_t installedClosedLimitTilt = 65534;
  WindowCoveringType_t coveringType = ROLLERSHADE;
  bool liftFeatureEnabled = false;
  bool tiltFeatureEnabled = false;

  bool requireLiftFeature() const;
  bool requireTiltFeature() const;

  EndPointOpenCB _onOpenCB = NULL;
  EndPointCloseCB _onCloseCB = NULL;
  EndPointLiftCB _onGoToLiftPercentageCB = NULL;
  EndPointTiltCB _onGoToTiltPercentageCB = NULL;
  EndPointStopCB _onStopCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
