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

namespace chip::app::Clusters {
class ValveConfigurationAndControlCluster;
}  // namespace chip::app::Clusters

namespace chip::System {
class Layer;
}  // namespace chip::System

/**
 * @brief Matter water valve endpoint (device type 0x0042, Valve Configuration and Control).
 *
 * Open indefinitely or for a timed duration (automatic close when the countdown
 * elapses). Open/Close only (no Level/percent control). CurrentState / TargetState /
 * OpenDuration / RemainingDuration are managed by the CHIP cluster, so they cannot
 * be read or written through getAttributeVal() / setAttributeVal() / updateAttributeVal().
 * Local open() / close() after Matter.begin() use the same paths as a Matter controller.
 */
class MatterWaterValve : public MatterEndPoint {
public:
  /**
   * @brief ValveStateEnum values. This library reports CLOSED or OPEN via the delegate (TRANSITIONING is not set locally).
   */
  enum ValveState_t {
    VALVE_STATE_CLOSED = (uint8_t)chip::app::Clusters::ValveConfigurationAndControl::ValveStateEnum::kClosed,               ///< Valve is closed.
    VALVE_STATE_OPEN = (uint8_t)chip::app::Clusters::ValveConfigurationAndControl::ValveStateEnum::kOpen,                   ///< Valve is open.
    VALVE_STATE_TRANSITIONING = (uint8_t)chip::app::Clusters::ValveConfigurationAndControl::ValveStateEnum::kTransitioning, ///< Spec value; not set locally.
  };

  /**
   * @brief ValveFaultBitmap values. Combine with bitwise OR.
   */
  enum ValveFault_t {
    VALVE_FAULT_GENERAL_FAULT = (uint16_t)chip::app::Clusters::ValveConfigurationAndControl::ValveFaultBitmap::kGeneralFault,
    VALVE_FAULT_BLOCKED = (uint16_t)chip::app::Clusters::ValveConfigurationAndControl::ValveFaultBitmap::kBlocked,
    VALVE_FAULT_LEAKING = (uint16_t)chip::app::Clusters::ValveConfigurationAndControl::ValveFaultBitmap::kLeaking,
    VALVE_FAULT_NOT_CONNECTED = (uint16_t)chip::app::Clusters::ValveConfigurationAndControl::ValveFaultBitmap::kNotConnected,
    VALVE_FAULT_SHORT_CIRCUIT = (uint16_t)chip::app::Clusters::ValveConfigurationAndControl::ValveFaultBitmap::kShortCircuit,
    VALVE_FAULT_CURRENT_EXCEEDED = (uint16_t)chip::app::Clusters::ValveConfigurationAndControl::ValveFaultBitmap::kCurrentExceeded,
  };

  /**
   * @brief Create a new Matter water valve endpoint.
   */
  MatterWaterValve();
  /**
   * @brief Destroy the water valve endpoint object.
   */
  ~MatterWaterValve();
  /**
   * @brief Create the endpoint and allocate the valve delegate. Call before @c Matter.begin().
   * @param defaultOpenDurationSeconds Reported as DefaultOpenDuration when non-zero; used by
   *        open() with no argument. 0 omits DefaultOpenDuration and open() (no argument) opens indefinitely.
   * @return true if successful, false otherwise.
   */
  bool begin(uint32_t defaultOpenDurationSeconds = 0);
  /**
   * @brief Stop processing Matter water valve events and release the valve delegate.
   */
  void end();

  /**
   * @brief Open using defaultOpenDurationSeconds from begin() (0 = open indefinitely).
   *
   * Requires Matter.begin() so the live cluster exists.
   *
   * @return true if successful, false otherwise (including when onOpen() returns false).
   */
  bool open();
  /**
   * @brief Open for durationSeconds, then close automatically (0 = open indefinitely).
   *
   * Requires Matter.begin() so the live cluster exists.
   *
   * @param durationSeconds Open duration in seconds, or 0 to open indefinitely regardless of the default.
   * @return true if successful, false otherwise (including when onOpen() returns false).
   */
  bool open(uint32_t durationSeconds);
  /**
   * @brief Close the valve. Requires Matter.begin() so the live cluster exists.
   * @return true if successful, false otherwise.
   */
  bool close();

  /**
   * @brief Get the valve's current state.
   * @return Current ValveState_t.
   */
  ValveState_t getCurrentState() {
    return currentState;
  }
  /**
   * @brief Get the valve's target state.
   * @return Target ValveState_t.
   */
  ValveState_t getTargetState() {
    return targetState;
  }
  /**
   * @brief Whether the valve is currently open.
   * @return true if current state is OPEN.
   */
  bool isOpen() {
    return currentState == VALVE_STATE_OPEN;
  }

  /**
   * @brief Duration in seconds used for the current/last timed open (0 = opened indefinitely).
   * @return Open duration in seconds.
   */
  uint32_t getOpenDuration() {
    return openDuration;
  }
  /**
   * @brief Default open duration used by open() when no explicit duration is given (0 = none configured).
   * @return Default open duration in seconds.
   */
  uint32_t getDefaultOpenDuration() {
    return defaultOpenDuration;
  }
  /**
   * @brief Remaining duration in seconds of the current timed open, updated automatically once per second.
   * @return Remaining duration in seconds.
   */
  uint32_t getRemainingDuration() {
    return remainingDuration;
  }

  /**
   * @brief Currently reported fault bitmap (combination of ValveFault_t values).
   * @return Fault bitmap.
   */
  uint16_t getValveFault() {
    return valveFault;
  }
  /**
   * @brief Report or clear fault bits. Pass 0 to clear.
   *
   * Call after begin(); may be called before Matter.begin() (cached and applied
   * in onStackStarted()). While any fault bit is set, the cluster rejects Open
   * commands (controller and local open()).
   *
   * @param fault Combination of ValveFault_t values, or 0 to clear.
   * @return true if successful, false otherwise.
   */
  bool setValveFault(uint16_t fault);

  /**
   * @brief Callback invoked when the valve is commanded open (controller or local open()).
   *
   * Perform the physical open here; return false if it could not be completed.
   * If no callback is registered, Matter state still moves to Open. Runs with
   * the CHIP stack lock held — keep work short. A failed Open is rolled back
   * after CHIP OpenValve() returns, which may invoke onClose().
   *
   * @return true if the physical open completed.
   */
  using EndPointOpenCB = std::function<bool()>;
  /**
   * @brief Register a callback for open commands.
   * @param onOpenCB Function to call when the valve is commanded open.
   */
  void onOpen(EndPointOpenCB onOpenCB) {
    _onOpenCB = onOpenCB;
  }

  /**
   * @brief Callback invoked when the valve is commanded closed.
   *
   * Fired by a controller, local close(), end of a timed open, or rollback after
   * a failed onOpen(). No return value; the CHIP delegate does not accept a close failure.
   */
  using EndPointCloseCB = std::function<void()>;
  /**
   * @brief Register a callback for close commands.
   * @param onCloseCB Function to call when the valve is commanded closed.
   */
  void onClose(EndPointCloseCB onCloseCB) {
    _onCloseCB = onCloseCB;
  }

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
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) override;

protected:
  // Attaches the delegate and pushes the cached state/fault into the code-driven cluster.
  void onStackStarted() override;

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

  // onOpen() failed: CloseValve() is owed once OpenValve() has unwound (CHIP starts the countdown after the
  // delegate returns, and the delegate has no error channel). Cleared by the rollback itself or by any close.
  bool openRollbackOwed = false;
  // A timer / deferred work that will run the rollback is currently armed.
  // Local open() closes inline only when nothing is armed, so onClose() never runs twice.
  bool openRollbackArmed = false;

  // Live CHIP cluster object for this endpoint, or nullptr before Matter.begin().
  chip::app::Clusters::ValveConfigurationAndControlCluster *getValveCluster();

  void rollbackFailedOpen();
  // Must be called with the CHIP stack lock held. Retried from the countdown tick while the rollback is owed.
  void armOpenRollback();
  static void rollbackTimerCB(chip::System::Layer *, void *ctx);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
